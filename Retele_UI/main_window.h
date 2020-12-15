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
    Ui::main_window *ui;

    typedef struct {
        int subscribed;
        int socketD;
        char *username;
    } info_to_pass;

    typedef struct{
        main_window *current;
        info_to_pass information;
    }obj_to_pass;

    obj_to_pass *thisObj;

    explicit main_window(QWidget *parent = nullptr);
    ~main_window();
    void showEvent(QShowEvent *ev);

    void set_name();


friend void display_alert(obj_to_pass currentObject,char*);

private slots:


void on_report_btn_clicked();
};

#endif // MAIN_WINDOW_H
