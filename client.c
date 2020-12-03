#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
int PORT;

extern int errno;

// Initialize a server structure.
struct sockaddr_in initialize_server(char *ip_address) {
  struct sockaddr_in server;

  bzero(&server, sizeof(server));

  server.sin_family = AF_INET;
  server.sin_addr.s_addr = inet_addr(ip_address);
  server.sin_port = htons(PORT);

  return server;
}

void printError(char *message) {
  perror(message);
  exit(errno);
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    printf("The syntax is : <server_address> <port>");
    return -1;
  }

  PORT = atoi(argv[2]);

  struct sockaddr_in serverStruct = initialize_server(argv[1]);
  int socketD;

  // Create the socket and check for errors.
  if ((socketD = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    printError("ERROR AT socket() function.\n");
  }

  // Connect to the server
  if (connect(socketD, (struct sockaddr *)&serverStruct,
              sizeof(struct sockaddr)) == -1) {
    printError("ERROR AT connect().\n");
  }

  char message[100];

  while (1) {
    printf("[Client]Introduceti un mesaj: ");
    fflush(stdout);

    int msgLength;

    if ((msgLength = read(0, message, 100)) <= 0) {
      printError("Nothing to read!");
    }
    message[msgLength - 1] = '\0';
    if (strcmp(message, "quit") == 0) {
      printf("Quit message was read\n");
      break;
    }

    // First write the length of the message
    if (write(socketD, &msgLength, sizeof(int)) < 0) {
      printError("Error at writing");
    }
    if (write(socketD, message, msgLength) < 0) {
      printError("Error at writing the message");
    }

    int newMsgLength;
    if (read(socketD, &newMsgLength, sizeof(int)) < 0) {
      printf("Error at reading from server");
    }
    printf("The length of your message is: %d \n", newMsgLength);
  }
  close(socketD);
}
