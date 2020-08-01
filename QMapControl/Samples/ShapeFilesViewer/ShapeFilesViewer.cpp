//
// Created by fuga on 18/07/2020.
//

#include "ShapeFilesViewer.h"

#include <QApplication>
#include <QMenuBar>
#include <QMenu>
#include <QFileDialog>
#include <QMessageBox>

#include <QDebug>
#include <QSettings>

#include <stdexcept>
#include <sstream>

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
    map->setBackgroundColour(Qt::white);

    setCentralWidget(map);

    buildMenu();
}

void ShapeFilesViewer::buildMenu()
{
    auto fileMenu = menuBar()->addMenu(tr("&File"));
    auto actionSelectShapeFile = new QAction("Load Shapefile...");
    fileMenu->addAction(actionSelectShapeFile);

    connect(actionSelectShapeFile, &QAction::triggered, this, &ShapeFilesViewer::onLoadShapeFile);

    auto actionSelectTiffFile = new QAction("Load Tiff...");
    fileMenu->addAction(actionSelectTiffFile);

    connect(actionSelectTiffFile, &QAction::triggered, this, &ShapeFilesViewer::onLoadTiffFile);

    auto layersMenu = menuBar()->addMenu(tr("&Layers"));
    auto actionLayermap = new QAction("Map");
    actionLayermap->setCheckable(true);
    actionLayermap->setChecked(true);
    layersMenu->addAction(actionLayermap);
    connect(actionLayermap, &QAction::toggled, this, [this](bool checked) {
        baseLayer->setVisible(checked);
    });

    auto actionShpLayer = new QAction("Shapefile");
    actionShpLayer->setCheckable(true);
    actionShpLayer->setChecked(true);
    layersMenu->addAction(actionShpLayer);
    connect(actionShpLayer, &QAction::toggled, this, [this](bool checked) {
        if (shpLayer != nullptr) {
            shpLayer->setVisible(checked);
        }
    });

    auto actionTiffLayer = new QAction("Tiff");
    actionTiffLayer->setCheckable(true);
    actionTiffLayer->setChecked(true);
    layersMenu->addAction(actionTiffLayer);
    connect(actionTiffLayer, &QAction::toggled, this, [this](bool checked) {
        if (tiffLayer != nullptr) {
            tiffLayer->setVisible(checked);
        }
    });

}

void ShapeFilesViewer::onLoadShapeFile()
{
    QSettings settings;

    try {
        auto basedir = settings.value("shapefiledir").toString();
        auto shapefile = QFileDialog::getOpenFileName(this, tr("Select Shapefile to load"), basedir,
                                                      tr("ShapeFiles (*.shp);;All files (*.*)"));

        if (!shapefile.isEmpty()) {
            if (shpDataSet != nullptr) {
                delete shpDataSet;
            }

            shpDataSet = (GDALDataset *) OGROpen(shapefile.toStdString().c_str(), 0, nullptr);
            if (!shpDataSet) {
                std::ostringstream ss;
                ss << "Can't load shapefile " << shapefile.toStdString() << ": ";
                throw std::runtime_error(ss.str());
            }

            auto stdShapeFileName = shapefile.toStdString();

            // NOTE: the second parameter *must* be either nullstring or the name
            // of a layer present in the shape file! Otherwise nothing will be displayed.
            shpAdapter = std::make_shared<ESRIShapefile>(shpDataSet, "");

            shpAdapter->setPenPolygon(QPen(Qt::red));
            QColor col(Qt::yellow);
            col.setAlpha(64);
            shpAdapter->setBrushPolygon(QBrush(col));

            shpLayer = std::make_shared<LayerESRIShapefile>("ShapeFile-Layer");
            shpLayer->addESRIShapefile(shpAdapter);

            map->addLayer(shpLayer);
            shpLayer->setVisible(true);

            QFileInfo info(shapefile);
            settings.setValue("shapefiledir", info.absolutePath());
        }
    }
    catch (std::exception &x) {
        QMessageBox::warning(this, "Error loading shapefile", x.what());
    }
}

void ShapeFilesViewer::onLoadTiffFile()
{
    QSettings settings;

    try {
        auto basedir = settings.value("tifffiledir").toString();
        auto tiffFileName = QFileDialog::getOpenFileName(this, tr("Select Tiff to load"), basedir,
                                                         tr("TIFF Files (*.tif);;All files (*.*)"));

        if (!tiffFileName.isEmpty()) {
            if (tiffDataSet != nullptr) {
                delete tiffDataSet;
            }

            tiffDataSet = (GDALDataset *) GDALOpen(tiffFileName.toStdString().c_str(), GA_ReadOnly);

            double adfGeoTransform[6];
            qDebug() << "Driver: " << tiffDataSet->GetDriver()->GetDescription() << " - "
                     << tiffDataSet->GetDriver()->GetMetadataItem(GDAL_DMD_LONGNAME);
            qDebug() << "Size:" << tiffDataSet->GetRasterXSize() << tiffDataSet->GetRasterYSize()
                     << tiffDataSet->GetRasterCount();
            if (tiffDataSet->GetProjectionRef() != NULL)
                qDebug() << "Projection: " << tiffDataSet->GetProjectionRef();
            if (tiffDataSet->GetGeoTransform(adfGeoTransform) == CE_None) {
                qDebug() << "Origin = (" << adfGeoTransform[0] << "," << adfGeoTransform[3] << ")";
                qDebug() << "Pixel Size = (" << adfGeoTransform[1] << "," << adfGeoTransform[5];
            }

            tiffAdapter = std::make_shared<AdapterRaster>(tiffDataSet, "");
            tiffLayer = std::make_shared<LayerRaster>("Tiff-Layer");
            tiffLayer->addRaster(tiffAdapter);

            map->addLayer(tiffLayer);
            tiffLayer->setVisible(true);

            QFileInfo info(tiffFileName);
            settings.setValue("tifffiledir", info.absolutePath());
        }
    }
    catch (std::exception &x) {
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
