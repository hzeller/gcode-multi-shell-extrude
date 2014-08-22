/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 * (c) 2014 Henner Zeller <h.zeller@acm.org>
 * Creative commons BY-SA
 */

#include <assert.h>
#include <getopt.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <vector>
#include <algorithm>

// Define this with empty, if you're not using gcc.
#define PRINTF_FMT_CHECK(fmt_pos, args_pos) \
    __attribute__ ((format (printf, fmt_pos, args_pos)))

static double distance(double dx, double dy, double dz) {
  return sqrt(dx*dx + dy*dy + dz*dz);
}

class Printer {
public:
  virtual void Preamble(double machine_limit_x, double machine_limit_y,
                        double feed_mm_per_sec) = 0;
  virtual void Postamble() = 0;
  virtual void Comment(const char *fmt, ...) PRINTF_FMT_CHECK(2, 3) = 0;
  virtual void SetSpeed(double feed_mm_per_sec) = 0;
  virtual void ResetExtrude() = 0;
  virtual void Retract(double amount) = 0;
  virtual void GoZPos(double z) = 0;
  virtual void MoveTo(double x, double y, double z) = 0;
  virtual void ExtrudeTo(double x, double y, double z) = 0;
  virtual void SwitchFan(bool on) = 0;
  virtual double GetExtrusionLength() = 0;
};

class GCodePrinter : public Printer {
public:
  GCodePrinter(double extrusion_factor)
    : filament_extrusion_factor_(extrusion_factor), extrude_dist_(0) {}

  virtual void Preamble(double machine_limit_x, double machine_limit_y,
                        double feed_mm_per_sec) {
    printf("G28\nG1 F%.1f\n", feed_mm_per_sec * 60);
    printf("G1 X150 Y10 Z30\n");
    printf("M109 S190\nM116\n");
    printf("M82 ; absolute extrusion\n");
    printf("G92 E0  ; nozzle clean extrusion\n");
    const double test_extrusion_from = 0.8 * machine_limit_x;
    const double test_extrusion_to = 0.2 * machine_limit_x;
    const double length = test_extrusion_from - test_extrusion_to;
    printf("G1 X%.1f Y10 Z0\nG1 X%.1f Y10 E%.3f F1000\n",
           test_extrusion_from, test_extrusion_to,
           length * filament_extrusion_factor_);
    printf("G1 Z5\n");
    printf("M83\nG1 E-3 ; retract\nM82\n");  // relative, retract, absolute
  }
  virtual void Postamble() {
    printf("M104 S0\n");
    printf("M106 S0 ; fan off\n");
    printf("G28 X0 Y0\n");
    printf("M84\n");
  }
  virtual double GetExtrusionLength() { return extrude_dist_; }
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
    printf("M83\nG1 E2 ; filament back to nozzle tip\nM82\n");
    printf("G92 E0  ; start extrusion\n");
    extrude_dist_ = 0;
  }
  virtual void Retract(double amount) {
    printf("M83\nG1 E%.1f ; retract\nM82\n", -amount);
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
  PostScriptPrinter() {}
  virtual void Preamble(double machine_limit_x, double machine_limit_y,
                        double feed_mm_per_sec) {
    const float mm_to_point = 1 / 25.4 * 72.0;
    printf("%%!PS-Adobe-3.0\n%%%%BoundingBox: 0 0 %.0f %.0f\n\n",
           machine_limit_x * mm_to_point, machine_limit_y * mm_to_point);
    printf("72.0 25.4 div dup scale  %% Switch to mm\n");
    printf("0.2 setlinewidth %% mm\n");
  }
  virtual void Postamble() {
    printf("stroke\nshowpage\n");
  }
  virtual void Comment(const char *fmt, ...) {
    printf("%% ");
    va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap);
  }
  virtual void SetSpeed(double feed_mm_per_sec) {}
  virtual void ResetExtrude() { }
  virtual void Retract(double amount) {}
  virtual void GoZPos(double z) {}
  virtual void MoveTo(double x, double y, double z) {
    printf("%.1f %.1f moveto\n", x, y);
  }
  virtual void ExtrudeTo(double x, double y, double z) {
    printf("%.1f %.1f lineto\n", x, y);
  }
  virtual void SwitchFan(bool on) {}
  virtual double GetExtrusionLength() { return 0; }
};

