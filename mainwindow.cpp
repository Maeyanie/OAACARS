#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QMessageBox>
#include <QTimer>

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
    memcat(&pos, 17);// pitch, roll, headings
    memcat(&pos, 20);// lat, lon, altitude
    memcat(&pos, 21);// loc, vel, dist traveled
    memcat(&pos, 45); // FF
    memcat(&pos, 63);// payload weights and CG
    memcat(&pos, 106); // switches 1: electrical
    memcat(&pos, 114); // annunciators: general #2
    memcat(&pos, 127); // warning status
    sock->writeDatagram(buffer, (pos-buffer), addr, 49000);

    memset(buffer, 0, 1024);
    pos = buffer;
    memcat(&pos, "ISET", 5);
    memcat(&pos, 64);
    memcat(&pos, "127.000.000.001", 16);
    memcat(&pos, "32123", 6); pos += 2;
    memcat(&pos, 1);
    sock->writeDatagram(buffer, (pos-buffer), addr, 49000);

    state = OFFLINE;
    startFuel = 0.0f;
    maxG = 0.0f;
    memset(&cur, 0, sizeof(cur));
    onTakeoff = onLanding = cur;
}

MainWindow::~MainWindow()
{
    sock->close();
    delete sock;
    delete ui;
}



void MainWindow::on_connectButton_clicked()
{
    timer.stop();
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

    ret = va.pilotConnection();
    QJsonParseError parseErr;
    QJsonDocument pc = QJsonDocument::fromJson(ret.toUtf8(), &parseErr);
    QJsonArray ja = pc.array();
    QJsonValue jv = ja.first();
    QJsonObject data = jv.toObject();
    pilot = data["id"].toString().toInt();

    if (data.find("departure") != data.end() && data["departure"] != "") {
        int yn = QMessageBox::question(this, "Flight Plan", "A flight plan was found. Load it?", QMessageBox::Yes, QMessageBox::No);
        if (yn == QMessageBox::Yes) {
            ui->depIcao->setText(data["departure"].toString());
            ui->arrIcao->setText(data["arrival"].toString());
            ui->alt1->setText(data["alternative"].toString());
            ui->route->setPlainText(data["route"].toString());
            ui->flightNo->setText(data["callsign"].toString());
            ui->depTime->setText(data["etd"].toString());
            ui->acIcao->setText(data["plane_icao"].toString());
            ui->eet->setText(data["duration"].toString());
            ui->tailNumber->setText(data["registry"].toString());
            ui->pax->setText(data["pax"].toString());
            ui->cargo->setText(data["cargo"].toString());
        }
    }

    state = CONNECTED;
    ui->statusBar->clearMessage();
    ui->statusBar->showMessage("Connected.");
    ui->startButton->setEnabled(true);
    ui->endButton->setEnabled(false);
}

void MainWindow::on_startButton_clicked()
{
    if (cur.agl > 20.0) {
        qint32 ret = QMessageBox::warning(this, "High Ground AGL",
                                          QString("Your AGL altitude of %1 seems a little too high. Are you sure you're on the ground?").arg(cur.agl),
                                          QMessageBox::Yes, QMessageBox::No);
        if (ret == QMessageBox::No) return;
    }
    groundAGL = cur.agl + 0.1;

    // 2017222102431964OPA1115
    // 2017-2-22 10:24:31.964 OPA1115
    flight = QDateTime::currentDateTime().toString("yyyyMdhhmmsszzz")+ui->flightNo->text();
    startTime = time(NULL);
    startLat = cur.lat;
    startLon = cur.lon;
    trackId = eventId = 1;

    state = PREFLIGHT;
    cur.flaps = 0;
    cur.gear = 1;
    for (int x = 0; x < 8; x++) cur.engine[x] = 0;
    mistakes.reset();

    ui->tabWidget->setCurrentWidget(ui->dataTab);

    newEvent("FLIGHT STARTED");

    sendUpdate();
    timer.start(60000);
    ui->startButton->setEnabled(false);
}

