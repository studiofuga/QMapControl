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
    int xSize, ySize;
    PointWorldCoord eW;
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
#if GDAL_VERSION_MAJOR >= 3
    destinationWCS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
#endif

    qDebug() << "Source: " << p->ds->GetProjectionRef();

    p->mTransformation = OGRCreateCoordinateTransformation(spatialReference, &destinationWCS);

    if (p->mTransformation == nullptr) {
        throw std::runtime_error("Coordinate Transformation fail");
    }

    if (p->ds->GetGeoTransform(p->geoTransformMatrix) == CE_None) {
        p->xSize = p->ds->GetRasterXSize();
        p->ySize = p->ds->GetRasterYSize();
        double x = p->geoTransformMatrix[0];
        double y = p->geoTransformMatrix[3];
        p->mTransformation->Transform(1, &x, &y);

        double x2 = p->geoTransformMatrix[0] + p->xSize * p->geoTransformMatrix[1] + p->ySize * p->geoTransformMatrix[2];
        double y2 =p->geoTransformMatrix[3] + p->xSize * p->geoTransformMatrix[4] + p->ySize * p->geoTransformMatrix[5];
        p->mTransformation->Transform(1, &x2, &y2);

        if (x2 < x)
            std::swap(x2,x);
        if (y2 < y)
            std::swap(y2,y);

        p->originWorld = PointWorldCoord(x, y);
        p->eW = PointWorldCoord(x2, y2);
        qDebug() << "Origin Set to: " << p->originWorld.rawPoint();
        qDebug() << "Opposite Point Set to: " << p->eW.rawPoint();

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
    qDebug() << "Image has " << pixmap.size() << " pix " << p->eW.rawPoint() << " world ";
}

void AdapterRaster::draw(QPainter &painter, const RectWorldPx &backbuffer_rect_px, int controller_zoom)
{
    // TODO check if the current controller zoom is outside the allowed zoom (min-max). In case, return.

    if (p->ds == nullptr) {
        return;
    }

    // Calculate the world coordinates.
    const RectWorldCoord backbuffer_rect_coord(
            projection::get().toPointWorldCoord(backbuffer_rect_px.topLeftPx(), controller_zoom),
            projection::get().toPointWorldCoord(backbuffer_rect_px.bottomRightPx(), controller_zoom));

    auto originPix = projection::get().toPointWorldPx(p->originWorld, controller_zoom).rawPoint();

    if (p->currentZoomFactor != controller_zoom) {
        // rescale
        auto extentPix = projection::get().toPointWorldPx(p->eW, controller_zoom);

        auto dx = std::abs(extentPix.x() - originPix.x());
        auto dy = std::abs(extentPix.y() - originPix.y());

        qDebug() << "Rescaling " << p->mapPixmap.size() << " to " << dx << "x" << dy;

        p->scaledPixmap = p->mapPixmap.scaled(dx, dy);
        p->currentZoomFactor = controller_zoom;
    }

    painter.drawPixmap(originPix, p->scaledPixmap);
}

}
