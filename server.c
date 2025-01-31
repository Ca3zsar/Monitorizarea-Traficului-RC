#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

const int PORT = 4201;
const char *database = "users.db";
socklen_t sockLength = (socklen_t)sizeof(struct sockaddr_in);
extern int errno;

fd_set activeFD;
fd_set readFD;
fd_set writeFD;
int closed[100] = {[0 ... 99] = 1};

sqlite3 *db;

pthread_mutex_t mlock = PTHREAD_MUTEX_INITIALIZER;

int volatile fdNumber;
int keepRunning = 1;
int volatile waitFor = 0;

// Struct to describe client.
struct clientInfo {
  char *username;
  char subscribed;
  float actualSpeed;
  float coordinates[2];
} clients[100];

// Struct to describe information from the traffic
// struct trafficInfo {
//   float speedLimit;
//   float coordinates[2];
// };

// Struct to describe an accident
struct trafficEvents {
  char *address;
  char *message;
  float coordinates[2];
};

// Struct to describe the newsletter. (For the subscribed clients only).
struct news {
  char *weather;
  char *sport;
  float prices[3];
};

struct args {
  int socketD;
  int clientId;
  int threadId;
  int active;
  pthread_t thread;
  struct sockaddr_in clientStruct;
  struct clientInfo client;
};

// Initialize a server structure.
struct sockaddr_in initialize_server() {
  struct sockaddr_in server;

  bzero(&server, sizeof(server));

  server.sin_family = AF_INET;
  server.sin_addr.s_addr = htonl(INADDR_ANY);
  server.sin_port = htons(PORT);

  return server;
}

void close_thread(struct args *arg, int bytes) {
  closed[arg->clientId] = 1;
  arg->active = 0;
  if (bytes == 0) {
    printf("Client %d disconnected.\n", arg->clientId);
  } else
    printf("[Thread %d]Error at reading from client.\n", arg->threadId);
  close(arg->clientId);
  pthread_exit(NULL);
}

void printError(char *message) {
  perror(message);
  exit(errno);
}

