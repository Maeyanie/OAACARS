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
    ui(new Ui::MainWindow),
    va(this)
{
    ui->setupUi(this);
    ui->password->setEchoMode(QLineEdit::Password);

    QFile loginFile("login.txt");
    if (loginFile.open(QFile::ReadOnly)) {
        QTextStream in(&loginFile);
        ui->callsign->setText(in.readLine().trimmed());
        ui->password->setText(in.readLine().trimmed());
        loginFile.close();
    }

    connect(&timer, SIGNAL(timeout()), this, SLOT(sendUpdate()));

    sock = new QUdpSocket(this);
    if (!sock->bind(32123)) {
        QMessageBox::critical(this, "Error", "Error: Could not bind to port 32123.", QMessageBox::Ok);
        exit(1);
    }
    connect(sock, SIGNAL(readyRead()), this, SLOT(gotUpdate()));

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

    state = -1;
    startFuel = 0.0f;
    maxG = 0.0f;
}

MainWindow::~MainWindow()
{
    sock->close();
    delete sock;
    delete ui;
}

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
                break;
            case 3: // speeds: kias  keas  ktas  ktgs  ----- mph   mphas mphgs
                ui->ias->setText(QString::number(val[0], 'f', 1)+" kn");
                ui->gs->setText(QString::number(val[3], 'f', 0)+" kn");
                break;
            case 4: //         mach  ----- fpm   Gnorm Gaxil Gside ----- -----
                ui->vs->setText(QString::number(val[2], 'f', 1)+" fpm");
                if (fabs(val[3]) + fabs(val[4]) + fabs(val[5]) > maxG) {
                    maxG = fabs(val[3]) + fabs(val[4]) + fabs(val[5]);
                    ui->flightEvents->append("New Max G: "+QString::number(maxG, 'f', 1));
                }
            case 13: //        telev talrn trudr fhndl fposn srtio sbhdl sbpos
                ui->flaps->setText(QString::number(val[4]*100, 'f', 0)+"%");
                break;
            case 14: //        gear  wbrak lbrak rbrak
                break;
            case 20: //        latd  lond  altsl altgl runwy aind lats  lonw
                ui->altitude->setText(QString::number(val[2], 'f', 0)+" ft");
                ui->lat->setText(QString::number(val[0], 'f', 2)+" deg");
                ui->lon->setText(QString::number(val[1], 'f', 2)+" deg");
                lat = val[0];
                lon = val[1];
                break;
            case 21: //        x     y     z     vX    vY    vZ   dstft dstnm
                break;
            case 63: // payload weights and CG
                if (val[2] > startFuel) startFuel = val[2];
                ui->fob->setText(QString::number(val[2], 'f', 0)+" lb");
                ui->fu->setText(QString::number(startFuel-val[2], 'f', 0)+" lb");
                ui->zfw->setText(QString::number(val[0]+val[1], 'f', 0)+" lb");
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

void MainWindow::sendUpdate() {
    /*[{"pilotId":"40","flightId":"2017222102431964OPA1115","departure":"EDDM","arrival":"EHAM","ias":"63","heading":"83",
     * "gs":"64","altitude":"1494","fuel":"6912","fuel_used":"26","latitude":"48.3630222722823","longitude":"11.7710648617527",
     * "time_passed":"817","perc_completed":"0","oat":"12","wind_deg":"12","wind_knots":"0","flight_status":"TAXI TO THE RWY ",
     * "plane_type":"B733","pending_nm":"358"}]
     */

    QJsonObject msg;
    msg["pilotId"] = pilot;
    msg["flightId"] = flight;
    msg["departure"] = ui->depIcao->text();
    msg["arrival"] = ui->arrIcao->text();
    msg["ias"] = ui->ias->text();
    msg["heading"] = heading;
    msg["gs"] = ui->gs->text();
    msg["altitude"] = ui->altitude->text();
    msg["fuel"] = ui->fob->text();
    msg["fuel_used"] = ui->fu->text();
    msg["latitude"] = lat;
    msg["longitude"] = lon;
    msg["time_passed"] = time(NULL) - startTime;
    msg["plane_type"] = ui->acIcao->text();

    // TODO:
    msg["perc_completed"] = 0;
    msg["pending_nm"] = 0;
    msg["oat"] = 0;
    msg["wind_deg"] = 0;
    msg["wind_knots"] = 0;

    QString statetext;
    switch (state) {
    case 0: // Should never happen, but just in case.
    case 1:
        statetext = "BOARDING";
        break;
    case 2:
        statetext = "TAXI TO THE RWY";
        break;
    case 3:
        statetext = "CLIMB";
        break;
    case 4:
        statetext = "CRUISE";
        break;
    case 5:
        statetext = "DESCEND";
        break;
    case 6:
        statetext = "TAXI TO THE GATE";
        break;
    case 7:
        statetext = "DEBOARDING";
        break;
    default:
        statetext = "UNKNOWN";
    }
    msg["flight_status"] = statetext;

    QJsonArray ja;
    ja.append(msg);
    QJsonDocument json;
    json.setArray(ja);
    va.sendUpdate(json);

    ui->statusBar->clearMessage();
    ui->statusBar->showMessage("Update sent: "+statetext, 10000);
}

