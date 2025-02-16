#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <string.h>
#include <windows.h>
#include <direct.h>
#include <sys/stat.h>

#pragma comment(lib, "ws2_32.lib")

#define MAX_CLIENTS 100
#define USERNAME_SIZE 50
#define PASSWORD_SIZE 50
#define BUFFER_SIZE 1024
#define QUEUE_SIZE 10
#define CLIENT_STORAGE_LIMIT 10240  // 10 KB limit
// User structure to store user credentials
typedef struct {
    char username[USERNAME_SIZE];
    char password[PASSWORD_SIZE];
    char clientFolder[256];
} User;

// Global user records array
User userRecords[MAX_CLIENTS];
int registeredUsers = 0;

// Function to check if the user exists
int authenticate_user(const char *username, const char *password) {
    for (int i = 0; i < registeredUsers; i++) {
        if (strcmp(userRecords[i].username, username) == 0 && 
            strcmp(userRecords[i].password, password) == 0) {
            return 1;  // User authenticated
        }
    }
    return 0;  // Authentication failed
}

// Function to register a new user
int register_user(const char *username, const char *password) {
    if (registeredUsers >= MAX_CLIENTS) return -1;  // User limit reached
    strcpy(userRecords[registeredUsers].username, username);
    strcpy(userRecords[registeredUsers].password, password);
    registeredUsers++;
    return 0;  // Registration successful
}

int check_available_space(const char* folderPath, int requiredSpace) {
    WIN32_FIND_DATA findFileData;
    HANDLE hFind = INVALID_HANDLE_VALUE;

    char searchPath[512];
    sprintf(searchPath, "%s\\*", folderPath);

    hFind = FindFirstFile(searchPath, &findFileData);

    if (hFind == INVALID_HANDLE_VALUE) {
        return 0;  // Unable to find files
    }

    int totalSize = 0;

    do {
        if (!(findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            totalSize += findFileData.nFileSizeLow;
        }
    } while (FindNextFile(hFind, &findFileData) != 0);

    FindClose(hFind);

    // Calculate available space
    int availableSpace = CLIENT_STORAGE_LIMIT - totalSize;
    return availableSpace >= requiredSpace;  // Return 1 if space is sufficient, else 0
}
typedef struct {
    SOCKET clientsocket;
} clientinfo;

typedef struct {
    char filePath[512];
    char buffer[BUFFER_SIZE];
    int isWrite;  // 1 for write, 0 for read
} FileOperation;

WSADATA wsData;

// Shared queue to hold file operations
FileOperation queue[QUEUE_SIZE];
int queueStart = 0;
int queueEnd = 0;
int queueCount = 0;

HANDLE mutex;          // Mutex for queue synchronization
HANDLE semaphore;      // Semaphore to signal when the queue is not empty

void fileTimeToString(FILETIME ft, char* dateStr) {
    SYSTEMTIME stUTC, stLocal;
    FileTimeToSystemTime(&ft, &stUTC);
    SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);
    sprintf(dateStr, "%02d/%02d/%d %02d:%02d", 
        stLocal.wDay, stLocal.wMonth, stLocal.wYear, 
        stLocal.wHour, stLocal.wMinute);
}

