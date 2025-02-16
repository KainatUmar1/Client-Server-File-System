#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <string.h>

#pragma comment(lib, "ws2_32.lib")

#define BUFFER_SIZE 1024

// XOR encryption/decryption function
void xor_encrypt_decrypt(char *data, int dataLen, const char *key) {
    int keyLen = strlen(key);
    for (int i = 0; i < dataLen; ++i) {
        data[i] ^= key[i % keyLen];  // XOR with the key, wrapping around as needed
    }
}

// RLE compression function
void rle_compress(const char *input, char *output, int inputLength) {
    int count;
    while (*input) {
        *output++ = *input;
        count = 1;
        while (*input == *(input + 1) && count < 255) {
            input++;
            count++;
        }
        *output++ = count;
        input++;
    }
    *output = '\0';
}

// RLE decompression function
void rle_decompress(const char *input, char *output) {
    while (*input) {
        char character = *input++;
        int count = *input++;
        while (count--) {
            *output++ = character;
        }
    }
    *output = '\0';
}

int main() {
    // Initialize Winsock
    WSADATA wsData;
    if (WSAStartup(MAKEWORD(2, 2), &wsData) != 0) {
        fprintf(stderr, "Failed to initialize Winsock\n");
        return 1;
    }

    // Create a socket
    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == INVALID_SOCKET) {
        fprintf(stderr, "Failed to create socket\n");
        WSACleanup();
        return 1;
    }

    // Set up server address structure
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(12345);  // Server port
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");  // Server IP address

    // Connect to the server
    if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        fprintf(stderr, "Failed to connect to the server\n");
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    // Task selection and folder name input
    int task;
    char clientFolder[256], fileName[256], buffer[BUFFER_SIZE];
    printf("Enter task (1 for upload, 2 for download, 3 for view): ");
    scanf("%d", &task);
     strcpy(clientFolder, "client1");

    // Send task and client folder to the server
    send(clientSocket, (char*)&task, sizeof(task), 0);
    send(clientSocket, clientFolder, strlen(clientFolder) + 1, 0);

    if (task == 1) {  // Upload
        // File upload logic
        printf("Enter file name to upload: ");
        scanf("%s", fileName);
        FILE *file = fopen(fileName, "rb");
        if (file == NULL) {
            fprintf(stderr, "File does not exist.\n");
            closesocket(clientSocket);
            WSACleanup();
            return 1;
        }

        // Send the file name to the server
        send(clientSocket, fileName, strlen(fileName) + 1, 0);

        int bytesRead;
        while ((bytesRead = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
            // Encrypt the buffer
            xor_encrypt_decrypt(buffer, bytesRead, "encryptionkey");

            // Compress the encrypted buffer
            char compressedBuffer[BUFFER_SIZE * 2];
            rle_compress(buffer, compressedBuffer, bytesRead);
            int compressedSize = strlen(compressedBuffer) + 1;

            send(clientSocket, compressedBuffer, compressedSize, 0);
        }
        fclose(file);
        printf("File uploaded, encrypted, and compressed successfully.\n");

    } else if (task == 2) { // Download
    char fileName[256];
    printf("Enter the name of the file you want to download :\n");
    scanf("%s", fileName);

    
    // Send file name with null-termination
    if (send(clientSocket, fileName, strlen(fileName) + 1, 0) == SOCKET_ERROR) {
        fprintf(stderr, "Send file name failed\n");
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    char filePath[512];
    strcpy(filePath, fileName);

    // Check if the file already exists
    int copyNumber = 1;
    FILE *existingFile;
    while ((existingFile = fopen(filePath, "r")) != NULL) {
        fclose(existingFile);  // Close the file handle if it exists

        // Strip off the ".txt" extension for renaming
        char baseName[256];
        strncpy(baseName, fileName, strlen(fileName) - 4); // Remove the last 4 chars (.txt)
        baseName[strlen(fileName) - 4] = '\0';  // Null terminate the string

        // Construct the new file path with _copy(N).txt
        sprintf(filePath, "%s_copy(%d).txt", baseName, copyNumber);
        copyNumber++;
    }

    printf("The path is %s\n", filePath);

    // Now create the new file
    FILE *file = fopen(filePath, "wb");
    if (file == NULL) {
        fprintf(stderr, "Failed to create file.\n");
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    char buffer[BUFFER_SIZE];

        int bytesReceived;
        while ((bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0)) > 0) {
            // Decompress the received buffer
            char decompressedBuffer[BUFFER_SIZE];
            rle_decompress(buffer, decompressedBuffer);

            // Decrypt the decompressed buffer
            xor_encrypt_decrypt(decompressedBuffer, strlen(decompressedBuffer), "encryptionkey");

            fwrite(decompressedBuffer, 1, strlen(decompressedBuffer), file);
        }
        fclose(file);
        printf("File downloaded, decrypted, and decompressed successfully.\n");

    } else if (task == 3) { // View ($VIEW$)
        char buffer[BUFFER_SIZE];
        int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0);
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0'; // Null-terminate the received data

            // Check for failure message
            if (strcmp(buffer, "$FAILURE$NO_CLIENT_DATA$") == 0) {
                printf("No files found in the client folder.\n");
            } else {
                printf("Files in folder:\n%s", buffer);
            }
        } else {
            fprintf(stderr, "Failed to receive file details.\n");
        }
    }

    // Close the socket and clean up
    closesocket(clientSocket);
    WSACleanup();
    return 0;
}
