//
// Created by fuga on 18/07/2020.
//

#ifndef QMAPCONTROL_SHAPEFILESVIEWER_H
#define QMAPCONTROL_SHAPEFILESVIEWER_H

#include "QMapControl/QMapControl.h"
#include "QMapControl/MapAdapterOSM.h"
#include "QMapControl/LayerMapAdapter.h"
#include "QMapControl/ESRIShapefile.h"
#include "QMapControl/LayerESRIShapefile.h"

#include <QtWidgets/QMainWindow>

#include <memory>

class ShapeFilesViewer : public QMainWindow
{
    Q_OBJECT

    qmapcontrol::QMapControl *map;
    std::shared_ptr<qmapcontrol::MapAdapterOSM> baseAdapter;
    std::shared_ptr<qmapcontrol::LayerMapAdapter> baseLayer;

    std::shared_ptr<qmapcontrol::ESRIShapefile> shpAdapter;
    std::shared_ptr<qmapcontrol::LayerESRIShapefile> shpLayer;

    GDALDataset *shpDataSet = nullptr;
public:
    ShapeFilesViewer();

private:
    void buildMenu();

public slots:
    void onLoadShapeFile();
};

#endif // QMAPCONTROL_SHAPEFILESVIEWER_H
