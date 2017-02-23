#include "mainwindow.h"
#include "ui_mainwindow.h"

void MainWindow::newEvent(QString desc, bool critical) {
    QJsonObject e;
    e["flight_id"] = flight;
    e["event_id"] = QString::number(eventId++);
    e["event_description"] = desc;
    e["event_timestamp"] = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    e["ias"] = QString::number(cur.ias, 'f', 0);
    e["altitude"] = QString::number(cur.asl, 'f', 0);
    e["radio_altimeter"] = QString::number(cur.agl, 'f', 0);
    e["fuel"] = QString::number(cur.fuel, 'f', 0);
    e["fuel_used"] = QString::number(startFuel - cur.fuel, 'f', 0);
    e["log_time"] = QString::number(time(NULL) - startTime);
    e["landing_vs"] = QString::number(onLanding.vs, 'f', 2);
    e["light_nav"] = QString::number(cur.nav);
    e["light_taxi"] = QString::number(cur.txi);
    e["light_sto"] = QString::number(cur.str);
    e["light_lnd"] = QString::number(cur.ldn);
    e["light_bea"] = QString::number(cur.bea);
    e["heading"] = QString::number(cur.heading);
    e["flaps"] = QString::number(cur.flaps);
    e["critical"] = critical ? "1" : "0";

    if (critical) {
        ui->critEvents->append(QTime::currentTime().toString("hh:mm:ss")+": "+desc);
    } else {
        ui->flightEvents->append(QTime::currentTime().toString("hh:mm:ss")+": "+desc);
    }

    va.newEvent(e);
}

void MainWindow::taxi() {
    state = TAXITORWY;
    newEvent("TAXI TO RWY");
}

void MainWindow::takeoff() {
    onTakeoff = cur;

    state = CLIMB;
    newEvent("TAKEOFF");
}

void MainWindow::climb() {
    state = CLIMB;
    newEvent("CLIMB");
}

void MainWindow::cruise() {
    state = CRUISE;
    newEvent("CRUISE");
}

void MainWindow::descend() {
    state = DESCEND;
    newEvent("DESCEND");
}

void MainWindow::landing() {
    state = TAXITOGATE;
    newEvent("LANDING");

    onLanding = cur;

    ui->endButton->setEnabled(true);
}
void MainWindow::deboard() {
    state = POSTFLIGHT;
}

void MainWindow::engineStart(int e) {
    newEvent(QString("STARTING ENGINE %1").arg(e+1));
    cur.engine[e] = 1;
}
void MainWindow::engineStop(int e) {
    newEvent(QString("STOPPING ENGINE %1").arg(e+1));
    cur.engine[e] = 0;

    if (state == TAXITOGATE) {
        int onCount = 0;
        for (int x = 0; x < 8; x++) {
            if (cur.engine[x] == 1) onCount++;
        }
        if (onCount == 0) deboard();
    }
}

void MainWindow::refuel() {
    if (mistakes.refuel == 0) {
        mistakes.refuel = 1;
        newEvent("REFUEL", true);
    }
}

void MainWindow::overspeed() {
    if (mistakes.overspeed == 0) {
        mistakes.overspeed = 1;
        newEvent("OVERSPEED", true);
    }
}

void MainWindow::stall() {
    if (mistakes.stall == 0) {
        mistakes.stall = 1;
        newEvent("STALL", true);
    }
}
