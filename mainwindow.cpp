#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QMessageBox>
#include <QTimer>

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
    memcat(&pos, 17);// pitch, roll, headings
    memcat(&pos, 20);// lat, lon, altitude
    memcat(&pos, 21);// loc, vel, dist traveled
    //memcat(&pos, 62);// fuel weights
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
    msg["heading"] = cur.heading;
    msg["gs"] = ui->gs->text();
    msg["altitude"] = ui->altitude->text();
    msg["fuel"] = ui->fob->text();
    msg["fuel_used"] = ui->fu->text();
    msg["latitude"] = cur.lat;
    msg["longitude"] = cur.lon;
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
    ui->flightNo->setText(data["callsign"].toString());
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
    startLat = cur.lat;
    startLon = cur.lon;

    state = 1;
    cur.flaps = 0;
    cur.gear = 1;
    cur.engine[0] = cur.engine[1] = cur.engine[2] = cur.engine[3] = 0;
    mistakes.reset();

    sendUpdate();
    timer.start(60000);
    ui->startButton->setEnabled(false);
    ui->endButton->setEnabled(true);
}

void MainWindow::on_endButton_clicked()
{
    timer.stop();
    ui->statusBar->showMessage("Submitting...");

    /* [{"flightId":"2017222102431964OPA1115","weight_unit":"Kg","departure":"EDDM","arrival":"EHAM","gvauserId":"40",
     * "callsign":"OPA1115","departure_time":"1225","cruise_speed":"","flight_level":"","pax":"113","cargo":"5091","eet":"",
     * "endurance":"","alt1":"EHAM","alt2":"","route":"INPUD UPALA TENLO ALAXA BOREP TESDU ALIBU DEMAB MASEK EDEGA MAPOX BIGGE RELBI RKN",
     * "remarks":"","flight_type":"IFR","aircraft":"X737-800_V500 ","aircraft_type":"B733","aircraft_registry":"D-ANKY",
     * "flight_status":"FLIGHT FINISHED","flight_duration":"1.08","flight_fuel":"1716.53","block_fuel":"1785.23",
     * "flight_date":"2017-2-22 11:45:12","distance":"373.2","landing_vs":"0.00","landing_ias":"0.00","landing_forceg":"0.0",
     * "landing_bank":"0.0","landing_pitch":"0.0","landing_winddeg":"0","landing_windknots":"0","landing_oat":"14","landing_flaps":"0",
     * "landing_light_bea":"0","landing_light_nav":"0","landing_light_ldn":"0","landing_light_str":"0","log_start":"2017-2-22 10:37:8",
     * "flight_start":"2017-2-22 10:38:31","log_end":"2017-2-22 11:45:12","flight_end":"","zfw":"47840","departure_metar":"","arrival_metar":"",
     * "network":"OFFLINE","comments":"","pause_time":"0.00","crash":"1","beacon_off":"0","ias_below_10000_ft":"1","lights_below_10000_ft":"0",
     * "lights_above_10000_ft":"1","overspeed":"0","pause":"0","refuel":"0","slew":"0","taxi_no_lights":"1","takeoff_ldn_lights_off":"0",
     * "landing_ldn_lights_off":"0","landed_not_in_planned":"0","stall":"1","taxi_speed":"0","qnh_takeoff":"0","qnh_landing":"0","final_fuel":"66",
     * "landing_hdg":"","aircraftreg":""}]
     */

    QJsonObject msg;
    msg["flightId"] = flight;
    msg["weight_unit"] = "Lbs";
    msg["departure"] = ui->depIcao->text();
    msg["arrival"] = ui->arrIcao->text();
    msg["gvauserId"] = QString::number(pilot);
    msg["callsign"] = ui->flightNo->text();
    msg["departure_time"] = ui->depTime->text();
    msg["cruise_speed"] = ui->cruiseSpeed->text();
    msg["flight_level"] = ui->altitude->text();
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
    msg["flight_duration"] = QString::number((onLanding.time-onTakeoff.time) / 60.0, 'f', 2);
    msg["flight_fuel"] = ui->fu->text();
    msg["block_fuel"] = QString::number(onTakeoff.fuel-onLanding.fuel, 'f', 0);
    msg["flight_date"] = QDateTime::currentDateTime().toString("yyyy-M-d hh:mm:ss");

    msg["distance"] = "0";

    msg["landing_vs"] = QString::number(onLanding.vs);
    msg["landing_ias"] = QString::number(onLanding.ias);
    msg["landing_forceg"] = QString::number(maxG);
    msg["landing_bank"] = QString::number(onLanding.bank);
    msg["landing_pitch"] = QString::number(onLanding.pitch);
    msg["landing_winddeg"] = QString::number(onLanding.winddeg);
    msg["landing_windknots"] = QString::number(onLanding.windknots);
    msg["landing_oat"] = QString::number(onLanding.oat);
    msg["landing_flaps"] = QString::number(onLanding.flaps);
    msg["landing_light_bea"] = QString::number(onLanding.bea);
    msg["landing_light_nav"] = QString::number(onLanding.nav);
    msg["landing_light_ldn"] = QString::number(onLanding.ldn);
    msg["landing_light_str"] = QString::number(onLanding.str);

    msg["log_start"] = QDateTime::fromTime_t(startTime).toString("yyyy-M-d hh:mm:ss");
    msg["flight_start"] = QDateTime::fromTime_t(startTime).toString("yyyy-M-d hh:mm:ss");
    msg["log_end"] = QDateTime::currentDateTime().toString("yyyy-M-d hh:mm:ss");
    msg["flight_end"] = QDateTime::currentDateTime().toString("yyyy-M-d hh:mm:ss");

    msg["zfw"] = ui->zfw->text();
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

    msg["final_fuel"] = ui->fob->text();
    msg["landing_hdg"] = "";
    msg["aircraftreg"] = "";

    QJsonArray ja;
    ja.append(msg);
    QJsonDocument json;
    json.setArray(ja);
    va.sendPirep(json);

    state = 0;
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
