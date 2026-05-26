#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    // QApplicationの初期化
    QApplication a(argc, argv);

    // メインウィンドウの起動と表示
    MainWindow w;
    w.show();

    return a.exec();
}
