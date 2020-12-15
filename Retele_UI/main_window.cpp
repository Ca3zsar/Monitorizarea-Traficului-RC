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

#include "main_window.h"
#include "ui_main_window.h"
#include <QMetaObject>

float speeds[10] = {20.1, 50.2, 40.1, 20.3, 64.2, 10.5, 55.3, 43.2, 83.2, 35.7};
float coordX[5] = {47.158857, 47.160379, 47.164124, 47.166358, 47.166092};
float coordY[5] = {27.58542, 27.586676, 27.581175, 27.577495, 27.570962};
int keepRunning = 1;


main_window::obj_to_pass *currentObject;

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

void* write_speed(void *currentObject) {
  printf("[Client] Entering speed thread\n");
  pthread_detach(pthread_self());

  int socketD = ((main_window::obj_to_pass*)currentObject)->information.socketD;

  int update_time = 60;
  int type = 1;
  int index = 0;

  while (keepRunning) {
    if (!keepRunning)
      break;
    sleep(update_time);
    // signal what type of input it will be
    if (!write(socketD, &type, sizeof(int))) {
      printf("[Client]Error at writing the type.\n");
      break;
    }

    if (!write(socketD, &speeds[index%10], sizeof(float))) {
      printf("[Client]Error at writing the speed.\n");
      break;
    }

    if (!write(socketD, &coordX[index % 5], sizeof(float))) {
      printf("[Client]Error at writing the speed.\n");
      break;
    }

    if (!write(socketD, &coordY[index % 5], sizeof(float))) {
      printf("[Client]Error at writing the speed.\n");
      break;
    }

    index = (index + 1) % 10;
  }

  pthread_exit(NULL);
}

void write_alert(char* alert,int socketD) {
  int type = 2;

  if (!write(socketD, &type, sizeof(int)))
      printf("[Client]Error at writing the type of message\n");
  fflush(stdout);

  if (!write_to_server(socketD, alert)) {
      printf("[Client]Error at sending the alert to server\n");
      fflush(stdout);
  }

}

void display_alert(main_window::obj_to_pass *currentObject,char *alert)
{
    QString existent = currentObject->current->ui->alert_log->toPlainText();

    existent.append(alert);
    existent.append('\n');

    QMetaObject::invokeMethod(currentObject->current->ui->alert_log,"setText",Qt::AutoConnection,Q_ARG(QString,existent));
}

void display_speed(main_window::obj_to_pass *currentObject,char *speed)
{
    QString speedString;
    speedString.append(speed);
    QMetaObject::invokeMethod(currentObject->current->ui->speed_label,"setText",Qt::AutoConnection,Q_ARG(QString,speedString));
}

void* read_news(void *currentObject) {
  printf("[Client] Entering info thread\n");
  pthread_detach(pthread_self());

  int socketD = ((main_window::obj_to_pass*)currentObject)->information.socketD;

  int type;
  char *alert;
  char *answer;

  while (keepRunning) {
    if (!read(socketD, &type, sizeof(int))) {
      printf("[Client]Error at reading type of message from server.\n");
      break;
    }
    if (type == 1) {

      if (!(answer = read_from_server(socketD))) {
        printf("[Client]Error at readin the speed answer.\n");
        break;
      }
      display_speed((main_window::obj_to_pass*)currentObject,answer);

    }
    if (type == 2) {
      if (!(alert = read_from_server(socketD))) {
        printf("[Client]Error at reading alert from server.\n");
        break;
      }
      display_alert((main_window::obj_to_pass*)currentObject,alert);
    }
    if (type == 3) {
      char *news;
      if (!(news = read_from_server(socketD))) {
        printf("[Client]Error at readin the news from server.\n");
        break;
      }
      printf("%s\n", news);
    }
  }

  pthread_exit(NULL);
}

void* ask_for_news(void *currentObject) {
  pthread_detach(pthread_self());

  int socketD = ((main_window::obj_to_pass*)currentObject)->information.socketD;

  int type = 3;

  while (keepRunning) {

    sleep(30);

    write(socketD, &type, sizeof(int));
  }

  pthread_exit(NULL);
}


void* lobby(void *object) {
    currentObject = (main_window::obj_to_pass*)malloc(sizeof(main_window::obj_to_pass));
    currentObject = (main_window::obj_to_pass*)object;

    int socketD = ((main_window::obj_to_pass*)object)->information.socketD;

    printf("lobby: %d\n",socketD);

    pthread_t speedThread;
    pthread_create(&speedThread, NULL, &write_speed, (void*)currentObject);

    pthread_t readThread;
    pthread_create(&readThread, NULL, &read_news, (void *)currentObject);

    if (currentObject->information.subscribed) {
        pthread_t newsThread;
        pthread_create(&newsThread, NULL, &ask_for_news,
                       (void *)currentObject);
    }
    keepRunning = 1;
    while (keepRunning) { // Keep the main thread open.
    }
    close(currentObject->information.socketD);
    pthread_exit(NULL);
}


main_window::main_window(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::main_window)
{
    ui->setupUi(this);
}

main_window::~main_window()
{
    delete ui;
}

void main_window::set_name()
{
    char* greeting;
    greeting = (char*)malloc(strlen(this->thisObj->information.username) + 8);
    sprintf(greeting,"Hello, %s",this->thisObj->information.username);

    this->ui->hello_label->setText(greeting);
}

void main_window::showEvent(QShowEvent *ev)
{
    QMainWindow::showEvent(ev);

    set_name();

    this->thisObj->current = (main_window*)malloc(sizeof(main_window));
    this->thisObj->current = this;

    pthread_t lobby_thread;
    printf("main: %d\n",this->thisObj->information.socketD);
    pthread_create(&lobby_thread,NULL,lobby,this->thisObj);
}

void main_window::on_pushButton_clicked()
{
    int socketD = thisObj->information.socketD;

    QString alert = this->ui->accident_input->text();

    QByteArray byte_array = alert.toLocal8Bit();
    char *char_alert = byte_array.data();

    if(strlen(char_alert)==0 || char_alert[0]=='\n')return;
    write_alert(char_alert,socketD);
    this->ui->accident_input->setText("");
}
