#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QMessageBox>
#include <QTimer>
#include <cstdarg>

/*
Events:
    ias,
    altitude,
    radio_altimeter,
    heading,
    fuel,
    fuel_used,
    log_time,
    landing_vs,
    flaps,
    light_nav,
    light_taxi,
    light_sto,
    light_lnd,
    light_bea,

Tracks:
    ias
    heading
    gs
    altitude
    fuel
    fuel_used
    latitude
    longitude
    time_passed
    oat
    wind_deg
    wind_knots
    plane_type
Needs to be calc'd from: latitude, longitude
    perc_completed
    pending_nm
    */
void memcat(char** ptr, const char* data, int len) {
    memcpy(*ptr, data, len);
    (*ptr) += len;
}
void memcat(char** ptr, const int data) {
    memcpy(*ptr, &data, sizeof(data));
    (*ptr) += sizeof(data);
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    sock = new QUdpSocket(this);
    if (!sock->bind(32123)) {
        QMessageBox::critical(this, "Error", "Error: Could not bind to port 32123.", QMessageBox::Ok);
        exit(1);
    }
    connect(sock, SIGNAL(readyRead()), this, SLOT(update()));

    QHostAddress addr("127.0.0.1");
    char buffer[1024];
    char* pos = buffer;
    memcat(&pos, "DSEL", 5);
    memcat(&pos, 1); // times
    memcat(&pos, 3); // speeds
    memcat(&pos, 4); // Mach, VVI, G-load
    memcat(&pos, 13);// trim/flap/slat/s-brakes
    memcat(&pos, 14);// gear/brakes
    memcat(&pos, 20);// lat, lon, altitude
    memcat(&pos, 21);// loc, vel, dist traveled
    //memcat(&pos, 62);// fuel weights
    memcat(&pos, 63);// payload weights and CG
    memcat(&pos, 106); // switches 1: electrical
    sock->writeDatagram(buffer, (pos-buffer), addr, 49000);

    memset(buffer, 0, 1024);
    pos = buffer;
    memcat(&pos, "ISET", 5);
    memcat(&pos, 64);
    memcat(&pos, "127.000.000.001", 16);
    memcat(&pos, "32123", 6); pos += 2;
    memcat(&pos, 1);
    sock->writeDatagram(buffer, (pos-buffer), addr, 49000);

    startFuel = 0.0f;
    maxG = 0.0f;
}

MainWindow::~MainWindow()
{
    sock->close();
    delete sock;
    delete ui;
}

void MainWindow::update() {
    qint32 len;
    char buffer[2048]; // I think the max UDP datagram size is 924 or so, so this should be plenty.
    char* ptr;
    char msg[5] = "    ";
    int type;
    float val[8];

    while (sock->hasPendingDatagrams()) {
        len = sock->readDatagram(buffer, 2048);

        //this->ui->textBrowser->append("Got packet len="+QString::number(len));
        memcpy(msg, buffer, 4);

        for (ptr = buffer+5; ptr < buffer+len; ptr += 36) {
            memcpy(&type, ptr, 4);
            memcpy(val, ptr+4, 8*sizeof(float));

            switch (type) {
                    //         0     1     2     3     4     5     6     7
            case 1: // times:  real  totl  missn timer ----- zulu  local hobbs
                break;
            case 3: // speeds: kias  keas  ktas  ktgs  ----- mph   mphas mphgs
                ui->ias->setPlainText(QString::number(val[0], 'f', 1)+" kn");
                ui->gs->setPlainText(QString::number(val[3], 'f', 0)+" kn");
                break;
            case 4: //         mach  ----- fpm   Gnorm Gaxil Gside ----- -----
                ui->vs->setPlainText(QString::number(val[2], 'f', 1)+" fpm");
                if (val[3] + val[4] + val[5] > maxG) {
                    maxG = val[3] + val[4] + val[5];
                    ui->flightEvents->append("New Max G: "+QString::number(maxG, 'f', 1));
                    ui->flightEvents->update();
                }
            case 13: //        telev talrn trudr fhndl fposn srtio sbhdl sbpos
                ui->flaps->setPlainText(QString::number(val[4]*100, 'f', 0)+"%");
                break;
            case 14: //        gear  wbrak lbrak rbrak
                break;
            case 20: //        latd  lond  altsl altgl runwy aind lats  lonw
                ui->altitude->setPlainText(QString::number(val[2], 'f', 0)+" ft");
                ui->lat->setPlainText(QString::number(val[0], 'f', 2)+" deg");
                ui->lon->setPlainText(QString::number(val[1], 'f', 2)+" deg");
                break;
            case 21: //        x     y     z     vX    vY    vZ   dstft dstnm
                break;
            case 62: // fuel weights
                break;
            case 63: // payload weights and CG
                if (val[2] > startFuel) startFuel = val[2];
                ui->fob->setPlainText(QString::number(val[2], 'f', 0)+" lb");
                ui->fu->setPlainText(QString::number(startFuel-val[2], 'f', 0)+" lb");
                ui->zfw->setPlainText(QString::number(val[0]+val[1], 'f', 0)+" lb");
                break;
            case 106: // switches 1: electrical
                break;
            default:
                //ui->textBrowser->append("Got "+QString(msg)+" with unexpected type="+QString::number(type)+" len="+QString::number(len)+", ignoring.");
                break;
            }
        }
    }
}
