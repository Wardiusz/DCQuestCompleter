#include <iostream>
#include <QApplication>
#include <QFile>
#include "MainWindow.h"

auto main(int argc, char* argv[]) -> int {
    QApplication app(argc, argv);

    MainWindow w;
    w.show();

    return QApplication::exec();
}