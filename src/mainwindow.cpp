#include "mainwindow.h"
#include "ui_mainwindow.h"

#ifndef APP_VERSION_STRING
#define APP_VERSION_STRING "1.0.0"
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle(QString("TwitchFollowerChecker - Ver %1").arg(APP_VERSION_STRING));
}

MainWindow::~MainWindow()
{
    delete ui;
}
