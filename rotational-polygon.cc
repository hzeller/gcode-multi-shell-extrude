/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 * (c) 2014 Henner Zeller <h.zeller@acm.org>
 * Creative commons BY-SA
 */
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "multi-shell-extrude.h"

namespace {
  class PolarFunction {
  public:
    // Initialization string corresponds to the linear rolled out
    // circumreference 'dents'.
    PolarFunction(const char *fun_init) {
      const unsigned char *init = (const unsigned char*) fun_init;
      int min = 0x100;
      int max = -1;
      for (const unsigned char *s = init; *s; ++s) {
        if (*s < min) min = *s;
        if (*s > max) max = *s;
      }
      double range = max - min;
      for (const unsigned char *s = init; *s; ++s) {
        if (range > 0)
          values_.push_back((*s - min) / range);
        else
          values_.push_back(0);
      }
    }

    // phi is fraction of 2PI, i.e. 0 = start, 1 = one turn.
    double value(double phi) {
      const int n = phi * values_.size();
      assert(n < (int) values_.size());
      // linear interpolation between this and the next value.
      const double a = values_[n];
      const double b = values_[(n+1) % values_.size()];
      double fraction = phi * values_.size() - n;
      return a + (b - a) * fraction;
    }

  private:
    std::vector<double> values_;
  };
}  // namespace

static int quantize_up(int x, int q) {
  return q * ((x + q - 1) / q);
}

static double AngleTwist(double twist, double r, double max_r) {
  return twist * r / max_r;
}

Polygon RotationalPolygon(const char *fun_init, double inner_radius,
			  double thread_depth, double twist) {
  Polygon result;
  const double max_r = inner_radius + thread_depth;
  PolarFunction fun(fun_init);
  const double max_error = 0.15 / 2;  // maximum error to tolerate
  // Maximum lenght of one edge of our cylinder, that should not differ more
  // than max_error in the middle. Half a segment is a nice perpendicular
  // triangle
  const double half_segment = sqrt((max_r*max_r)
                                   - (max_r - max_error)*(max_r - max_error));
  int faces = ceil((2 * M_PI * max_r) / (2 * half_segment));
  faces = quantize_up(faces, strlen(fun_init));   // same sampling per letter.
  if (fabs(twist) > 0.05) {
    faces *= 4;  // when twisting, we do more. TODO: calculate better.
  }
  for (int f = 0; f < faces; ++f) {
    const double angle = 1.0 * f / faces;
    double pol_value = fun.value(angle);
    const double r = inner_radius + thread_depth * pol_value;
    const double x = r * cos((angle + AngleTwist(twist, r, max_r)) * 2 * M_PI);
    const double y = r * sin((angle + AngleTwist(twist, r, max_r)) * 2 * M_PI);
    result.push_back(Point2D(x, y));
  }
  return result;
}
