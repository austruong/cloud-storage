// The 'client.c' code goes here.
// Significant portions of this code was based on the discussion 5 code

#include<stdio.h>
#include<unistd.h>
#include "Md5.c"  // Feel free to include any other .c files that you need in the 'Client Domain'.
#include <stdlib.h>
#include <netdb.h>
#include <sys/types.h>

// Added includes from discussion slides
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#define PORT 9999

// Multithreading code from G4Gs
// #include <pthread.h>
// #include <semaphore.h>

// Need to support these commands:
    // pause    <time>              DONE <-- Client only    | Not affected my MUTEX
    // append   <file_name>         DONE
    // upload   <file_name>         DONE
    // download <file_name>         DONE
    // delete   <file_name>         DONE
    // syncheck <file_name>         In Progress             | Not affected my MUTEX
    // quit                         DONE <-- Client only    | Not affected my MUTEX
    
    // MD5 helper function          DONE
    // Multithreading + Locking     In Progress
// those not with the Client only comment are to processed  by the server
// Can send in the buffer where the MSB is the command so pause would be 1 and quit be 7, followed by the parameter value
// For client, simply need to tokenize it

// This runs after the first send of the parsed commmand to the server
int doesFileExistRemotely(int client_socket, char* fileName, int printError) {
    // first receive the results of the server check
    int flag = 0;
    recv(client_socket, &flag, sizeof(flag), 0);
    
    fileName[strcspn(fileName, "\n")] = 0;
    if( (flag == 0) && (printError == 1) ) {
        printf("File [%s] could not be found in remote directory.\n", fileName);
    }
    
    return flag;
}

int isFileLocked(int client_socket, char* fileName, int printError) {
    int flag = 0;
    recv(client_socket, &flag, sizeof(flag), 0);
    
    fileName[strcspn(fileName, "\n")] = 0;
    
    if( (flag == 1) && (printError == 1) ) {
        printf("File [%s] is currently locked by another user.\n", fileName);
    }
    return flag;
}

// Taken from hw.c from Assignment #4
char** tokenize(char* input) {  
    int size = 0;
    char** tokens;
    char* temp;
    temp = strtok(input, " ");

    // Need to temp initialize the tokens array with empty strings/char arrays, in this case just 1 element as of now
    tokens = calloc(1, sizeof(char*));
    while (temp != NULL) {
        tokens[size] = temp;
        size++;

        // increase the tokens array size by 1 additional element
        tokens = realloc(tokens, sizeof(char*)*(size+1));            
        temp = strtok(NULL, " ");
    }
    // add NULL to the end of the tokens array
    tokens[size] = NULL;

    return tokens;
}

// Based off of discussion 5 code
void clientSend(int client_socket, char* message) {
    ssize_t sent_size;
    sent_size = send(client_socket, message, strlen(message), 0);
}

void clientAppendFile(int client_socket, char* fileName, FILE *cmdPtr) {    
    // if so, now we do our little tokenizer for close, pause, etc.
    // Taken from hw.c from Assignment #4
    char* line = NULL;
    size_t lineLen = 256;
    ssize_t lineSize;
    
    char *inputBuffer =(char*)malloc(256);
    char** args;
    while((lineSize = getline(&line, &lineLen, cmdPtr)) != -1) {
        printf("Appending> %s", line);
        strcpy(inputBuffer, line);
        args = tokenize((char *) inputBuffer);
        line[strcspn(line, "\n")] = 0;
        
        if (!strcmp(args[0], "pause")) {
            sleep(atoi(args[1]));
        } else if (!strcmp(args[0], "close\n")) {
            send(client_socket, "close", strlen("close"), 0);
            int junkData = 0;
            recv(client_socket, &junkData, sizeof(junkData), 0);
            break;
        } else {
            clientSend(client_socket, line);
            int junkData = 0;
            recv(client_socket, &junkData, sizeof(junkData), 0);
        }
    }
}

