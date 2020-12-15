#include "client.h"
#include <QApplication>
#pragma once
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    client login_page;

    login_page.show();

    return a.exec();
}
