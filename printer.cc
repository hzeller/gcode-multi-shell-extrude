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
  GCodePrinter(double extrusion_factor, double temperature)
    : filament_extrusion_factor_(extrusion_factor), temperature_(temperature), extrude_dist_(0) {}

  virtual void Preamble(const Vector2D &machine_limit,
                        double feed_mm_per_sec) {
    printf("; G-Code\n\n");
  }

  virtual void Init(const Vector2D &machine_limit,
                    double feed_mm_per_sec) {
    printf("G28 X0 Y0\nG28 Z0\nG1 F%.1f\n", feed_mm_per_sec * 60);
    printf("M82 ; absolute E\nG92 E0 ; zero E\n");
    printf("G0 X%.1f Y10 Z30 F6000; move to center front\n", machine_limit.x/2);
    SetTemperature(temperature_);
    printf("M109 S%.0f\nM116\n", temperature_);
    printf("G1 E3 ; squirt out some test in air\n"); // squirt out some test
    printf("G92 E0\n; test extrusion...\n");
    const double test_extrusion_from = 0.3 * machine_limit.x;
    const double test_extrusion_to = 0.2 * machine_limit.x;
    SetSpeed(300.0);
    MoveTo(Vector2D(test_extrusion_from, 10), 0.1);
    SetSpeed(feed_mm_per_sec / 3);
    ExtrudeTo(Vector2D(test_extrusion_to, 10), 0.1);
    Retract();
    GoZPos(5);
  }
  virtual void Postamble() {
    printf("M104 S0 ; hotend off\n");
    printf("M106 S0 ; fan off\n");
    printf("G28 X0 Y0\n");  // We keep z-axis as is.
    printf("G92 E0\n");
    printf("M84\n");
  }
  virtual void SetTemperature(double temperature) {
    if (temperature != temperature_)
      printf("M104 S%.0f\n", temperature);
    temperature_ = temperature;
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
  virtual void MoveTo(const Vector2D &pos, double z) {
    printf("G1 X%.3f Y%.3f Z%.3f\n", pos.x, pos.y, z);
    last_x = pos.x; last_y = pos.y; last_z = z;
  }
  virtual void ExtrudeTo(const Vector2D &pos, double z) {
    extrude_dist_ += distance(pos.x - last_x, pos.y - last_y, z - last_z);
    printf("G1 X%.3f Y%.3f Z%.3f E%.3f\n", pos.x, pos.y, z,
           extrude_dist_ * filament_extrusion_factor_);
    last_x = pos.x; last_y = pos.y; last_z = z;
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
  double temperature_;
  double last_x, last_y, last_z;
  double extrude_dist_;
};

class PostScriptPrinter : public Printer {
public:
  PostScriptPrinter(bool show_move_as_line, double line_thickness)
    : show_move_as_line_(show_move_as_line), line_thickness_(line_thickness),
      in_move_color_(false), r_(0), g_(0), b_(0) {
  }
  virtual void Preamble(const Vector2D &machine_limit,
                        double feed_mm_per_sec) {
    const float mm_to_point = 1 / 25.4 * 72.0;
    printf("%%!PS-Adobe-3.0\n%%%%BoundingBox: 0 0 %.0f %.0f\n\n",
           machine_limit.x * mm_to_point, machine_limit.y * mm_to_point);
  }
  virtual void Init(const Vector2D &machine_limit,
                    double feed_mm_per_sec) {
    printf("/extrude-to { lineto } def\n");
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
  virtual void SetTemperature(double t) {}
  virtual void ResetExtrude() {
    printf("%% Flush lines but remember where we are.\n"
           "currentpoint\nstroke\nmoveto\n");
  }
  virtual void Retract() {}
  virtual void GoZPos(double z) {}
  virtual void MoveTo(const Vector2D &pos, double z) {
    if (show_move_as_line_) {
      if (!in_move_color_) {
        ColorSwitch(0, 0, 0, 0.9);  // blue move color
        in_move_color_ = true;
      }
      printf("%.3f %.3f lineto\n", pos.x, pos.y);
    } else {
      printf("%.3f %.3f moveto\n", pos.x, pos.y);
    }
  }
  virtual void ExtrudeTo(const Vector2D &pos, double z) {
    if (in_move_color_) {
      ColorSwitch(line_thickness_, r_, g_, b_);
      in_move_color_ = false;
    }
    printf("%.3f %.3f extrude-to\n", pos.x, pos.y);
  }
  virtual void SwitchFan(bool on) {}
  virtual double GetExtrusionDistance() { return 0; }
  virtual void SetColor(float r, float g, float b) {
    r_ = r; g_ = g; b_ = b;
    if (!in_move_color_) {
      ColorSwitch(line_thickness_, r, g, b);
    }
  }
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
  float r_, g_, b_;   // color.
};

}  // end anonymous namespace.

// Public interface
Printer *CreateGCodePrinter(double extrusion_mm_to_e_axis_factor, double temp) {
  return new GCodePrinter(extrusion_mm_to_e_axis_factor, temp);
}
Printer *CreatePostscriptPrinter(bool show_move_as_line,
                                 double line_thickness_mm) {
  return new PostScriptPrinter(show_move_as_line, line_thickness_mm);
}
