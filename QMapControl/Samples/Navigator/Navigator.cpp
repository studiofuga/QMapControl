//
// Created by fuga on 18/07/2020.
//

#include "Navigator.h"

#include "QMapControl/LayerMapAdapter.h"
#include "QMapControl/MapAdapterOSM.h"
#include "QMapControl/MapAdapterGoogle.h"
#include "QMapControl/QMapControl.h"
#include "QMapControl/LayerGeometry.h"
#include "QMapControl/GeometryLineString.h"
#include "QMapControl/ESRIShapefile.h"
#include "QMapControl/AdapterRaster.h"
#include "QMapControl/LayerESRIShapefile.h"
#include "QMapControl/LayerRaster.h"

#include <gdal_priv.h>

#include <QApplication>
#include <QMenuBar>
#include <QMenu>
#include <QToolBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QSettings>
#include <QVBoxLayout>
#include <QDial>
#include <QLabel>
#include <QTimer>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

#include <QDebug>

#include <stdexcept>
#include <cmath>
#include <sstream>

using namespace qmapcontrol;

struct NavPoint {
    int zoom;
    qmapcontrol::PointWorldCoord navPoint;
};

static double greatCircle(qmapcontrol::PointWorldCoord from, qmapcontrol::PointWorldCoord to)
{
    double tolat = M_PI * to.latitude() / 180.0;
    double tolong = M_PI * to.longitude() / 180.0;
    double fromlat = M_PI * from.latitude() / 180.0;
    double fromlong = M_PI * from.longitude() / 180.0;

    int radius = 6371; // 6371km is the radius of the earth
    double dLat = tolat - fromlat;
    double dLon = tolong - fromlong;
    double a = std::pow(std::sin(dLat / 2), 2) +
               std::cos(fromlat) * std::cos(tolat) * std::pow(std::sin(dLon / 2), 2);
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));
    double d = radius * c;

    return d;
}

static double cartesianDistance(qmapcontrol::PointWorldCoord from, qmapcontrol::PointWorldCoord to)
{
    auto dlat = to.latitude() - from.latitude();
    auto dlon = to.longitude() - from.longitude();
    return std::sqrt(dlat * dlat + dlon * dlon);
}

static double cartesianBearing(qmapcontrol::PointWorldCoord from, qmapcontrol::PointWorldCoord to)
{
    auto dlat = to.latitude() - from.latitude();
    auto dlon = to.longitude() - from.longitude();
    return std::atan2(dlat, dlon) - M_PI_2;
}

class NavigatorAnimation {
    std::vector<NavPoint> navPoints;
    double courseSpeed = 0.0;
    qmapcontrol::PointWorldCoord speed;
    double currentCourse, targetCourse;
    qmapcontrol::PointWorldCoord currentPosition;
    int numSteps = 0, currentStep = 0;
    int numMicroSteps = 0, currentMicroStep = 0;
    enum State {
        Idle, Rotating, Moving
    };
    State state = Idle;

    bool calcNextRotation()
    {
        auto nextPos = navPoints[currentStep].navPoint;
        targetCourse = cartesianBearing(currentPosition, nextPos);
        numMicroSteps = 10;
        auto delta = (targetCourse - currentCourse);
        if (delta > M_PI) {
            delta = delta - M_2_PI;
        }

        courseSpeed = (delta) / numMicroSteps;
        qDebug() << "Next Rotation: " << (180.0 * currentCourse / M_PI) << " => "
                 << (180.0 * targetCourse / M_PI) << " speed " << (180.0 * courseSpeed / M_PI);
        return true;
    }

    bool calcNextMovement()
    {
        numMicroSteps = 50;
        auto nextPos = navPoints[currentStep].navPoint;
        speed = qmapcontrol::PointWorldCoord{
                (nextPos.longitude() - currentPosition.longitude()) / numMicroSteps,
                (nextPos.latitude() - currentPosition.latitude()) / numMicroSteps
        };
        qDebug() << "Next Movement: " << currentPosition.rawPoint() << " => " << nextPos.rawPoint() << " Speed: "
                 << speed.rawPoint();
        return true;
    }

    /**
     * @brief move to the next step in the path
     * @return true if everything is completed.
     */
    bool nextStep()
    {
        if (state == Rotating) {
            calcNextMovement();
            qDebug() << "Moving speed " << speed.rawPoint();
            state = Moving;
            currentMicroStep = 0;
        } else if (state == Moving || state == Idle) {

            if (state == Moving) {
                ++currentStep;
                qDebug() << "Hit Point " << currentStep << "(" << currentMicroStep << ") Current Position: "
                         << currentPosition.rawPoint();
            } else {
                currentStep = 1;
            }
            currentMicroStep = 0;
            calcNextRotation();
            if (finished()) {
                return true;
            }
            state = Rotating;
        }
        return false;
    }

