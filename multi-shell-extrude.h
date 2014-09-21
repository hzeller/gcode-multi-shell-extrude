/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 * (c) 2014 Henner Zeller <h.zeller@acm.org>
 * Creative commons BY-SA
 */
#ifndef MULTI_SHELL_EXTRUDE_H_
#define MULTI_SHELL_EXTRUDE_H_

#include <vector>
#include <math.h>

struct Vector2D {
  Vector2D() : x(0), y(0) {}
  Vector2D(double xx, double yy) : x(xx), y(yy){}
  double x, y;
};
typedef std::vector<Vector2D> Polygon;

// Calculate euclidian distance.
inline double distance(double dx, double dy, double dz) {
  return sqrt(dx*dx + dy*dy + dz*dz);
}

// Create a polygon from a string "fun_init", describing "thread_depth"
// offsets from an "inner_radius". In rotational-polygon.cc
Polygon RotationalPolygon(const char *fun_init, double inner_radius,
			  double thread_depth, double twist);

// Offset an polygon. Minkowski with disk of radius "offset".
// The actual Minkowski sum would have arc segments, that is flattened as
// line segments. In polygon-offset.cc
enum OffsetType { kOffsetRound, kOffsetSquare, kOffsetMiter };
Polygon PolygonOffset(const Polygon &in, double offset,
                      OffsetType type = kOffsetRound);

#endif  // MULTI_SHELL_EXTRUDE_H_
