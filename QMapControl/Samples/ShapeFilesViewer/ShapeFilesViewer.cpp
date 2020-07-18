//
// Created by fuga on 18/07/2020.
//

#include "ShapeFilesViewer.h"

#include <QApplication>

ShapeFilesViewer::ShapeFilesViewer()
: QMainWindow(nullptr)
{

}


int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    ShapeFilesViewer mainWindow;
    mainWindow.show();

    return app.exec();
}