    bool stepFinished() const
    {
        return currentMicroStep >= numMicroSteps;
    }

public:
    explicit NavigatorAnimation(const std::vector<NavPoint> &navPoints) : navPoints(navPoints)
    {
    }

    void setCurrentCourse(double currentCourse)
    {
        NavigatorAnimation::currentCourse = M_PI * currentCourse / 180.0;
    }

    void setCurrentPosition(const PointWorldCoord &currentPosition)
    {
        NavigatorAnimation::currentPosition = currentPosition;
    }

    void start()
    {
        currentStep = 0;
        currentPosition = navPoints[0].navPoint;
        nextStep();
    }

    void animate()
    {
        if (stepFinished()) {
            if (nextStep()) {
                return;
            }
        }
        if (state == Moving) {
            currentPosition = qmapcontrol::PointWorldCoord{
                    currentPosition.longitude() + speed.longitude(),
                    currentPosition.latitude() + speed.latitude()
            };
        } else if (state == Rotating) {
            currentCourse = currentCourse + courseSpeed;
        }
        ++currentMicroStep;
    }

    double getCurrentCourse() const
    {
        return 180.0 * currentCourse / M_PI;
    }

    const PointWorldCoord &getCurrentPosition() const
    {
        return currentPosition;
    }

    bool finished() const
    {
        return currentStep >= navPoints.size();
    }

};

struct Navigator::Impl {
    qmapcontrol::QMapControl *map;
    std::shared_ptr<qmapcontrol::MapAdapter> baseAdapter;
    std::shared_ptr<qmapcontrol::LayerMapAdapter> baseLayer;

    std::string pathLayerName;
    std::shared_ptr<qmapcontrol::LayerGeometry> pathLayer;

    GDALDataset *shpDataSet = nullptr;
    std::shared_ptr<qmapcontrol::ESRIShapefile> shpAdapter;
    std::shared_ptr<qmapcontrol::LayerESRIShapefile> shpLayer;

    GDALDataset *tiffDataSet = nullptr;
    std::shared_ptr<qmapcontrol::AdapterRaster> tiffAdapter;
    std::shared_ptr<qmapcontrol::LayerRaster> tiffLayer;

    QDial *dial;
    QLabel *labelAirplane;
    QLabel *labelPosition;
    QLabel *labelCourse;
    QTimer timer;

    std::vector<NavPoint> navPoints;
    std::unique_ptr<NavigatorAnimation> animation;
};

Navigator::Navigator()
        : QMainWindow(nullptr),
          p(new Impl())
{
    statusBar()->show();

    p->map = new QMapControl(QSizeF(800, 600), this);

    p->baseAdapter = std::make_shared<MapAdapterGoogle>();
    p->baseLayer = std::make_shared<LayerMapAdapter>("Google", p->baseAdapter);
    p->pathLayerName = "Paths";
    p->pathLayer = std::make_shared<LayerGeometry>(p->pathLayerName);

    p->map->addLayer(p->baseLayer);
    p->map->addLayer(p->pathLayer);

    p->map->setMapFocusPoint(PointWorldCoord(-77.042793, -12.046374));
    p->map->setZoom(9);
    p->map->setBackgroundColour(Qt::white);

    setCentralWidget(p->map);

    buildMenu();
    buildOnMapControls();

    connect(p->map, &QMapControl::mapFocusPointChanged, this, &Navigator::mapFocusPointChanged);
    connect(p->map, &QMapControl::mouseEventMoveCoordinate, this, &Navigator::mapMouseMove);
    connect(p->map, &QMapControl::mapCourseChanged, this, &Navigator::mapRotationChanged);

    connect(&p->timer, &QTimer::timeout, this, &Navigator::animate);
}

Navigator::~Navigator()
{
    delete p;
}

