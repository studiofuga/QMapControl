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

#pragma once

// STD includes.
#include <memory>
#include <set>
#include <vector>

// Local includes.
#include "qmapcontrol_global.h"
#include "Point.h"

/*!
 * @author Chris Stylianou <chris5287@gmail.com>
 */
namespace qmapcontrol
{
    class Geometry;

    /*!
     * Based on: http://en.wikipedia.org/wiki/Quadtree
     */    
    class QMAPCONTROL_EXPORT QuadTreeContainer
    {
    public:
        //! Constuctor.
        /*!
         * Quad Tree Container constructor.
         * @param capacity The number of items this quad tree container can store before it's children are created/used.
         * @param boundary_coord The bounding box area that this quad tree container covers in coordinates.
         */
        QuadTreeContainer(const size_t& capacity, const RectWorldCoord& boundary_coord);

        //! Disable copy constructor.
        ///QuadTreeContainer(const QuadTreeContainer&) = delete; @todo re-add once MSVC supports default/delete syntax.

        //! Disable copy assignment.
        ///QuadTreeContainer& operator=(const QuadTreeContainer&) = delete; @todo re-add once MSVC supports default/delete syntax.

        //! Destructor.
        virtual ~QuadTreeContainer() { } /// = default; @todo re-add once MSVC supports default/delete syntax.

        /*!
         * Fetches objects within the specified bounding box range.
         * @param return_points The objects that are within the specified range are added to this.
         * @param range_coord The bounding box range.
         */
        void query(std::set<std::shared_ptr<Geometry>>& return_points, const RectWorldCoord& range_coord) const;

        /*!
         * Inserts an object into the quad tree container.
         * @param point_coord The objects's point in coordinates.
         * @param object The object to insert.
         * @return whether the object was inserted into this quad tree container.
         */
        bool insert(const PointWorldCoord& point_coord, const std::shared_ptr<Geometry>& object);

        /*!
         * Removes an object from the quad tree container.
         * @param point_coord The objects's point in coordinates.
         * @param object The object to remove.
         */
        void erase(const PointWorldCoord& point_coord, const std::shared_ptr<Geometry>& object);

        /*!
         * Removes all objects from the quad tree container.
         */
        void clear();

    private:
        //! Disable copy constructor.
        QuadTreeContainer(const QuadTreeContainer&); /// @todo remove once MSVC supports default/delete syntax.

        //! Disable copy assignment.
        QuadTreeContainer& operator=(const QuadTreeContainer&); /// @todo remove once MSVC supports default/delete syntax.

        /*!
         * Creates the child nodes.
         */
        void subdivide();

    private:
        /// Quad tree node capacity.
        const size_t m_capacity;

        /// Boundary of this quad tree node.
        const RectWorldCoord m_boundary_coord;

        /// Points in this quad tree node.
        std::vector<std::pair<PointWorldCoord, std::shared_ptr<Geometry>>> m_points;

        /// Child: north east quad tree node.
        std::unique_ptr<QuadTreeContainer> m_child_north_east;

        /// Child: north west quad tree node.
        std::unique_ptr<QuadTreeContainer> m_child_north_west;

        /// Child: south east quad tree node.
        std::unique_ptr<QuadTreeContainer> m_child_south_east;

        /// Child: south west quad tree node.
        std::unique_ptr<QuadTreeContainer> m_child_south_west;
    };
}
