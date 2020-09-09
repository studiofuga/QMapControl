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
#include <QTimer>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

#include <QDebug>

#include <stdexcept>
#include <cmath>

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
        courseSpeed = (targetCourse - currentCourse) / numMicroSteps;
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

    void nextStep()
    {
        if (state == Rotating) {
            calcNextMovement();
            qDebug() << "Moving: " << speed.rawPoint();
            state = Moving;
        } else if (state == Moving || state == Idle) {

            if (state == Moving) {
                ++currentStep;
            } else {
                currentStep = 0;
            }
            currentMicroStep = 0;
            calcNextRotation();
            qDebug() << "Step " << currentStep << "Rotating: " << courseSpeed;
            if (finished()) {
                qDebug() << "End";
                return;
            }
            state = Rotating;
        }
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
        nextStep();
    }

    void animate()
    {
        if (state == Moving) {
            currentPosition = qmapcontrol::PointWorldCoord{
                    currentPosition.longitude() + speed.longitude(),
                    currentPosition.latitude() + speed.latitude()
            };
        } else if (state == Rotating) {
            currentCourse = currentCourse + courseSpeed;
//            qDebug() << "Current Course: " << (180.0 * currentCourse / M_PI);
        }
        ++currentMicroStep;
        if (stepFinished()) {
            nextStep();
        }
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
    std::shared_ptr<qmapcontrol::MapAdapterOSM> baseAdapter;
    std::shared_ptr<qmapcontrol::LayerMapAdapter> baseLayer;

    QDial *dial;
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

    connect(&p->timer, &QTimer::timeout, this, &Navigator::animate);
}

Navigator::~Navigator()
{
    delete p;
}

void Navigator::buildMenu()
{
    auto mapMenu = menuBar()->addMenu(tr("&Map"));
    auto actionLoadPath = new QAction(tr("&Load Path..."));
    auto actionSavePath = new QAction(tr("&Save Path..."));
    auto actionRecordPoint = new QAction(tr("&Record"));
    auto actionReplay = new QAction(tr("&Replay"));
    mapMenu->addAction(actionLoadPath);
    mapMenu->addAction(actionSavePath);
    mapMenu->addAction(actionRecordPoint);
    mapMenu->addAction(actionReplay);

    connect(actionRecordPoint, &QAction::triggered, this, &Navigator::onActionRecordPoint);
    connect(actionSavePath, &QAction::triggered, this, &Navigator::onActionSavePath);
    connect(actionLoadPath, &QAction::triggered, this, &Navigator::onActionLoadPath);
    connect(actionReplay, &QAction::triggered, this, &Navigator::onActionPlayPath);

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

void Navigator::onActionRecordPoint()
{
    auto point = NavPoint{
            p->map->getCurrentZoom(),
            p->map->mapFocusPointCoord()
    };
    p->navPoints.push_back(point);
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
    }
}

void Navigator::onActionPlayPath()
{
    p->animation = std::make_unique<NavigatorAnimation>(p->navPoints);
    p->animation->setCurrentPosition(p->map->mapFocusPointCoord());
    p->animation->setCurrentCourse(p->map->mapRotation());
    p->animation->start();
    p->timer.start(100);
}

void Navigator::animate()
{
    p->animation->animate();
    p->map->setMapRotation(p->animation->getCurrentCourse());
    p->map->setMapFocusPoint(p->animation->getCurrentPosition());
    if (p->animation->finished()) {
        p->timer.stop();
    }
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    Navigator mainWindow;
    mainWindow.resize(800, 600);
    mainWindow.show();

    return app.exec();
}
