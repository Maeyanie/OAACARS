#include "mainwindow.h"
#include "ui_mainwindow.h"

void MainWindow::gotUpdate() {
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
                // TODO: Handle time compression here.
                break;

            case 3: // speeds: kias  keas  ktas  ktgs  ----- mph   mphas mphgs
                ui->ias->setText(QString::number(val[0], 'f', 1)+" kn");
                ui->gs->setText(QString::number(val[3], 'f', 0)+" kn");

                cur.ias = val[0];
                break;

            case 4: //         mach  ----- fpm   Gnorm Gaxil Gside ----- -----
                ui->vs->setText(QString::number(val[2], 'f', 1)+" fpm");

                cur.vs = val[2];
                if (state >= 3 && state <= 5) {
                    if (val[2] > 100.0 && state != 3) climb();
                    else if (val[2] > -100.0 && state != 4) cruise();
                    else if (state != 5) descend();
                }
                if (fabs(val[3]) > maxG) {
                    maxG = fabs(val[3]);
                    ui->flightEvents->append("New Max G: "+QString::number(maxG, 'f', 1));
                }
                break;

            case 13: //        telev talrn trudr fhndl fposn srtio sbhdl sbpos
                ui->flaps->setText(QString::number(val[4]*100.0, 'f', 0)+"%");
                cur.flaps = val[4]*100.0;
                break;

            case 14: //        gear  wbrak lbrak rbrak
                cur.gear = val[0] > 0.5;
                break;

            case 17: //        pitch roll  hdt   hdm
                cur.pitch = val[0];
                cur.bank = val[1];
                cur.heading = val[3];
                break;

            case 20: //        latd  lond  altsl altgl runwy aind lats  lonw
                ui->altitude->setText(QString::number(val[2], 'f', 0)+" ft");
                ui->lat->setText(QString::number(val[0], 'f', 2)+" deg");
                ui->lon->setText(QString::number(val[1], 'f', 2)+" deg");

                cur.lat = val[0];
                cur.lon = val[1];
                if (state < 3 && val[4] == 0.0) takeoff();
                else if (state < 6 && val[4] == 1.0) landing();
                break;

            case 21: //        x     y     z     vX    vY    vZ   dstft dstnm
                break;

            case 34: // engine power
                for (int x = 0; x < 8; x++) {
                    if ((val[x] > 0.0) && !cur.engine[x]) engineStart(x);
                    else if ((val[x] < 0.0) && cur.engine[x]) engineStop(x);
                }
                break;

            case 63: // payload weights and CG
                ui->fob->setText(QString::number(val[2], 'f', 0)+" lb");
                ui->fu->setText(QString::number(startFuel-val[2], 'f', 0)+" lb");
                ui->zfw->setText(QString::number(val[0]+val[1], 'f', 0)+" lb");

                if (val[2] > cur.fuel + 0.1) refuel();
                cur.fuel = val[2];
                break;

            case 106: // switches 1: electrical
                cur.nav = val[1];
                cur.bea = val[2];
                cur.str = val[3];
                cur.ldn = val[4];
                cur.txi = val[5];
                break;

            case 114: // annunciators: general #2
                if (val[7] > 0.0) overspeed();
                break;

            case 127: //
                if (val[6] > 0.0) stall();
                break;

            default:
                //ui->textBrowser->append("Got "+QString(msg)+" with unexpected type="+QString::number(type)+" len="+QString::number(len)+", ignoring.");
                break;
            }
        }
    }
}
