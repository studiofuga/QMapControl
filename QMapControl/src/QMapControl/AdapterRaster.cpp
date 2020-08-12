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
    OGRCoordinateTransformation *mInvTransformation;

    double geoTransformMatrix[6];
    double xPixFactor, yPixFactor;
    int xSize, ySize;
    PointWorldCoord eW;
    int currentZoomFactor = -1;

    int channels = 0;
    int cols = 0;
    int rows = 0;
    int offsetX, offsetY;
    char *databuf = nullptr;
    size_t dataSize = 0;
    QVector<QRgb> imageColorTable;

    QPixmap loadPixmap(size_t offX, size_t offY, size_t szX, size_t szY, size_t dstSx, size_t dstSy);

    QPointF toWorldCoordinates(QPointF rc)
    {
        return QPointF{
                geoTransformMatrix[0] + geoTransformMatrix[1] * rc.x() + geoTransformMatrix[2] * rc.y(),
                geoTransformMatrix[3] + geoTransformMatrix[4] * rc.x() + geoTransformMatrix[5] * rc.y()
        };
    }

    // Simplified version
    QPointF toRasterCoordinates(QPointF wc)
    {
        return QPointF{
                (wc.x() - geoTransformMatrix[0]) / geoTransformMatrix[1],
                (wc.y() - geoTransformMatrix[3]) / geoTransformMatrix[5]
        };
    }

    QPointF inverseTransform(QPointF pt)
    {
        double x = pt.x();
        double y = pt.y();
        mInvTransformation->Transform(1, &x, &y);
        return QPointF{x, y};
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
        p->xSize = p->ds->GetRasterXSize();
        p->ySize = p->ds->GetRasterYSize();
        double x = p->geoTransformMatrix[0];
        double y = p->geoTransformMatrix[3];
        p->mTransformation->Transform(1, &x, &y);

        double x2 = p->geoTransformMatrix[0] + p->xSize * p->geoTransformMatrix[1] + p->ySize * p->geoTransformMatrix[2];
        double y2 =p->geoTransformMatrix[3] + p->xSize * p->geoTransformMatrix[4] + p->ySize * p->geoTransformMatrix[5];
        p->mTransformation->Transform(1, &x2, &y2);

        if (x2 < x) {
            std::swap(x2, x);
        }
        if (y2 > y) {
            std::swap(y2, y);
        }

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

    auto topLeftWC = projection::get().toPointWorldCoord(backbuffer_rect_px.topLeftPx(), controller_zoom).rawPoint();
    auto botRightWC = projection::get().toPointWorldCoord(backbuffer_rect_px.bottomRightPx(),
                                                          controller_zoom).rawPoint();


    auto botRightC = p->toRasterCoordinates(p->inverseTransform(botRightWC));
    auto topLeftRasterC = p->toRasterCoordinates(p->inverseTransform(topLeftWC));

    qDebug() << "TopLeft WC: " << topLeftWC << " RasterC: " << topLeftRasterC;
    qDebug() << "BotRigh WC: " << botRightWC << " RasterC: " << botRightC;

    auto originPix = projection::get().toPointWorldPx(p->originWorld, controller_zoom).rawPoint();
    qDebug() << "Origin: " << originPix;

    p->offsetX = topLeftRasterC.x() - originPix.x();
    p->offsetY = topLeftRasterC.y() - originPix.y();

    if (p->offsetX < 0) { p->offsetX = 0; }
    if (p->offsetY < 0) { p->offsetY = 0; }

    qDebug() << "Offset: " << p->offsetX << p->offsetY;

    auto extentPix = projection::get().toPointWorldPx(p->eW, controller_zoom);

    qDebug() << "Extent: " << extentPix.rawPoint();

    auto lx = botRightWC.x() - extentPix.x();
    auto ly = botRightWC.y() - extentPix.y();

    qDebug() << "lx: " << lx << " ly: " << ly;

    if (lx < 0) { lx = 0; }
    if (ly < 0) { ly = 0; }

    if (p->currentZoomFactor != controller_zoom) {
        // rescale
        auto opsX = p->xSize - p->offsetX - lx;
        auto opsY = p->ySize - p->offsetY - ly;

        qDebug() << "Size: " << p->xSize << " ds: " << opsX;
        qDebug() << "Size: " << p->ySize << " ds: " << opsY;

        auto dx = extentPix.x() - originPix.x();
        auto dy = extentPix.y() - originPix.y();

        p->scaledPixmap = p->loadPixmap(p->offsetX, p->offsetY,
                                        opsX, opsY,
                                        dx, dy
        );
        p->currentZoomFactor = controller_zoom;
    }

    painter.drawPixmap(originPix, p->scaledPixmap);
}

QPixmap
AdapterRaster::Impl::loadPixmap(size_t srcX, size_t srcY, size_t srcSx, size_t srcSy, size_t dstSx, size_t dstSy)
{
    qDebug() << "Loading. Offset: " << srcX << srcY << " sz " << srcSx << srcSy;
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

    int scanlineSize = std::ceil(static_cast<float>(channels) * dstSy / 4.0) * 4;

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