void listFilesInDirectory(const char* folderPath, char* fileDetails) {
    WIN32_FIND_DATA findFileData;
    HANDLE hFind = INVALID_HANDLE_VALUE;

    char searchPath[512];
    sprintf(searchPath, "%s\\*", folderPath);

    hFind = FindFirstFile(searchPath, &findFileData);

    if (hFind == INVALID_HANDLE_VALUE) {
        strcpy(fileDetails, "$FAILURE$NO_CLIENT_DATA$");
        return;
    }

    int hasFiles = 0;
    do {
        if (!(findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            hasFiles = 1;
            
            strcat(fileDetails, findFileData.cFileName);
            strcat(fileDetails, " | ");

            char fileSize[64];
            sprintf(fileSize, "Size: %lld bytes | ", (long long)findFileData.nFileSizeLow);
            strcat(fileDetails, fileSize);

            char dateStr[64];
            fileTimeToString(findFileData.ftCreationTime, dateStr);
            strcat(fileDetails, "Date: ");
            strcat(fileDetails, dateStr);
            strcat(fileDetails, "\n");
        }
    } while (FindNextFile(hFind, &findFileData) != 0);

    FindClose(hFind);

    if (!hasFiles) {
        strcpy(fileDetails, "$FAILURE$NO_CLIENT_DATA$");
    }
}

// File handler thread function
DWORD WINAPI fileHandler(LPVOID lpParam) {
    while (1) {
        // Wait for the semaphore to signal that the queue is not empty
        WaitForSingleObject(semaphore, INFINITE);

        // Lock the mutex to access the queue
        WaitForSingleObject(mutex, INFINITE);

        if (queueCount > 0) {
            // Dequeue the next file operation
            FileOperation operation = queue[queueStart];
            queueStart = (queueStart + 1) % QUEUE_SIZE;
            queueCount--;

            // Release the mutex after accessing the queue
            ReleaseMutex(mutex);

            // Perform the file operation
            if (operation.isWrite) {
                // Write operation
                FILE *file = fopen(operation.filePath, "wb");
                if (file) {
                    fwrite(operation.buffer, 1, BUFFER_SIZE, file);
                    fclose(file);
                    printf("File %s written successfully\n", operation.filePath);
                } else {
                    printf("Failed to write file %s\n", operation.filePath);
                }
            } else {
                // Read operation
                FILE *file = fopen(operation.filePath, "rb");
                if (file) {
                    fread(operation.buffer, 1, BUFFER_SIZE, file);
                    fclose(file);
                    printf("File %s read successfully\n", operation.filePath);
                } else {
                    printf("Failed to read file %s\n", operation.filePath);
                }
            }
        }

        // If the queue is empty, the handler will wait for the next semaphore signal
    }
    return 0;
}

// Client handler thread
DWORD WINAPI clientHandler(LPVOID lpParam) {
    clientinfo* newclient = (clientinfo*)lpParam;
    SOCKET clientSocket = newclient->clientsocket;
    int task, receivedBytes;
    char buffer[BUFFER_SIZE];
    char  fileName[256], filePath[512];
    char fileDetails[BUFFER_SIZE * 10];  // Larger buffer to store file details

    User user;  // Declare a User struct

    // Receive username and password from the client
    recv(clientSocket, user.username, USERNAME_SIZE, 0);
    recv(clientSocket, user.password, PASSWORD_SIZE, 0);

    // Check for user authentication
    if (authenticate_user(user.username, user.password)) {
        send(clientSocket, "AUTH_SUCCESS", strlen("AUTH_SUCCESS") + 1, 0);
    } else {
        if (register_user(user.username, user.password) == 0) {
            send(clientSocket, "REGISTRATION_SUCCESS", strlen("REGISTRATION_SUCCESS") + 1, 0);
        } else {
            send(clientSocket, "USER_LIMIT_REACHED", strlen("USER_LIMIT_REACHED") + 1, 0);
        }
    }
  // Set the client folder name to "username_password"
snprintf(user.clientFolder, sizeof(user.clientFolder), "%s_%s", user.username, user.password);

    user.clientFolder[sizeof(user.clientFolder) - 1] = '\0';  // Null-terminate
    if (recv(clientSocket, (char*)&task, sizeof(task), 0) == SOCKET_ERROR) {
        fprintf(stderr, "Failed to receive task\n");
        closesocket(clientSocket);
        return 1;
    }
  

    _mkdir(user.clientFolder);

    if (task == 1) {  // Upload
        if ((receivedBytes = recv(clientSocket, fileName, sizeof(fileName) - 1, 0)) == SOCKET_ERROR) {
            fprintf(stderr, "Failed to receive file name\n");
            closesocket(clientSocket);
            return 1;
        }
        fileName[receivedBytes] = '\0';
        sprintf(filePath, "%s/%s", user.clientFolder, fileName);
if (!check_available_space(user.clientFolder, receivedBytes)) {
            const char *failureMessage = "$FAILURE$LOW_SPACE$";
            send(clientSocket, failureMessage, strlen(failureMessage), 0);
            closesocket(clientSocket);
            return 1;  // Terminate client connection
        }
        // Queue the write operation
        WaitForSingleObject(mutex, INFINITE);  // Lock queue access
        if (queueCount < QUEUE_SIZE) {
            FileOperation op;
            strcpy(op.filePath, filePath);
            recv(clientSocket, buffer, sizeof(buffer), 0);  // Receive file data
            memcpy(op.buffer, buffer, BUFFER_SIZE);
            op.isWrite = 1;
            queue[queueEnd] = op;
            queueEnd = (queueEnd + 1) % QUEUE_SIZE;
            queueCount++;
        }
        ReleaseMutex(mutex);  // Unlock queue access

        ReleaseSemaphore(semaphore, 1, NULL);  // Signal the file handler
    } else if (task == 2) {  // Download
        if ((receivedBytes = recv(clientSocket, fileName, sizeof(fileName) - 1, 0)) == SOCKET_ERROR) {
            fprintf(stderr, "Failed to receive file name\n");
            closesocket(clientSocket);
            return 1;
        }
        fileName[receivedBytes] = '\0';
        sprintf(filePath, "%s/%s", user.clientFolder, fileName);

        // Queue the read operation
        WaitForSingleObject(mutex, INFINITE);  // Lock queue access
        if (queueCount < QUEUE_SIZE) {
            FileOperation op;
            strcpy(op.filePath, filePath);
            op.isWrite = 0;
            queue[queueEnd] = op;
            queueEnd = (queueEnd + 1) % QUEUE_SIZE;
            queueCount++;
        }
        ReleaseMutex(mutex);  // Unlock queue access

        ReleaseSemaphore(semaphore, 1, NULL);  // Signal the file handler

        // Wait for the file handler to process the request
        Sleep(1000);  // Adjust as needed for proper synchronization

        // Send the file data to the client
        FILE *file = fopen(filePath, "rb");
        if (file) {
            char buffer[BUFFER_SIZE];
            size_t bytesRead;
            while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) {
                send(clientSocket, buffer, bytesRead, 0);
            }
            fclose(file);
        } else {
        // If file not found, send failure message to client
        const char *failureMessage = "$FAILURE$FILE_NOT_FOUND$";
        send(clientSocket, failureMessage, strlen(failureMessage), 0);
    }
    }
     else if (task == 3) {  // List files in client folder
        memset(fileDetails, 0, sizeof(fileDetails));  // Clear the buffer to store file details
        listFilesInDirectory(user.clientFolder, fileDetails);  // List the files

        // Send file details back to the client
        send(clientSocket, fileDetails, strlen(fileDetails), 0);
    }

    closesocket(clientSocket);
    return 0;
}

