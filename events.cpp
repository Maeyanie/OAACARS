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
        critEvents++;
        ui->critEvents->append(QTime::currentTime().toString("hh:mm:ss")+": "+desc);
    } else {
        ui->flightEvents->append(QTime::currentTime().toString("hh:mm:ss")+": "+desc);
    }

    va.newEvent(e);
}

void MainWindow::taxi() {
    state = TAXITORWY;
    newEvent("TAXI TO RWY");

    if (!cur.txi) taxiNoLights();
}

void MainWindow::takeoff() {
    if (onTakeoff.time == 0) onTakeoff = cur;

    state = CRUISE;
    newEvent("TAKEOFF");

    if (!cur.ldn) takeoffNoLights();
    if (fabs(cur.qnhReal - cur.qnhSet) > 0.05) qnhTakeoff();
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
    if (cur.vs < -650.0) crashed(QString("IMPACT AT %1 FPM").arg(cur.vs));
    else if (cur.g > 15.0) crashed(QString("IMPACT AT %1 G").arg(cur.g));
    else if (!cur.onRwy) crashed("NOT ON RUNWAY");
    else newEvent("LANDING");

    if (!cur.ldn) landingNoLights();
    if (fabs(cur.qnhReal - cur.qnhSet) > 0.05) qnhLanding();
    if (greatcircle(cur.lat, cur.lon, arr) > 10.0) wrongAirport();

    state = TAXITOGATE;
    onLanding = cur;

    ui->fsDepIcao->setText(ui->depIcao->text());
    ui->fsArrIcao->setText(ui->arrIcao->text());
    ui->fsDistance->setText(QString::number(onLanding.distance-onTakeoff.distance, 'f', 1));
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

    if (!cur.bea) beaconOff();
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

void MainWindow::refuel(float prev, float cur) {
    static qint64 last = 0;
    if (last < time(NULL) - 10) newEvent(QString("REFUEL: %1 -> %2 (%3)").arg(prev).arg(cur).arg(cur-prev), true);
    last = time(NULL);

    mistakes.refuel = 1;
}
void MainWindow::checkFuel(float fuel) {
    static float window[20];
    static int pos = 0;

    int max = 0.0;
    for (int x = 0; x < 20; x++) {
        if (window[x] > max) max = window[x];
    }

    if (fuel > max && state > PREFLIGHT && (fuel - max) > 1.0) refuel(max, fuel);

    window[pos++] = fuel;
    if (pos >= 20) pos = 0;
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

void MainWindow::beaconOff() {
    static qint64 last = 0;
    if (last < time(NULL) - 10) newEvent("ENGINE RUNNING WITH BEACON OFF", true);
    last = time(NULL);

    mistakes.beaconOff = true;
}

void MainWindow::iasBelow10k() {
    static qint64 last = 0;
    if (last < time(NULL) - 10) newEvent("IAS ABOVE 250 KNOTS BELOW 10000 ft", true);
    last = time(NULL);

    mistakes.iasLow = true;
}

void MainWindow::lightsBelow10k() { // Not yet called anywhere
    static qint64 last = 0;
    if (last < time(NULL) - 10) newEvent("LIGHTS OFF BELOW 10000 ft", true);
    last = time(NULL);

    mistakes.lightsLow = true;
}

void MainWindow::lightsAbove10k() { // Not yet called anywhere
    static qint64 last = 0;
    if (last < time(NULL) - 10) newEvent("LIGHTS ON ABOVE 10000 ft", true);
    last = time(NULL);

    mistakes.lightsHigh = true;
}

void MainWindow::slew(float distance) {
    newEvent(QString("SLEW: %1 nmi").arg(distance), true);

    mistakes.slew = true;
}

void MainWindow::taxiNoLights() {
    static qint64 last = 0;
    if (last < time(NULL) - 10) newEvent("TAXING WITH TAXI LIGHTS OFF", true);
    last = time(NULL);

    mistakes.taxiLights = true;
}

void MainWindow::takeoffNoLights() {
    newEvent("TAKEOFF WITH LANDING LIGHTS OFF", true);

    mistakes.takeoffLights = true;
}

void MainWindow::landingNoLights(){
    newEvent("LANDING WITH LANDING LIGHTS OFF", true);

    mistakes.landingLights = true;
}

void MainWindow::wrongAirport() {
    newEvent("LANDED AT WRONG LOCATION", true);

    mistakes.landingAirport = true;
}

void MainWindow::taxiSpeed(float speed) {
    static qint64 last = 0;
    if (last < time(NULL) - 10) newEvent(QString("TAXI SPEED ABOVE 25 KTS (%1)").arg(speed), true);
    last = time(NULL);

    mistakes.taxiSpeed = true;
}

void MainWindow::qnhTakeoff() {
    newEvent("WRONG QNH AT TAKEOFF", true);

    mistakes.qnhTakeoff = true;
}

void MainWindow::qnhLanding() {
    newEvent("WRONG QNH AT LANDING", true);

    mistakes.qnhLanding = true;
}

void MainWindow::crashed(QString reason) {
    if (state <= PREFLIGHT || mistakes.crash) return;

    newEvent("CRASHED: "+reason, true);

    mistakes.crash = true;
}
