//
// Created by happycactus on 01/08/20.
//

#ifndef QMAPCONTROL_ADAPTERRASTER_H
#define QMAPCONTROL_ADAPTERRASTER_H

#include "utils/spimpl.h"

#include <QObject>
#include <gdal_priv.h>

namespace qmapcontrol {

class AdapterRaster : public QObject {
Q_OBJECT
    struct Impl;
    spimpl::unique_impl_ptr<Impl> p;
public:

    explicit AdapterRaster(GDALDataset *datasource, std::string layer_name, QObject *parent = nullptr);
};

} // namespace qmapcontrol

#endif // QMAPCONTROL_ADAPTERRASTER_H
