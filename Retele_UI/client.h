#ifndef CLIENT_H
#define CLIENT_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui { class client; }
QT_END_NAMESPACE

class client : public QMainWindow
{
    Q_OBJECT

public:
    client(QWidget *parent = nullptr);
    ~client();

private slots:

    void on_login_btn_clicked();

    void on_register_switch_clicked();

    void on_login_switch_clicked();

    void on_register_btn_clicked();

private:
    Ui::client *ui;
};
#endif // CLIENT_H
