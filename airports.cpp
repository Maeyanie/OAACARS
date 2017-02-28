#include "airports.h"
#include "curl/curl.h"
#include <QFile>
#include <QTextStream>
#include <QMessageBox>

static size_t write_data(void *buffer, size_t size, size_t nmemb, void *userp) {
    QByteArray* curlData = (QByteArray*)userp;

    curlData->append((const char*)buffer, size*nmemb);

    return size * nmemb;
}

Airports::Airports(QObject *parent) : QObject(parent)
{
    QFile dataFile("airports.csv");
    if (dataFile.open(QFile::ReadOnly)) {
        QTextStream in(&dataFile);
        QString line, icao;
        double lat, lon;
        QStringList parts;
        int count = 0;
        while (in.readLineInto(&line)) {
            parts = line.trimmed().split(',');
            icao = parts.takeFirst();
            lat = parts.takeFirst().toDouble();
            lon = parts.takeFirst().toDouble();
            //qDebug("[Airports] Loaded %s: %f,%f", icao.toUtf8().data(), lat, lon);
            airports.insert(icao, QPair<double,double>(lat,lon));
            count++;
        }
        qInfo("Loaded %d airports from file", count);
        dataFile.close();
    } else {
        qWarning("Could not open 'airports.csv'");
        curl_global_init(CURL_GLOBAL_ALL);
        CURL* curl = curl_easy_init();
        QByteArray curlData;
        curl_easy_setopt(curl, CURLOPT_URL, "http://www.maeyanie.com/oaacars/airports.csv");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &curlData);

        qint32 ret = curl_easy_perform(curl);
        long code;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
        qInfo("Fetching airports.csv, returned: %d\n", ret);
        if (ret != CURLE_OK || code >= 400) {
            QMessageBox::critical(NULL, "Error loading airports",
                                  "Could not open or download airports.csv!",
                                  QMessageBox::Ok, QMessageBox::NoButton);
            exit(1);
        }
        curl_easy_cleanup(curl);

        if (dataFile.open(QFile::WriteOnly | QFile::Truncate)) {
            dataFile.write(curlData);
            dataFile.close();
        }

        QTextStream in(curlData);
        QString line, icao;
        double lat, lon;
        QStringList parts;
        int count = 0;
        while (in.readLineInto(&line)) {
            parts = line.trimmed().split(',');
            icao = parts.takeFirst();
            lat = parts.takeFirst().toDouble();
            lon = parts.takeFirst().toDouble();
            //qDebug("[Airports] Loaded %s: %f,%f", icao.toUtf8().data(), lat, lon);
            airports.insert(icao, QPair<double,double>(lat,lon));
            count++;
        }
        qInfo("Loaded %d airports from HTTP", count);
    }
}

QPair<double,double>* Airports::get(QString icao) {
    auto i = airports.find(icao);
    if (i == airports.end()) return NULL;
    return &*i;
}
