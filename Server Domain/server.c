// The 'server.c' code goes here.
// Only the server needs multithread to be implemented
// Significant portions of this code was based on the discussion 5 code

#include<stdio.h>
#include<unistd.h>
#include <fcntl.h>
#include "Md5.c"  // Feel free to include any other .c files that you need in the 'Server Domain'.

// Added includes from discussion slides
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#define PORT 9999
#include <arpa/inet.h>

// Multithreading code
#include <pthread.h>
#include <semaphore.h>

// list of files currently locked
struct lockedFile {
    int size;
    char* fileName;
};
struct lockedFile *lockedFilesList;
int numLockedFiles;
int lockFileListInterator = 0;

// FIRST THINGS FIRST, ESTABLISH A CONNECTION BEFORE DOING ANYTHING ELSE
// Server does not take in any in-line terminal commands, it simply accepts/processes commands from connected clients

// Need to support these commands:
    // pause <time>               DONE  <-- Client only | Not affected my MUTEX
    // append <file_name>         DONE
    // upload <file_name>         DONE
    // download <file_name>       DONE
    // delete <file_name>         DONE
    // syncheck <file_name>       In Progress           | Not affected my MUTEX
    // quit                       DONE  <-- Client only | Not affected my MUTEX

    // MD5 helper function        DONE
    // Multithreading + Locking   In Progress
// Can send in the buffer where the MSB is the command so pause would be 1 and quit be 7, followed by the parameter value

int isFileLocked(int client_socket, char* fileName) {
    int status = 0;
    int i;
    for(i = 0; i < numLockedFiles; i++) {
        if(strcmp(lockedFilesList[i].fileName, fileName) == 0) {
            printf("Locked File Located!\n");
            status = 1;
            break;
        }
    }
    
    send(client_socket, &status, sizeof(status), 0);
    return status;
}

int getFileSize(char* fileName) {
    fileName[strcspn(fileName, "\n")] = 0;
    int chunk_size = 1000;
    int file_size = 0;
    
    char buffer[1024];
    
    if(fopen(fileName, "rb") != NULL) { // Open a file in read-binary mode., if it's NULL it doesn't exist
        FILE *fptr = fopen(fileName, "rb");
        fseek(fptr, 0L, SEEK_END);  // Sets the pointer at the end of the file.
        file_size = ftell(fptr);  // Get file size.
        printf("Server: file size = %i bytes\n", file_size);
        fseek(fptr, 0L, SEEK_SET);  // Sets the pointer back to the beginning of the file.
        fclose(fptr);
    }
    return file_size;
}

int doesFileExist(int client_socket, char* fileName) {
    int flagger = 1;            
    fileName[strcspn(fileName, "\n")] = 0;
    if(fopen(fileName, "rb") == NULL) {
        flagger = 0;
    }
    send(client_socket, &flagger, sizeof(flagger), 0);
    
    if(flagger == 0) {
        printf("File [%s] could not be found in local directory.\n", fileName);
    }
    
    return flagger;
}

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

void serverAppendFile(int client_socket, char* fileName) {
    int lockStatus = isFileLocked(client_socket, fileName);
    if ( lockStatus == 0 ) {        
        lockedFilesList[lockFileListInterator].fileName = fileName;
        lockedFilesList[lockFileListInterator].size = getFileSize(fileName);
        lockFileListInterator += 1;

        fileName[strcspn(fileName, "\n")] = 0;
        ssize_t received_size;
        char** tokens;

        printf("Appending %s\n", fileName);
        FILE *fptr;
        int chunk_size = 1000;
        char file_chunk[chunk_size];
        char inputBuffer[chunk_size];
        fptr = fopen(fileName, "a");
        fseek(fptr, 0L, SEEK_END);  // Sets the pointer at the end of the file.
        fputs("\n", fptr);
        while(1) {
            bzero(file_chunk, chunk_size);

            received_size = recv(client_socket, file_chunk, chunk_size, 0);
            printf("Received: %s\n", file_chunk);
            strcpy(inputBuffer, file_chunk);
            printf("Append Data: %s\n", inputBuffer); 
            tokens = tokenize((char *) file_chunk);
            if(!strcmp(tokens[0], "close") || !strcmp(tokens[0], "close\n")) {
                printf("Exiting Appending Terminal\n");
                int flagger = 1; 
                send(client_socket, &flagger, sizeof(flagger), 0);
                fclose(fptr);
                break;
            } else {
                printf("New! %s\n", file_chunk);
                strcat(inputBuffer, "\n");
                fputs(inputBuffer, fptr);
                int flagger = 1; 
                send(client_socket, &flagger, sizeof(flagger), 0);
            }
            printf("Next iter\n");
        }
        
        // Remove file from lockedList
        int i;
        for(i = 0; i < numLockedFiles; i++) {
            if(strcmp(lockedFilesList[i].fileName, fileName) == 0) {
                lockedFilesList[i].fileName = "";
                lockedFilesList[i].size = 0;
                break;
            }
        }
        
                                             
    } else {
        printf("MUTEX: LOCKED\n");
    }
    printf("Outside\n");
}