void Navigator::buildMenu()
{
    auto mapMenu = menuBar()->addMenu(tr("&File"));
    auto actionLoadPath = new QAction(tr("&Load Path..."));
    auto actionSavePath = new QAction(tr("&Save Path..."));
    mapMenu->addAction(actionLoadPath);
    mapMenu->addAction(actionSavePath);

    auto pathMenu = menuBar()->addMenu(tr("&Path"));
    auto actionClearPath = new QAction(tr("&Clear Path"));
    auto actionRecordPoint = new QAction(QIcon(":/record.png"), tr("&Record current Point"));
    auto actionReplay = new QAction(QIcon(":/replay.png"), tr("&Replay"));
    pathMenu->addAction(actionClearPath);
    pathMenu->addAction(actionRecordPoint);
    pathMenu->addAction(actionReplay);

    auto toolbar = addToolBar("main");
    toolbar->addAction(actionRecordPoint);
    toolbar->addAction(actionReplay);

    connect(actionClearPath, &QAction::triggered, this, [this]() {
        p->navPoints.clear();
        updatePath();
    });
    connect(actionRecordPoint, &QAction::triggered, this, &Navigator::onActionRecordPoint);
    connect(actionSavePath, &QAction::triggered, this, &Navigator::onActionSavePath);
    connect(actionLoadPath, &QAction::triggered, this, &Navigator::onActionLoadPath);
    connect(actionReplay, &QAction::triggered, this, &Navigator::onActionPlayPath);

    auto layersMenu = menuBar()->addMenu(tr("&Layers"));
    auto actionLoadShapefile = new QAction("Load &Shapefile");
    connect(actionLoadShapefile, &QAction::triggered, this, &Navigator::onActionLoadShapefile);
    layersMenu->addAction(actionLoadShapefile);

    auto actionLoadTiff = new QAction("Load &Tiff");
    connect(actionLoadTiff, &QAction::triggered, this, &Navigator::onActionLoadTiff);
    layersMenu->addAction(actionLoadTiff);

    layersMenu->addSeparator();

    auto actionLayermap = new QAction("Map");
    actionLayermap->setCheckable(true);
    actionLayermap->setChecked(true);
    layersMenu->addAction(actionLayermap);
    connect(actionLayermap, &QAction::toggled, this, [this](bool checked) {
        p->baseLayer->setVisible(checked);
    });

    auto actionLayerPaths = new QAction("Path");
    actionLayerPaths->setCheckable(true);
    actionLayerPaths->setChecked(true);
    layersMenu->addAction(actionLayerPaths);
    connect(actionLayerPaths, &QAction::toggled, this, [this](bool checked) {
        p->pathLayer->setVisible(checked);
    });

    auto actionShpLayer = new QAction("Shapefile");
    actionShpLayer->setCheckable(true);
    actionShpLayer->setChecked(true);
    layersMenu->addAction(actionShpLayer);
    connect(actionShpLayer, &QAction::toggled, this, [this](bool checked) {
        p->shpLayer->setVisible(checked);
    });

    auto actionTiffLayer = new QAction("TIFF");
    actionTiffLayer->setCheckable(true);
    actionTiffLayer->setChecked(true);
    layersMenu->addAction(actionTiffLayer);
    connect(actionTiffLayer, &QAction::toggled, this, [this](bool checked) {
        p->tiffLayer->setVisible(checked);
    });
}

void Navigator::mapFocusPointChanged(qmapcontrol::PointWorldCoord focusPoint)
{
    p->labelPosition->setText(tr("Position: lon %1, lat %2").arg(focusPoint.longitude()).arg(focusPoint.latitude()));
}

void Navigator::mapMouseMove(QMouseEvent *mouseEvent, qmapcontrol::PointWorldCoord pressedPos,
                             qmapcontrol::PointWorldCoord currentPos)
{
    auto focusPoint = p->map->mapFocusPointCoord();
    p->labelPosition->setText(tr("Position: lon %1, lat %2").arg(focusPoint.longitude()).arg(focusPoint.latitude()));
}

void Navigator::mapRotationChanged(qreal courseDegrees)
{
    p->dial->setValue(courseDegrees + 180);
    p->labelCourse->setText(tr("Course: %1 deg").arg((360 + (int) courseDegrees) % 360));
}

