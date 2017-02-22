#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QUdpSocket>
#include "va.h"

struct Status {
    qint64 time;
    float pitch, bank, heading, vs, ias, lat, lon;
    float flaps, fuel;
    bool engine[8];
    bool gear;
    bool bea, nav, ldn, str, txi;
    float winddeg, windknots, oat;
};

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void gotUpdate();
    void sendUpdate();

    void on_connectButton_clicked();
    void on_startButton_clicked();

    void on_endButton_clicked();

    void on_callsign_textChanged(const QString &arg1);
    void on_password_textChanged(const QString &arg1);

private:
    void takeoff();
    void climb();
    void cruise();
    void descend();
    void landing();
    void engineStart(int e);
    void engineStop(int e);
    void refuel();
    void overspeed();
    void stall();

    Ui::MainWindow *ui;
    QUdpSocket* sock;
    VA va;
    QTimer timer;
    qint32 state; // connected, preflight, taxi-to-runway, climb, cruise, descend, taxi-to-gate, postflight, -1=offline
    qint32 pilot;
    QString flight;
    qint64 startTime;
    float startLat, startLon;
    float startFuel, maxG;

    Status cur, onTakeoff, onLanding;

    struct Mistakes {
        bool crash, beaconOff, iasLow, lightsLow, lightsHigh, overspeed, pause,
            refuel, slew, taxiLights, takeoffLights, landingLights,
            landingAirport, stall, taxiSpeed, qnhTakeoff, qnhLanding;

        void reset() {
            crash = beaconOff = iasLow = lightsLow = lightsHigh = overspeed = pause
                    = refuel = slew = taxiLights = takeoffLights = landingLights
                    = landingAirport = stall = taxiSpeed = qnhTakeoff = qnhLanding = 0;
        }
    } mistakes;
};

#endif // MAINWINDOW_H
