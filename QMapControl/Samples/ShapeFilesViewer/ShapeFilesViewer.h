//
// Created by fuga on 18/07/2020.
//

#ifndef QMAPCONTROL_SHAPEFILESVIEWER_H
#define QMAPCONTROL_SHAPEFILESVIEWER_H

#include "QMapControl/QMapControl.h"
#include "QMapControl/MapAdapterOSM.h"
#include "QMapControl/LayerMapAdapter.h"

#include <QtWidgets/QMainWindow>

#include <memory>

class ShapeFilesViewer : public QMainWindow
{
    Q_OBJECT

    qmapcontrol::QMapControl *map;
    std::shared_ptr<qmapcontrol::MapAdapterOSM> baseAdapter;
    std::shared_ptr<qmapcontrol::LayerMapAdapter> baseMap;

public:
    ShapeFilesViewer();
};

#endif // QMAPCONTROL_SHAPEFILESVIEWER_H
