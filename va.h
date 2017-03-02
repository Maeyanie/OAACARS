#ifndef VA_H
#define VA_H

#include <QObject>
#include <QtNetwork>
#include <QString>
#include <QUrl>
#include <QNetworkReply>
#include <curl/curl.h>

class VA : public QObject
{
    Q_OBJECT

public:
    explicit VA(QObject* parent = 0);
    ~VA();
    QString login(const QString& user, const QString& pass);
    QString getAircraft();
    QString pilotConnection();
    void sendUpdate(QJsonDocument &data);
    QString sendPirep(QJsonDocument &data);
    void newEvent(QJsonObject& e);
    void newTrack(QJsonObject& t);
    QString sendEvents();
    QString sendTracks();

signals:

private slots:
    void updateFinished(QNetworkReply* reply);

private:
    QNetworkAccessManager nam;
    CURL* curl;
    QByteArray curlData;
    QString baseUrl;
    QString username;
    QString password;
    QJsonArray events;
    QJsonArray tracks;
};

#endif // VA_H
