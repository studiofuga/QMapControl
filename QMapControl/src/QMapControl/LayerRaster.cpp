//
// Created by happycactus on 01/08/20.
//

#include "LayerRaster.h"

namespace qmapcontrol {

struct LayerRaster::Impl {
    std::string layerName;
};

LayerRaster::LayerRaster(std::string layername)
        : Layer(Layer::LayerType::LayerUnknown, layername), p(spimpl::make_unique_impl<Impl>())
{
    p->layerName = std::move(layername);
}

void LayerRaster::addRaster(std::shared_ptr<AdapterRaster> adapter)
{

}

void LayerRaster::draw(QPainter &painter, const RectWorldPx &backbuffer_rect_px, const int &controller_zoom) const
{

}

bool LayerRaster::mousePressEvent(const QMouseEvent *mouse_event, const PointWorldCoord &mouse_point_coord,
                                  const int &controller_zoom) const
{
    return false;
}

}