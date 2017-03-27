#include "mainwindow.h"
#include "ipdialog.h"
#include "ui_ipdialog.h"

IPDialog::IPDialog(MainWindow *parent) :
    QDialog(parent),
    ui(new Ui::IPDialog)
{
    ui->setupUi(this);
    ui->lineEdit->setText(parent->myip.toString());
    ui->lineEdit_2->setText(parent->remoteip.toString());
}

IPDialog::~IPDialog()
{
    delete ui;
}

void IPDialog::on_buttonBox_accepted()
{
    MainWindow* p = (MainWindow*)parent();

    p->myip = ui->lineEdit->text();
    p->remoteip = ui->lineEdit_2->text();

    QSettings settings("OAAE", "OAACARS", this);
    settings.setValue("myip", p->myip.toString());
    settings.setValue("remoteip", p->remoteip.toString());

    p->connectToSim();
    this->hide();
    this->destroy();
}

void IPDialog::on_buttonBox_rejected()
{
    this->hide();
    this->destroy();
}
