#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QUdpSocket>
#include "va.h"

enum State {
    OFFLINE = -1,
    CONNECTED = 0,
    PREFLIGHT,
    TAXITORWY,
    CLIMB,
    CRUISE,
    DESCEND,
    TAXITOGATE,
    POSTFLIGHT
};

struct Status {
    qint64 time;
    float pitch, bank, heading, vs, ias, gs, lat, lon, asl, agl;
    float flaps, fuel, distance, completed, remaining;
    char engine[8];
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
    void newEvent(QString desc, bool critical = 0);
    void taxi();
    void takeoff();
    void climb();
    void cruise();
    void descend();
    void landing();
    void deboard();
    void engineStart(int e);
    void engineStop(int e);
    void refuel();
    void overspeed();
    void stall();

    Ui::MainWindow *ui;
    QUdpSocket* sock;
    VA va;
    QTimer timer;
    State state;
    qint32 pilot;
    qint64 startTime;
    float startLat, startLon;
    float maxG;
    QString flight;
    float startFuel;
    Status cur, onTakeoff, onLanding;
    qint32 trackId, eventId, critEvents;

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
