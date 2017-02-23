#ifndef VA_H
#define VA_H

#include <QtNetwork>
#include <QString>
#include <QUrl>
#include <curl/curl.h>

class VA : public QObject {
public:
    VA(QObject* parent);
    QString login(const QString& user, const QString& pass);
    QString getAircraft();
    QString pilotConnection();
    QString sendUpdate(QJsonDocument &data);
    QString sendPirep(QJsonDocument &data);
    void event(QJsonObject& e);
    void track(QJsonObject& t);
    QString sendEvents();
    QString sendTracks();

private:
    CURL* curl;
    QByteArray curlData;
    QString baseUrl;
    QString username;
    QString password;
    QJsonArray events;
    QJsonArray tracks;
};

#endif // VA_H