void serverDeleteFile(int client_socket, char* fileName) {
    int lockStatus = isFileLocked(client_socket, fileName);
    if ( lockStatus == 0 ) {        
        fileName[strcspn(fileName, "\n")] = 0;
        if (remove(fileName) == 0)
          printf("%s deleted successfully.\n", fileName);
       else
          printf("File [%s] could not be found in remote directory.\n", fileName);
        
        // Remove file from lockedList
        int i;
        for(i = 0; i < numLockedFiles; i++) {
            if(strcmp(lockedFilesList[i].fileName, fileName) == 0) {
                lockedFilesList[i].fileName = "";
                lockedFilesList[i].size = 0;
                break;
            }
        }
        
    } else {
        printf("MUTEX: LOCKED\n");
    }
    printf("Outside\n");
}

void serverReceiveFile(int client_socket, char* fileName) {
    int status = 0;
    int i;
    fileName[strcspn(fileName, "\n")] = 0;
    for(i = 0; i < numLockedFiles; i++) {
        if(strcmp(lockedFilesList[i].fileName, fileName) == 0) {
            printf("FOUND!\n");
            status = 1;
            break;
        }
    }
    send(client_socket, &status, sizeof(status), 0);
    printf("%d | %s : %d\n", status, lockedFilesList[status].fileName, lockedFilesList[status].size);
    
    if (status != 1) {
        printf("File Unlocked!\n");
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
        printf("Received %s from client with size = %li\n", fileName, received_size);
        fclose(fptr);
    } else {
        printf("File Locked!\n");
    }
}

void serverProvideFile(int client_socket, char* fileName) {
    int lockStatus = isFileLocked(client_socket, fileName);
    if ( lockStatus == 0 ) {
        printf("Sending Server File!\n");
        
        fileName[strcspn(fileName, "\n")] = 0;
        int chunk_size = 1000;
        char file_chunk[chunk_size];

        if(fopen(fileName, "rb") != NULL) { // Open a file in read-binary mode., if it's NULL it doesn't exist
            FILE *fptr = fopen(fileName, "rb");
            fseek(fptr, 0L, SEEK_END);  // Sets the pointer at the end of the file.
            int file_size = ftell(fptr);  // Get file size.
            printf("Server: file size = %i bytes\n", file_size);
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

                printf("Server: sent to client %li bytes. Total bytes sent so far = %i.\n", sent_bytes, total_bytes);
            }
            fclose(fptr);
        } else {
            printf("File [%s] could not be found in remote directory.\n", fileName);
        }
        
        // Remove file from lockedList
        int i;
        for(i = 0; i < numLockedFiles; i++) {
            if(strcmp(lockedFilesList[i].fileName, fileName) == 0) {
                lockedFilesList[i].fileName = "";
                lockedFilesList[i].size = 0;
                break;
            }
        }
        
    } else {
        printf("MUTEX: LOCKED\n");
    }
    printf("Outside\n");
}

void serverGatherFileData(int client_socket, char* fileName) {
    printf("Server Sync Check!\n");
    // Send file size
    int file_size = getFileSize(fileName);
    send(client_socket, &file_size, sizeof(file_size), 0);
    
    // Determine Server File Hash
    unsigned char* serverHash;
    serverHash = (unsigned char *)malloc(MD5_DIGEST_LENGTH * sizeof(unsigned char));
    serverHash = md5HashFunction(fileName);
    printf("Server Hash Value: ");
    int i;
    for(i = 0; i < 16; i++) printf("%02x", serverHash[i]);
    printf("\n");
    
    // Receive Client File Hash
    unsigned char* clientHash;
    int junk = 0;
    clientHash = (unsigned char *)malloc(MD5_DIGEST_LENGTH * sizeof(unsigned char));
    junk = recv(client_socket, (unsigned char*)clientHash, sizeof(clientHash)*2, 0);
    printf("JUNK: %d\n", junk);
    printf("Client Hash Value: ");
    for(i = 0; i < 16; i++) printf("%02x", clientHash[i]);
    printf("\n");
    
    // Send Syncheck Results
    int isSynch = 1;
    printf("Checking if Hashes are the same...\n");
    if(memcmp(serverHash, clientHash, sizeof(clientHash)*2) != 0)
        isSynch = 0;
    printf("\nSending to Client, isSynch: %d\n", isSynch);
    send(client_socket, &isSynch, sizeof(isSynch), 0);
    
    junk = recv(client_socket, (unsigned char*)clientHash, sizeof(clientHash)*2, 0);
    isFileLocked(client_socket, fileName);
}

