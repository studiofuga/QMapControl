//
// Created by fuga on 18/07/2020.
//

#ifndef QMAPCONTROL_SHAPEFILESVIEWER_H
#define QMAPCONTROL_SHAPEFILESVIEWER_H

#include "QMapControl/ESRIShapefile.h"
#include "QMapControl/LayerESRIShapefile.h"
#include "QMapControl/LayerMapAdapter.h"
#include "QMapControl/MapAdapterGoogle.h"
#include "QMapControl/Projection.h"
#include "QMapControl/QMapControl.h"

#include "QMapControl/AdapterRaster.h"
#include "QMapControl/LayerRaster.h"

#include <QtWidgets/QMainWindow>

#include <memory>

class ShapeFilesViewer : public QMainWindow {
Q_OBJECT

qmapcontrol::QMapControl* map;
std::shared_ptr<qmapcontrol::MapAdapterGoogle> baseAdapter;
std::shared_ptr<qmapcontrol::LayerMapAdapter> baseLayer;

GDALDataset *shpDataSet = nullptr;
    std::shared_ptr<qmapcontrol::ESRIShapefile> shpAdapter;
    std::shared_ptr<qmapcontrol::LayerESRIShapefile> shpLayer;

    GDALDataset *tiffDataSet = nullptr;
    std::shared_ptr<qmapcontrol::AdapterRaster> tiffAdapter;
    std::shared_ptr<qmapcontrol::LayerRaster> tiffLayer;

public:
    ShapeFilesViewer();

private:
    void buildMenu();

public slots:

    void onLoadShapeFile();

    void onLoadTiffFile();

    void mapFocusPointChanged(qmapcontrol::PointWorldCoord);

    void mapMouseMove(QMouseEvent *, qmapcontrol::PointWorldCoord, qmapcontrol::PointWorldCoord);
};

#endif // QMAPCONTROL_SHAPEFILESVIEWER_H