void Navigator::buildOnMapControls()
{
    // Create an inner layout to display buttons/"mini" map control.
    auto layout_inner = new QGridLayout;

    // Note 0 is south for dial. Add 180degs.
    p->dial = new QDial();
    p->dial->setMinimum(0);
    p->dial->setMaximum(359);
    p->dial->setWrapping(true);
    p->dial->setMaximumSize(QSize(100, 100));
    p->dial->setValue(180);

    connect(p->dial, &QDial::valueChanged, this, [this](int value) {
        onCourseChanged(value - 180.0);
    });

    layout_inner->setColumnStretch(0, 0);
    layout_inner->setColumnStretch(1, 1);
    layout_inner->setRowStretch(0, 1);
    layout_inner->setRowStretch(1, 0);

    auto frame = new QFrame;
    frame->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    frame->setFrameShape(QFrame::Shape::StyledPanel);
    frame->setFrameShadow(QFrame::Shadow::Raised);
    layout_inner->addWidget(frame, 1, 0);

    auto *vb = new QVBoxLayout;
    frame->setLayout(vb);

    vb->addWidget(p->dial);

    p->labelAirplane = new QLabel("Airplane: ");
    p->labelAirplane->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
    vb->addWidget(p->labelAirplane);
    p->labelPosition = new QLabel("Position: ");
    p->labelPosition->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
    vb->addWidget(p->labelPosition);
    p->labelCourse = new QLabel("Course: ");
    p->labelCourse->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
    vb->addWidget(p->labelCourse);

    // Set the main map control to use the inner layout.
    p->map->setLayout(layout_inner);
}

void Navigator::onCourseChanged(qreal newcourse)
{
    p->map->setMapRotation(newcourse);
}

void Navigator::onActionRecordPoint()
{
    auto point = NavPoint{
            p->map->getCurrentZoom(),
            p->map->mapFocusPointCoord()
    };
    p->navPoints.push_back(point);
    updatePath();
    statusBar()->showMessage(tr("New Point added, currently %1 point(s) in path").arg(p->navPoints.size()));
}

void Navigator::onActionSavePath()
{
    auto saveFileName = QFileDialog::getSaveFileName(this, tr("Save Path"));
    if (!saveFileName.isEmpty()) {
        QJsonObject path;
        QJsonArray points;
        for (auto const &point : p->navPoints) {
            QJsonObject jpoint;
            jpoint["long"] = point.navPoint.longitude();
            jpoint["lat"] = point.navPoint.latitude();
            jpoint["zoom"] = point.zoom;

            points.push_back(jpoint);
        }
        path["path"] = points;

        QJsonDocument doc(path);

        QFile saveFile(saveFileName);
        if (!saveFile.open(QIODevice::WriteOnly)) {
            QMessageBox::warning(this, tr("Cant save"), tr("Couldn't open save file."));
            return;
        }

        saveFile.write(doc.toJson());
        saveFile.close();
    }
}

void Navigator::onActionLoadPath()
{
    auto openFileName = QFileDialog::getOpenFileName(this, tr("Open Path"));
    if (!openFileName.isEmpty()) {
        QFile openFile(openFileName);

        if (!openFile.open(QIODevice::ReadOnly)) {
            QMessageBox::warning(this, tr("Can't load"), tr("Couldn't load file."));
            return;
        }

        QByteArray data = openFile.readAll();

        QJsonDocument loadDoc(QJsonDocument::fromJson(data));

        auto points = loadDoc["path"].toArray();
        p->navPoints.clear();
        for (int i = 0; i < points.size(); ++i) {
            auto obj = points[i].toObject();
            NavPoint pt;
            pt.navPoint.setLatitude(obj["lat"].toDouble());
            pt.navPoint.setLongitude(obj["long"].toDouble());
            pt.zoom = obj["zoom"].toInt();

            p->navPoints.push_back(pt);
        }

        p->map->setMapFocusPoint(p->navPoints[0].navPoint);
        updatePath();
        statusBar()->showMessage(tr("File Path loaded, %1 points").arg(p->navPoints.size()));
    }
}

void Navigator::onActionPlayPath()
{
    p->animation = std::make_unique<NavigatorAnimation>(p->navPoints);
    p->animation->setCurrentPosition(p->map->mapFocusPointCoord());
    p->animation->setCurrentCourse(p->map->mapRotation());
    p->animation->start();
    auto pix = QPixmap(":/airplane.png");
    if (pix.isNull())
        qWarning() << "Can't load airplane.png";
    p->map->setCenterPixmap(pix);
    p->map->enableCrosshairs(false);
    p->timer.start(100);

    statusBar()->showMessage(tr("Trip starting"));
}

void Navigator::updatePath()
{
    QPen pen(QColor(0, 0, 255, 100));
    pen.setWidth(5);

    // Add the points of the sights tour.
    std::vector<PointWorldCoord> points;
    for (auto navPoint : p->navPoints) {
        points.emplace_back(navPoint.navPoint.longitude(), navPoint.navPoint.latitude());
    }

    // Create the sights tour as a Line String and add it to the notes layer.
    p->pathLayer->clearGeometries();
    auto lines = std::make_shared<GeometryLineString>(points);
    lines->setPen(pen);
    p->pathLayer->addGeometry(lines);
}

