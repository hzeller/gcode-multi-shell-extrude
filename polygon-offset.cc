/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 * (c) 2014 Henner Zeller <h.zeller@acm.org>
 * Creative commons BY-SA
 */

#include "multi-shell-extrude.h"

// Offset using the clipper library.
// http://www.angusj.com/delphi/clipper/documentation/Docs/Units/ClipperLib/Classes/ClipperOffset/_Body.htm

#include "third_party/clipper.hpp"

Polygon PolygonOffset(const Polygon &polygon, double offset) {
  if (fabs(offset) < 0.01)
    return polygon;

  ClipperLib::Path path;
  for (std::size_t i = 0; i < polygon.size(); ++i) {
    const Point &p = polygon[i];
    path.push_back(ClipperLib::IntPoint(100 * p.x, 100 * p.y));
  }

  ClipperLib::Paths solutions;
  ClipperLib::ClipperOffset co(2.0, 5); // 5/100mm = 1/20mm
  co.AddPath(path, ClipperLib::jtRound, ClipperLib::etClosedPolygon);
  co.Execute(solutions, 100.0 * offset);

  Polygon result;
  // We essentially look at the first solution.
  ClipperLib::Path &cp_solution = solutions[0];
  for (std::size_t i = 0; i < cp_solution.size(); ++i) {
    const ClipperLib::IntPoint &p = cp_solution[i];
    result.push_back(Point(p.X / 100.0, p.Y / 100.0));
  }
  return result;
}