int main() {
    if (WSAStartup(MAKEWORD(2, 2), &wsData) != 0) {
        fprintf(stderr, "Failed to initialize Winsock\n");
        return 1;
    }

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        fprintf(stderr, "Failed to create socket\n");
        WSACleanup();
        return 1;
    }

    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(12345);

    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        fprintf(stderr, "Bind failed\n");
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }
    printf("Server is listening on port 12345...\n");

    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        fprintf(stderr, "Listen failed\n");
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    mutex = CreateMutex(NULL, FALSE, NULL);
    semaphore = CreateSemaphore(NULL, 0, QUEUE_SIZE, NULL);

    // File handler thread
    HANDLE fileHandlerThread = CreateThread(NULL, 0, fileHandler, NULL, 0, NULL);

    while (1) {
        struct sockaddr_in clientAddr;
        int clientAddrSize = sizeof(clientAddr);
        SOCKET clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrSize);
        if (clientSocket == INVALID_SOCKET) {
            fprintf(stderr, "Failed to accept connection\n");
            continue;
        }

        clientinfo* newClient = (clientinfo*)malloc(sizeof(clientinfo));
        newClient->clientsocket = clientSocket;

        HANDLE clientThread = CreateThread(NULL, 0, clientHandler, newClient, 0, NULL);
        CloseHandle(clientThread);
    }

    WaitForSingleObject(fileHandlerThread, INFINITE);

    CloseHandle(mutex);
    CloseHandle(semaphore);
    closesocket(serverSocket);
    WSACleanup();

    return 0;
}