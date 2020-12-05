#include <arpa/inet.h>
#include <ctype.h>
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

int write_to_server(int socketD, char *message) {
  int msgLength = strlen(message);
  write(socketD, &msgLength, sizeof(int));

  int written;
  if ((written = (write(socketD, message, msgLength))) <= 0) {
    printf("Error at writing to server through socket %d", socketD);
    return 0;
  }

  return 1;
}

char *read_from_server(int socketD) {
  int readLength;
  char *message;

  if (recv(socketD, &readLength, sizeof(int),MSG_NOSIGNAL) <= 0) {
    printf("Error at reading length of message from server through socket %d",
           socketD);
    return 0;
  }

  message = (char *)malloc(readLength + 1);

  if (recv(socketD, message, readLength,MSG_NOSIGNAL) != readLength) {
    printf("Error at reading message from server through socket %d", socketD);
    return 0;
  }
  message[readLength] = '\0';
  return message;
}

int registerNew(int socketD) {
  char *question;
  char *answer;

  int notRegistered = 1;
  int corect, secondCorect;

  char c;
  while ((c = getchar()) != '\n' && c != EOF)
    ;

  while (notRegistered) {
    if (!(question = read_from_server(socketD)))
      return 0;

    printf("%s\n", question);

    char username[100];
    do {
      bzero(username, 100);
      scanf("%s", username);
    } while (username[0] == '\n');

    username[strlen(username)] = '\0';
    if (!write_to_server(socketD, username))
      return 0;

    if (!read(socketD, &corect, sizeof(int)))
      return 0;

    if (corect) {
      if (!(answer = read_from_server(socketD)))
        return 0;

      printf("%s\n", answer);

      char pass[100];
      do {
        bzero(pass, 100);
        scanf("%s", pass);
      } while (pass[0] == '\n');
      pass[strlen(pass)] = '\0';

      if (!write_to_server(socketD, pass))
        return 0;

      if (!(answer = read_from_server(socketD)))
        return 0;

      printf("%s\n", answer);

      int sub;
      do {
        scanf("%d", &sub);
      } while (sub != 1 || sub != 0);

      if (!write(socketD, &sub, sizeof(int)))
        return 0;

      if (!(answer = read_from_server(socketD)))
        return 0;

      printf("%s\n", answer);

      return 1;
    }
  }

  return 1;
}

int login(int socketD) {
  char *question;
  char *answer;
  int corect;

  char c;
  while ((c = getchar()) != '\n' && c != EOF)
    ;

  int tries = 3;
  while (tries > 0) {
    if (!(question = read_from_server(socketD)))
      return 0;

    printf("%s\n", question);

    char username[100];
    bzero(username, 100);
    scanf("%s", username);

    username[strlen(username)] = '\0';
    if (!write_to_server(socketD, username))
      return 0;

    if (!(question = read_from_server(socketD)))
      return 0;

    printf("%s\n", question);

    char pass[100];
    bzero(pass, 100);
    scanf("%s", pass);

    pass[strlen(pass)] = '\0';

    if (!write_to_server(socketD, pass))
      return 0;

    if (!read(socketD, &corect, sizeof(int)))
      return 0;

    if (corect) {
      char *confirm = read_from_server(socketD);
      printf("%s%s\n", confirm, username);
      free(question);
      free(confirm);
      return 1;

    } else {
      char *confirm = read_from_server(socketD);
      printf("%s", confirm);
      free(confirm);
      tries--;
    }
  }
  answer = read_from_server(socketD);
  printf("%s\n", answer);
  free(answer);
  free(question);
  return 0;
}

int validate(int socketD) {
  int msgLength;
  int readLength;

  char *question;

  if (!(question = read_from_server(socketD)))
    return 0;

  printf("%s\n", question);

  char answer;
  int notOK = 1;
  do {
    if (read(0, &answer, sizeof(char)) <= 0)
      printError("[Client] Can't read the answer to the login message");

    if (tolower(answer) == 'l' || tolower(answer) == 'r')
      notOK = 0;

  } while (notOK);

  answer = tolower(answer);
  if (write(socketD, &answer, sizeof(char)) <= 0)
    printError("[Client] Didn't send back the answer");

  if (answer == 'l') {
    if (!login(socketD)) {
      return 0;
    }
  } else {
    if (!registerNew(socketD)) {
      return 0;
    }
  }

  return 1;
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

  if (!validate(socketD)) {
    close(socketD);
    return 0;
  }

  while (1) {
    printf("[Client]Introduceti un mesaj: ");
    fflush(stdout);

    int msgLength;
    fflush(stdin);
    if ((msgLength = read(0, message, 100)) <= 0) {
      printError("Nothing to read!");
    }
    message[msgLength - 1] = '\0';
    if (strcmp(message, "quit") == 0) {
      printf("Quit message was read\n");
      break;
    }

    // First write the length of the message
    if (!write_to_server(socketD, message))
      printError("Error at writing to server!\n");

    int newMsgLength;
    if (read(socketD, &newMsgLength, sizeof(int)) < 0) {
      printf("Error at reading from server");
    }
    printf("The length of your message is: %d \n", newMsgLength);
  }
  close(socketD);
}
