//
// Created by happycactus on 19/11/20.
//

#include "PalGeometry.h"

#include <pal/palgeometry.h>
#include <pal/label.h>

#include <ogr_p.h>
#include <geos_c.h>

#include <string>
#include <iostream>

#include <sstream>

GEOSWKTReader *PalGeometry::reader = nullptr;

PalGeometry::PalGeometry(OGRPolygon *poly)
{
    useCounter = 0;

    if (reader == nullptr) {
        reader = GEOSWKTReader_create();
    }

    char *wkt;
    poly->exportToWkt(&wkt);
    the_geom = GEOSWKTReader_read(reader, wkt);
    delete wkt;
}

PalGeometry::PalGeometry(const char *wkt)
{
    useCounter = 0;

    if (reader == nullptr) {
        reader = GEOSWKTReader_create();
    }

    the_geom = GEOSWKTReader_read(reader, wkt);
    delete wkt;
}

PalGeometry::~PalGeometry()
{
    if (useCounter > 0) {
        throw std::logic_error("Invalid useCounter when destroying PalGeometry");
    }

    GEOSGeom_destroy(the_geom);
}

/**
* \brief get the geometry in WKB hexa format
* This method is called by Pal each time it needs a geom's coordinates
* \return WKB Hex buffer
*/
GEOSGeometry *PalGeometry::getGeosGeometry()
{
    ++useCounter;
    return the_geom;
}


/**
* \brief Called by Pal when it doesn't need the coordinates anymore
*/
void PalGeometry::releaseGeosGeometry(GEOSGeometry *the_geom)
{
    --useCounter;
}
