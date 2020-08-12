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

    QPixmap scaledPixmap;
    PointWorldCoord originWorld;
    PointWorldCoord oppositeWorld;

    OGRCoordinateTransformation *mTransformation;       /**< Transformation from Data Source to World (Local) */

    OGRCoordinateTransformation *mInvTransformation;    /**< Inverse Transformation from World to DS */
    double geoTransformMatrix[6];
    double xPixFactor, yPixFactor;
    int rasterSizeX, rasterSizeY;
    int currentZoomFactor = -1;

    int channels = 0;
    int cols = 0;
    int rows = 0;
    int offsetX, offsetY;
    char *databuf = nullptr;
    size_t dataSize = 0;
    QVector<QRgb> imageColorTable;

    QPixmap loadPixmap(size_t offX, size_t offY, size_t szX, size_t szY, size_t dstSx, size_t dstSy);

    QPointF rasterToWorldCoordinates(QPointF rc)
    {
        rc = dsToWorld(rc);
        return QPointF{
                geoTransformMatrix[0] + geoTransformMatrix[1] * rc.x() + geoTransformMatrix[2] * rc.y(),
                geoTransformMatrix[3] + geoTransformMatrix[4] * rc.x() + geoTransformMatrix[5] * rc.y()
        };
    }

    // Simplified version
    QPointF worldToRasterCoordinates(QPointF wc) const
    {
        wc = worldToDs(wc);
        return QPointF{
                (wc.x() - geoTransformMatrix[0]) / geoTransformMatrix[1],
                (wc.y() - geoTransformMatrix[3]) / geoTransformMatrix[5]
        };
    }

    QPointF worldToDs(QPointF pt) const
    {
        double x = pt.x();
        double y = pt.y();
        mInvTransformation->Transform(1, &x, &y);
        return QPointF{x, y};
    }

    QPointF dsToWorld(QPointF pt) const
    {
        double x = pt.x();
        double y = pt.y();
        mTransformation->Transform(1, &x, &y);
        return QPointF{x, y};
    }

    QPointF clipToRaster(QPointF ps) const
    {
        return QPointF{
                std::min(std::max(ps.x(), 0.0), static_cast<double>(rasterSizeX)),
                std::min(std::max(ps.y(), 0.0), static_cast<double>(rasterSizeY))
        };
    }
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

    p->mTransformation = OGRCreateCoordinateTransformation(spatialReference, &destinationWCS);
    p->mInvTransformation = OGRCreateCoordinateTransformation(&destinationWCS, spatialReference);

    if (p->mTransformation == nullptr || p->mInvTransformation == nullptr) {
        throw std::runtime_error("Coordinate Transformation fail");
    }

    if (p->ds->GetGeoTransform(p->geoTransformMatrix) == CE_None) {
        p->rasterSizeX = p->ds->GetRasterXSize();
        p->rasterSizeY = p->ds->GetRasterYSize();
        double x = p->geoTransformMatrix[0];
        double y = p->geoTransformMatrix[3];
        p->mTransformation->Transform(1, &x, &y);

        qDebug() << "Transform Check: " << p->geoTransformMatrix[0] << p->geoTransformMatrix[3] << " => "
                 << x << y;
        qDebug() << "Inv: " << x << y << p->worldToDs(QPointF{x, y});

        double x2 = p->geoTransformMatrix[0] + p->rasterSizeX * p->geoTransformMatrix[1] +
                    p->rasterSizeY * p->geoTransformMatrix[2];
        double y2 = p->geoTransformMatrix[3] + p->rasterSizeX * p->geoTransformMatrix[4] +
                    p->rasterSizeY * p->geoTransformMatrix[5];
        // (x2,y2) are in ds coordinates
        p->mTransformation->Transform(1, &x2, &y2);

        if (x2 < x) {
            std::swap(x2, x);
        }
        if (y2 > y) {
            std::swap(y2, y);
        }

        p->originWorld = PointWorldCoord(x, y);
        p->oppositeWorld = PointWorldCoord(x2, y2);
        qDebug() << "Origin Set to: " << p->originWorld.rawPoint();
        qDebug() << "Opposite Point Set to: " << p->oppositeWorld.rawPoint();

        p->xPixFactor = p->geoTransformMatrix[1];
        p->yPixFactor = p->geoTransformMatrix[5];
        qDebug() << "Pix Factors: " << p->xPixFactor << "x" << p->yPixFactor;
    }
}

PointWorldCoord AdapterRaster::getOrigin() const
{
    return p->originWorld;
}

