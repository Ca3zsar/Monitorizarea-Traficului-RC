#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#pragma once

struct {
  char *username;
  int subscribed;
} clientInfo;

// Initialize a server structure.
struct sockaddr_in initialize_server(int PORT,const char *ip_address) {
  struct sockaddr_in server;

  bzero(&server, sizeof(server));

  server.sin_family = AF_INET;
  server.sin_addr.s_addr = inet_addr(ip_address);
  server.sin_port = htons(PORT);

  return server;
}

void printError(const char *message) {
  perror(message);
}

int auth_write_to_server(int socketD, char *message) {
  int msgLength = strlen(message);
  write(socketD, &msgLength, sizeof(int));

  int written;
  if ((written = (write(socketD, message, msgLength))) <= 0) {
    printf("Error at writing to server through socket %d", socketD);
    return 0;
  }

  return 1;
}

char *auth_read_from_server(int socketD) {
  int readLength;
  char *message;

  if (recv(socketD, &readLength, sizeof(int), MSG_NOSIGNAL) <= 0) {
    printf("Error at reading length of message from server through socket %d",
           socketD);
    return 0;
  }

  message = (char *)malloc(readLength + 1);

  if (recv(socketD, message, readLength, MSG_NOSIGNAL) != readLength) {
    printf("Error at reading message from server through socket %d", socketD);
    return 0;
  }
  message[readLength] = '\0';
  return message;
}

void receive_client_data(int socketD) {
  clientInfo.username = auth_read_from_server(socketD);
  read(socketD, &clientInfo.subscribed, sizeof(int));
}

int registerNew(int socketD,char *username,char *password,int subscribed) {
  int corect;

  if (!auth_write_to_server(socketD, username))
      return -1;

  if (!read(socketD, &corect, sizeof(int)))
      return -1;

  if (corect) {
      if (!auth_write_to_server(socketD, password))
          return -1;

      if (!write(socketD, &subscribed, sizeof(int)))
          return -1;

      return 1;
  } else {
      return 0;
  }
}

int login(int socketD, char *username, char *password) {
  int corect;

  if (!auth_write_to_server(socketD, username))
      return -1;

  if (!auth_write_to_server(socketD, password))
      return -1;

  if (!read(socketD, &corect, sizeof(int)))
      return -1;

  if (corect) {
      return 1;
  } else {
      return 0;
  }
}

int validate(int socketD,char way,char *username, char *password,int subscribed=0) {
  int status = 0;
  if (write(socketD, &way, sizeof(char)) <= 0){
    printError("[Client] Didn't send back the answer");
    return -1;
  }

  if (way == 'l') {
    status=login(socketD,username,password);
    if(status<=0)return status;
  } else {
     status=registerNew(socketD,username,password,subscribed);
     if(status<=0)return status;
  }

  receive_client_data(socketD);

  return 1;
}

int connect_to_server()
{
    const char *char_port = "4201";
    const char *char_ip = "127.0.0.1";

    int PORT = atoi(char_port);

    struct sockaddr_in serverStruct = initialize_server(PORT,char_ip);
    int socketD;

    // Create the socket and check for errors.
    if ((socketD = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
      printError("ERROR AT socket() function.\n");
      return -1;
    }

    // Connect to the server
    if (connect(socketD, (struct sockaddr *)&serverStruct,
                sizeof(struct sockaddr)) == -1) {
      printError("ERROR AT connect().\n");
      return -1;
    }
    return socketD;
}
