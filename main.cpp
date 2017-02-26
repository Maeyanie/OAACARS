#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();

    return a.exec();
}

void memcat(char** ptr, const char* data, int len) {
    memcpy(*ptr, data, len);
    (*ptr) += len;
}
void memcat(char** ptr, const char* data) {
    memcpy(*ptr, data, strlen(data)+1);
    (*ptr) += strlen(data)+1;
}
void memcat(char** ptr, const int data) {
    memcpy(*ptr, &data, sizeof(data));
    (*ptr) += sizeof(data);
}

void sendDRef(QUdpSocket* sock, const char* name, qint32 id, qint32 freq) {
    QHostAddress addr("127.0.0.1");
    char buffer[1024];

    memset(buffer, 0, 1024);
    char* pos = buffer;
    memcat(&pos, "RREF", 5);
    memcat(&pos, freq);
    memcat(&pos, id);
    memcat(&pos, name);
    sock->writeDatagram(buffer, 413, addr, 49000);
}

void setDRef(QUdpSocket* sock, const char* name, float val) {
    QHostAddress addr("127.0.0.1");
    char buffer[1024];

    memset(buffer, 0, 1024);
    char* pos = buffer;
    memcat(&pos, "DREF", 5);
    memcat(&pos, val);
    memcat(&pos, name);
    sock->writeDatagram(buffer, 509, addr, 49000);
}
