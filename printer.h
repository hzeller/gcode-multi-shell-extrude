/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 * (c) 2014 Henner Zeller <h.zeller@acm.org>
 * Creative commons BY-SA
 */
#ifndef SHELL_EXTRUDE_PRINTER_H_
#define SHELL_EXTRUDE_PRINTER_H_

#include "multi-shell-extrude.h"

// Define this with empty, if you're not using gcc.
#ifdef __GNUC__
#  define PRINTF_FMT_CHECK(fmt_pos, args_pos) \
      __attribute__ ((format (printf, fmt_pos, args_pos)))
#else
#  define PRINTF_FMT_CHECK(fmt_pos, args_pos)
#endif

// Abstract definition of some 3D output - moving and extruding in the 3D space.
// With this abstraction, it is possible to output to different kinds of
// printers. Initially, we support a 3D-printer GCode output and a PostScript
// output.
class Printer {
public:
  // Preamble: what to do to start the file.
  virtual void Preamble(const Vector2D &machine_limit,
                        double feed_mm_per_sec) = 0;

  // Initialization. To be called after Preamble, which might create some
  // settings in the
  virtual void Init(const Vector2D &machine_limit,
                    double feed_mm_per_sec) = 0;
  virtual void Postamble() = 0;
  virtual void Comment(const char *fmt, ...) PRINTF_FMT_CHECK(2, 3) = 0;
  virtual void SetTemperature(double temperature) = 0;
  virtual void SetSpeed(double feed_mm_per_sec) = 0;
  virtual void ResetExtrude() = 0;
  virtual void Retract() = 0;

  // Go to z-position without changing x/y
  virtual void GoZPos(double z) = 0;

  // Move to absolute position.
  virtual void MoveTo(const Vector2D &pos, double z) = 0;

  // Extrude/"Line" to absolute position.
  virtual void ExtrudeTo(const Vector2D &pos, double z) = 0;
  virtual void SwitchFan(bool on) = 0;
  virtual double GetExtrusionDistance() = 0;
  // Nice-to-have. Mostly for visualization reasons, doesn't change
  virtual void SetColor(float r, float g, float b) {}
};

// Create a printer that outputs GCode to stdout.
// "extrusion_mm_to_e_axis_factor" translates mm extruded length to E-axis
// output.
Printer *CreateGCodePrinter(double extrusion_mm_to_e_axis_factor,
                            double temperature);

// Create printer that outputs PostScript to stdout.
// If "show_move_as_line" is true, visualizes moves as blue lines.
Printer *CreatePostscriptPrinter(bool show_move_as_line,
                                 double line_thickness_mm);

#undef PRINTF_FMT_CHECK

#endif // SHELL_EXTRUDE_PRINTER_H_
