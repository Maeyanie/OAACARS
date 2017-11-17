#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();

    return a.exec();
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

double greatcircle(QPair<double,double> src, QPair<double,double> tgt) {
    return greatcircle(src.first, src.second, tgt.first, tgt.second);
}
double greatcircle(double lat1, double lon1, QPair<double,double> tgt) {
    return greatcircle(lat1, lon1, tgt.first, tgt.second);
}
double greatcircle(QPair<double,double> src, double lat2, double lon2) {
    return greatcircle(src.first, src.second, lat2, lon2);
}
double greatcircle(double lat1, double lon1, double lat2, double lon2) {
    lat1 *= (M_PI/180.0);
    lon1 *= (M_PI/180.0);
    lat2 *= (M_PI/180.0);
    lon2 *= (M_PI/180.0);

    double deltalat = lat2 - lat1;
    double deltalon = lon2 - lon1;

    double a = sin(deltalat/2) * sin(deltalat/2) +
                cos(lat1) * cos(lat2) *
                sin(deltalon/2) * sin(deltalon/2);
    double c = 2 * atan2(sqrt(a), sqrt(1-a));
    return 3443.9185 * c; // Earth radius in nmi
}
