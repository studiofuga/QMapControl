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
    QPointF originDs, oppositeDs;

    OGRCoordinateTransformation *mTransformation;       /**< Transformation from Data Source to World (Local) */

    OGRCoordinateTransformation *mInvTransformation;    /**< Inverse Transformation from World to DS */
    double geoTransformMatrix[6];
    double xPixFactor, yPixFactor;
    int rasterSizeX, rasterSizeY;

    int channels = 0;
    int cols = 0;
    int rows = 0;
    int offsetX=-1, offsetY=-1;
    int srcDx=0, srcDy = 0;
    int dx =0,dy = 0;
    char *databuf = nullptr;
    size_t dataSize = 0;
    QVector<QRgb> imageColorTable;

    virtual ~Impl() {
        if (databuf != nullptr)
            delete []databuf;
    }

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

        double x2 = p->geoTransformMatrix[0] + p->rasterSizeX * p->geoTransformMatrix[1] +
                    p->rasterSizeY * p->geoTransformMatrix[2];
        double y2 = p->geoTransformMatrix[3] + p->rasterSizeX * p->geoTransformMatrix[4] +
                    p->rasterSizeY * p->geoTransformMatrix[5];

        if (x2 < x) {
            std::swap(x2, x);
        }
        if (y2 > y) {
            std::swap(y2, y);
        }

        p->originDs = QPointF{x, y};
        p->mTransformation->Transform(1, &x, &y);

        qDebug() << "Transform Check: " << p->geoTransformMatrix[0] << p->geoTransformMatrix[3] << " => "
                 << x << y;
        qDebug() << "Inv: " << x << y << p->worldToDs(QPointF{x, y});

        // (x2,y2) are in ds coordinates
        p->oppositeDs = QPointF{x2, y2};
        p->mTransformation->Transform(1, &x2, &y2);

        p->originWorld = PointWorldCoord(x, y);
        p->oppositeWorld = PointWorldCoord(x2, y2);
        qDebug() << "Origin Set to: " << p->originWorld.rawPoint() << " DS: " << p->originDs;
        qDebug() << "Opposite Point Set to: " << p->oppositeWorld.rawPoint() << " DS: " << p->oppositeDs;

        p->xPixFactor = p->geoTransformMatrix[1];
        p->yPixFactor = p->geoTransformMatrix[5];
        qDebug() << "Pix Factors: " << p->xPixFactor << "x" << p->yPixFactor;


        {
            auto oxt = p->worldToRasterCoordinates(p->originWorld.rawPoint());
            auto ext = p->worldToRasterCoordinates(p->oppositeWorld.rawPoint());
            qDebug() << "Testing Origin: " << p->originWorld.rawPoint() << " => raster: " << oxt;
            qDebug() << "Testing Oppost: " << p->oppositeWorld.rawPoint() << " => raster: " << ext;
        }


    }
}

PointWorldCoord AdapterRaster::getOrigin() const { return p->originWorld; }

PointWorldCoord AdapterRaster::getCenter() const
{
    return PointWorldCoord((p->originWorld.longitude() + p->oppositeWorld.longitude()) / 2,
                           (p->originWorld.latitude() + p->oppositeWorld.latitude()) / 2);
}

