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
int PORT;

extern int errno;

float speeds[10] = {20.1, 50.2, 40.1, 20.3, 64.2, 10.5, 55.3, 43.2, 83.2, 35.7};
float coordX[5] = {47.158857, 47.160379, 47.164124, 47.166358, 47.166092};
float coordY[5] = {27.58542, 27.586676, 27.581175, 27.577495, 27.570962};
static int keepRunning = 1;

struct info {
  char *username;
  int subscribed;
} clientInfo;

// Initialize a server structure.
struct sockaddr_in initialize_server(const char *ip_address) {
  struct sockaddr_in server;

  bzero(&server, sizeof(server));

  server.sin_family = AF_INET;
  server.sin_addr.s_addr = inet_addr(ip_address);
  server.sin_port = htons(PORT);

  return server;
}

void printError(const char *message) {
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
  clientInfo.username = read_from_server(socketD);
  read(socketD, &clientInfo.subscribed, sizeof(int));
}

char *get_pass() {
  struct termios oflags, nflags;
  char password[100];
  bzero(password, 100);

  // disabling echo
  tcgetattr(fileno(stdin), &oflags);
  nflags = oflags;
  nflags.c_lflag &= ~ECHO;
  nflags.c_lflag |= ECHONL;

  if (tcsetattr(fileno(stdin), TCSANOW, &nflags) != 0) {
    printError("tcsetattr");
  }

  do {
    bzero(password, 100);
    scanf("%s", password);
  } while (password[0] == '\n');
  password[strlen(password)] = '\0';

  password[strlen(password) - 1] = '\0';
  char *newpass = (char *)malloc(strlen(password));
  sprintf(newpass, "%s", password);

  /* restore terminal */
  if (tcsetattr(fileno(stdin), TCSANOW, &oflags) != 0) {
    printError("tcsetattr");
  }

  return newpass;
}

int registerNew(int socketD) {
  char *question;
  char *answer;

  int notRegistered = 1;
  int corect;

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

      char *pass = get_pass();

      if (!write_to_server(socketD, pass))
        return 0;

      if (!(answer = read_from_server(socketD)))
        return 0;

      printf("%s\n", answer);

      int sub;
      do {
        scanf("%d", &sub);
        printf("%d\n", sub);
      } while (sub != 1 && sub != 0);
      if (!write(socketD, &sub, sizeof(int)))
        return 0;

      if (!(answer = read_from_server(socketD)))
        return 0;

      printf("%s\n", answer);

      return 1;
    } else {
      if (!(answer = read_from_server(socketD)))
        return 0;

      printf("%s\n", answer);
    }
  }

  return 1;
}

int login(int socketD, char *username, char *password) {
  char *answer;
  int corect;

  if (!write_to_server(socketD, username))
      return 0;

  if (!write_to_server(socketD, password))
      return 0;

  if (!read(socketD, &corect, sizeof(int)))
      return 0;

  if (corect) {
      return 1;
  } else {
      return 0;
  }
}

int validate(int socketD,char way,char *username, char *password) {

  if (write(socketD, &way, sizeof(char)) <= 0)
    printError("[Client] Didn't send back the answer");

  if (way == 'l') {
    if (!login(socketD,username,password)) {
      return 0;
    }
  } else {
    if (!registerNew(socketD)) {
      return 0;
    }
  }

  receive_client_data(socketD);

  return 1;
}

void stopHandler() {
  keepRunning = 0;
  printf("SIGINT sent. Closing client..\n");
  fflush(stdout);
}

