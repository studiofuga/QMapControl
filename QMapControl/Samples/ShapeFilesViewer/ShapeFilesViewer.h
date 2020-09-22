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
#include <vector>

class ShapeFilesViewer : public QMainWindow {
Q_OBJECT

    qmapcontrol::QMapControl *map;
    std::shared_ptr<qmapcontrol::MapAdapterGoogle> baseAdapter;
    std::shared_ptr<qmapcontrol::LayerMapAdapter> baseLayer;

    struct Shapefile {
        GDALDataset *dataset = nullptr;
        std::string name;
        std::shared_ptr<qmapcontrol::ESRIShapefile> adapter;
        std::shared_ptr<qmapcontrol::LayerESRIShapefile> layer;

        ~Shapefile()
        { if (dataset != nullptr) { delete dataset; }}
    };

    struct Rasterfile {
        GDALDataset *dataset = nullptr;
        std::string name;
        std::shared_ptr<qmapcontrol::AdapterRaster> adapter;
        std::shared_ptr<qmapcontrol::LayerRaster> layer;

        ~Rasterfile()
        { if (dataset != nullptr) { delete dataset; }}
    };

    std::vector<std::shared_ptr<Shapefile>> shapefiles;
    std::vector<std::shared_ptr<Rasterfile>> rasterfiles;

    QMenu *displayMenu, *layerDeleteMenu;
public:
    ShapeFilesViewer();

private:
    void buildMenu();

    void updateLayersMenu();

public slots:

    void onLoadShapeFile();

    void onLoadTiffFile();

    void mapFocusPointChanged(qmapcontrol::PointWorldCoord);

    void mapMouseMove(QMouseEvent *, qmapcontrol::PointWorldCoord, qmapcontrol::PointWorldCoord);
};

#endif // QMAPCONTROL_SHAPEFILESVIEWER_H
