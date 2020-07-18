//
// Created by fuga on 18/07/2020.
//

#include "ShapeFilesViewer.h"

#include <QApplication>

using namespace qmapcontrol;

ShapeFilesViewer::ShapeFilesViewer()
: QMainWindow(nullptr)
{
    map = new QMapControl(QSizeF(800,600),this);

    baseAdapter = std::make_shared<MapAdapterOSM>();
    baseMap = std::make_shared<LayerMapAdapter>("OpenStreetMap", baseAdapter);

    map->addLayer(baseMap);

    map->setMapFocusPoint(PointWorldCoord(  -77.042793, -12.046374));
    map->setZoom(9);

    setCentralWidget(map);
}


int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    ShapeFilesViewer mainWindow;
    mainWindow.resize(800, 600);
    mainWindow.show();

    return app.exec();
}