void* write_speed(void *socketCopy) {
  printf("[Client] Entering speed thread\n");
  pthread_detach(pthread_self());

  int *socketD = (int*)socketCopy;

  int update_time = 60;
  int type = 1;
  int index = 0;

  while (keepRunning) {
    signal(SIGINT, (__sighandler_t)stopHandler);
    if (!keepRunning)
      break;
    sleep(update_time);
    // signal what type of input it will be
    if (!write(*socketD, &type, sizeof(int))) {
      printf("[Client]Error at writing the type.\n");
      break;
    }

    if (!write(*socketD, &coordX[index % 5], sizeof(float))) {
      printf("[Client]Error at writing the speed.\n");
      break;
    }

    if (!write(*socketD, &coordY[index % 5], sizeof(float))) {
      printf("[Client]Error at writing the speed.\n");
      break;
    }

    if (!write(*socketD, &speeds[index], sizeof(float))) {
      printf("[Client]Error at writing the speed.\n");
      break;
    }

    index = (index + 1) % 10;
  }

  pthread_exit(NULL);
}

void* write_alert(void *socketCopy) {
  printf("[Client] Entering alert thread\n");
  fflush(stdout);
  pthread_detach(pthread_self());

  int *socketD = (int*)socketCopy;

  int type = 2;
  int bytes;

  while (keepRunning) {
    signal(SIGINT, (__sighandler_t)stopHandler);
    if (!keepRunning)
      break;
    printf("[Client] Write here any accident that happened: ");
    fflush(stdout);

    char location[100];
    bzero(location, 100);

    bytes = read(0, location, 100);
    if (bytes <= 0)
      continue;
    if (location[0] == '\n')
      continue;
    int length = strlen(location);
    location[length - 1] = '\0';

    if (!write(*socketD, &type, sizeof(int)))
      printf("[Client]Error at writing the type of message\n");
    fflush(stdout);
    if (!write_to_server(*socketD, location)) {
      printf("[Client]Error at sending the alert to server\n");
      fflush(stdout);
    }
  }

  pthread_exit(NULL);
}

void* read_news(void *socketCopy) {
  printf("[Client] Entering info thread\n");
  pthread_detach(pthread_self());

  int *socketD = (int*)socketCopy;

  int type;
  char *alert;
  char *answer;

  while (keepRunning) {
    if (!read(*socketD, &type, sizeof(int))) {
      printf("[Client]Error at reading type of message from server.\n");
      break;
    }
    if (type == 1) {

      if (!(answer = read_from_server(*socketD))) {
        printf("[Client]Error at readin the speed answer.\n");
        break;
      }

      printf("%s\n", answer);
    }
    if (type == 2) {
      if (!(alert = read_from_server(*socketD))) {
        printf("[Client]Error at reading alert from server.\n");
        break;
      }
      printf("%s\n", alert);
    }
    if (type == 3) {
      char *news;
      if (!(news = read_from_server(*socketD))) {
        printf("[Client]Error at readin the news from server.\n");
        break;
      }
      printf("%s\n", news);
    }
  }

  pthread_exit(NULL);
}

void* ask_for_news(void *socketCopy) {
  pthread_detach(pthread_self());

  int *socketD = (int*)socketCopy;

  int type = 3;

  while (keepRunning) {
    signal(SIGINT, (__sighandler_t)stopHandler);
    if (!keepRunning)
      break;

    sleep(30);

    write(*socketD, &type, sizeof(int));
  }

  pthread_exit(NULL);
}

int connect_to_server()
{
    const char *char_port = "4201";
    const char *char_ip = "127.0.0.1";

    PORT = atoi(char_port);

    struct sockaddr_in serverStruct = initialize_server(char_ip);
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
    return socketD;
}

void lobby(int socketD) {
  int *socketCopy = &socketD;

  // Open a thread for writing the speeds.
  pthread_t speedThread;
  pthread_create(&speedThread, NULL, &write_speed, (void*)socketCopy);

  pthread_t readThread;
  pthread_create(&readThread, NULL, &read_news, (void *)socketCopy);

  pthread_t alertThread;
  pthread_create(&alertThread, NULL, &write_alert, (void *)socketCopy);

  if (clientInfo.subscribed) {
    pthread_t newsThread;
    pthread_create(&newsThread, NULL, &ask_for_news,
                   (void *)socketCopy);
  }

  while (keepRunning) { // Keep the main thread open.
  }
  close(socketD);
}
