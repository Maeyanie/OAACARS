#ifndef IPDIALOG_H
#define IPDIALOG_H

#include <QDialog>
#include "mainwindow.h"

namespace Ui {
class IPDialog;
}

class IPDialog : public QDialog
{
    Q_OBJECT

public:
    explicit IPDialog(MainWindow *parent = 0);
    ~IPDialog();

private slots:
    void on_buttonBox_accepted();

    void on_buttonBox_rejected();

private:
    Ui::IPDialog *ui;
};

#endif // IPDIALOG_H
