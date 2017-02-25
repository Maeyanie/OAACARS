#include "mainwindow.h"
#include "ui_mainwindow.h"

void MainWindow::gotUpdate() {
    static float prevOverspeed = 0.0, prevStall = 0.0;
    static bool isPaused = false;

    qint32 len;
    char buffer[2048]; // I think the max UDP datagram size is 924 or so, so this should be plenty.
    char* ptr;
    char msg[5] = "    ";
    int type;
    float val[8];

    while (sock->hasPendingDatagrams()) {
        cur.time = time(NULL);
        len = sock->readDatagram(buffer, 2048);

        //this->ui->textBrowser->append("Got packet len="+QString::number(len));
        memcpy(msg, buffer, 4);

        for (ptr = buffer+5; ptr < buffer+len; ptr += 36) {
            memcpy(&type, ptr, 4);
            memcpy(val, ptr+4, 8*sizeof(float));

            switch (type) {
                    //         0     1     2     3     4     5     6     7
            case 1: // times:  real  totl  missn timer ----- zulu  local hobbs
                if (val[0] > cur.realTime && val[1] == cur.flightTime) {
                    cur.pauseTime += val[0] - cur.realTime;
                    if (!isPaused) {
                        paused();
                        isPaused = true;
                    }
                } else if (isPaused) {
                    unpaused();
                    isPaused = false;
                }
                cur.realTime = val[0];
                cur.flightTime = val[1];
                break;

            case 3: // speeds: kias  keas  ktas  ktgs  ----- mph   mphas mphgs
                ui->ias->setText(QString::number(val[0], 'f', 1)+" kn");
                ui->gs->setText(QString::number(val[3], 'f', 0)+" kn");

                cur.ias = val[0];
                cur.gs = val[3];

                if (state <= PREFLIGHT && cur.gs > 2.0) taxi();
                break;

            case 4: //         mach  ----- fpm   ----- Gnorm Gaxil Gside -----
                ui->vs->setText(QString::number(val[2], 'f', 1)+" fpm");

                cur.vs = val[2];
                if (state >= CLIMB && state <= DESCEND) {
                    if (val[2] > 200.0) {
                        if (state != CLIMB) climb();
                    } else if (val[2] > -200.0) {
                        if (state != CRUISE) cruise();
                    } else {
                        if (state != DESCEND) descend();
                    }
                }
                cur.g = val[4];
                if (val[4] < 0.0) val[4] = -val[4];
                if (val[4] < 500.0 && val[4] > maxG) {
                    maxG = val[4];
                    qInfo("New Max G: %f", maxG);
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
                ui->lat->setText(QString::number(val[0], 'f', 2)+" deg");
                ui->lon->setText(QString::number(val[1], 'f', 2)+" deg");
                ui->altitude->setText(QString::number(val[2], 'f', 0)+" ft");

                cur.lat = val[0];
                cur.lon = val[1];
                cur.asl = val[2];
                cur.agl = val[3];
                if ((state < CLIMB || state > DESCEND) && val[3] > (groundAGL + 5.0)) takeoff();
                else if (state >= CLIMB && state <= DESCEND && val[3] < groundAGL) landing();
                break;

            case 21: //        x     y     z     vX    vY    vZ   dstft dstnm
                cur.distance = val[7];
                break;

            case 34: // engine power
                break;

            case 45: // FF
                if (state >= PREFLIGHT) {
                    for (int x = 0; x < 8; x++) {
                        if ((val[x] > 0.0) && !cur.engine[x]) engineStart(x);
                        else if ((val[x] == 0.0) && cur.engine[x]) engineStop(x);
                    }
                }
                break;

            case 63: // payload weights and CG
                if (state <= CONNECTED) startFuel = val[2];

                ui->fob->setText(QString::number(val[2], 'f', 0)+" lb");
                ui->fu->setText(QString::number(startFuel-val[2], 'f', 0)+" lb");
                ui->zfw->setText(QString::number(val[0]+val[1], 'f', 0)+" lb");

                if (val[2] > cur.fuel + 1.0 && state > PREFLIGHT) refuel();
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
                if (val[7] > prevOverspeed) {
                    overspeed();
                    prevOverspeed = val[7];
                }
                break;

            case 127: //
                if (val[6] > prevStall) {
                    stall();
                    prevStall = val[6];
                }
                break;

            default:
                //ui->textBrowser->append("Got "+QString(msg)+" with unexpected type="+QString::number(type)+" len="+QString::number(len)+", ignoring.");
                break;
            }
        }
    }

    if (!isPaused) {
        altChart.addPoint(cur.flightTime, cur.agl);
        vsChart.addPoint(cur.flightTime, cur.vs);
        gChart.addPoint(cur.flightTime, cur.g);
    }
}

void MainWindow::sendUpdate() {
    QString statetext;
    switch (state) {
    case CONNECTED: // Should never happen, but just in case.
    case PREFLIGHT: statetext = "BOARDING"; break;
    case TAXITORWY: statetext = "TAXI TO THE RWY"; break;
    case CLIMB: statetext = "CLIMB"; break;
    case CRUISE: statetext = "CRUISE"; break;
    case DESCEND: statetext = "DESCEND"; break;
    case TAXITOGATE: statetext = "TAXI TO THE GATE"; break;
    case POSTFLIGHT: statetext = "DEBOARDING"; break;
    default: statetext = "UNKNOWN";
    }

    QJsonObject track;
    track["flight_id"] = flight;
    track["track_id"] = QString::number(trackId++);
    track["ias"] = QString::number(cur.ias, 'f', 0);
    track["gs"] = QString::number(cur.gs, 'f', 0);
    track["heading"] = QString::number(cur.heading, 'f', 0);
    track["altitude"] = QString::number(cur.asl, 'f', 0);
    track["agl"] = QString::number(cur.agl, 'f', 0);
    track["fuel"] = QString::number(cur.fuel, 'f', 0);
    track["fuel_used"] = QString::number(startFuel - cur.fuel, 'f', 0);
    track["latitude"] = QString::number(cur.lat);
    track["longitude"] = QString::number(cur.lon);
    track["time_passed"] = QString::number(cur.time - startTime, 'f', 0);
    track["time_flag"] = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    track["perc_completed"] = QString::number(cur.completed, 'f', 0);
    track["oat"] = QString::number(cur.oat, 'f', 0);
    track["wind_deg"] = QString::number(cur.winddeg, 'f', 0);
    track["wind_knots"] = QString::number(cur.windknots, 'f', 0);
    track["flight_status"] = statetext;
    track["plane_type"] = ui->acIcao->text();
    track["pending_nm"] = QString::number(cur.remaining, 'f', 0);
    va.newTrack(track);



    QJsonObject msg;
    msg["pilotId"] = pilot;
    msg["flightId"] = flight;
    msg["departure"] = ui->depIcao->text();
    msg["arrival"] = ui->arrIcao->text();
    msg["ias"] = ui->ias->text();
    msg["heading"] = cur.heading;
    msg["gs"] = ui->gs->text();
    msg["altitude"] = ui->altitude->text();
    msg["fuel"] = ui->fob->text();
    msg["fuel_used"] = ui->fu->text();
    msg["latitude"] = cur.lat;
    msg["longitude"] = cur.lon;
    msg["time_passed"] = time(NULL) - startTime;
    msg["plane_type"] = ui->acIcao->text();
    msg["perc_completed"] = QString::number(cur.completed, 'f', 0);
    msg["pending_nm"] = QString::number(cur.remaining, 'f', 0);
    msg["oat"] = QString::number(cur.oat, 'f', 0);
    msg["wind_deg"] = QString::number(cur.winddeg, 'f', 0);
    msg["wind_knots"] = QString::number(cur.windknots, 'f', 0);
    msg["flight_status"] = statetext;

    QJsonArray ja;
    ja.append(msg);
    QJsonDocument json;
    json.setArray(ja);
    va.sendUpdate(json);

    ui->statusBar->clearMessage();
    ui->statusBar->showMessage("Update sent: "+statetext, 10000);
}

void MainWindow::uiUpdate() {
    static bool simCon = false;

    qint64 tnow = time(NULL);

    if (cur.time < tnow - 5 && simCon) {
        // Disconnected
        ui->conSim->setStyleSheet("QPushButton { color: rgb(255,0,0); }");
    } else if (cur.time >= tnow - 5 && !simCon) {
        // Connected
        ui->conSim->setStyleSheet("QPushButton { color: rgb(0,255,0); }");
    }

    if (ui->checkBox->isChecked()) altChart.update();
    if (ui->checkBox_2->isChecked()) vsChart.update();
    if (ui->checkBox_3->isChecked()) gChart.update();
}
