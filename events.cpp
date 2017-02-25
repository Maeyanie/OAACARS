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

    state = CRUISE;
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

    ui->fsDepIcao->setText(ui->depIcao->text());
    ui->fsArrIcao->setText(ui->arrIcao->text());
    ui->fsDistance->setText(QString::number(onLanding.distance, 'f', 1));
    ui->fsZFW->setText(ui->zfw->text());
    ui->fsCritEvents->setText(QString::number(critEvents));
    ui->fsDuration->setText(QString::number((onLanding.time-onTakeoff.time)/3600.0, 'f', 2));
    ui->fsBFuel->setText(QString::number(startFuel-onLanding.fuel, 'f', 0));
    ui->fsFFuel->setText(QString::number(onTakeoff.fuel-onLanding.fuel, 'f', 0));
    ui->fsVS->setText(QString::number(onLanding.vs, 'f', 0));
    ui->fsG->setText(QString::number(maxG, 'f', 1));
    ui->fsPitch->setText(QString::number(onLanding.pitch, 'f', 0));
    ui->fsIAS->setText(QString::number(onLanding.ias, 'f', 1));
    ui->fsBank->setText(QString::number(onLanding.bank, 'f', 1));
    ui->fsFlaps->setText(QString::number(onLanding.flaps, 'f', 0));

    ui->tabWidget->setCurrentWidget(ui->summaryTab);
    ui->endButton->setEnabled(true);
}
void MainWindow::deboard() {
    state = POSTFLIGHT;
    newEvent("DEBOARDING");
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
    static qint64 last = 0;
    if (last < time(NULL) - 10) newEvent("REFUEL", true);
    last = time(NULL);

    mistakes.refuel = 1;
}

void MainWindow::overspeed() {
    static qint64 last = 0;
    if (last < time(NULL) - 10) newEvent("OVERSPEED", true);
    last = time(NULL);

    mistakes.overspeed = 1;
}

void MainWindow::stall() {
    static qint64 last = 0;
    if (last < time(NULL) - 10) newEvent("STALL", true);
    last = time(NULL);

    mistakes.stall = 1;
}

void MainWindow::paused() {
    newEvent("PAUSED", true);
    mistakes.pause = 1;
}
void MainWindow::unpaused() {
    newEvent("UNPAUSED", true);
}
