#include "client.h"
#include "./ui_client.h"
#include <tcp_client.c>
#include <stdio.h>
client::client(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::client)
{
    ui->setupUi(this);
}

int connected = 0;
int socketD;

client::~client()
{
    delete ui;
}

void client::on_register_switch_clicked()
{
    ui->stackedWidget->setCurrentIndex(1);
}

void client::on_login_switch_clicked()
{
    ui->stackedWidget->setCurrentIndex(0);
}

void client::on_login_btn_clicked()
{
    if(!connected)
    {
        socketD = connect_to_server();
        if(socketD > 0)
        {
            connected=1;
            ui->correct_label->setText("Connected to server");
        }
        else{
            ui->correct_label->setText("Can't connect to server");
        }
    }

    QString username = ui->user_input->text();
    QString password = ui->pass_input->text();

    QByteArray ba = username.toLocal8Bit();
    char *char_user = ba.data();

    ba = password.toLocal8Bit();
    char *char_password = ba.data();

    if(connected){
        int answer = validate(socketD,'l',char_user,char_password);

        if(answer==0)ui->correct_label->setText("Wrong username or password!");

        if(answer==-1){
            ui->correct_label->setText("Can't connect to server");
            connected=0;
        }
        if(answer>0) ui->correct_label->setText("");
    }
}

void client::on_register_btn_clicked()
{
    if(!connected)
    {
        socketD = connect_to_server();
        if(socketD > 0)
        {
            connected=1;
            ui->correct_label_2->setText("");
        }
        else{
            ui->correct_label_2->setText("Can't connect to server");
        }
    }
    QString username = ui->new_user_input->text();
    QString password = ui->new_pass_input->text();

    QByteArray ba = username.toLocal8Bit();
    char *char_user = ba.data();

    ba = password.toLocal8Bit();
    char *char_password = ba.data();

    int subscribed = ui->subscribe_check->isChecked();

    if(connected){
        int answer = validate(socketD,'r',char_user,char_password,subscribed);

        if(answer==0)ui->correct_label_2->setText("Username already existent!");

        if(answer==-1){
            ui->correct_label_2->setText("Can't connect to server");
            connected=0;
        }
        if(answer>0) ui->correct_label_2->setText("");
    }
}
