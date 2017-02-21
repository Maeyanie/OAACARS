#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QUdpSocket>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void update();

private:
    Ui::MainWindow *ui;
    QUdpSocket* sock;
    float startFuel;
    float maxG;
};

#endif // MAINWINDOW_H