void clientUpload(int client_socket, char* fileName) {
    fileName[strcspn(fileName, "\n")] = 0;
    int chunk_size = 1000;
    char file_chunk[chunk_size];
    
    if(fopen(fileName, "rb") != NULL) { // Open a file in read-binary mode.
        FILE *fptr = fopen(fileName, "rb");
        fseek(fptr, 0L, SEEK_END);  // Sets the pointer at the end of the file.
        int file_size = ftell(fptr);  // Get file size.
        fseek(fptr, 0L, SEEK_SET);  // Sets the pointer back to the beginning of the file.

        int total_bytes = 0;  // Keep track of how many bytes we read so far.
        int current_chunk_size;  // Keep track of how many bytes we were able to read from file (helpful for the last chunk).
        ssize_t sent_bytes;

        send(client_socket, &file_size, sizeof(file_size), 0);

        while (total_bytes < file_size){
            // Clean the memory of previous bytes.
            // Both 'bzero' and 'memset' works fine.
            bzero(file_chunk, chunk_size);
            // memset(file_chunk, '\0', chunk_size);

            // Read file bytes from file.
            current_chunk_size = fread(&file_chunk, sizeof(char), chunk_size, fptr);

            // Sending a chunk of file to the socket.
            sent_bytes = send(client_socket, &file_chunk, current_chunk_size, 0);

            // Keep track of how many bytes we read/sent so far.
            // total_bytes = total_bytes + current_chunk_size;
            total_bytes = total_bytes + sent_bytes;
        }
        printf("%i bytes uploaded successfully.\n", total_bytes);
        fclose(fptr);
    }
}

void clientDownload(int client_socket, char* fileName) {
    fileName[strcspn(fileName, "\n")] = 0;
    ssize_t received_size;
    int chunk_size = 1000;
    char file_chunk[chunk_size];
    int total_bytes = 0;
    
    // Get's the file's size
    int file_size = 0;
    recv(client_socket, &file_size, sizeof(file_size), 0);

    FILE *fptr;

    // Opening a new file in write-binary mode to write the received file bytes into the disk using fptr.
    fptr = fopen(fileName, "wb");

    // Keep receiving bytes until we receive the whole file.
    // Heavily based off of the piazza post: https://piazza.com/class/l1d2ss2rhcc3uu?cid=426
    while (1){
        bzero(file_chunk, chunk_size);
        memset(&file_chunk, 0, chunk_size);
        int remaining_bytes = file_size - total_bytes;
        if (remaining_bytes <= chunk_size){
            received_size = recv(client_socket, file_chunk, remaining_bytes, 0);
            fwrite(&file_chunk, sizeof(char), received_size, fptr);
            break;
        }
        received_size = recv(client_socket, file_chunk, chunk_size, 0);
        total_bytes += received_size;
        fwrite(&file_chunk, sizeof(char), received_size, fptr);
    }
    fclose(fptr);
    printf("%d bytes downloaded successfully.\n", file_size);
}

void clientSynCheck(int client_socket, char* fileName) {
    fileName[strcspn(fileName, "\n")] = 0;
    int chunk_size = 1000;
    
    fileName[strcspn(fileName, "\n")] = 0;
    if(fopen(fileName, "rb") != NULL) { // if file exists, then we can print out it's data
        FILE *fptr = fopen(fileName, "rb");
        fseek(fptr, 0L, SEEK_END);  // Sets the pointer at the end of the file.
        int file_size = ftell(fptr);  // Get file size.
        fseek(fptr, 0L, SEEK_SET);  // Sets the pointer back to the beginning of the file.
        
        printf("- Local Directory:\n");
        printf("-- File Size: %d bytes.\n", file_size);
        fclose(fptr);
    }
}

