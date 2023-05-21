/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 * (c) 2014 Henner Zeller <h.zeller@acm.org>
 * Creative commons BY-SA
 */

#include "printer.h"

#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

#include "multi-shell-extrude.h"  // for distance()

namespace {
class GCodePrinter : public Printer {
public:
  GCodePrinter(double extrusion_factor, double retract_amount,
               double temperature, double bed_temp)
    : filament_extrusion_factor_(extrusion_factor),
      retract_amount_(retract_amount),
      temperature_(temperature), bed_temp_(bed_temp), extrude_dist_(0) {}

  virtual void Preamble(const Vector2D &machine_limit,
                        double feed_mm_per_sec) {
    printf("(G-Code)\n\n");
  }

  virtual void Init(const Vector2D &machine_limit,
                    double feed_mm_per_sec) {
    printf("G28\nG1 F%.1f\n", feed_mm_per_sec * 60);
    printf("G1 Z5\n");
    printf("M82      ; absolute E\n"
           "G92 E0.0 ; zero E\n");
    const bool with_heated_bed = bed_temp_ > 0 && bed_temp_ < 120;
    if (with_heated_bed) {
      printf("M140 S%.0f  ; not waiting for it yet\n", bed_temp_);
    }

    // Bed leveling
    printf("\n");
    Comment("Bed leveling\n");
    printf("M84 E         ; turn off e motor\n");
    printf("M109 S170     ; min temperature not have soft nozzle buggers\n");
    printf("G1 E-2 F2400  ; retract to not ooze while bed leveling\n");
    printf("M84 E\n");
    printf("G28 Z0        ; Establish a general Z0\n");
    printf("G29           ; bed levelling after everything is hot\n\n");

    Comment("Wait for all temperatures reached\n");
    printf("G1 E0\n");
    printf("G0 X%.1f Y10 Z30 F6000 ; move to center front while heating\n",
           machine_limit.x/2);

    SetTemperature(temperature_);

    // Waiting for temperature
    printf("M109 S%.0f\n", temperature_);
    if (with_heated_bed) {
      printf("M190 S%.0f ; wait for bed-temp\n", bed_temp_);
    }

    printf("M82      ; absolute E\nG92 E0.0 ; zero E\n");
    printf("G1 E3    ; squirt out some test in air\n"); // squirt out some test
    printf("G92 E0.0\n\n; test extrusion...\n");
    const double test_extrusion_from = 0.5 * machine_limit.x;
    const double test_extrusion_to = 0.1 * machine_limit.x;
    SetSpeed(300.0);
    MoveTo(Vector2D(test_extrusion_from, 10), 0.2);
    SetSpeed(15);
    ExtrudeTo(Vector2D((test_extrusion_from + test_extrusion_to)/2, 10),
              0.2, 1.0);
    // Remaining just move to wipe nozzle properly.
    MoveTo(Vector2D(test_extrusion_to, 10), 0.2);
    Retract();
    GoZPos(5);
  }
  virtual void Postamble() {
    printf("M104 S0 ; hotend off\n");
    printf("M140 S0 ; heated bed off\n");
    printf("M106 S0 ; fan off\n");
    printf("G1 X0\n");  // We keep z-axis as is.
    printf("G92 E0.0\n");
    printf("M84\n");
  }
  virtual void SetTemperature(double temperature) {
    if (temperature != temperature_)
      printf("M104 S%.0f\n", temperature);
    temperature_ = temperature;
  }
  virtual double GetExtrusionDistance() { return extrude_dist_; }
  virtual void Comment(const char *fmt, ...) {
    printf("; ");   // TODO: not all printers might be able to deal with ';'
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
  virtual void ExtrudeTo(const Vector2D &pos, double z,
                         double extrusion_multiplier) {
    extrude_dist_ += distance(pos.x - last_x, pos.y - last_y, z - last_z);
    printf("G1 X%.3f Y%.3f Z%.3f E%.3f\n", pos.x, pos.y, z,
           extrude_dist_ * filament_extrusion_factor_ * extrusion_multiplier);
    last_x = pos.x; last_y = pos.y; last_z = z;
  }
  virtual void ResetExtrude() {
    assert(in_retract_);
    in_retract_ = false;
    printf("M83      ; relative E\n"  // extruder relative mode
           "G1 E%.1f  ; filament back to nozzle tip\n"
           "M82      ; absolute E\n", // extruder absolute mode
           1.1 * retract_amount_);  // fudging... a bit more squeeze.
    printf("G92 E0.0 ; start extrusion, set E to zero\n");
    extrude_dist_ = 0;
  }
  virtual void Retract() {
    assert(!in_retract_);
    printf("M83      ; relative E\n"
           "G1 E%.1f ; retract\n"
           "M82      ; Back to absolute\n", -retract_amount_);
    in_retract_ = true;
  }
  virtual void SwitchFan(bool on) {
    printf("M106 S%d\n", on ? 255 : 0);
  }

private:
  const double filament_extrusion_factor_;
  const double retract_amount_;
  double temperature_;
  double bed_temp_;
  double last_x, last_y, last_z;
  double extrude_dist_;
  bool in_retract_ = false;
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
  virtual void ExtrudeTo(const Vector2D &pos, double /*z*/,
                         double /*extrusion_multiplier*/) {
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
Printer *CreateGCodePrinter(double extrusion_mm_to_e_axis_factor,
                            double retract_amount,
                            double temp, double bed_temp) {
  return new GCodePrinter(extrusion_mm_to_e_axis_factor, retract_amount,
                          temp, bed_temp);
}
Printer *CreatePostscriptPrinter(bool show_move_as_line,
                                 double line_thickness_mm) {
  return new PostScriptPrinter(show_move_as_line, line_thickness_mm);
}
