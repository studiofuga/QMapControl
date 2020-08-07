//
// Created by happycactus on 07/08/20.
//

#include "RasterUtils.h"

#include <gdal_priv.h>
#include <QDebug>

QImage RasterUtils::imageFromRaster(GDALDataset *dataset)
{
    int rows = dataset->GetRasterYSize();
    int cols = dataset->GetRasterXSize();
    int channels = dataset->GetRasterCount();

    qDebug() << "Raster x,y,channels: " << rows << cols << channels;

    int scanlineSize = std::ceil(3.0 * cols / 4.0) * 4;

    qDebug() << "Scanline: " << scanlineSize;

    char *data = new char[rows * scanlineSize];

    // Iterate over each channel
    std::vector<int> bands(channels);
    std::iota(bands.begin(), bands.end(), 1);

    auto conversionResult = dataset->RasterIO(GF_Read, 0, 0,
                                              cols, rows,
                                              data,
                                              cols, rows,
                                              GDT_Byte,
                                              3, bands.data(),
                                              3,            // pixelspace
                                              scanlineSize, // linespace
                                              1,            // bandspace
                                              nullptr);

    if (conversionResult != CE_None) {
        qWarning() << "GetRaster returned " << conversionResult;
        return QImage{};
    }

    QImage image(reinterpret_cast<uchar *>(data), cols, rows, QImage::Format::Format_RGB888);

//    image.save("qimage.png");
    return image;
}
