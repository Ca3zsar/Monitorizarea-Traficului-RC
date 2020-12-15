#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H
#pragma once
#include <QMainWindow>

namespace Ui {
class main_window;
}

class main_window : public QMainWindow
{
    Q_OBJECT

public:
    explicit main_window(QWidget *parent = nullptr);
    ~main_window();
    void showEvent(QShowEvent *ev);

    typedef struct {
        int subscribed;
        int socketD;
        char *username;
    } info_to_pass;

    info_to_pass information;

private slots:

private:
    Ui::main_window *ui;

};

#endif // MAIN_WINDOW_H
