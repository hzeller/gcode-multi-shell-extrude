/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 * (c) 2014 Henner Zeller <h.zeller@acm.org>
 * Creative commons BY-SA
 */

#include "printer.h"

#include <stdio.h>
#include <stdarg.h>

#include "multi-shell-extrude.h"  // for distance()

namespace {
class GCodePrinter : public Printer {
  static const double kRetractAmount = 2;
public:
  GCodePrinter(double extrusion_factor)
    : filament_extrusion_factor_(extrusion_factor), extrude_dist_(0) {}

  virtual void Preamble(double machine_limit_x, double machine_limit_y,
                        double feed_mm_per_sec) {
    printf("; G-Code\n\n");
  }

  virtual void Init(double machine_limit_x, double machine_limit_y,
                    double feed_mm_per_sec) {
    printf("G28\nG1 F%.1f\n", feed_mm_per_sec * 60);
    printf("G1 X150 Y10 Z30\n");
    printf("M109 S190\nM116\n");
    printf("M82 ; absolute extrusion\n");
    printf("G1 E5\n"); // squirt out stuff.
    printf("G92 E0\n; test extrusion...\n");
    const double test_extrusion_from = 0.8 * machine_limit_x;
    const double test_extrusion_to = 0.2 * machine_limit_x;
    const double length = test_extrusion_from - test_extrusion_to;
    printf("G1 X%.1f Y10 Z0\nG1 X%.1f Y10 E%.3f F1000\n",
           test_extrusion_from, test_extrusion_to,
           length * filament_extrusion_factor_);
    printf("M83\nG1 E%.1f ; retract\nM82\n", -kRetractAmount); // in rel mode.
    printf("G1 Z5\n");
  }
  virtual void Postamble() {
    printf("M104 S0 ; hotend off\n");
    printf("M106 S0 ; fan off\n");
    printf("G28 X0 Y0\n");
    printf("M84\n");
  }
  virtual double GetExtrusionDistance() { return extrude_dist_; }
  virtual void Comment(const char *fmt, ...) {
    printf("; ");
    va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap);
  }

  virtual void SetSpeed(double feed_mm_per_sec) {
    printf("G1 F%.1f  ; feedrate=%.1fmm/s\n", feed_mm_per_sec * 60,
           feed_mm_per_sec);
  }
  virtual void GoZPos(double z) {
    printf("G1 Z%.3f\n", z);
  }
  virtual void MoveTo(double x, double y, double z) {
    printf("G1 X%.3f Y%.3f Z%.3f\n", x, y, z);
    last_x = x; last_y = y; last_z = z;
  }
  virtual void ExtrudeTo(double x, double y, double z) {
    extrude_dist_ += distance(x - last_x, y - last_y, z - last_z);
    printf("G1 X%.3f Y%.3f Z%.3f E%.3f\n", x, y, z,
           extrude_dist_ * filament_extrusion_factor_);
    last_x = x; last_y = y; last_z = z;
  }
  virtual void ResetExtrude() {
    printf("M83\n"  // extruder relative mode
           "G1 E%.1f ; filament back to nozzle tip\n"
           "M82\n", // extruder absolute mode
           1.1 * kRetractAmount);  // fudging... a bit more squeeze.
    printf("G92 E0  ; start extrusion\n");
    extrude_dist_ = 0;
  }
  virtual void Retract() {
    printf("M83\nG1 E%.1f ; retract\nM82\n", -kRetractAmount);
  }
  virtual void SwitchFan(bool on) {
    printf("M106 S%d\n", on ? 255 : 0);
  }

private:
  const double filament_extrusion_factor_;
  double last_x, last_y, last_z;
  double extrude_dist_;
};

class PostScriptPrinter : public Printer {
public:
  PostScriptPrinter(bool show_move_as_line, double line_thickness)
    : show_move_as_line_(show_move_as_line), line_thickness_(line_thickness),
      in_move_color_(false) {}
  virtual void Preamble(double machine_limit_x, double machine_limit_y,
                        double feed_mm_per_sec) {
    const float mm_to_point = 1 / 25.4 * 72.0;
    printf("%%!PS-Adobe-3.0\n%%%%BoundingBox: 0 0 %.0f %.0f\n\n",
           machine_limit_x * mm_to_point, machine_limit_y * mm_to_point);
  }
  virtual void Init(double machine_limit_x, double machine_limit_y,
                    double feed_mm_per_sec) {
    printf("72.0 25.4 div dup scale  %% Switch to mm\n");
    printf("1 setlinejoin\n");
    printf("%.2f setlinewidth %% mm\n", line_thickness_);
    printf("0 0 moveto\n");
  }

  virtual void Postamble() {
    printf("stroke\nshowpage\n");
  }
  virtual void Comment(const char *fmt, ...) {
    printf("%% ");
    va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap);
  }
  virtual void SetSpeed(double feed_mm_per_sec) {}
  virtual void ResetExtrude() {
    printf("%% Flush lines but remember where we are.\n"
           "currentpoint\nstroke\nmoveto\n");
  }
  virtual void Retract() {}
  virtual void GoZPos(double z) {}
  virtual void MoveTo(double x, double y, double z) {
    if (show_move_as_line_) {
      if (!in_move_color_) {
        ColorSwitch(0, 0, 0, 0.9);
        in_move_color_ = true;
      }
      printf("%.1f %.1f lineto\n", x, y);
    } else {
      printf("%.1f %.1f moveto\n", x, y);
    }
  }
  virtual void ExtrudeTo(double x, double y, double z) {
    if (in_move_color_) {
      ColorSwitch(line_thickness_, 0, 0, 0);
      in_move_color_ = false;
    }
    printf("%.1f %.1f lineto\n", x, y);
  }
  virtual void SwitchFan(bool on) {}
  virtual double GetExtrusionDistance() { return 0; }

private:
  void ColorSwitch(float line_width, float r, float g, float b) {
    printf("currentpoint\nstroke\n");   // finish last path; remember pos
    printf("%.1f setlinewidth %% mm\n", line_width);
    printf("%.1f %.1f %.1f setrgbcolor\n", r, g, b);
    printf("moveto\n");   // set current point to remembered pos.
  }

  const bool show_move_as_line_;
  const float line_thickness_;
  bool in_move_color_;
};

}  // end anonymous namespace.

// Public interface
Printer *CreateGCodePrinter(double extrusion_mm_to_e_axis_factor) {
    return new GCodePrinter(extrusion_mm_to_e_axis_factor);
}
Printer *CreatePostscriptPrinter(bool show_move_as_line,
                                 double line_thickness_mm) {
    return new PostScriptPrinter(show_move_as_line, line_thickness_mm);
}
