#include "mainwindow.h"
#include "ui_mainwindow.h"

void MainWindow::takeoff() {
    onTakeoff = cur;
    onTakeoff.time = time(NULL);

    climb();
}

void MainWindow::climb() {
    state = 3;
}

void MainWindow::cruise() {
    state = 4;
}

void MainWindow::descend() {
    state = 5;
}

void MainWindow::landing() {
    state = 6;

    onLanding = cur;
    onLanding.time = time(NULL);
}

void MainWindow::engineStart(int e) {
    ui->flightEvents->append(QString("ENGINE %1 START").arg(e+1));
    cur.engine[e] = 1;
}
void MainWindow::engineStop(int e) {
    ui->flightEvents->append(QString("ENGINE %1 STOP").arg(e+1));
    cur.engine[e] = 0;
}

void MainWindow::refuel() {
    if (mistakes.refuel == 0) {
        mistakes.refuel = 1;
        ui->critEvents->append("REFUEL");
    }
}

void MainWindow::overspeed() {
    if (mistakes.overspeed == 0) {
        mistakes.overspeed = 1;
        ui->critEvents->append("OVERSPEED");
    }
}

void MainWindow::stall() {
    if (mistakes.stall == 0) {
        mistakes.stall = 1;
        ui->critEvents->append("STALL");
    }
}
