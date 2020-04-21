/*
*
* This file is part of QMapControl,
* an open-source cross-platform map widget
*
* Copyright (C) 2007 - 2008 Kai Winter
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with QMapControl. If not, see <http://www.gnu.org/licenses/>.
*
* Contact e-mail: kaiwinter@gmx.de
* Program URL   : http://qmapcontrol.sourceforge.net/
*
*/

#include "ProjectionWorldMercator.h"
#include "ImageManager.h"
#include <cmath>

namespace qmapcontrol
{
    int ProjectionWorldMercator::tilesX(const int zoom) const
    {
        // Return the number of tiles for the x-axis.
        return std::pow(2, zoom);
    }

    int ProjectionWorldMercator::tilesY(const int zoom) const
    {
        // Return the number of tiles for the y-axis.
        return std::pow(2, zoom);
    }

    constexpr double kEarthMajor = 6378137.0;     // WGS84 semi-major axis
    constexpr double kEarthMinor = 6356752.3142;  // WGS84 semi-minor axis
    constexpr double kRatio = kEarthMinor / kEarthMajor;
    static const double kEccentricity = sqrt(1 - kRatio * kRatio);
    constexpr double kOriginShift = 2 * M_PI * kEarthMajor / 2;

    constexpr double RAD2DEG = 180.0 / M_PI;
    constexpr double DEG2RAD = M_PI / 180.0;

    double ProjectionWorldMercator::resolution(int tile_size_px, int zoom) const
    {
        const double initialResolution = 2 * M_PI * kEarthMajor / tile_size_px;
        return initialResolution / std::pow(2, zoom);
    }

    PointWorldPx ProjectionWorldMercator::toPointWorldPx(const PointWorldCoord& point_coord, const int zoom) const
    {
        const int tile_size_px = ImageManager::get().tileSizePx();

        // latlon -> meters
        double lat_rad = point_coord.latitude() * DEG2RAD;
        double con = kEccentricity * sin(lat_rad);
        double ts = tan(M_PI/4 - lat_rad / 2) / pow((1 - con) / (1 + con), kEccentricity / 2);

        double x_m = point_coord.longitude() * DEG2RAD * kEarthMajor;
        double y_m = -kEarthMajor * log(std::max(ts, 1e-10));

        // meters -> pixels
        const double res = resolution(tile_size_px, zoom);

        double x_px = (x_m + kOriginShift) / res;
        double y_px = (kOriginShift - y_m) / res;

        return PointWorldPx(x_px, y_px);
    }

    PointWorldCoord ProjectionWorldMercator::toPointWorldCoord(const PointWorldPx& point_px, const int zoom) const
    {
        const int tile_size_px = ImageManager::get().tileSizePx();
        // pixels -> meters
        const double res = resolution(tile_size_px, zoom);

        double x_m = point_px.x() * res - kOriginShift;
        double y_m = kOriginShift - point_px.y() * res;

        // meters -> latlon
        double ts = exp(-y_m / kEarthMajor);
        double lat_rad = M_PI/2 - 2 * atan(ts);

        double dphi = 0.1;
        for (int i = 0; i < 15 && std::abs(dphi) > 1e-7; i++) {
            double con = kEccentricity * sin(lat_rad);
            con = pow((1 - con) / (1 + con), kEccentricity / 2);
            dphi = M_PI/2 - 2 * atan(ts * con) - lat_rad;
            lat_rad += dphi;
        }

        double lon_rad = x_m / kEarthMajor;
        return PointWorldCoord(lon_rad * RAD2DEG, lat_rad * RAD2DEG);
    }
}