void cmdLoop(int client_socket) {
    while(1) {
        char cmdBuffer[1024];
        int received_size = recv(client_socket, cmdBuffer, 1024, 0);
        if (received_size == 0){  // Socket is closed by the other end.
            close(client_socket);
            break;
        }
        char** tokens = tokenize(cmdBuffer);
        
        if (!strcmp(tokens[0], "append")) {
            printf("C%ld-AP> %s || %s", pthread_self(), tokens[0], tokens[1]);
            if(doesFileExist(client_socket, tokens[1]) == 1) {
                int junkData = 0;
                recv(client_socket, &junkData, sizeof(junkData), 0);
                serverAppendFile(client_socket, tokens[1]);
            } else {
                printf("File [%s] does not exist on the remote directory", tokens[1]);
            }

        } else if (!strcmp(tokens[0], "upload")) {
            // when server recieves this command, it downloads the file from the client
            printf("C%ld-UP> %s || %s", pthread_self(), tokens[0], tokens[1]);
            serverReceiveFile(client_socket, tokens[1]);

        } else if (!strcmp(tokens[0], "download")) {
            // when the server recieves this command, it uploads the file to the client
            printf("C%ld-DL> %s || %s", pthread_self(), tokens[0], tokens[1]);
            if(doesFileExist(client_socket, tokens[1]) == 1) {
                int junkData = 0;
                recv(client_socket, &junkData, sizeof(junkData), 0);
                serverProvideFile(client_socket, tokens[1]);
            } else {
                printf("File [%s] does not exist on the remote directory", tokens[1]);
            }

        } else if (!strcmp(tokens[0], "delete")) {
            printf("C%ld-DE> %s || %s", pthread_self(), tokens[0], tokens[1]);
            if(doesFileExist(client_socket, tokens[1]) == 1) {
                int junkData = 0;
                recv(client_socket, &junkData, sizeof(junkData), 0);
                serverDeleteFile(client_socket, tokens[1]);
            }

        } else if (!strcmp(tokens[0], "syncheck")) {
            printf("C%ld-SY> %s || %s", pthread_self(), tokens[0], tokens[1]);
            if(doesFileExist(client_socket, tokens[1]) == 1) {
                int junkData = 0;
                recv(client_socket, &junkData, sizeof(junkData), 0);
                serverGatherFileData(client_socket, tokens[1]);
            }

        } else if (!strcmp(tokens[0], "quit\n")) {
            break;
        }
        memset(cmdBuffer, 0, sizeof(cmdBuffer));
    }
}

// Taken from lecture notes
void *threader(void *vargp) {
    pthread_detach(pthread_self());
    printf("\n[!] Client %ld Connected\n", pthread_self());
    int client_socket = *((int *)vargp);
    cmdLoop(client_socket);
    printf("\n[!] Client %ld Disconnected\n", pthread_self());
    close(client_socket);
}

int main(int argc, char *argv[]) {
    /* create and configure the listening socket */
    char* serverAddress = argv[1];
    
    // Taken from the discussion example slides
    int server_socket, client_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    
    // Creating socket file descriptor
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    
    int socket_status = setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR , &opt, sizeof(opt));
    if (socket_status) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    
    //    address.sin_addr.s_addr = INADDR_ANY;
    // The server IP address should be supplied as an argument when running the application.
    
    address.sin_addr.s_addr = inet_addr(serverAddress); // "128.195.27.25"
    address.sin_port = htons(PORT);
    
    int bind_status = bind(server_socket, (struct sockaddr*)&address, sizeof(address));
    if (bind_status < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    
    // This section is for listening for and connecting client sockets
    int listen_status = listen(server_socket, 10);
    if (listen_status < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    
    printf("=============== Cloud Storage Terminal ===============\n");
    printf("|-> IP address: %s:%d\n", serverAddress, PORT);
    printf("|-> Multithreaded: ACTIVATED\n");
    chdir("./Remote Directory");    // Stay here, since we only ever interact with the files in this directory

    pthread_t tid;
    
    numLockedFiles = 50;
    lockedFilesList = (struct lockedFile *)malloc(numLockedFiles * sizeof(struct lockedFile));
    int i;
    for(i = 0; i < numLockedFiles; i++) {
        lockedFilesList[i].fileName = "temporaryFileNameDoNotReUse";
        lockedFilesList[i].size = 0;
    }
    
    // Only terminates when Ctrl+C'd
    while(1) {
        /* Accept(): wait for a connection request */
        client_socket = accept(server_socket, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        if (client_socket < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }
        
        /* echo(): read and echo input lines from client till EOF */
        pthread_create(&tid, NULL, threader, &client_socket);
    }
}