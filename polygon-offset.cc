/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 * (c) 2014 Henner Zeller <h.zeller@acm.org>
 * Creative commons BY-SA
 */

#include "multi-shell-extrude.h"

Polygon PolygonOffset(const Polygon &polygon, double offset) {
    Polygon result;
    for (std::size_t i = 0; i < polygon.size(); ++i) {
        const Point &p = polygon[i];
        double from_center = distance(p.x, p.y, 0);
        double stretch = (from_center + offset) / from_center;
        result.push_back(Point(p.x * stretch, p.y * stretch));
    }
    return result;
}
