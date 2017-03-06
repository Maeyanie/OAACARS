#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QMessageBox>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    va(this)
{
    ui->setupUi(this);
    ui->password->setEchoMode(QLineEdit::Password);

    QSettings settings("OAAE", "OAACARS", this);
    QFile loginFile("login.txt");
    if (loginFile.exists() && loginFile.open(QFile::ReadOnly)) {
        QTextStream in(&loginFile);
        settings.setValue("callsign", in.readLine().trimmed());
        settings.setValue("password", in.readLine().trimmed());
        loginFile.remove();
    }
    ui->callsign->setText(settings.value("callsign").toString());
    ui->password->setText(settings.value("password").toString());
    ui->applyWeight->setChecked(settings.value("applyWeight", true).toBool());

    connect(&timer, SIGNAL(timeout()), this, SLOT(sendUpdate()));
    connect(&uiTimer, SIGNAL(timeout()), this, SLOT(uiUpdate()));

    sock = new QUdpSocket(this);
    if (!sock->bind(32123)) {
        QMessageBox::critical(this, "Error", "Error: Could not bind to port 32123.", QMessageBox::Ok);
        exit(1);
    }
    connect(sock, SIGNAL(readyRead()), this, SLOT(gotUpdate()));

    state = OFFLINE;
    startFuel = 0.0f;
    startTime = 0;
    maxG = 0.0f;
    memset(&cur, 0, sizeof(cur));
    onTakeoff = onLanding = cur;
    sRealTime = new QLabel(this);
    sRealTime->hide();
    ui->statusBar->addPermanentWidget(sRealTime);
    sFlightTime = new QLabel(this);
    sFlightTime->hide();
    ui->statusBar->addPermanentWidget(sFlightTime);
    uiTimer.start(1000);
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
        ui->connectButton->setStyleSheet("QPushButton { color: rgb(255,0,0); }");
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
    if (pilot == 0) {
        ui->connectButton->setStyleSheet("QPushButton { color: rgb(255,0,0); }");
        return;
    }
    ui->connectButton->setStyleSheet("QPushButton { color: rgb(0,255,0); }");

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
    qint64 tNow = time(NULL);
    if (cur.time < tNow - 5 || cur.rref < tNow - 5) {
        qint32 ret = QMessageBox::warning(this, "X-Plane Not Connected",
                                          QString("I don't seem to be getting all the needed data from X-Plane.\nThis flight may not work properly.\n"
                                                  "Continue anyhow?"),
                                          QMessageBox::Yes, QMessageBox::No);
        if (ret == QMessageBox::No) return;
    }

    if (cur.agl > 20.0) {
        qint32 ret = QMessageBox::warning(this, "High Ground AGL",
                                          QString("Your AGL altitude of %1 seems a little too high.\nAre you sure you're on the ground?").arg(cur.agl),
                                          QMessageBox::Yes, QMessageBox::No);
        if (ret == QMessageBox::No) return;
    }
    groundAGL = cur.agl;

    ui->depIcao->setText(ui->depIcao->text().toUpper());
    QPair<double,double>* point = airports.get(ui->depIcao->text());
    if (!point) {
        QMessageBox::critical(this, "Airport Not Found",
                              QString("Could not find the departure airport '%1' in the database.").arg(ui->depIcao->text()),
                              QMessageBox::Ok);
        return;
    }
    dep = *point;

    ui->arrIcao->setText(ui->arrIcao->text().toUpper());
    point = airports.get(ui->arrIcao->text());
    if (!point) {
        QMessageBox::critical(this, "Airport Not Found",
                              QString("Could not find the destination airport '%1' in the database.").arg(ui->arrIcao->text()),
                              QMessageBox::Ok);
        return;
    }
    arr = *point;

    double depDist = greatcircle(QPair<double,double>(cur.lat, cur.lon), dep);
    if (depDist > 10.0) {
        qint32 ret = QMessageBox::warning(this, "Not At Departure Airport",
                                          QString("Your distance of %1 nmi from the departure airport %2 seems a little too high.\n"
                                                  "Are you sure you're at the right place?").arg(depDist).arg(ui->depIcao->text()),
                                          QMessageBox::Yes, QMessageBox::No);
        if (ret == QMessageBox::No) return;
    }

    if (ui->applyWeight->isChecked()) {
        float totalWeight = ui->pax->text().toFloat() * 80.0 + ui->cargo->text().toFloat();
        setDRef(sock, "sim/flightmodel/weight/m_fixed", totalWeight);
    }

    // 2017222102431964OPA1115
    // 2017-2-22 10:24:31.964 OPA1115
    flight = QDateTime::currentDateTime().toString("yyyyMdhhmmsszzz")+ui->flightNo->text();
    startTime = time(NULL);
    startLat = cur.lat;
    startLon = cur.lon;
    trackId = eventId = 1;
    critEvents = 0;

    state = PREFLIGHT;
    cur.flaps = 0;
    cur.gear = 1;
    for (int x = 0; x < 8; x++) cur.engine[x] = 0;
    mistakes.reset();

    newEvent("FLIGHT STARTED");
    setDRef(sock, "oaacars/tracking", 1);

    sendUpdate();
    timer.start(60000);
    ui->tabWidget->setCurrentWidget(ui->dataTab);
    ui->startButton->setEnabled(false);
    sRealTime->show();
    sFlightTime->show();
}