// From discussion 5 example slides - client_2.c
int open_clientfd(char* serverAddress) {
    struct sockaddr_in serv_addr;
    
    int client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (client_socket < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    
    // The server IP address should be supplied as an argument when running the application.
    int addr_status = inet_pton(AF_INET, serverAddress, &serv_addr.sin_addr);
    if (addr_status <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }
    
    int connect_status = connect(client_socket, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    if (connect_status < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }
    return client_socket;
}

int main(int argc, char *argv[]) {
    char* cmdFileName = argv[1];
    char* serverAddress = argv[2];
    int client_socket = open_clientfd(serverAddress);
    // where client_example_1(client_socket) was called
    if(client_socket != -1) {
        printf("Welcome to ICS53 Online Cloud Storage.\n");
        FILE *cmdPtr;
        size_t lineLen = 0;
        ssize_t lineSize;
        cmdPtr = fopen(cmdFileName, "r");
        
        // Taken from hw.c from Assignment #4
        char* line = NULL;
        char** args;
        chdir("./Local Directory");    // Stay here, since we only ever interact with the files in this directory
        while((lineSize = getline(&line, &lineLen, cmdPtr)) != -1) {
            line[strcspn(line, "\n")] = 0;
            printf("> %s\n", line);
            args = tokenize((char *) line);

            if (!strcmp(args[0], "pause")) {
                printf("pause %s", args[1]);
                sleep(atoi(args[1]));
                
            } else if (!strcmp(args[0], "quit\n") || !strcmp(args[0], "quit")) {
                clientSend(client_socket, args[0]);
                close(client_socket);
                break;
                
            } else if ((args[1] != NULL) && ( (!strcmp(args[0], "append")) || (!strcmp(args[0], "upload")) || (!strcmp(args[0], "download")) || (!strcmp(args[0], "delete")) || (!strcmp(args[0], "syncheck")) )){
                char* temp = malloc(100);
                strcat(temp, args[0]);
                strcat(temp, " ");
                strcat(temp, args[1]);

                if (!strcmp(args[0], "append")) {
                    clientSend(client_socket, temp);
                    
                    // receive if the file exists on the remote directory
                    if(doesFileExistRemotely(client_socket, args[1], 1) == 1){
                        clientSend(client_socket, "1");
                        // if so, then check if the file is locked
                        if(isFileLocked(client_socket, args[1], 1) == 0) {
                            clientAppendFile(client_socket, args[1], cmdPtr);
                        }
                    }
                    
                } else if (!strcmp(args[0], "upload")) {
                    if(fopen(args[1], "rb") != NULL) {
                        clientSend(client_socket, temp);
                        if(isFileLocked(client_socket, args[1], 1) == 0) {
                            clientUpload(client_socket, args[1]);
                        }
                    } else {
                        printf("File [%s] could not be found in local directory.\n", args[1]);
                    }

                } else if (!strcmp(args[0], "download")) {
                    clientSend(client_socket, temp);
                    
                    // receive if the file exists on the remote directory
                    if(doesFileExistRemotely(client_socket, args[1], 1) == 1){
                        clientSend(client_socket, "1");
                        // if so, then check if the file is locked
                        if(isFileLocked(client_socket, args[1], 1) == 0) {
                            clientDownload(client_socket, args[1]);
                        }
                    }
                    
                } else if (!strcmp(args[0], "delete")) {
                    clientSend(client_socket, temp);
                    // receive if the file exists on the remote directory
                    if(doesFileExistRemotely(client_socket, args[1], 1) == 1){
                        clientSend(client_socket, "1");
                        isFileLocked(client_socket, args[1], 1);
                    }
                    
                } else if (!strcmp(args[0], "syncheck")) {
                    printf("Sync Check Report:\n");
                    clientSynCheck(client_socket, args[1]);
                    
                    // Remote Server Syncheck
                    clientSend(client_socket, temp);
                    if(doesFileExistRemotely(client_socket, args[1], 0) == 1){
                        clientSend(client_socket, "1");
                        
                        // Receive Server's File Size
                        int serverFileSize = 0;
                        recv(client_socket, &serverFileSize, sizeof(serverFileSize), 0);
                        
                        // Send Client's File Hash                        
                        unsigned char* clientHash;
                        clientHash = (unsigned char *)malloc(MD5_DIGEST_LENGTH * sizeof(unsigned char));
                        if(fopen(args[1], "rb") != NULL) {
                            clientHash = md5HashFunction(args[1]);
                        }
                        send(client_socket, clientHash, sizeof(clientHash)*2, 0);
                        // Receive Sync Status
                        int syncStatus = 0;
                        recv(client_socket, &syncStatus, sizeof(syncStatus), 0);
                        
                        clientSend(client_socket, "1");
                        
                        // Receive Lock Status
                        int synLockStatus = isFileLocked(client_socket, args[1], 0);
                        
                        printf("- Remote Directory:\n");
                        printf("-- File Size: %d bytes.\n", serverFileSize);

                        if(syncStatus == 0)
                            printf("-- Sync Status: unsynced.\n");
                        else
                            printf("-- Sync Status: synced.\n");

                        if(synLockStatus == 0)
                            printf("-- Lock Status: unlocked.\n");
                        else
                            printf("-- Lock Status: locked.\n");
                    }
                }
                memset(temp, 0, sizeof(temp));
            } else {
                line[strcspn(line, "\n")] = 0;
                printf("Command [%s] is not recognized.\n", line);
            }
        }
        fclose(cmdPtr);
        return 0;
    }
}