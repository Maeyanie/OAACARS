#include "airports.h"
#include <QFile>
#include <QTextStream>

Airports::Airports(QObject *parent) : QObject(parent)
{
    QFile dataFile("airports.csv");
    if (dataFile.open(QFile::ReadOnly)) {
        QTextStream in(&dataFile);
        QString line, icao;
        double lat, lon;
        QStringList parts;
        while (in.readLineInto(&line)) {
            parts = line.trimmed().split(',');
            icao = parts.takeFirst();
            lat = parts.takeFirst().toDouble();
            lon = parts.takeFirst().toDouble();
            qDebug("[Airports] Loaded %s: %f,%f", icao.toUtf8().data(), lat, lon);
            airports.insert(icao, QPair<double,double>(lat,lon));
        }
        dataFile.close();
    } else {
        qWarning("Could not open 'airports.csv'");
    }
}

QPair<double,double>* Airports::get(QString icao) {
    auto i = airports.find(icao);
    if (i == airports.end()) return NULL;
    return &*i;
}