void Navigator::animate()
{
    p->animation->animate();
    p->map->setMapRotation(p->animation->getCurrentCourse());
    p->map->setMapFocusPoint(p->animation->getCurrentPosition());
    if (p->animation->finished()) {
        p->map->clearCenterPixmap();
        p->map->enableCrosshairs(true);
        p->timer.stop();
        statusBar()->showMessage(tr("Trip Completed"));
    }
}

void Navigator::onActionLoadShapefile()
{
    QSettings settings;

    try {
        auto basedir = settings.value("shapefiledir").toString();
        auto shapefile = QFileDialog::getOpenFileName(this, tr("Select Shapefile to load"), basedir,
                                                      tr("ShapeFiles (*.shp);;All files (*.*)"));

        if (!shapefile.isEmpty()) {
            if (p->shpDataSet != nullptr) {
                delete p->shpDataSet;
            }

            p->shpDataSet = (GDALDataset *) OGROpen(shapefile.toStdString().c_str(), 0, nullptr);
            if (!p->shpDataSet) {
                std::ostringstream ss;
                ss << "Can't load shapefile " << shapefile.toStdString() << ": ";
                throw std::runtime_error(ss.str());
            }

            auto stdShapeFileName = shapefile.toStdString();

            // NOTE: the second parameter *must* be either nullstring or the name
            // of a layer present in the shape file! Otherwise nothing will be displayed.
            p->shpAdapter = std::make_shared<ESRIShapefile>(p->shpDataSet, "");

            p->shpAdapter->setPenPolygon(QPen(Qt::red));
            QColor col(Qt::yellow);
            col.setAlpha(64);
            p->shpAdapter->setBrushPolygon(QBrush(col));

            p->shpLayer = std::make_shared<LayerESRIShapefile>("ShapeFile-Layer");
            p->shpLayer->addESRIShapefile(p->shpAdapter);

            p->map->addLayer(p->shpLayer);
            p->shpLayer->setVisible(true);

            p->map->removeLayer(p->pathLayerName);
            p->map->addLayer(p->pathLayer);

            QFileInfo info(shapefile);
            settings.setValue("shapefiledir", info.absolutePath());
        }
    }
    catch (std::exception &x) {
        QMessageBox::warning(this, "Error loading shapefile", x.what());
    }
}

void Navigator::onActionLoadTiff()
{
    QSettings settings;

    try {
        auto basedir = settings.value("tifffiledir").toString();
        auto tiffFileName = QFileDialog::getOpenFileName(this, tr("Select Raster file to load"), basedir,
                                                         tr("Raster Files (*.tif *.ecw);;All files (*.*)"));

        if (!tiffFileName.isEmpty()) {
            if (p->tiffDataSet != nullptr) {
                delete p->tiffDataSet;
            }

            p->tiffDataSet = (GDALDataset *) GDALOpen(tiffFileName.toStdString().c_str(), GA_ReadOnly);
            if (p->tiffDataSet == nullptr) {
                throw std::runtime_error("File not supported or error");
            }

            double adfGeoTransform[6];
            OGRSpatialReference *oSRS = new OGRSpatialReference;

            // TODO ask the user to enter the correct WCG and Projection
            oSRS->importFromEPSG(32628);
            p->tiffDataSet->SetSpatialRef(oSRS);

            p->tiffAdapter = std::make_shared<AdapterRaster>(p->tiffDataSet, oSRS, "");
            p->map->setMapFocusPoint(p->tiffAdapter->getCenter());

            p->tiffLayer = std::make_shared<LayerRaster>("Tiff-Layer");
            p->tiffLayer->addRaster(p->tiffAdapter);

            p->map->addLayer(p->tiffLayer);
            p->tiffLayer->setVisible(true);

            p->map->removeLayer(p->pathLayerName);
            p->map->addLayer(p->pathLayer);

            QFileInfo info(tiffFileName);
            settings.setValue("tifffiledir", info.absolutePath());
        }
    }
    catch (std::exception &x) {
        qWarning() << "Exception: " << x.what();
        QMessageBox::warning(this, "Import TIFF", QString("Import failed: %1").arg(x.what()));
    }

}

int main(int argc, char *argv[])
{
    QCoreApplication::setOrganizationName("QMapControl");
    QCoreApplication::setOrganizationDomain("qmapcontrol.sample.local");
    QCoreApplication::setApplicationName("NavigatorSample");

    QApplication app(argc, argv);

    GDALAllRegister();

    Navigator mainWindow;
    mainWindow.resize(800, 600);
    mainWindow.show();

    return app.exec();
}