void AdapterRaster::draw(QPainter &painter, const RectWorldPx &backbuffer_rect_px, int controller_zoom)
{
    // TODO check if the current controller zoom is outside the allowed zoom (min-max). In case, return.

    if (p->ds == nullptr) {
        return;
    }

    auto topLeftWC = projection::get().toPointWorldCoord(backbuffer_rect_px.topLeftPx(), controller_zoom).rawPoint();
    auto botRightWC = projection::get().toPointWorldCoord(backbuffer_rect_px.bottomRightPx(),
                                                          controller_zoom).rawPoint();


    auto topLeftRasterC = p->worldToRasterCoordinates(topLeftWC);
    auto botRightC = p->worldToRasterCoordinates(botRightWC);

    qDebug() << "TopLeft WC: " << topLeftWC << " Ds: " << p->worldToDs(topLeftWC) << " RasterC: "
             << topLeftRasterC;
    qDebug() << "BotRigh WC: " << botRightWC << " Ds: " << p->worldToDs(botRightWC) << " RasterC: "
             << botRightC;

    topLeftRasterC = p->clipToRaster(topLeftRasterC);
    botRightC = p->clipToRaster(botRightC);

    /* Check */

    auto originPix = projection::get().toPointWorldPx(p->originWorld, controller_zoom).rawPoint();
    qDebug() << "Origin: " << originPix;

    p->offsetX = topLeftRasterC.x();
    p->offsetY = topLeftRasterC.y();

    qDebug() << "Offset: " << p->offsetX << p->offsetY;

    auto extentPix = projection::get().toPointWorldPx(p->oppositeWorld, controller_zoom);

    qDebug() << "Extent: " << extentPix.rawPoint();

//    if (p->currentZoomFactor != controller_zoom) {
    // rescale
    auto dx = extentPix.x() - originPix.x();
    auto dy = extentPix.y() - originPix.y();

    qDebug() << "DSz: " << dx << dy;

    p->scaledPixmap = p->loadPixmap(p->offsetX, p->offsetY,
                                    botRightC.x() - p->offsetX, botRightC.y() - p->offsetY,
                                    dx, dy
    );
    p->currentZoomFactor = controller_zoom;
//    }

    painter.drawPixmap(originPix, p->scaledPixmap);
}

QPixmap
AdapterRaster::Impl::loadPixmap(size_t srcX, size_t srcY, size_t srcSx, size_t srcSy, size_t dstSx, size_t dstSy)
{
    qDebug() << "Resizing: Offset: " << srcX << srcY << " sz " << srcSx << srcSy;
    qDebug() << "Dest Size: " << dstSx << dstSy;

    if (channels == 0) {
        channels = ds->GetRasterCount();
    }
    if (rows == 0) {
        rows = ds->GetRasterYSize();
    }
    if (cols == 0) {
        cols = ds->GetRasterXSize();
    }

    int scanlineSize = std::ceil(static_cast<float>(channels) * dstSx / 4.0) * 4;

    qDebug() << "Scanline: " << scanlineSize;

    size_t newdataSize = rows * scanlineSize;
    if (newdataSize != dataSize) {
        if (databuf != nullptr) {
            delete[]databuf;
        }
        databuf = new char[newdataSize];
        dataSize = newdataSize;
    }

    // Iterate over each channel
    std::vector<int> bands(channels);
    std::iota(bands.begin(), bands.end(), 1);

    auto conversionResult = ds->RasterIO(GF_Read, srcX, srcY,
                                         srcSx, srcSy,
                                         databuf,
                                         dstSx, dstSy,
                                         GDT_Byte,
                                         bands.size(), bands.data(),
                                         bands.size(),            // pixelspace
                                         scanlineSize, // linespace
                                         (channels > 1 ? 1 : 0),            // bandspace
                                         nullptr);

    if (conversionResult != CE_None) {
        qWarning() << "GetRaster returned " << conversionResult;
        return QPixmap{};
    }

    QImage::Format imageFormat = QImage::Format::Format_RGB888;
    auto mainBand = ds->GetRasterBand(1);
    GDALColorTable *ct;

    if (mainBand && (ct = mainBand->GetColorTable()) && imageColorTable.isEmpty()) {
        imageFormat = QImage::Format::Format_Indexed8;

        auto n = ct->GetColorEntryCount();
        for (int i = 0; i < n; ++i) {
            GDALColorEntry entry;
            ct->GetColorEntryAsRGB(i, &entry);
            imageColorTable.push_back(
                    qRgb(entry.c1, entry.c2, entry.c3)
            );
        }
    }

    QImage image(reinterpret_cast<uchar *>(databuf), dstSx, dstSy, imageFormat);
    if (!imageColorTable.empty()) {
        image.setColorTable(imageColorTable);
    }

    image.save("SampleSave.png");

    return QPixmap::fromImage(image);
}

}