void AdapterRaster::draw(QPainter& painter,
                         const RectWorldPx& backbuffer_rect_px,
                         int controller_zoom)
{
    // TODO check if the current controller zoom is outside the allowed zoom (min-max). In case,
    // return.

    if (p->ds == nullptr) {
        return;
    }

    auto topLeftWC = projection::get()
            .toPointWorldCoord(backbuffer_rect_px.topLeftPx(), controller_zoom)
            .rawPoint();
    auto botRightWC = projection::get()
            .toPointWorldCoord(backbuffer_rect_px.bottomRightPx(), controller_zoom)
            .rawPoint();

    auto topLeftRasterC = p->worldToRasterCoordinates(topLeftWC);
    auto botRightC = p->worldToRasterCoordinates(botRightWC);

//    qDebug() << "Top Left RasterC (1): " << topLeftRasterC;
//    qDebug() << "Bot Rigt RasterC (1): " << botRightC;

    auto topLeftDs = p->worldToDs(topLeftWC);
    auto botRightDs = p->worldToDs(botRightWC);

/*
    auto bbTopLeftClippedX = std::max(backbuffer_rect_px.topLeftPx().x(), p->originDs.x());
    auto bbTopLeftClippedY = std::max(backbuffer_rect_px.topLeftPx().y(), p->originDs.y());
    auto bbBotRClippedX = std::min(backbuffer_rect_px.bottomRightPx().x(), p->oppositeDs.x());
    auto bbBotRClippedY = std::min(backbuffer_rect_px.bottomRightPx().y(), p->oppositeDs.y());
*/

    topLeftRasterC = p->clipToRaster(topLeftRasterC);
    botRightC = p->clipToRaster(botRightC);

//    qDebug() << "Top Left RasterC (2): " << topLeftRasterC;
//    qDebug() << "Bot Rigt RasterC (2): " << botRightC;

    /* Check */

    auto originPix = projection::get().toPointWorldPx(p->originWorld, controller_zoom).rawPoint();

    auto newOffsetX = (int) std::floor(topLeftRasterC.x() + 0.5);
    auto newOffsetY = (int) std::floor(topLeftRasterC.y() + 0.5);

    auto extentPix = projection::get().toPointWorldPx(p->oppositeWorld, controller_zoom);

/*
    auto bbTopLPx = projection::get().toPointWorldPx(PointWorldCoord{bbTopLeftClippedX, bbTopLeftClippedY},
                                                     controller_zoom);
    auto bbBotRPx = projection::get().toPointWorldPx(PointWorldCoord{bbBotRClippedX, bbBotRClippedY}, controller_zoom);
*/

    // rescale
//    auto newDx = (int) std::floor(botRightWC.x() - topLeftWC.x() + 0.5);
//    auto newDy = (int) std::floor(topLeftWC.y() - botRightWC.y() + 0.5);

    auto bbTopLPx = QPointF{
            std::max(backbuffer_rect_px.topLeftPx().x(), originPix.x()),
            std::max(backbuffer_rect_px.topLeftPx().y(), originPix.y())
    };
    auto bbBotRPx = QPointF{
            std::min(backbuffer_rect_px.bottomRightPx().x(), extentPix.x()),
            std::min(backbuffer_rect_px.bottomRightPx().y(), extentPix.y())
    };

    auto newDx = (int) std::floor(bbBotRPx.x() - bbTopLPx.x() + 0.5);
    auto newDy = (int) std::floor(bbBotRPx.y() - bbTopLPx.y() + 0.5);

    auto newSrcDx = (int) std::floor(botRightC.x() - newOffsetX + 0.5);
    auto newSrcDy = (int) std::floor(botRightC.y() - newOffsetY + 0.5);

    if (newSrcDx == 0 || newSrcDy == 0) {
        qDebug() << "--- Zoom: " << controller_zoom;
        qDebug() << "TopLeft WC: " << topLeftWC << " Ds: " << topLeftDs << " RasterC: "
                 << topLeftRasterC << " Clipped: " << bbTopLPx;
        qDebug() << "BotRigh WC: " << botRightWC << " Ds: " << botRightDs << " RasterC: "
                 << botRightC << " Clipped: " << bbBotRPx;
        qDebug() << "Offset: " << newOffsetX << newOffsetY;
        qDebug() << "WARN: Sz: " << newSrcDx << newSrcDy;
        return;
    }

    if (newOffsetX != p->offsetX ||
        newOffsetY != p->offsetY ||
        newSrcDx != p->srcDx ||
        newSrcDy != p->srcDy ||
        newDx != p->dx ||
        newDy != p->dy
            ) {
        p->offsetX = newOffsetX;
        p->offsetY = newOffsetY;
        p->srcDx = newSrcDx;
        p->srcDy = newSrcDy;
        p->dx = newDx;
        p->dy = newDy;

        qDebug() << "--- Zoom: " << controller_zoom;
        qDebug() << "TopLeft WC: " << topLeftWC << " Ds: " << topLeftDs << " RasterC: "
                 << topLeftRasterC << " Clipped: " << bbTopLPx;
        qDebug() << "BotRigh WC: " << botRightWC << " Ds: " << botRightDs << " RasterC: "
                 << botRightC << " Clipped: " << bbBotRPx;
        qDebug() << "Origin: " << originPix;
        qDebug() << "Offset: " << newOffsetX << newOffsetY;
        qDebug() << "Extent: " << extentPix.rawPoint();
        qDebug() << "DSz: " << newDx << newDy;

        qDebug() << "BackBuffer: " << backbuffer_rect_px.rawRect();
        qDebug() << "Viewport: " << painter.viewport();

        // TODO fix this 0.25 factor to a programmable factor deduced from Pixel Device Ratio
        auto r = 1.0;// 0.25 * painter.viewport().width() / p->dx;
        auto sx = p->dx * r;
        auto sy = p->dy * r;
        qDebug() << "Pixmap Size: " << sx << sy;

        p->scaledPixmap = p->loadPixmap(p->offsetX, p->offsetY,
                                        p->srcDx, p->srcDy,
                                        sx, sy);
    }

    painter.drawPixmap(bbTopLPx, p->scaledPixmap);
//    painter.drawPixmap(QRect(originPix.x(), originPix.y(), p->dx, p->dy), p->scaledPixmap, QRect());

    painter.setPen(QPen(QBrush(Qt::red), 4));

    painter.drawEllipse(originPix, 20, 20);
    painter.drawEllipse(extentPix.rawPoint(), 20, 20);

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

    if (mainBand && (ct = mainBand->GetColorTable()) ) {
        imageFormat = QImage::Format::Format_Indexed8;

        if (imageColorTable.isEmpty())
        {
            auto n = ct->GetColorEntryCount();
            for(int i = 0; i < n; ++i)
            {
                GDALColorEntry entry;
                ct->GetColorEntryAsRGB(i, &entry);
                imageColorTable.push_back(qRgb(entry.c1, entry.c2, entry.c3));
            }
        }
    }

    QImage image(reinterpret_cast<uchar *>(databuf), dstSx, dstSy, imageFormat);
    if (!imageColorTable.empty()) {
        image.setColorTable(imageColorTable);
    }

    return QPixmap::fromImage(image);
}

}
