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
  float magnitude() const { return sqrt(x*x + y*y); }

  double x, y;
};
typedef std::vector<Vector2D> Polygon;

inline Vector2D operator+(const Vector2D &a, const Vector2D &b) {
  return Vector2D(a.x + b.x, a.y + b.y);
}
inline Vector2D operator-(const Vector2D &a, const Vector2D &b) {
  return Vector2D(a.x - b.x, a.y - b.y);
}
inline Vector2D operator*(const Vector2D &a, float factor) {
  return Vector2D(a.x * factor, a.y * factor);
}
inline Vector2D operator/(const Vector2D &a, float div) {
  return Vector2D(a.x / div, a.y / div);
}
inline Vector2D rotate(const Vector2D &v, float angle) {
  return Vector2D(v.x * cos(angle) - v.y * sin(angle),
                  v.y * cos(angle) + v.x * sin(angle));
}

// Calculate euclidian distance.
inline double distance(double dx, double dy, double dz) {
  return sqrt(dx*dx + dy*dy + dz*dz);
}

// Determine the centroid for polygon.
Vector2D Centroid(const Polygon &polygon);

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
