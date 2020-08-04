//
// Created by happycactus on 01/08/20.
//

#include "AdapterRaster.h"
#include "Point.h"
#include "Projection.h"

#include <QPainter>
#include <QDebug>

#include <stdexcept>

namespace qmapcontrol {

struct AdapterRaster::Impl {
    GDALDataset *ds = nullptr;
    std::string layername;

    QPixmap mapPixmap;
    QPixmap scaledPixmap;
    PointWorldCoord originWorld;

    OGRCoordinateTransformation *mTransformation;

    double geoTransformMatrix[6];
    double xPixFactor, yPixFactor;
    PointWorldCoord extentWorld;
    int currentZoomFactor = -1;
};

AdapterRaster::AdapterRaster(GDALDataset *datasource, OGRSpatialReference*spatialReference,
                             std::string layer_name, QObject *parent)
        : QObject(parent), p(spimpl::make_unique_impl<Impl>())
{
    p->ds = datasource;
    p->layername = std::move(layer_name);

    OGRSpatialReference destinationWCS;
    // TODO check the correct destination WCS.
    if (destinationWCS.importFromEPSG(4326) != OGRERR_NONE) {
        throw std::runtime_error("Can't import EPSG");
    }

    qDebug() << "Source: " << p->ds->GetProjectionRef();

    p->mTransformation = OGRCreateCoordinateTransformation(spatialReference, &destinationWCS);

    if (p->mTransformation == nullptr) {
        throw std::runtime_error("Coordinate Transformation fail");
    }

    if (p->ds->GetGeoTransform(p->geoTransformMatrix) == CE_None) {
        double x = p->geoTransformMatrix[0];
        double y = p->geoTransformMatrix[3];
        p->mTransformation->Transform(1, &x, &y);
        p->originWorld = PointWorldCoord(x, y);
        qDebug() << "Origin Set to: " << p->originWorld.rawPoint();

        p->xPixFactor = p->geoTransformMatrix[1];
        p->yPixFactor = p->geoTransformMatrix[5];
        qDebug() << "Pix Factors: " << p->xPixFactor << "x" << p->yPixFactor;
    }

    // here we should import the pixmap.
}

PointWorldCoord AdapterRaster::getOrigin() const
{
    return p->originWorld;
}

void AdapterRaster::setPixmap(QPixmap pixmap)
{
    p->mapPixmap = pixmap;
    double dx = std::abs(pixmap.width() * p->xPixFactor);
    double dy = std::abs(pixmap.height() * p->yPixFactor);
    qDebug() << "Raw Extent: " << dx << dy;

    p->mTransformation->Transform(1, &dx, &dy);
    qDebug() << "TRN Extent: " << dx << dy;

    p->extentWorld = PointWorldCoord{dx, dy};

    qDebug() << "Image has " << pixmap.size() << " pix " << p->extentWorld.rawPoint() << " world ";
}

void AdapterRaster::draw(QPainter &painter, const RectWorldPx &backbuffer_rect_px, int controller_zoom)
{
    // TODO check if the current controller zoom is outside the allowed zoom (min-max). In case, return.

//    qDebug() << "Drawing: " << backbuffer_rect_px.rawRect() << " zoom " << controller_zoom;

    if (p->ds == nullptr) {
        return;
    }

    // Calculate the world coordinates.
    const RectWorldCoord backbuffer_rect_coord(
            projection::get().toPointWorldCoord(backbuffer_rect_px.topLeftPx(), controller_zoom),
            projection::get().toPointWorldCoord(backbuffer_rect_px.bottomRightPx(), controller_zoom));
//    qDebug() << "Drawing in px: " << backbuffer_rect_coord.rawRect() << " zoom " << controller_zoom;
/*

    auto topLeftWorld = projection::get().toPointWorldCoord(backbuffer_rect_px.topLeftPx(), controller_zoom);
    auto botRightWorld = projection::get().toPointWorldCoord(backbuffer_rect_px.bottomRightPx(), controller_zoom);
    auto topLeftPix = projection::get().toPointWorldPx(topLeftWorld, controller_zoom).rawPoint();
    auto botRightPix = projection::get().toPointWorldPx(botRightWorld, controller_zoom).rawPoint();
*/

    auto originPix = projection::get().toPointWorldPx(p->originWorld, controller_zoom).rawPoint();

    if (p->currentZoomFactor != controller_zoom) {
        // rescale
        auto extentPix = projection::get().toPointWorldPx(p->extentWorld, controller_zoom);

        auto dx = std::abs(extentPix.x() - originPix.x());
        auto dy = std::abs(extentPix.y() - originPix.y());

        qDebug() << "Rescaling " << p->mapPixmap.size() << " to " << dx << "x" << dy;

        p->scaledPixmap = p->mapPixmap.scaled(dx, dy);
        p->currentZoomFactor = controller_zoom;
    }

    painter.drawPixmap(originPix, p->scaledPixmap);
}

}
