/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 * (c) 2014 Henner Zeller <h.zeller@acm.org>
 * Creative commons BY-SA
 */

#include "multi-shell-extrude.h"

#include <limits.h>

// Offset using the clipper library.
// http://www.angusj.com/delphi/clipper/documentation/Docs/Units/ClipperLib/Classes/ClipperOffset/_Body.htm

#include "third_party/clipper.hpp"

// A path is centered if the rectangle aorund it is covering the centroid.
static bool is_centered(const Vector2D &centroid, const ClipperLib::Path &path) {
  int min_x = INT_MAX, min_y = INT_MAX;
  int max_x = INT_MIN, max_y = INT_MIN;
  for (std::size_t i = 0; i < path.size(); ++i) {
    const ClipperLib::IntPoint &p = path[i];
    if (p.X < min_x) min_x = p.X;
    if (p.X > max_x) max_x = p.X;
    if (p.Y < min_y) min_y = p.Y;
    if (p.Y > max_y) max_y = p.Y;
  }
  return (min_x < centroid.x && max_x > centroid.x
          && min_y < centroid.y && max_y > centroid.y);
}

Polygon PolygonOffset(const Polygon &polygon, double offset,
                      OffsetType type) {
  // Converting float to clipper integer values. Make sure
  // to stay within limits.
  const float kAccuracy = 500;

  ClipperLib::Path path;
  Vector2D centroid;
  for (std::size_t i = 0; i < polygon.size(); ++i) {
    const Vector2D &p = polygon[i];
    Vector2D clipper_point = p * kAccuracy;
    path.push_back(ClipperLib::IntPoint(clipper_point.x, clipper_point.y));
    centroid = centroid + clipper_point;
  }
  centroid = centroid / polygon.size();

  ClipperLib::Paths solutions;
  ClipperLib::ClipperOffset co(2.0, 5); // 5/kAccuracy mm
  ClipperLib::JoinType join = ClipperLib::jtRound;
  switch (type) {
  case kOffsetRound:  join = ClipperLib::jtRound; break;
  case kOffsetSquare: join = ClipperLib::jtSquare; break;
  case kOffsetMiter:  join = ClipperLib::jtMiter; break;
  }
  co.AddPath(path, join, ClipperLib::etClosedPolygon);
  co.Execute(solutions, kAccuracy * offset);

  if (solutions.size() == 0)  // Nothing left.
    return Polygon();

  // A polygon might become pieces when offset. Use the one that is centered.
  ClipperLib::Path &centered_polygon = solutions[0];
  for (std::size_t i = 0; i < solutions.size(); ++i) {
    if (is_centered(centroid, solutions[i])) {
      centered_polygon = solutions[i];
      break;
    }
  }

  Polygon tmp;
  for (std::size_t i = 0; i < centered_polygon.size(); ++i) {
    const ClipperLib::IntPoint &p = centered_polygon[i];
    tmp.push_back(Vector2D(p.X / kAccuracy, p.Y / kAccuracy));
  }

  // The way the clipper library works, the offset polygon might start at a
  // different point - after all, it is a different polygon.
  // Let's try to find the one that is closest to the start of the input polygon
  // by looking for the closest point to the line represented by the start-vector,
  // i.e. closest point in the same direction.
  const Vector2D &reference = polygon[0];
  double smallest = -1;
  std::size_t offset_index = 0;
  for (std::size_t i = 0; i < tmp.size(); ++i) {
    const Vector2D &p = tmp[i];
#if 0
    // TODO: find center and make line relative to that.
    // Then use http://en.wikipedia.org/wiki/Distance_from_a_point_to_a_line#Line_defined_by_two_points
    const double dist = abs(reference.y * p.x - reference.x * p.y);
    const bool same_side = (reference.x * p.x >= 0) && (reference.y * p.y >= 0);
#else
    const double dist = distance(p.x - reference.x, p.y - reference.y, 0);
    const bool same_side = true;
#endif
    if (i == 0 || (dist < smallest && same_side)) {
      offset_index = i;
      smallest = dist;
    }
  }

  // .. then create the result by shifting that.
  Polygon result;
  for (std::size_t i = 0; i < tmp.size(); ++i) {
    result.push_back(tmp[(i + offset_index) % tmp.size()]);
  }
  return result;
}
