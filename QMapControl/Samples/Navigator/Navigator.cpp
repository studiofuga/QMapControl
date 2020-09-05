//
// Created by fuga on 18/07/2020.
//

#include "Navigator.h"

#include <QApplication>
#include <QMenuBar>
#include <QMenu>
#include <QMessageBox>
#include <QStatusBar>

#include <QDebug>
#include <QSettings>

#include <stdexcept>

using namespace qmapcontrol;

Navigator::Navigator()
        : QMainWindow(nullptr)
{
    statusBar()->show();

    map = new QMapControl(QSizeF(800, 600), this);

    baseAdapter = std::make_shared<MapAdapterOSM>();
    baseLayer = std::make_shared<LayerMapAdapter>("OpenStreetMap", baseAdapter);

    map->addLayer(baseLayer);

    map->setMapFocusPoint(PointWorldCoord(-77.042793, -12.046374));
    map->setZoom(9);
    map->setBackgroundColour(Qt::white);

    setCentralWidget(map);

    buildMenu();

    connect(map, &QMapControl::mapFocusPointChanged, this, &Navigator::mapFocusPointChanged);
    connect(map, &QMapControl::mouseEventMoveCoordinate, this, &Navigator::mapMouseMove);
}

void Navigator::buildMenu()
{

    auto layersMenu = menuBar()->addMenu(tr("&Layers"));
    auto actionLayermap = new QAction("Map");
    actionLayermap->setCheckable(true);
    actionLayermap->setChecked(true);
    layersMenu->addAction(actionLayermap);
    connect(actionLayermap, &QAction::toggled, this, [this](bool checked) {
        baseLayer->setVisible(checked);
    });

}

void Navigator::mapFocusPointChanged(qmapcontrol::PointWorldCoord focusPoint)
{
    statusBar()->showMessage(
            QString("Map Center Point: (lon %1, lat %2)").arg(focusPoint.longitude()).arg(focusPoint.latitude()));
}

void Navigator::mapMouseMove(QMouseEvent *mouseEvent, qmapcontrol::PointWorldCoord pressedPos,
                             qmapcontrol::PointWorldCoord currentPos)
{
    auto focusPoint = map->mapFocusPointCoord();
    statusBar()->showMessage(
            QString("Map Center Point: (lon %1, lat %2) - Mouse Point: (lon %3, lat %4)")
                    .arg(focusPoint.longitude()).arg(focusPoint.latitude())
                    .arg(currentPos.longitude()).arg(currentPos.latitude()));
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    Navigator mainWindow;
    mainWindow.resize(800, 600);
    mainWindow.show();

    return app.exec();
}
