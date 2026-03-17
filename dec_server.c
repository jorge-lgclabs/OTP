#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>

#define MAX_PIDS 5
#define MAX_MESSAGE_SIZE 81920

pid_t PID_STORE[MAX_PIDS];
const char VALID_CHARS[27] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ ";

void check_text_and_key(char *text, char *key);
int check_validity(char *text);

void init_pid_store();
int is_pid_store_full();
void add_pid(pid_t new_pid);
void remove_pid(pid_t old_pid);
void reap_nohang(pid_t child_pid);
void reap_pids();

char *get_message(int connectionSocket);
void send_message(char *message, int connectionSocket);
int is_message_complete(char *buffer);

void setupAddressStruct(struct sockaddr_in* address, int portNumber);
void connection_handler(int connectionSocket, struct sockaddr_in clientAddress);
void fork_handler(int connectionSocket, struct sockaddr_in clientAddress);

char *decode_message(char *ciphertext, char *key);
void error(const char *msg, const int is_fatal);

// Checks the overall validity of the text:key combo (no special chars, correct lengths)
// Sends error and exits if invali
void check_text_and_key(char *text, char *key) {
    if (!check_validity(text)) {
        free(text);
        free(key);
        error("ERROR: text contains invalid characters", 1);
    }
    if (!check_validity(key)) {
        free(text);
        free(key);
        error("ERROR: Key is invalid", 1);
    }
    if (strlen(text) > strlen(key)) {
        free(text);
        free(key);
        error("ERROR: Key is too short", 1);
    }
}

// Checks that the string is valid (no special chars) returns 0 if invalid, 1 if valid
int check_validity(char *text) {
    int len = strlen(text);
    for (int i = 0; i < len - 1; i++) {
        int result = 0;
        for (int j = 0; j < 27; j++) {
            if (text[i] == VALID_CHARS[j]) {
                result = 1;
                break;
            }
        }
        if (!result) {
            return 0;
        }
    }
    return 1;
}

// Initializes the array which stores the PIDs
void init_pid_store() {
    for (int i = 0; i < MAX_PIDS; i++) {
        PID_STORE[i] = -1;
    }
}

// Checks whether the PID_STORE is full, returns 1 if true (pid store is full) 0 if false (there are empty slots)
int is_pid_store_full() {
    for (int i = 0; i < MAX_PIDS; i++) {
        if (PID_STORE[i] == -1) {
            return 0;
        }
    }
    return 1;
}

// Function to add a PID to the PID storage
void add_pid(pid_t new_pid) {
    for (int i = 0; i < MAX_PIDS; i++) {
        if (PID_STORE[i] == -1) {
            PID_STORE[i] = new_pid;
            return;
        }
    }
    error("ERROR: Too many PIDs", 1);
}
// Function to remove a PID from the PID storage
void remove_pid(pid_t old_pid) {
    for (int i = 0; i < MAX_PIDS; i++) {
        if (PID_STORE[i] == old_pid) {
            PID_STORE[i] = -1;
            return;
        }
    }
    error("ERROR: Error removing PID", 1);
}

// Function which attempts to reap child process but will not wait for it, removes PID from storage upon success
void reap_nohang(pid_t child_pid) {
    int exit_status;
    int result = waitpid(child_pid, &exit_status, WNOHANG);
    //printf("result: %d\nexit_status: %d\n", result, exit_status);
    switch (result) {
        case -1:
            perror("waitpid");
            break;
        case 0:
            return;
        default:
            remove_pid(child_pid);
            //printf("background pid %d is done: ", child_pid);
    }
}

// Function to reap all potentially zombified child processes
void reap_pids() {
    for (int i = 0; i < MAX_PIDS; i++) {
        if (PID_STORE[i] != -1) {
            reap_nohang(PID_STORE[i]);
        }
    }
}

// Receive connection and collect full message and then return it
char *get_message(int connectionSocket) {
    char buffer[256];
    memset(buffer, '\0', 256);
    char *full_message = calloc(MAX_MESSAGE_SIZE, 1);
    int charsRead;
    int get_more = 0;

    while (1) {
        memset(buffer, '\0', 256);
        // Read the client's message from the socket
        charsRead = recv(connectionSocket, buffer, 255, 0);
        if (charsRead < 0){
            error("ERROR: Error reading from socket, closing connection...", 0);
            close(connectionSocket);
            exit(1);
        }
        strcat(full_message, buffer);
        get_more = is_message_complete(buffer);
        if (get_more == 0) {
            break;
        }
    }
    return full_message;
}

void send_message(char *message, int connectionSocket) {
    int charsWritten;
    // Send a message back to the client
    charsWritten = send(connectionSocket, message, strlen(message), 0);
    if (charsWritten < 0){
        error("ERROR: Error writing to socket, closing connection...", 0);
        close(connectionSocket);
        exit(1);
    }
    if (charsWritten < strlen(message)){
        error("ERROR: Not all data written to socket!\n", 0);
    }
}

// Checks buffer for end-message char, returns 0 if message is complete (char found) or 1 if not (more recv needed)
int is_message_complete(char *buffer) {
    for (int i = 0; i < 256; i++) {
        if (buffer[i] == '\n') {
            return 0;
        }
    }
    return 1;
}

