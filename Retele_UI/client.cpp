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

void client::on_login_btn_clicked()
{
    QString username = ui->user_input->text();
    QString password = ui->pass_input->text();

    QByteArray ba = username.toLocal8Bit();
    char *char_user = ba.data();

    ba = password.toLocal8Bit();
    char *char_password = ba.data();

    if(!connected)
    {
        socketD = connect_to_server();
        connected=1;
    }

    validate(socketD,'l',char_user,char_password);

}