void MainWindow::on_connectButton_clicked()
{
    ui->statusBar->showMessage("Connecting...");

    QString ret = va.login(ui->callsign->text(), ui->password->text());
    if (ret == "FAIL") {
        QMessageBox::warning(this, "Warning", "Login failed.", QMessageBox::Ok);
        return;
    }

    ret = va.getAircraft();
    if (ret == "FAIL") {
        ui->aircraft->setText("");
    } else if (ret != "") {
        // Need to see what this actually looks like.
        QMessageBox::information(this, "GetAircraft", "I couldn't get that to work in my testing, but apparently it did for you!\n"
                                 "Please send me this text:\n"+ret, QMessageBox::Ok);
    }

    // [{"id":"40","departure":"","arrival":"","alternative":"","route":"","callsign":"","etd":"","plane_icao":"","duration":"","registry":"","pax":"","cargo":""}]
    ret = va.pilotConnection();
    QJsonParseError parseErr;
    QJsonDocument pc = QJsonDocument::fromJson(ret.toUtf8(), &parseErr);

    qInfo("isArray=%d isObject=%d parseErr=%s\n",
          pc.isArray(), pc.isObject(), parseErr.errorString().toUtf8().data());

    QJsonArray ja = pc.array();
    QJsonValue jv = ja.first();
    QJsonObject data = jv.toObject();
    pilot = data["id"].toString().toInt();
    qInfo("Got pilot %d\n", pilot);

    ui->depIcao->setText(data["departure"].toString());
    ui->arrIcao->setText(data["arrival"].toString());
    ui->alt1->setText(data["alternative"].toString());
    ui->route->setPlainText(data["route"].toString());
    ui->tailNumber->setText(data["callsign"].toString());
    ui->depTime->setText(data["etd"].toString());
    ui->acIcao->setText(data["plane_icao"].toString());
    ui->eet->setText(data["duration"].toString());
    ui->tailNumber->setText(data["registry"].toString());
    ui->pax->setText(data["pax"].toString());
    ui->cargo->setText(data["cargo"].toString());

    state = 0; // connected
    ui->statusBar->clearMessage();
    ui->statusBar->showMessage("Connected.");
    ui->startButton->setEnabled(true);
    ui->endButton->setEnabled(false);
    timer.stop();
}

void MainWindow::on_startButton_clicked()
{
    // 2017222102431964OPA1115
    // 2017-2-22 10:24:31.964 OPA1115
    flight = QDateTime::currentDateTime().toString("yyyyMdhhmmsszzz")+ui->flightNo->text();
    startTime = time(NULL);
    startLat = lat;
    startLon = lon;

    state = 1;
    flaps = 0;
    gear = 1;
    engine[0] = engine[1] = engine[2] = engine[3] = 0;

    sendUpdate();
    timer.start(60000);
    ui->startButton->setEnabled(false);
    ui->endButton->setEnabled(true);
}

void MainWindow::on_endButton_clicked()
{
    timer.stop();
    state = 0;
    ui->endButton->setEnabled(false);
    ui->statusBar->showMessage("Flight submitted.", 10000);
}

void MainWindow::on_callsign_textChanged(const QString &arg1)
{
    QFile loginFile("login.txt");
    if (loginFile.open(QFile::WriteOnly | QFile::Truncate)) {
        QTextStream out(&loginFile);
        out << ui->callsign->text() << endl << ui->password->text() << endl;
        loginFile.close();
    }
}

void MainWindow::on_password_textChanged(const QString &arg1)
{
    QFile loginFile("login.txt");
    if (loginFile.open(QFile::WriteOnly | QFile::Truncate)) {
        QTextStream out(&loginFile);
        out << ui->callsign->text() << endl << ui->password->text() << endl;
        loginFile.close();
    }
}