// Set up the address struct for the server socket
void setupAddressStruct(struct sockaddr_in* address, int portNumber){

    // Clear out the address struct
    memset((char*) address, '\0', sizeof(*address));

    // The address should be network capable
    address->sin_family = AF_INET;
    // Store the port number
    address->sin_port = htons(portNumber);
    // Allow a client at any address to connect to this server
    address->sin_addr.s_addr = INADDR_ANY;
}

void connection_handler(int connectionSocket, struct sockaddr_in clientAddress) {
    // 1. Confirm identity of client, if invalid, close connection and return to listener
    char *confirm_identity = get_message(connectionSocket);
    if (strcmp(confirm_identity, "this is dec_client\n") != 0) {
        error("ERROR: Connection attempted from invalid client", 0);
        send_message("rejected\n", connectionSocket);
        close(connectionSocket);
        free(confirm_identity);
        exit(1);
    }
    printf("SERVER: Connected to client running at host %d port %d\n", ntohs(clientAddress.sin_addr.s_addr), ntohs(clientAddress.sin_port));
    free(confirm_identity);
    send_message("valid\n", connectionSocket);
    printf("SERVER: Client is valid\n");
    fflush(stdout);

    // 2a. Get ciphertext
    char *ciphertext = get_message(connectionSocket);
    ciphertext[strlen(ciphertext) - 1] = '\0'; // strip the mandatory line break

    // 2b. Confirm receipt of plaintext
    send_message("2b\n", connectionSocket);

    // 3a. Get key
    char *key = get_message(connectionSocket);
    key[strlen(key) - 1] = '\0'; // strip the mandatory line break

    // 3b. Confirm receipt of key
    send_message("3b\n", connectionSocket);

    // 4. Decode ciphertext if key is valid
    check_text_and_key(ciphertext, key);  // if message and key are valid
    char *plaintext = decode_message(ciphertext, key);

    // 5. Send plaintext to client
    send_message(plaintext, connectionSocket);
    printf("SERVER: Plaintext successfully sent to client\n");
    free(plaintext);

    // Close the connection socket for this client
    close(connectionSocket);
    printf("SERVER: Connection with client closed\n");
    exit(0);
}

// Handles forking the connection handling process
void fork_handler(int connectionSocket, struct sockaddr_in clientAddress) {
    // 1. Check if PID_STORE is full
    if (is_pid_store_full()) {
        error("ERROR: There are no available slots for client\n", 0);
        return;
    }
    // 2. If not full, spawn fork
    pid_t spawn_pid = fork();
    switch (spawn_pid) {
        case -1:  //error
            error("ERROR: Error creating child process", 0);
            return;
        case 0:  // child process executes here
            connection_handler(connectionSocket, clientAddress);
            break;
        default:
            add_pid(spawn_pid);
            fflush(stdout);
            break;
    }
}

// Receive ciphertext and key and return decode it to plaintext
char *decode_message(char *ciphertext, char *key) {
    size_t len = strlen(ciphertext);
    char *decoded_message = calloc((len + 1), 1);
    int message_int;
    int key_int;
    for (int i = 0; i < len; i++) {
        // Account for ' ' as the 27th char in message
        if (ciphertext[i] == ' ') {
            message_int = 26;
        }
        else {
            message_int = ciphertext[i] - 'A';
        }
        // Same for ' ' in key
        if (key[i] == ' ') {
            key_int = 26;
        }
        else {
            key_int = key[i] - 'A';
        }
        // Calculate the decoded integer
        int dec_int = (message_int - key_int);
        if (dec_int < 0) {
            dec_int += 27;
        }
        // Convert decoded int to char and add it to decoded message
        if (dec_int == 26) {
            decoded_message[i] = ' ';
        }
        else {
            decoded_message[i] = dec_int + 'A';
        }
    }
    decoded_message[len] = '\n';
    free(ciphertext);
    free(key);
    return decoded_message;
}

// Error function used for reporting issues, is_fatal = 1 means it must exit, 0 means do not exit
void error(const char *msg, const int is_fatal) {
    fprintf(stderr,"%s\n", msg);
    if (is_fatal) {
        exit(1);
    }
}

int main(int argc, char *argv[]) {
    init_pid_store();
    int connectionSocket, charsRead;
    char buffer[256];
    struct sockaddr_in serverAddress, clientAddress;
    socklen_t sizeOfClientInfo = sizeof(clientAddress);

    // Check usage & args
    if (argc < 2) {
        fprintf(stderr,"USAGE: %s port\n", argv[0]);
        exit(1);
    }

    // Create the socket that will listen for connections
    int listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket < 0) {
        error("ERROR opening socket", 1);
    }

    // Set up the address struct for the server socket
    setupAddressStruct(&serverAddress, atoi(argv[1]));

    // Associate the socket to the port
    if (bind(listenSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0){
        error("ERROR on binding", 1);
            }

    // Start listening for connections. Allow up to 5 connections to queue up
    listen(listenSocket, 5);

    // Accept a connection, blocking if one is not available until one connects
    while(1){
        // Accept the connection request which creates a connection socket
        connectionSocket = accept(listenSocket, (struct sockaddr *)&clientAddress, &sizeOfClientInfo);
        if (connectionSocket < 0){
            error("ERROR on accept", 0);
        }
        fork_handler(connectionSocket, clientAddress);
        reap_pids();
    }
    // Close the listening socket
    close(listenSocket);
    return 0;
}