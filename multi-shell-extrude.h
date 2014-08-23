/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 * (c) 2014 Henner Zeller <h.zeller@acm.org>
 * Creative commons BY-SA
 */
#ifndef MULTI_SHELL_EXTRUDE_H_
#define MULTI_SHELL_EXTRUDE_H_

#include <vector>

struct Point {
  Point() : x(0), y(0) {}
  Point(double xx, double yy) : x(xx), y(yy){}
  double x, y;
};
typedef std::vector<Point> Polygon;

// Create a polygon
Polygon RotationalPolygon(const char *fun_init, double inner_radius,
			  double thread_depth, double twist);

#endif  // MULTI_SHELL_EXTRUDE_H_
