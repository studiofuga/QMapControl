//
// Created by happycactus on 19/11/20.
//

#ifndef QMAPCONTROL_PALGEOMETRY_H
#define QMAPCONTROL_PALGEOMETRY_H

#include <geos_c.h>

#include <pal/palgeometry.h>

class OGRPolygon;

class PalGeometry : public pal::PalGeometry {

private:
    GEOSGeometry *the_geom;
    int useCounter;

    static GEOSWKTReader *reader;

public:
    explicit PalGeometry(OGRPolygon *polygon);

    explicit PalGeometry(const char *wkt);

    virtual ~PalGeometry();

    GEOSGeometry *getGeosGeometry();

    void releaseGeosGeometry(GEOSGeometry *the_geom);
};


#endif //QMAPCONTROL_PALGEOMETRY_H
