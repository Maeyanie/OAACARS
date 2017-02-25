#ifndef CHARTS_H
#define CHARTS_H

#include <QChartView>
#include <QChart>
#include <QLineSeries>
using namespace QtCharts;

class Charts : public QObject
{
    Q_OBJECT

public:
    explicit Charts(QObject* parent = 0);
    ~Charts();
    void addPoint(float time, float val);
    void show(QLayout* frame);
    void hide(QLayout* frame);

signals:

public slots:
    void update();

private:
    QList<QPointF> list;
    QChartView* chartView;
    QChart* chart;
    QLineSeries* line;
};

#endif // CHARTS_H
