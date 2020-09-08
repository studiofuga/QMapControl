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

    struct Impl;
    Impl *p;
public:
    Navigator();

    virtual ~Navigator();

private:

    void buildMenu();

    void buildOnMapControls();

public slots:

    void onActionRecordPoint();

    void onActionSavePath();

    void onActionLoadPath();

    void onActionPlayPath();

    void onCourseChanged(qreal newcourse);

    void animate();


    void mapFocusPointChanged(qmapcontrol::PointWorldCoord);

    void mapMouseMove(QMouseEvent *, qmapcontrol::PointWorldCoord, qmapcontrol::PointWorldCoord);
};

#endif // QMAPCONTROL_DEMO_NAVIGATOR_H
