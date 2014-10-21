/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 * (c) 2014 Henner Zeller <h.zeller@acm.org>
 * Creative commons BY-SA
 */

#include "multi-shell-extrude.h"

Vector2D Centroid(const Polygon &polygon) {
    Vector2D result;
    for (Polygon::size_type i = 0; i < polygon.size(); ++i) {
        result = result + polygon[i];
    }
    return result / polygon.size();
}