class PolarFunction {
public:
  // Initialization string corresponds to the linear rolled out
  // circumreference 'dents'.
  PolarFunction(const unsigned char *init, double rotation_per_mm)
    : rotation_per_mm_(rotation_per_mm) {
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
  double value(double phi, double height_mm) {
    // We want screws to turn right, hence we substract
    phi = fmodl(phi - rotation_per_mm_ * height_mm, 1.0);
    assert(phi >= 0);
    const int n = phi * values_.size();
    // linear interpolation between this and the next value.
    const double a = values_[n];
    const double b = values_[(n+1) % values_.size()];
    // fraction between values.
    const double fraction = values_.size() * fmodl(phi, 1.0 / values_.size());
    return a + (b - a) * fraction;
  }

private:
  const double rotation_per_mm_;
  std::vector<double> values_;
};

static int quantize_up(int x, int q) {
  return q * ((x + q - 1) / q);
}

static int usage(const char *progname) {
  fprintf(stderr, "Usage: %s -h <height> [<options>]\n",
          progname);
  fprintf(stderr,
          "Template (flag -t) describes the shape. The letters in that string\n"
          "describe the screw depth for a full turn.\n"
          "A template 'AAZZZAAZZZAAZZZ' is a screw with three parallel threads,"
          "\nwith 'inner parts' (the one with the lower letter 'A') being 2/3\n"
          "the width of the outer parts. 'AAZZZ' would have one thread per turn."
          "\nThe string-length represents a full turn, so 'AAZZZZZZZZ' would\n"
          "have one narrow thread.\n"
          "The range of letters (here A..Z) is linearly mapped to the thread\n"
          "depth.\n"
          "Try 'AAZZMMZZZZ'. If you only use two letters, then 'AABBBAABBB'\n"
          "is equivalent to 'AAZZZAAZZZ'\n"
          "Required parameter: -h <height>\n\n"
          "\t -t <template>     : template string, described above\n"
          "\t                     (default \"AABBBAABBBAABBB\")\n"
          "\t -h <height>       : Total height to be printed\n"
          "\t -n <number-of-screws> : number of screws to be printed\n"
          "\t -r <radius>       : radius of the smallest screw\n"
          "\t -R <radius-increment> : increment between screws\n"
          "\t -w <twist>        : tWist ratio of angle per radius fraction\n"
          "\t                     (good range: -0.3...0.3)\n"
          "\t -d <thread-depth> : depth of thread (default: radius/5)\n"
          "\t -l <layer-height> : Height of each layer\n"
          "\t -f <feed-rate>    : maximum, in mm/s\n"
          "\t -T <layer-time>   : min time per layer; dynamically influences -f\n"
          "\t -p <pitch>        : how many mm height a full screw-turn takes\n"
          "\t -L <x,y>          : x/y size limit of your printbed.\n"
          "\t -o <dx,dy>        : dx/dy offset per print. Clearance needed from\n"
          "\t                     hotend-tip to left and front essentially.\n"
          "\t -P                : PostScript output instead of GCode output\n");
  return 1;
}

static double AngleTwist(double twist, double r, double max_r) {
  return twist * r / max_r;
}

// 'angle step' is in fraction of a circle, so 0=begin 1.0=full turn.
static void CreateExtrusion(PolarFunction *fun,
                            Printer *printer,
                            double thread_depth,
                            double offset_x, double offset_y, double radius,
                            double twist,
                            double layer_height, double total_height,
                            double angle_step) {
  printer->Comment("Screw center X=%.1f Y=%.1f r=%.1f thread-depth=%.1f\n",
                  offset_x, offset_y, radius, thread_depth);
  const double height_step = layer_height * angle_step;
  const double max_r = radius + thread_depth;
  bool fan_is_on = false;
  bool do_extrusion = true;
  printer->SwitchFan(false);
  double angle = 0;
  double height = 0;
  for (height=0, angle=0; height < total_height;
       height+=height_step, angle+=angle_step) {
    const double r = radius + thread_depth * fun->value(angle, height);
    const double x = r * cos((angle + AngleTwist(twist, r, max_r)) * 2 * M_PI);
    const double y = r * sin((angle + AngleTwist(twist, r, max_r)) * 2 * M_PI);
    if (do_extrusion && height > 0) {
      printer->ExtrudeTo(x + offset_x, y + offset_y, height);
    } else {
      printer->MoveTo(x + offset_x, y + offset_y, height);
    }
    if (height > 1.5 && !fan_is_on) {
      printer->SwitchFan(true);  // 1.5mm reached - fan on.
      fan_is_on = true;
    }
    // The last half round, we run without extrusion to have a nice, smooth
    // ending.
    if (do_extrusion && (height > total_height - layer_height/2)) {
      do_extrusion = false;
      printer->Comment("Reaching end of spiral. Retracting 1mm\n");
      printer->Retract(1);
    }
  }
}

int main(int argc, char *argv[]) {
  const double start_x = 10;
  const double start_y = 10;

  // Some useful default values.
  double pitch = 30.0;
  double layer_height = 0.16;
  double nozzle_radius = 0.4 / 2;
  double filament_radius = 1.75 / 2;
  double total_height = -1;
  double machine_limit_x = 150, machine_limit_y = 150;
  int faces = 720;
  double radius = 10.0;
  double radius_increment = 1.6;
  double head_offset_x = 45;
  double head_offset_y = 45;
  double feed_mm_per_sec = 100;
  double min_layer_time = 4.5;
  double thread_depth = -1;
  double extrusion_fudge_factor = 1.9;  // empiric...
  int screw_count = 2;
  double twist = 0.0;
  bool do_postscript = false;

  const char *fun_init = "AABBBAABBBAABBB";

  int opt;
  while ((opt = getopt(argc, argv, "t:h:n:r:R:d:l:f:p:T:L:o:w:P")) != -1) {
    switch (opt) {
    case 't': fun_init = strdup(optarg); break;
    case 'h': total_height = atof(optarg); break;
    case 'n': screw_count = atoi(optarg); break;
    case 'r': radius = atof(optarg); break;
    case 'R': radius_increment = atof(optarg); break;
    case 'd': thread_depth = atof(optarg); break;
    case 'l': layer_height = atof(optarg); break;
    case 'f': feed_mm_per_sec = atof(optarg); break;
    case 'T': min_layer_time = atof(optarg); break;
    case 'w': twist = atof(optarg); break;
    case 'L':
      if (2 != sscanf(optarg, "%lf,%lf", &machine_limit_x, &machine_limit_y)) {
        usage(argv[0]);
      }
      break;
    case 'o':
      if (2 != sscanf(optarg, "%lf,%lf", &head_offset_x, &head_offset_y)) {
        usage(argv[0]);
      }
      break;
    case 'p': pitch = atof(optarg); break;
    case 'P': do_postscript = true; break;
    default:
      return usage(argv[0]);
    }
  }

  if (total_height < 0)
    return usage(argv[0]);

  if (thread_depth < 0)
    thread_depth = radius / 5;

  const double filament_extrusion_factor = extrusion_fudge_factor *
    (nozzle_radius * (layer_height/2)) / (filament_radius*filament_radius);

  Printer *printer = NULL;
  if (do_postscript) {
    total_height = 3 * layer_height;  // not needed more.
    printer = new PostScriptPrinter();
  } else {
    printer = new GCodePrinter(filament_extrusion_factor);
  }
  printer->Preamble(machine_limit_x, machine_limit_y, feed_mm_per_sec);

  // Some sensible start pos.

  // We want the number of faces in a way, that the error introduced would be
  // less than the layer_height.
  // max_r is the maxium radius we'll see on the biggest shell
  const double max_r = radius + (screw_count-1) * radius_increment + thread_depth;
  const double max_error = layer_height/2;  // maximum error to tolerate
  // Maximum lenght of one edge of our cylinder, that should not differ more
  // than max_error in the middle. Half a segment is a nice perpendicular
  // triangle
  const double half_segment = sqrt((max_r*max_r)
                                   - (max_r - max_error)*(max_r - max_error));
  faces = ceil((2 * M_PI * max_r) / (2 * half_segment));
  if (fabs(twist) > 0.01) {
    faces *= 4;  // when twisting, we do more
  }
  faces = quantize_up(faces, strlen(fun_init));   // same sampling per letter.
  printer->Comment("https://github.com/hzeller/gcode-multi-shell-extrude\n");
  printer->Comment("\n");
  printer->Comment(" ");
  for (int i = 0; i < argc; ++i) printf("%s ", argv[i]);
  printf("\n");
  printer->Comment("\n");
  printer->Comment("screw template '%s'\n", fun_init);
  printer->Comment("r=%.1fmm h=%.1fmm n=%d (radius-increment=%.1fmm)\n",
                  radius, total_height, screw_count, radius_increment);
  printer->Comment("thread-depth=%.1fmm faces=%d\n",
                  thread_depth, faces);
  printer->Comment("feed=%.1fmm/s (maximum; layer time at least %.1f s)\n",
                  feed_mm_per_sec, min_layer_time);
  printer->Comment("pitch=%.1fmm/turn layer-height=%.3f\n", pitch, layer_height);
  printer->Comment("machine limits: bed: (%.0f/%.0f):  "
                  "head-offset: (%.0f,%.0f)\n",
                  machine_limit_x, machine_limit_y,
                  head_offset_x, head_offset_y);
  printer->Comment("----\n");

  double rotation_per_mm = (pitch == 0) ? 10000000.0 : 1.0 / pitch;
  PolarFunction f((unsigned char*) fun_init, rotation_per_mm);

  double total_time = 0;
  double total_travel = 0;
  // We want to overshoot with the number of faces one rotation a bit so
  // that we are matching up with the start of the new rotation of the polar
  // function. That way, we can have few faces without alias problems.
  const double angle_step = (1.0 + (rotation_per_mm * layer_height)) / faces;
  double x = std::max(start_x, radius + thread_depth + 5);
  double y = std::max(start_y, radius + thread_depth + 5);
  printer->SetSpeed(feed_mm_per_sec);  // initial speed.
  for (int i = 0; i < screw_count; ++i) {
    if (x + radius + thread_depth + 5 > machine_limit_x
        || y + radius + thread_depth + 5 > machine_limit_y) {
      fprintf(stderr, "With currently configured bedsize and printhead-offset, "
              "only %d screws fit.\n"
              "Configure your machine constraints with -L <x/y> -o < dx,dy> "
              "(currently -L %.0f,%.0f -o %.0f,%.0f)\n", i,
              machine_limit_x, machine_limit_y, head_offset_x, head_offset_y);
      break;
    }
    printer->MoveTo(x, y, 0);
    double layer_feedrate = 2 * M_PI * radius / min_layer_time;
    layer_feedrate = std::min(layer_feedrate, feed_mm_per_sec);
    printer->ResetExtrude();
    printer->SetSpeed(layer_feedrate);
    CreateExtrusion(&f, printer, thread_depth, x, y, radius, twist,
                    layer_height, total_height, angle_step);
    double travel = printer->GetExtrusionLength();
    total_travel += travel;
    total_time += travel / layer_feedrate;  // roughly (without acceleration)
    printer->SetSpeed(feed_mm_per_sec);
    printer->Retract(3);
    printer->GoZPos(total_height + 5);
    x += head_offset_x + 2 * radius + radius_increment;
    y += head_offset_y + 2 * radius + radius_increment;
    radius += radius_increment;
  }

  printer->GoZPos(total_height + 5);
  printer->Postamble();
  fprintf(stderr, "Total time about %.0f seconds; %.2fm filament\n",
          total_time, total_travel * filament_extrusion_factor / 1000);
}
