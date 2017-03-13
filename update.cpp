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
    bool onRwy = false;

    while (sock->hasPendingDatagrams()) {
        len = sock->readDatagram(buffer, 2048);

        memcpy(msg, buffer, 4);
        if (!memcmp(msg, "DATA", 4)) {
            cur.time = time(NULL);
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
                    cur.ias = val[0];
                    cur.gs = val[3];

                    if (state <= PREFLIGHT && cur.gs > 2.0) taxi();
                    if ((state == TAXITORWY || state == TAXITOGATE) && cur.gs > 25.0 && !onRwy) taxiSpeed(cur.gs);
                    if (cur.ias > 250.0 && cur.asl < 10000.0) iasBelow10k();
                    break;

                case 4: //         mach  ----- fpm   ----- Gnorm Gaxil Gside -----
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
                    cur.g = fabs(val[4]) + fabs(val[5]) + fabs(val[6]);
                    if (state > PREFLIGHT && cur.g > maxG) {
                        maxG = cur.g;
                        qInfo("New Max G: %f", maxG);
                    }
                    break;

                case 13: //        telev talrn trudr fhndl fposn srtio sbhdl sbpos
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
                    // Travelling 0.1 nmi in 1/20th of a second would be about mach 10, so fairly safe call it a slew.
                    if (greatcircle(cur.lat, cur.lon, val[0], val[1]) > 0.1 && state >= PREFLIGHT && cur.lat != 0.0 && cur.lon != 0.0)
                        slew(greatcircle(cur.lat, cur.lon, val[0], val[1]));
                    cur.lat = val[0];
                    cur.lon = val[1];
                    cur.asl = val[2];
                    cur.agl = val[3];
                    onRwy = (val[4] != 0.0);
                    if ((state < CLIMB || state > DESCEND) && val[3] > (groundAGL + 10.0)) takeoff();
                    if (state >= CLIMB && state <= DESCEND && val[3] < groundAGL + 5 && cur.gs < 25) {
                        newEvent("Fallback landing triggered.", true);
                        landing();
                    }
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
                    if (!isPaused) checkFuel(val[2]);
                    cur.zfw = val[0]+val[1];
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
                    qDebug("Got packet with unexpected type=%d len=%d, ignoring.", type, len);
                    break;
                }
            }
        } else if (!memcmp(msg, "RREF", 4)) {
            for (ptr = buffer+5; ptr < buffer+len; ptr += 8) {
                memcpy(&type, ptr, 4);
                memcpy(val, ptr+4, 4);

                switch (type) {
                case 1: // sim/flightmodel/forces/fnrml_gear
                    if (state >= CLIMB && state <= DESCEND && val[0] > 0.0) landing();
                    cur.rref = time(NULL); // Keep this in the most-recently-added packet type.
                    break;

                default:
                    qInfo("Unexpected RREF type %d", type);
                    sendDRef(sock, "", type, 0);
                }
            }
        } else {
            qDebug("Got unexpected message type '%4s'", msg);
        }
    }
}

void MainWindow::sendUpdate() {
    ui->statusBar->showMessage("Sending update...");

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
}

void MainWindow::updateDone() {
    ui->statusBar->clearMessage();
    ui->statusBar->showMessage("Update sent.", 10000);
}

void MainWindow::uiUpdate() {
    static qint32 simCon = 0;
    qint64 tnow = time(NULL);

    if (cur.time < tnow - 5 && cur.rref < tnow - 5) {
        // Disconnected
        if (simCon != 0) {
            ui->conSim->setStyleSheet("QPushButton { color: rgb(255,0,0); }");
            simCon = 0;
            setDRef(sock, "oaacars/connected", 0);
        }
    } else if (cur.time < tnow - 5 || cur.rref < tnow - 5) {
        // Partly connected
        if (simCon != 1) {
            ui->conSim->setStyleSheet("QPushButton { color: rgb(255,255,0); }");
            simCon = 1;
            setDRef(sock, "oaacars/connected", 0);
        }
    } else {
        // Connected
        if (simCon != 2) {
            ui->conSim->setStyleSheet("QPushButton { color: rgb(0,255,0); }");
            simCon = 2;
            setDRef(sock, "oaacars/connected", cur.realTime);
        }
    }


    ui->ias->setText(QString::number(cur.ias, 'f', 1)+" kn");
    ui->gs->setText(QString::number(cur.gs, 'f', 1)+" kn");
    ui->vs->setText(QString::number(cur.vs, 'f', 1)+" fpm");

    ui->flaps->setText(QString::number(cur.flaps, 'f', 0)+"%");

    ui->lat->setText(QString::number(cur.lat, 'f', 2)+" deg");
    ui->lon->setText(QString::number(cur.lon, 'f', 2)+" deg");
    ui->altitude->setText(QString::number(cur.agl, 'f', 0)+" ft");

    ui->fob->setText(QString::number(cur.fuel, 'f', 0)+" lb");
    ui->fu->setText(QString::number(startFuel-cur.fuel, 'f', 0)+" lb");
    ui->zfw->setText(QString::number(cur.zfw, 'f', 0)+" lb");

    sRealTime->setText("Real: "+QDateTime::fromTime_t(tnow - startTime, Qt::UTC).toString("hh:mm:ss"));
    QString fTime;
    if (state >= CLIMB) {
        if (state <= DESCEND)
            fTime = QDateTime::fromTime_t(cur.time - onTakeoff.time - cur.pauseTime, Qt::UTC).toString("hh:mm:ss");
        else
            fTime = QDateTime::fromTime_t(onLanding.time - onTakeoff.time - onLanding.pauseTime, Qt::UTC).toString("hh:mm:ss");
    } else {
        fTime = "00:00:00";
    }
    sFlightTime->setText("Flight: "+fTime);

    if (state >= PREFLIGHT) {
        double distTotal = greatcircle(dep, arr);
        double distLeft = greatcircle(cur.lat, cur.lon, arr);
        double done = distLeft / distTotal;
        cur.remaining = distLeft;
        cur.completed = (1.0 - done) * 100;
        ui->distLeft->setText(QString::number(distLeft, 'f', 1));
        ui->completed->setValue(cur.completed);
    }
}