void MainWindow::on_endButton_clicked()
{
    double arrDist = greatcircle(QPair<double,double>(cur.lat, cur.lon), arr);
    if (arrDist > 10.0) {
        qint32 ret = QMessageBox::warning(this, "Not At Arrival Airport",
                                          QString("Your distance of %1 nmi from the arrival airport %2 seems a little too high.\n"
                                                  "Are you sure you're at the right place?").arg(arrDist).arg(ui->arrIcao->text()),
                                          QMessageBox::Yes, QMessageBox::No);
        if (ret == QMessageBox::No) return;
    }
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
    msg["flight_duration"] = QString::number((onLanding.time-onTakeoff.time-onLanding.pauseTime) / 3600.0, 'f', 2);
    msg["flight_fuel"] = QString::number(onTakeoff.fuel-onLanding.fuel, 'f', 0);
    msg["block_fuel"] = QString::number(startFuel-cur.fuel, 'f', 0);
    msg["final_fuel"] = QString::number(cur.fuel, 'f', 0);
    msg["zfw"] = ui->zfw->text();
    msg["flight_date"] = QDateTime::currentDateTime().toString("yyyy-M-d hh:mm:ss");

    msg["distance"] = greatcircle(dep, arr);

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

    msg["pause_time"] = QString::number(onLanding.pauseTime, 'f', 1);
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

    QJsonArray ja;
    ja.append(msg);
    QJsonDocument json;
    json.setArray(ja);
    va.sendPirep(json);

    va.sendEvents();
    va.sendTracks();

    setDRef(sock, "oaacars/tracking", 0);
    state = OFFLINE;
    ui->endButton->setEnabled(false);
    ui->statusBar->clearMessage();
    ui->statusBar->showMessage("Flight submitted.", 10000);
}

void MainWindow::on_callsign_textChanged(const QString &arg1)
{
    QSettings settings("OAAE", "OAACARS", this);
    settings.setValue("callsign", arg1);
}

void MainWindow::on_password_textChanged(const QString &arg1)
{
    QSettings settings("OAAE", "OAACARS", this);
    settings.setValue("password", arg1);
}

void MainWindow::on_applyWeight_stateChanged(int arg1)
{
    QSettings settings("OAAE", "OAACARS", this);
    settings.setValue("applyWeight", arg1);
}

void MainWindow::on_conSim_clicked()
{
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
    memcat(&pos, "127.0.0.1", 10); pos += 6;
    memcat(&pos, "32123", 6); pos += 2;
    memcat(&pos, 1);
    sock->writeDatagram(buffer, (pos-buffer), addr, 49000);

    sendDRef(sock, "sim/flightmodel/forces/fnrml_gear", 1);
}
