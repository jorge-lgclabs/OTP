#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/types.h>  // ssize_t
#include <sys/socket.h> // send(),recv()
#include <netdb.h>      // gethostbyname()

#define IDENTITY "this is enc_client\n"
#define MAX_MESSAGE_SIZE 81920

const char VALID_CHARS[27] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ ";

void check_text_and_key(char *text, char *key);
int check_validity(char *text);

char *get_message(int connectionSocket);
void send_message(char *message, int connectionSocket);
int is_message_complete(char *buffer);
int evaluate_confirmation_message(char *message, char *expected_value);

void setupAddressStruct(struct sockaddr_in* address, int portNumber, char* hostname);
char *readfile(char *filename);

void error(const char *msg);

// Checks the overall validity of the text:key combo (no special chars, correct lengths)
// Sends error and exits if invalid
void check_text_and_key(char *text, char *key) {
  if (!check_validity(text)) {
    error("enc_client error: input contains bad characters");
  }
  if (!check_validity(key)) {
    error("ERROR: Key is invalid");
  }
  if (strlen(text) > strlen(key)) {
    error("ERROR: Key is too short");
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
      close(connectionSocket);
      error("ERROR: Error reading from socket, closing connection...");
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
    error("ERROR: Error writing to socket, closing connection...");
    close(connectionSocket);
    exit(1);
  }
  if (charsWritten < strlen(message)){
    error("ERROR: Not all data written to socket!\n");
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

// Compares the content of a confirmation message sent back from the server against an expected value
// returns 0 if it is not the expected value, 1 if it is (message is confirmed to have been received by server)
int evaluate_confirmation_message(char *message, char *expected_value) {
  if (strcmp(message, expected_value) == 0) {
    // if the return message is as expected
    free(message);
    return 1;
  }
  return 0;
}

// Set up the address struct
void setupAddressStruct(struct sockaddr_in* address, int portNumber, char* hostname){
  // Clear out the address struct
  memset((char*) address, '\0', sizeof(*address));

  // The address should be network capable
  address->sin_family = AF_INET;
  // Store the port number
  address->sin_port = htons(portNumber);

  // Get the DNS entry for this host name
  struct hostent* hostInfo = gethostbyname(hostname);
  if (hostInfo == NULL) {
    fprintf(stderr, "ERROR: no such host\n");
    exit(0);
  }
  // Copy the first IP address from the DNS entry to sin_addr.s_addr
  memcpy((char*) &address->sin_addr.s_addr,
        hostInfo->h_addr_list[0],
        hostInfo->h_length);
}

char *readfile(char *filename) {
  FILE *fd = fopen(filename, "r");
  if (fd == NULL) {
    error("ERROR: Could not open file");
  }
  fseek(fd, 0, SEEK_END);
  long len = ftell(fd);  // get length
  fseek(fd, 0, SEEK_SET);
  char *file_content = calloc(len + 1, 1);
  fread(file_content, 1, len, fd);
  file_content[len] = '\0';
  fclose(fd);
  return file_content;
}

// Error function used for reporting issues
void error(const char *msg) {
  fflush(stdout);
  fprintf(stderr, "%s\n", msg);
  exit(1);
}

int main(int argc, char *argv[]) {
  char *plaintext;
  char *key;
  int socketFD, charsWritten, charsRead;
  struct sockaddr_in serverAddress;
  char buffer[256];
  // Check usage & args
  if (argc < 4) {
    fprintf(stderr,"USAGE: %s plaintext key port\n", argv[0]);
    exit(1);
  }

  // Read files
  plaintext = readfile(argv[1]);
  key = readfile(argv[2]);

  // Check file validity
  check_text_and_key(plaintext, key);

  // Create a socket
  socketFD = socket(AF_INET, SOCK_STREAM, 0);
  if (socketFD < 0){
    error("ERROR: Error opening socket");
  }

   // Set up the server address struct
  setupAddressStruct(&serverAddress, atoi(argv[3]), "localhost");


  // Connect to server
  if (connect(socketFD, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0){
    error("ERROR: Error while connecting");
  }


  // 1a. Send identity confirmation message to server
  send_message(IDENTITY, socketFD);

  // 1b. Get return message from server for identity confirmation
  char *identity_confirm = get_message(socketFD);

  if (!evaluate_confirmation_message(identity_confirm, "valid\n")) {
    fprintf(stderr, "ERROR: Server rejected connection from client on port %d\n", atoi(argv[3]));
    exit(2);  // Report error and exit with status 2
  }

  // 2a. Send plaintext to server
  send_message(plaintext, socketFD);

  // 2b. Get confirmation of receipt from server
  if (!evaluate_confirmation_message(get_message(socketFD), "2b\n")) {
    error("Server did not receive plaintext");
  }

  // 3a. Send key to server
  send_message(key, socketFD);

  // 3b. Get confirmation of receipt from sever
  if (!evaluate_confirmation_message(get_message(socketFD), "3b\n")) {
    error("Server did not receive key");
  }

  // 4. Get the ciphertext from server
  char *ciphertext = get_message(socketFD);

  // 5. Print normally
  printf("%s", ciphertext);

  // // 6. Print precisely
  // for (int i = 0; i < strlen(plaintext); i++) {
  //   printf("%c", ciphertext[i]);
  // }
  // printf("\n");



  // Close the socket
  free(plaintext);
  free(key);
  free(ciphertext);
  close(socketFD);
  return 0;
}