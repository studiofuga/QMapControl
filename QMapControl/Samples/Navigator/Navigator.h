//
// Created by fuga on 18/07/2020.
//

#ifndef QMAPCONTROL_DEMO_NAVIGATOR_H
#define QMAPCONTROL_DEMO_NAVIGATOR_H

#include "QMapControl/Projection.h"
#include "QMapControl/LayerMapAdapter.h"
#include "QMapControl/MapAdapterOSM.h"
#include "QMapControl/QMapControl.h"

#include <QtWidgets/QMainWindow>

#include <memory>

class Navigator : public QMainWindow {
Q_OBJECT

    qmapcontrol::QMapControl *map;
    std::shared_ptr<qmapcontrol::MapAdapterOSM> baseAdapter;
    std::shared_ptr<qmapcontrol::LayerMapAdapter> baseLayer;

public:
    Navigator();

private:
    void buildMenu();

public slots:

    void mapFocusPointChanged(qmapcontrol::PointWorldCoord);

    void mapMouseMove(QMouseEvent *, qmapcontrol::PointWorldCoord, qmapcontrol::PointWorldCoord);
};

#endif // QMAPCONTROL_DEMO_NAVIGATOR_H
