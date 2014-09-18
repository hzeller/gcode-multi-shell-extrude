/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 * (c) 2014 Henner Zeller <h.zeller@acm.org>
 * Creative commons BY-SA
 */

#include "multi-shell-extrude.h"

#include <limits.h>

// Offset using the clipper library.
// http://www.angusj.com/delphi/clipper/documentation/Docs/Units/ClipperLib/Classes/ClipperOffset/_Body.htm

#include "third_party/clipper.hpp"

// A path is centered if the rectangle aorund it is covering the origin.
static bool is_centered(const ClipperLib::Path &path) {
  int min_x = INT_MAX, min_y = INT_MAX;
  int max_x = INT_MIN, max_y = INT_MIN;
  for (std::size_t i = 0; i < path.size(); ++i) {
    const ClipperLib::IntPoint &p = path[i];
    if (p.X < min_x) min_x = p.X;
    if (p.X > max_x) max_x = p.X;
    if (p.Y < min_y) min_y = p.Y;
    if (p.Y > max_y) max_y = p.Y;
  }
  return (min_x < 0 && max_x > 0 && min_y < 0 && max_y > 0);
}

Polygon PolygonOffset(const Polygon &polygon, double offset) {
  ClipperLib::Path path;
  for (std::size_t i = 0; i < polygon.size(); ++i) {
    const Point2D &p = polygon[i];
    path.push_back(ClipperLib::IntPoint(100 * p.x, 100 * p.y));
  }

  ClipperLib::Paths solutions;
  ClipperLib::ClipperOffset co(2.0, 5); // 5/100mm = 1/20mm
  co.AddPath(path, ClipperLib::jtRound, ClipperLib::etClosedPolygon);
  co.Execute(solutions, 100.0 * offset);

  Polygon result;
  // A polygon might become pieces when offset. Use the one that is centered.
  ClipperLib::Path &centered_polygon = solutions[0];
  for (std::size_t i = 0; i < solutions.size(); ++i) {
    if (is_centered(solutions[i])) {
      centered_polygon = solutions[i];
      break;
    }
  }
  for (std::size_t i = 0; i < centered_polygon.size(); ++i) {
    const ClipperLib::IntPoint &p = centered_polygon[i];
    result.push_back(Point2D(p.X / 100.0, p.Y / 100.0));
  }
  return result;
}