void MainWindow::on_endButton_clicked()
{
    timer.stop();
    ui->statusBar->showMessage("Submitting...");

    QJsonObject msg;
    msg["flightId"] = flight;
    msg["weight_unit"] = "Lbs";
    msg["departure"] = ui->depIcao->text();
    msg["arrival"] = ui->arrIcao->text();
    msg["gvauserId"] = QString::number(pilot);
    msg["callsign"] = ui->flightNo->text();
    msg["departure_time"] = ui->depTime->text();
    msg["cruise_speed"] = ui->cruiseSpeed->text();
    msg["flight_level"] = ui->cruiseAlt->text();
    msg["pax"] = ui->pax->text();
    msg["cargo"] = ui->cargo->text();
    msg["eet"] = ui->eet->text();
    msg["endurance"] = ui->endurance->text();
    msg["alt1"] = ui->alt1->text();
    msg["alt2"] = ui->alt2->text();
    msg["route"] = ui->route->toPlainText();
    msg["remarks"] = ui->remarks->toPlainText();
    msg["flight_type"] = ui->flightType->currentText();
    msg["aircraft"] = ui->aircraft->text();
    msg["aircraft_type"] = ui->acIcao->text();
    msg["aircraft_registry"] = ui->tailNumber->text();
    msg["flight_status"] = "FLIGHT FINISHED";
    msg["flight_duration"] = QString::number((onLanding.time-onTakeoff.time) / 3600.0, 'f', 2);
    msg["flight_fuel"] = QString::number(onTakeoff.fuel-onLanding.fuel, 'f', 0);
    msg["block_fuel"] = QString::number(startFuel-cur.fuel, 'f', 0);
    msg["final_fuel"] = QString::number(cur.fuel, 'f', 0);
    msg["zfw"] = ui->zfw->text();
    msg["flight_date"] = QDateTime::currentDateTime().toString("yyyy-M-d hh:mm:ss");

    msg["distance"] = onLanding.distance;

    msg["landing_vs"] = QString::number(onLanding.vs);
    msg["landing_ias"] = QString::number(onLanding.ias);
    msg["landing_forceg"] = QString::number(maxG);
    msg["landing_bank"] = QString::number(onLanding.bank);
    msg["landing_pitch"] = QString::number(onLanding.pitch);
    msg["landing_hdg"] = QString::number(onLanding.heading);
    msg["landing_winddeg"] = QString::number(onLanding.winddeg);
    msg["landing_windknots"] = QString::number(onLanding.windknots);
    msg["landing_oat"] = QString::number(onLanding.oat);
    msg["landing_flaps"] = QString::number(onLanding.flaps);
    msg["landing_light_bea"] = QString::number(onLanding.bea);
    msg["landing_light_nav"] = QString::number(onLanding.nav);
    msg["landing_light_ldn"] = QString::number(onLanding.ldn);
    msg["landing_light_str"] = QString::number(onLanding.str);

    msg["log_start"] = QDateTime::fromTime_t(startTime).toString("yyyy-M-d hh:mm:ss");
    msg["flight_start"] = QDateTime::fromTime_t(onTakeoff.time).toString("yyyy-M-d hh:mm:ss");
    msg["flight_end"] = QDateTime::fromTime_t(onLanding.time).toString("yyyy-M-d hh:mm:ss");
    msg["log_end"] = QDateTime::currentDateTime().toString("yyyy-M-d hh:mm:ss");

    msg["departure_metar"] = "";
    msg["arrival_metar"] = "";
    msg["network"] = "UNSUPPORTED";
    msg["comments"] = "";

    msg["pause_time"] = "0.00";
    msg["crash"] = QString::number(mistakes.crash);
    msg["beacon_off"] = QString::number(mistakes.beaconOff);
    msg["ias_below_10000_ft"] = QString::number(mistakes.iasLow);
    msg["lights_below_10000_ft"] = QString::number(mistakes.lightsLow);
    msg["lights_above_10000_ft"] = QString::number(mistakes.lightsHigh);
    msg["overspeed"] = QString::number(mistakes.overspeed);
    msg["pause"] = QString::number(mistakes.pause);
    msg["refuel"] = QString::number(mistakes.refuel);
    msg["slew"] = QString::number(mistakes.slew);
    msg["taxi_no_lights"] = QString::number(mistakes.taxiLights);
    msg["takeoff_ldn_lights_off"] = QString::number(mistakes.takeoffLights);
    msg["landing_ldn_lights_off"] = QString::number(mistakes.landingLights);
    msg["landed_not_in_planned"] = QString::number(mistakes.landingAirport);
    msg["stall"] = QString::number(mistakes.stall);
    msg["taxi_speed"] = QString::number(mistakes.taxiSpeed);
    msg["qnh_takeoff"] = QString::number(mistakes.qnhTakeoff);
    msg["qnh_landing"] = QString::number(mistakes.qnhLanding);

    msg["aircraftreg"] = ""; // This was sent blank in SIM ACARS too.

    QJsonArray ja;
    ja.append(msg);
    QJsonDocument json;
    json.setArray(ja);
    va.sendPirep(json);

    va.sendEvents();
    va.sendTracks();

    state = OFFLINE;
    ui->endButton->setEnabled(false);
    ui->statusBar->clearMessage();
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