void set_socket(int *socketD) {
  if ((*socketD = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    printError("Error at creating the socket");
  }
  int opt = 1;
  setsockopt(*socketD, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  // fcntl(*socketD, F_SETFL, O_NONBLOCK);
}

void initialize_db() {
  int status;
  status = sqlite3_open(database, &db);

  if (status != SQLITE_OK) {
    sqlite3_close(db);
    printError("Cannot open the database!");
  }
}

int is_in_database(char *username) {
  sqlite3_stmt *result;
  char *sql = "SELECT COUNT(*) FROM Users WHERE name =?";

  int status = sqlite3_prepare_v2(db, sql, -1, &result, 0);
  if (status == SQLITE_OK) {
    sqlite3_bind_text(result, 1, username, strlen(username), NULL);
  } else {
    printf("Failed to execute SQL query\n");
    return 0;
  }

  int step = sqlite3_step(result);
  int res;
  if (step == SQLITE_ROW) {
    res = sqlite3_column_int(result, 0);
    sqlite3_finalize(result);
    return res;
  }
  sqlite3_finalize(result);
  return 0;
}

int add_to_db(char *username, char *password, int subscribed) {
  char sql[100];
  char *err_msg = 0;
  int status;

  sprintf(sql, "INSERT INTO Users VALUES('%s' , '%s', '%d');", username,
          password, subscribed);

  status = sqlite3_exec(db, sql, 0, 0, &err_msg);

  if (status != SQLITE_OK) {

    fprintf(stderr, "SQL error: %s\n", err_msg);

    sqlite3_free(err_msg);
    return 0;
  }

  return 1;
}

int write_to_client(int threadId, int socketD, char *message) {
  int msgLength = strlen(message);
  send(socketD, &msgLength, sizeof(int), MSG_NOSIGNAL);

  int written;
  if ((written = (send(socketD, message, msgLength, MSG_NOSIGNAL))) !=
      msgLength) {
    printf("[Thread %d] Error at writing to client %d", threadId, socketD);
    return 0;
  }

  return 1;
}

char *read_from_client(int threadId, int socketD) {
  int readLength;
  char *message;

  int status;

  if ((status = read(socketD, &readLength, sizeof(int))) <= 0) {
    if (status == 0)
      printf("[Thread %d]Client %d disconnected\n.", threadId, socketD);
    else
      printf("[Thread %d] Error at reading length of message from client %d",
             threadId, socketD);
    return 0;
  }

  message = (char *)malloc(readLength + 1);

  if ((status = read(socketD, message, readLength)) != readLength) {
    if (status == 0)
      printf("[Thread %d]Client %d disconnected.\n", threadId, socketD);
    else
      printf("[Thread %d] Error at reading message from client %d", threadId,
             socketD);
    return 0;
  }
  // if(message[readLength-1]=='\n')message[readLength-1]='\0';
  message[readLength] = '\0';
  return message;
}

void send_client_data(struct args *arg) {
  // write the username
  write_to_client(arg->threadId, arg->clientId,
                  clients[arg->clientId].username);
  write(arg->clientId, &clients[arg->clientId].subscribed, sizeof(int));
}

void append_client(int clientId, struct clientInfo newClient) {
  clients[clientId].username = (char *)malloc(strlen(newClient.username) + 1);
  sprintf(clients[clientId].username, "%s", newClient.username);

  clients[clientId].subscribed = newClient.subscribed;
}

void delete_client(int clientId) {
  bzero(&clients[clientId], sizeof(struct clientInfo));
}

int correct_user(char *username, char *password, struct clientInfo *client) {
  // char *password;
  sqlite3_stmt *result;
  char *sql = "SELECT password, subscribed FROM Users WHERE name =?";

  int status = sqlite3_prepare_v2(db, sql, -1, &result, 0);
  if (status == SQLITE_OK) {
    sqlite3_bind_text(result, 1, username, strlen(username), NULL);
  } else {
    printf("Failed to execute SQL query\n");
    return 0;
  }

  int res;
  int step = sqlite3_step(result);
  if (step == SQLITE_ROW) {
    client->username = (char *)malloc(strlen(username) + 1);
    sprintf(client->username, "%s", username);
    client->subscribed = sqlite3_column_int(result, 1);
    res = strcmp((char *)sqlite3_column_text(result, 0), password);
    sqlite3_finalize(result);
    return !res;
  }
  sqlite3_finalize(result);

  return 0;
}

//encrypt the password
char *encrypt(char *pass)
{
  char *encrypted;
  const char *salt = "$1$etNnh7FA$OlM7eljE/B7F1J4XYNnk81";

  encrypted = crypt(pass, salt);

  return encrypted;
}

int login(struct args *arg) {
  int tries = 3;

  char *question = "Enter your username: ";
  char *askPass = "Enter your password: \n";
  int status;

  while (tries > 0) {
    if (!write_to_client(arg->threadId, arg->clientId, question))
      return 0;
    char *username;

    if (!(username = read_from_client(arg->threadId, arg->clientId)))
      return 0;
    int corect;

    if (!write_to_client(arg->threadId, arg->clientId, askPass))
      return 0;

    char *userPass;
    if (!(userPass = read_from_client(arg->threadId, arg->clientId)))
      return 0;

    char *encrypted = encrypt(userPass);

    if (correct_user(username, encrypted, &arg->client)) {
      append_client(arg->clientId, arg->client);

      corect = 1;
      write(arg->clientId, &corect,
            sizeof(int)); // Tell that the name was correct.
      write_to_client(arg->threadId, arg->clientId, "You are now logged in, ");
      return 1;
    } else {
      corect = 0;
      write(arg->clientId, &corect, sizeof(int));

      char *incorrect = "Incorrect username or password\n";
      write_to_client(arg->threadId, arg->clientId, incorrect);
      tries--;
    }
  }
  write_to_client(arg->threadId, arg->clientId, "Number of tries exceeded\n");
  printf("[Thread %d]Client %d disconnected.\n", arg->threadId, arg->clientId);
  fflush(stdout);
  return 0;
}

int registerNew(struct args *arg) {
  char *question = "Enter your username: ";
  char *askPass = "Enter your password: ";
  char *askSub = "Do you want to subscribe? (0/1)";
  int status;

  int notRegistered = 1;
  while (notRegistered) {
    if (!write_to_client(arg->threadId, arg->clientId, question))
      return 0;

    char *username;

    if (!(username = read_from_client(arg->threadId, arg->clientId)))
      return 0;

    if (!is_in_database(username)) {
      fflush(stdout);
      int corect = 1;
      if (!write(arg->clientId, &corect, sizeof(int)))
        return 0;

      if (!write_to_client(arg->threadId, arg->clientId, askPass))
        return 0;

      char *password;
      if (!(password = read_from_client(arg->threadId, arg->clientId)))
        return 0;

      if (!write_to_client(arg->threadId, arg->clientId, askSub))
        return 0;

      int sub;
      if (!read(arg->clientId, &sub, sizeof(int)))
        return 0;

      char *encrypted = encrypt(password);

      if (!add_to_db(username, encrypted, sub))
        return 0;

      char *success = "Successfully registered, ";

      if (!write_to_client(arg->threadId, arg->clientId, success))
        return 0;

      printf("[Thread %d]Successfully added new user to database.\n",
             arg->threadId);
      fflush(stdout);

      arg->client.username = (char *)malloc(strlen(username) + 1);
      sprintf(arg->client.username, "%s", username);
      arg->client.subscribed = sub;
      append_client(arg->clientId, arg->client);

      return 1;
    } else {
      int corect = 0;
      if (!write(arg->clientId, &corect, sizeof(int)))
        return 0;

      char *failed = "Username already existent!\n";
      if (!write_to_client(arg->threadId, arg->clientId, failed))
        return 0;
    }
  }

  return 1;
}

int validate(struct args *arg) {
  char *question = "Do you want to log in or register? [L/R]";

  if (!write_to_client(arg->threadId, arg->clientId, question))
    return 0;

  char answer;
  int readLength;

  if ((readLength = read(arg->clientId, &answer, sizeof(char))) <= 0) {
    if (readLength < 0)
      printf("[Thread %d] Error at reading login option from client %d\n",
             arg->threadId, arg->clientId);
    else
      printf("Client %d disconnected \n", arg->clientId);
    return 0;
  }

  printf("[Thread %d] Client %d chose to %s\n", arg->threadId, arg->clientId,
         answer == 'l' ? "login" : "register");

  if (answer == 'l') {
    if (!login(arg)) {
      return 0;
    } else {
      printf("[Thread %d] Client %d logged in.\n", arg->threadId,
             arg->clientId);
      fflush(stdout);
    }
  } else if (answer == 'r') {
    if (!registerNew(arg)) {
      return 0;
    } else {
      printf("[Thread %d] Client %d registered a new account.\n", arg->threadId,
             arg->clientId);
      fflush(stdout);
    }
  }
  return 1;
}

void add_new_client(struct args *arg) {
  pthread_detach(pthread_self());
  bzero(&arg->clientStruct, sockLength);
  arg->clientId =
      accept(arg->socketD, (struct sockaddr *)&arg->clientStruct, &sockLength);

  printf("[Thread %d] Authenticating client %d\n", arg->threadId,
         arg->clientId);
  fflush(stdout);

  int opts = fcntl(arg->clientId, F_GETFL);
  fcntl(arg->clientId, F_SETFL, opts & O_NONBLOCK & ~O_NONBLOCK);

  if (arg->clientId < 0) {
    closed[arg->clientId] = 1;
    arg->active = 0;
    perror("[Server] Error at accepting client\n");
    return;
  }

  if (!validate(arg)) {
    closed[arg->clientId] = 1;
    arg->active = 0;
    close(arg->clientId);
    return;
  }
  closed[arg->clientId] = 0;
  pthread_mutex_lock(&mlock);
  if (fdNumber < arg->clientId)
    fdNumber = arg->clientId;

  send_client_data(arg);

  FD_SET(arg->clientId, &activeFD);
  pthread_mutex_unlock(&mlock);
  arg->active = 0;
  pthread_exit(NULL);
}

int announce_all(struct args *arg, char *alert) {
  // Announce all clients.
  int type = 2; // Type 2 is for alerts.
  for (int fd = 4; fd <= fdNumber; fd++) {
    if (!closed[fd] && FD_ISSET(fd, &writeFD) && fd != arg->clientId) {
      pthread_mutex_lock(&mlock);
      waitFor = 1;
      if (!write(fd, &type, sizeof(int))) {
        printf("[Thread %d]Failed to send alert type to client %d",
               arg->threadId, fd);
        pthread_mutex_unlock(&mlock);
        waitFor = 0;
        return 0;
      }
      char *message;
      message = (char *)malloc(strlen(alert) + strlen("[ALERTA] : ") + 1);
      sprintf(message, "[ALERTA] : %s", alert);
      if (!write_to_client(arg->threadId, fd, message)) {
        printf("[Thread %d]Failed to send alert to client %d", arg->threadId,
               fd);
        pthread_mutex_unlock(&mlock);
        waitFor = 0;
        return 0;
      }
      waitFor = 0;
      pthread_mutex_unlock(&mlock);
    }
  }
  return 1;
}

void speed_operations(struct args *arg) {
  int bytes;
  int type = 1;
  if ((bytes = read(arg->clientId, &clients[arg->clientId].actualSpeed,
                    sizeof(int))) <= 0) {
    close_thread(arg, bytes);
  }
  if ((bytes = read(arg->clientId, &clients[arg->clientId].coordinates[0],
                    sizeof(int))) <= 0) {
    close_thread(arg, bytes);
  }
  if ((bytes = read(arg->clientId, &clients[arg->clientId].coordinates[1],
                    sizeof(int))) <= 0) {
    close_thread(arg, bytes);
  }

  while (waitFor)
    ;

  write(arg->clientId, &type, sizeof(int));
  if (clients[arg->clientId].actualSpeed > 40)
    write_to_client(arg->threadId, arg->clientId,
                    "The speed limit is 40, slow down.");
  else {
    write_to_client(arg->thread, arg->clientId,
                    "The speed limit is 40, your speed is less.");
  }

  float speed = clients[arg->clientId].actualSpeed;
  float coordX = clients[arg->clientId].coordinates[0];
  float coordY = clients[arg->clientId].coordinates[1];

  printf("[Thread %d]The location of the client %d is : %0.5f , %0.5f . The "
         "speed of Client %d is %0.2f\n",
         arg->threadId, arg->clientId, coordX, coordY, arg->clientId, speed);
  fflush(stdout);
}

void read_ready(struct args *arg) {
  pthread_detach(pthread_self());

  printf("[Thread %d]Ready to read input from Client %d \n", arg->threadId,
         arg->clientId);
  fflush(stdout);

  int bytes;

  // Read the type of the input.
  int type;
  if ((bytes = read(arg->clientId, &type, sizeof(int))) <= 0) {
    close_thread(arg, bytes);
  }
  fflush(stdout);

  if (type == 1) // Speed input
  {
    speed_operations(arg);
  }
  if (type == 2) // Alert input
  {
    char *alert;
    if (!(alert = (read_from_client(arg->threadId, arg->clientId)))) {
      arg->active = 0;
      printf("[Thread %d]Error at reading the alert from client. \n",
             arg->threadId);
      pthread_exit(NULL);
    }
    printf("[Thread %d]Client %d : %s\n", arg->threadId, arg->clientId, alert);
    fflush(stdout);
    if (!announce_all(arg, alert)) {
      arg->active = 0;
      printf("[Thread %d] Failed to send message to all clients.\n",
             arg->threadId);
      pthread_exit(NULL);
    }
  }
  if (type == 3) // Subscribtion input
  {
    char *news = "STIRI DE TEST";
    write(arg->clientId, &type, sizeof(int));
    if (!write_to_client(arg->threadId, arg->clientId, news)) {
      arg->active = 0;
      printf("[Thread %d] Failed to send newsletter.\n", arg->threadId);
      pthread_exit(NULL);
    }
  }
  arg->active = 0;
  FD_SET(arg->clientId, &activeFD);
  pthread_exit(NULL);
}

void stopHandler() {
  printf("Closing the server now...\n");
  keepRunning = 0;
}

int main() {
  struct sockaddr_in serverStruct = initialize_server();
  struct sockaddr_in clientStruct;

  int socketD;
  struct args arg[100];

  struct timeval outTime;
  int fd;

  set_socket(&socketD);

  initialize_db();

  // Bind the socket.
  if (bind(socketD, (struct sockaddr *)&serverStruct,
           sizeof(struct sockaddr)) == -1)
    printError("Error at binding the socket");

  // Make the server listen for the clients.
  if (listen(socketD, 5) == -1)
    printError("[Server] Error at listen()");

  FD_ZERO(&activeFD);
  FD_ZERO(&readFD);
  FD_ZERO(&writeFD);
  FD_SET(socketD, &activeFD);

  outTime.tv_sec = 30;
  outTime.tv_usec = 0;

  fdNumber = socketD;

  int index = 0;
  printf("[Server] Waiting at port %d \n", PORT);
  int first = 1;
  // Wait for the clients to connect and then serve them.
  while (keepRunning) {
    // See if the server was closed
    signal(SIGINT, stopHandler);
    if (!keepRunning)
      break;

    int clientId;
    FD_ZERO(&readFD);
    FD_ZERO(&writeFD);
    bcopy((char *)&activeFD, (char *)&readFD, sizeof(activeFD));
    bcopy((char *)&activeFD, (char *)&writeFD, sizeof(activeFD));

    outTime.tv_sec = 2;

    select(fdNumber + 1, &readFD, &writeFD, NULL, &outTime);

    if (FD_ISSET(socketD, &readFD)) {
      bzero((struct args *)&arg[index], sizeof(struct args));
      arg[index % 100].socketD = socketD;
      arg[index % 100].threadId = index % 100;
      arg[index % 100].active = 1;
      pthread_create(&arg[index % 100].thread, NULL, (void *)&add_new_client,
                     &arg[index % 100]);
      index = (index + 1) % 100;
    }

    for (fd = 4; fd <= fdNumber; fd++) /* parcurgem multimea de descriptori */
    {
      if (fd != socketD && (FD_ISSET(fd, &readFD))) {
        while (arg[index % 100].active) {
          index = (index + 1) % 100;
        }
        arg[index].threadId = index;
        arg[index].clientId = fd;
        arg[index].active = 1;
        pthread_create(&arg[(index)].thread, NULL, (void *)&read_ready,
                       &arg[(index)]);
        index = (index + 1) % 100;
        FD_CLR(fd, &activeFD);
      }
    }
  }
  sqlite3_close(db);
  for (fd = 4; fd <= fdNumber; fd++) {
    if (closed[fd] == 0) {
      shutdown(fd, SHUT_RD);
      close(fd);
    }
  }
  close(socketD);
  return 0;
}
