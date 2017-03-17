#ifndef AIRPORTS_H
#define AIRPORTS_H

#include <QObject>
#include <QMap>
#include <QPair>

class Airports : public QObject
{
    Q_OBJECT
public:
    explicit Airports(QObject *parent = 0);
    QPair<double, double> *get(QString icao);

signals:

public slots:

private:
    QMap<QString, QPair<double,double> > airports;
};

#endif // AIRPORTS_H
