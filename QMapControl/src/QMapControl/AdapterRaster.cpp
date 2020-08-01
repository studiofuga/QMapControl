//
// Created by happycactus on 01/08/20.
//

#include "AdapterRaster.h"

namespace qmapcontrol {

struct AdapterRaster::Impl {
    GDALDataset *ds = nullptr;
    std::string layername;
};

AdapterRaster::AdapterRaster(GDALDataset *datasource, std::string layer_name, QObject *parent)
        : QObject(parent), p(spimpl::make_unique_impl<Impl>())
{
    p->ds = datasource;
    p->layername = std::move(layer_name);
}


}