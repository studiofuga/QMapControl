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

#include "MapAdapterOSM.h"

namespace qmapcontrol
{
    namespace {
        const std::pair<QUrl, int> getTileServerInfo(const MapAdapterOSM::TileServer tileServer)
        {
            const std::map<MapAdapterOSM::TileServer, std::pair<QUrl, int>> tileServers = {
                { MapAdapterOSM::TileServer::OpenStreetMap, { QUrl("http://tile.openstreetmap.org/%zoom/%x/%y.png"),       19 }},
                { MapAdapterOSM::TileServer::OpenTopoMap,   { QUrl("https://tile.opentopomap.org/%zoom/%x/%y.png"),        17 }},
                { MapAdapterOSM::TileServer::OpenCycleMap,  { QUrl("http://tile.thunderforest.com/cycle/%zoom/%x/%y.png"), 20 }},
                { MapAdapterOSM::TileServer::StamenToner,   { QUrl("http://tile.stamen.com/toner/%zoom/%x/%y.png"),        20 }},
            };

            return tileServers.at(tileServer);
        }
    }

    MapAdapterOSM::MapAdapterOSM(TileServer tileServer, QObject* parent)
        : MapAdapterOSM(getTileServerInfo(tileServer), parent)
    {
    }

    MapAdapterOSM::MapAdapterOSM(const std::pair<QUrl, int>& server, QObject* parent)
        : MapAdapterTile(server.first, { projection::EPSG::SphericalMercator }, 0, server.second, 0, false, parent)
    {
    }
}
