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

#include "QuadTreeContainer.h"
#include "Geometry.h"

namespace qmapcontrol
{
    QuadTreeContainer::QuadTreeContainer(const size_t& capacity, const RectWorldCoord& boundary_coord)
        : m_capacity(capacity),
          m_boundary_coord(boundary_coord)
    {
        // Reserve the container size.
        m_points.reserve(capacity);
    }

    void QuadTreeContainer::query(std::set<std::shared_ptr<Geometry>>& return_points, const RectWorldCoord& range_coord) const
    {
        // Does the range intersect with our boundary.
        if (range_coord.rawRect().intersects(m_boundary_coord.rawRect()))
        {
            // Check whether any of our points are contained in the range.
            for (const auto& point : m_points)
            {
                const auto& geometry = point.second;

                // Is the point contained by the query range.
                if (range_coord.rawRect().contains(point.first.rawPoint()))
                {
                    // Add to the return points.
                    return_points.insert(geometry);
                } else if ((geometry->geometryType() == Geometry::GeometryType::GeometryLineString)
                           || (geometry->geometryType() == Geometry::GeometryType::GeometryPolygon)) {
                    // For lines and polygons we also need to check whole bounds
                    // not just individual constituent points.
                    const QRectF bbox = geometry->boundingBox(0).rawRect().normalized();
                    if (range_coord.rawRect().normalized().intersects(bbox)) {
                        return_points.insert(geometry);
                    }
                }
            }

            // Do we have any child quad tree nodes?
            if (m_child_north_east != nullptr)
            {
                // Search each child and add the points they return.
                m_child_north_east->query(return_points, range_coord);
                m_child_north_west->query(return_points, range_coord);
                m_child_south_east->query(return_points, range_coord);
                m_child_south_west->query(return_points, range_coord);
            }
        }
    }

    bool QuadTreeContainer::insert(const PointWorldCoord& point_coord, const std::shared_ptr<Geometry>& object)
    {
        // Keep track of our success.
        bool success(false);

        // Does this boundary contain the point?
        if (m_boundary_coord.rawRect().contains(point_coord.rawPoint()))
        {
            // Have we reached our capacity?
            if (m_points.size() < m_capacity)
            {
                // Add the point.
                m_points.emplace_back(point_coord, object);

                // Update our success.
                success = true;
            }
            else
            {
                // Do we already have child quad tree nodes?
                if (m_child_north_east == nullptr)
                {
                    // We need to create the child quad tree nodes before we continue.
                    subdivide();
                }

                // Try inserting into north east.
                if (m_child_north_east->insert(point_coord, object))
                {
                    // Update our success.
                    success = true;
                }
                // Try inserting into north west.
                else if (m_child_north_west->insert(point_coord, object))
                {
                    // Update our success.
                    success = true;
                }
                // Try inserting into south east.
                else if (m_child_south_east->insert(point_coord, object))
                {
                    // Update our success.
                    success = true;
                }
                // Try inserting into south west.
                else if(m_child_south_west->insert(point_coord, object))
                {
                    // Update our success.
                    success = true;
                }
                else
                {
                    // We cannot insert, fail!
                    throw std::runtime_error("Unable to insert into quad tree container.");
                }
            }
        }

        // Return our success.
        return success;
    }

    void QuadTreeContainer::erase(const PointWorldCoord& point_coord, const std::shared_ptr<Geometry>& object)
    {
        // Does this boundary contain the point?
        if(m_boundary_coord.rawRect().contains(point_coord.rawPoint()))
        {
            // Check whether any of our points are contained in the range.
            auto itr_point = m_points.begin();
            while(itr_point != m_points.end())
            {
                // Have we found the object?
                if(itr_point->second == object)
                {
                    // Remove the object from the container.
                    itr_point = m_points.erase(itr_point);
                }
                else
                {
                    // Move on to the next point.
                    ++itr_point;
                }
            }

            // Do we have child quad tree nodes?
            if(m_child_north_east != nullptr)
            {
                // Search each child and remove the object if found.
                m_child_north_east->erase(point_coord, object);
                m_child_north_west->erase(point_coord, object);
                m_child_south_east->erase(point_coord, object);
                m_child_south_west->erase(point_coord, object);
            }
        }
    }

    void QuadTreeContainer::clear()
    {
        // Clear the points.
        m_points.clear();

        // Reset the child nodes.
        m_child_north_east.reset(nullptr);
        m_child_north_west.reset(nullptr);
        m_child_south_east.reset(nullptr);
        m_child_south_west.reset(nullptr);
    }

    void QuadTreeContainer::subdivide()
    {
        // Calculate half the size of the boundary.
        const QSizeF half_size = m_boundary_coord.rawRect().size() / 2.0;

        // Construct the north east child.
        const RectWorldCoord north_east(PointWorldCoord(m_boundary_coord.rawRect().left() + half_size.width(), m_boundary_coord.rawRect().top()), half_size);
        m_child_north_east.reset(new QuadTreeContainer(m_capacity, north_east));

        // Construct the north west child.
        const RectWorldCoord north_west(PointWorldCoord(m_boundary_coord.rawRect().left(), m_boundary_coord.rawRect().top()), half_size);
        m_child_north_west.reset(new QuadTreeContainer(m_capacity, north_west));

        // Construct the south east child.
        const RectWorldCoord south_east(PointWorldCoord(m_boundary_coord.rawRect().left() + half_size.width(), m_boundary_coord.rawRect().top() + half_size.height()), half_size);
        m_child_south_east.reset(new QuadTreeContainer(m_capacity, south_east));

        // Construct the south west child.
        const RectWorldCoord south_west(PointWorldCoord(m_boundary_coord.rawRect().left(), m_boundary_coord.rawRect().top() + half_size.height()), half_size);
        m_child_south_west.reset(new QuadTreeContainer(m_capacity, south_west));
    }
}
