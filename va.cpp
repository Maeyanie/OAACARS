#include "va.h"


size_t write_data(void *buffer, size_t size, size_t nmemb, void *userp) {
    QByteArray* curlData = (QByteArray*)userp;

    curlData->append((const char*)buffer, size*nmemb);

    return size * nmemb;
}

VA::VA(QObject *parent) : QObject(parent) {
    baseUrl = "http://openair-alliance.eu/vaManager/vam/";

    curl_global_init(CURL_GLOBAL_ALL);

    curl = curl_easy_init();

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &curlData);

    curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: text/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
}

QString VA::login(const QString &user, const QString &pass) {
    username = user;
    password = pass;

    QByteArray url = QUrl(baseUrl+"vam_acars_remove_book_aircraft.php").toString().toUtf8();
    QByteArray postData = QString("{\"pilot\":\""+username+"\",\"password\":\""+password+"\"}").toUtf8();

    curl_easy_setopt(curl, CURLOPT_URL, url.data());
    curl_easy_setopt(curl, CURLOPT_POST, 1);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.data());

    curlData.clear();
    qint32 ret = curl_easy_perform(curl);
    qInfo("URL: %s\npostData: %s\nret: %d\nresponse: %s\n", url.data(), postData.data(), ret, curlData.data());
    return curlData;
}

QString VA::getAircraft() {
    QByteArray url = QUrl(baseUrl+"vam_acars_get_aircraft.php").toString().toUtf8();
    QByteArray postData = QString("{\"pilot\":\""+username+"\",\"password\":\""+password+"\"}").toUtf8();

    curl_easy_setopt(curl, CURLOPT_URL, url.data());
    curl_easy_setopt(curl, CURLOPT_POST, 1);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.data());

    curlData.clear();
    qint32 ret = curl_easy_perform(curl);
    qInfo("URL: %s\npostData: %s\nret: %d\nresponse: %s\n", url.data(), postData.data(), ret, curlData.data());
    return curlData;
}

QString VA::pilotConnection() {
    QByteArray url = QUrl(baseUrl+"vam_acars_pilot_connection.php").toString().toUtf8();
    QByteArray postData = QString("{\"pilot\":\""+username+"\",\"password\":\""+password+"\"}").toUtf8();

    curl_easy_setopt(curl, CURLOPT_URL, url.data());
    curl_easy_setopt(curl, CURLOPT_POST, 1);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.data());

    curlData.clear();
    qint32 ret = curl_easy_perform(curl);
    qInfo("URL: %s\npostData: %s\nret: %d\nresponse: %s\n", url.data(), postData.data(), ret, curlData.data());
    return curlData;
}

QString VA::sendUpdate(QJsonDocument& data) {
    QByteArray url = QUrl(baseUrl+"vamliveacars.php").toString().toUtf8();
    QByteArray postData = data.toJson();

    curl_easy_setopt(curl, CURLOPT_URL, url.data());
    curl_easy_setopt(curl, CURLOPT_POST, 1);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.data());

    curlData.clear();
    qint32 ret = curl_easy_perform(curl);
    qInfo("URL: %s\npostData: %s\nret: %d\nresponse: %s\n", url.data(), postData.data(), ret, curlData.data());
    return curlData;
}
