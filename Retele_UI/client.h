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

private:
    Ui::client *ui;
};
#endif // CLIENT_H
