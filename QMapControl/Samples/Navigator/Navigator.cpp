//
// Created by fuga on 18/07/2020.
//

#include "Navigator.h"

#include <QApplication>
#include <QMenuBar>
#include <QMenu>
#include <QMessageBox>
#include <QStatusBar>
#include <QSettings>
#include <QVBoxLayout>
#include <QDial>

#include <QDebug>

#include <stdexcept>

using namespace qmapcontrol;

struct Navigator::Impl {
    qmapcontrol::QMapControl *map;
    std::shared_ptr<qmapcontrol::MapAdapterOSM> baseAdapter;
    std::shared_ptr<qmapcontrol::LayerMapAdapter> baseLayer;

    QDial *dial;
};

Navigator::Navigator()
        : QMainWindow(nullptr),
          p(new Impl())
{
    statusBar()->show();

    p->map = new QMapControl(QSizeF(800, 600), this);

    p->baseAdapter = std::make_shared<MapAdapterOSM>();
    p->baseLayer = std::make_shared<LayerMapAdapter>("OpenStreetMap", p->baseAdapter);

    p->map->addLayer(p->baseLayer);

    p->map->setMapFocusPoint(PointWorldCoord(-77.042793, -12.046374));
    p->map->setZoom(9);
    p->map->setBackgroundColour(Qt::white);

    setCentralWidget(p->map);

    buildMenu();
    buildOnMapControls();

    connect(p->map, &QMapControl::mapFocusPointChanged, this, &Navigator::mapFocusPointChanged);
    connect(p->map, &QMapControl::mouseEventMoveCoordinate, this, &Navigator::mapMouseMove);
}

Navigator::~Navigator()
{
    delete p;
}

void Navigator::buildMenu()
{

    auto layersMenu = menuBar()->addMenu(tr("&Layers"));
    auto actionLayermap = new QAction("Map");
    actionLayermap->setCheckable(true);
    actionLayermap->setChecked(true);
    layersMenu->addAction(actionLayermap);
    connect(actionLayermap, &QAction::toggled, this, [this](bool checked) {
        p->baseLayer->setVisible(checked);
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
    auto focusPoint = p->map->mapFocusPointCoord();
    statusBar()->showMessage(
            QString("Map Center Point: (lon %1, lat %2) - Mouse Point: (lon %3, lat %4)")
                    .arg(focusPoint.longitude()).arg(focusPoint.latitude())
                    .arg(currentPos.longitude()).arg(currentPos.latitude()));
}

void Navigator::buildOnMapControls()
{
    // Create an inner layout to display buttons/"mini" map control.
    QVBoxLayout *layout_inner = new QVBoxLayout;

    // Note 0 is south for dial. Add 180degs.
    p->dial = new QDial();
    p->dial->setMinimum(0);
    p->dial->setMaximum(359);
    p->dial->setWrapping(true);
    p->dial->setMaximumSize(QSize(200, 200));
    p->dial->setValue(180);

    connect(p->dial, &QDial::valueChanged, this, [this](int value) {
        onCourseChanged(value - 180.0);
    });

    layout_inner->addWidget(p->dial);

    // Set the main map control to use the inner layout.
    p->map->setLayout(layout_inner);
}

void Navigator::onCourseChanged(qreal newcourse)
{
    qDebug() << "New Course: " << newcourse;
    p->map->setMapRotation(newcourse);
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    Navigator mainWindow;
    mainWindow.resize(800, 600);
    mainWindow.show();

    return app.exec();
}
