//
// Created by fuga on 18/07/2020.
//

#include "ShapeFilesViewer.h"

#include <QApplication>
#include <QMenuBar>
#include <QMenu>
#include <QFileDialog>

#include <QDebug>

using namespace qmapcontrol;

ShapeFilesViewer::ShapeFilesViewer()
: QMainWindow(nullptr)
{
    map = new QMapControl(QSizeF(800,600),this);

    baseAdapter = std::make_shared<MapAdapterOSM>();
    baseLayer = std::make_shared<LayerMapAdapter>("OpenStreetMap", baseAdapter);

    map->addLayer(baseLayer);

    map->setMapFocusPoint(PointWorldCoord(  -77.042793, -12.046374));
    map->setZoom(9);

    setCentralWidget(map);

    buildMenu();
}

void ShapeFilesViewer::buildMenu() {
    auto fileMenu = menuBar()->addMenu(tr("&File"));
    auto actionSelectShapeFile = new QAction("Load Shapefile...");
    fileMenu->addAction(actionSelectShapeFile);

    connect(actionSelectShapeFile, &QAction::triggered, this, &ShapeFilesViewer::onLoadShapeFile);
}

void ShapeFilesViewer::onLoadShapeFile() {
    qDebug() << "Triggered";
    auto shapefile = QFileDialog::getOpenFileName(this, tr("Select Shapefile to load"),
                                                  QString(),
                                                  tr("ShapeFiles (*.shp);;All files (*.*)")
                                                  );

    qDebug() << "Selected: " << shapefile;
    if (!shapefile.isEmpty()) {
        auto stdShapeFileName = shapefile.toStdString();
        shpAdapter = std::make_shared<ESRIShapefile>(stdShapeFileName, "ShapeFile");
        shpLayer = std::make_shared<LayerESRIShapefile>("ShapeFile-Layer");
        shpLayer->addESRIShapefile(shpAdapter);

        map->addLayer(shpLayer);
    }
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    GDALAllRegister();

    ShapeFilesViewer mainWindow;
    mainWindow.resize(800, 600);
    mainWindow.show();

    return app.exec();
}
