#include "charts.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QChart>
#include <QChartView>

#define CHARTSIZE (20*60*5) // 5 minutes

Charts::Charts(QObject* parent) : QObject(parent) {
    chartView = NULL;
    chart = NULL;
    line = NULL;
}

Charts::~Charts() {
    /*if (chartView) chartView->deleteLater();
    if (chart) chart->deleteLater();
    if (line) line->deleteLater();*/
}

void Charts::addPoint(float time, float val) {
    list.append(QPointF(time, val));
    if (list.count() > CHARTSIZE) list.pop_front();
}

void Charts::show(QLayout* frame) {
    line = new QLineSeries();
    line->append(list);

    chart = new QChart();
    chart->legend()->hide();
    chart->addSeries(line);

    chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);

    frame->addWidget(chartView);
}

void Charts::hide(QLayout* frame) {
    frame->removeWidget(chartView);
}

void Charts::update() {
    line->deleteLater();

    line = new QLineSeries();
    line->append(list);

    chart->removeAllSeries();
    chart->addSeries(line);
}
