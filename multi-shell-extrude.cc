/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 * (c) 2014 Henner Zeller <h.zeller@acm.org>
 * Creative commons BY-SA
 */

#include <assert.h>
#include <getopt.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <vector>

#include "multi-shell-extrude.h"

// Define this with empty, if you're not using gcc.
#define PRINTF_FMT_CHECK(fmt_pos, args_pos) \
    __attribute__ ((format (printf, fmt_pos, args_pos)))

class Printer {
public:
  // Preamble: what to do to start the file.
  virtual void Preamble(double machine_limit_x, double machine_limit_y,
                        double feed_mm_per_sec) = 0;

  // Initialization
  virtual void Init(double machine_limit_x, double machine_limit_y,
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
    printf("; G-Code\n\n");
  }

  virtual void Init(double machine_limit_x, double machine_limit_y,
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
    printf("M104 S0 ; hotend off\n");
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
  }
  virtual void Init(double machine_limit_x, double machine_limit_y,
                    double feed_mm_per_sec) {
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
          "\t -D <data>         : data file with polygon\n"
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
          "\t -P                : PostScript output instead of GCode output\n"
          "\t -u <pump>         : Pump up polygon as if the center was not a\n"
          "\t                     dot but a cylinder of <pump> radius\n"
          "\t -m                : For Postscript: show nested (Matryoshka doll "
          "style)\n");

  return 1;
}

// The total length of distance going through a polygon.
double CalcPolygonLen(const Polygon &polygon) {
  double len = 0;
  const int size = polygon.size();
  for (int i = 1; i < size; ++i) {
    len += distance(polygon[i].x - polygon[i-1].x,
                    polygon[i].y - polygon[i-1].y, 0);
  }
  // Back to the beginning.
  len += distance(polygon[size-1].x - polygon[0].x,
                  polygon[size-1].y - polygon[0].y, 0);
  return len;
}

// Requires: Polygon with centroid on (0,0)
static void CreateExtrusion(const Polygon &p,
                            Printer *printer,
                            double offset_x, double offset_y,
                            double layer_height, double total_height,
                            double rotation_per_mm) {
  printer->Comment("Screw center X=%.1f Y=%.1f\n", offset_x, offset_y);
  const double polygon_len = CalcPolygonLen(p);
  const double rotation_per_layer = layer_height * rotation_per_mm * 2 * M_PI;
  bool fan_is_on = false;
  printer->SwitchFan(false);
  double height = 0;
  double angle = 0;
  double run_len = 0;
  const int psteps = p.size();
  printer->MoveTo(p[0].x + offset_x, p[0].y + offset_y, 0);
  for (height = 0, angle=0; height < total_height;
       height += layer_height, angle += rotation_per_layer) {
    for (int i = 0; i < psteps; ++i) {
      if (i == 0) {
        run_len = 0;
      } else {
        run_len += distance(p[i].x - p[i-1].x, p[i].y - p[i-1].y, 0);
      }
      double fraction = run_len / polygon_len;
      double a = angle + fraction * rotation_per_layer;
      double x = p[i].x * cos(a) - p[i].y * sin(a);
      double y = p[i].y * cos(a) + p[i].x * sin(a);
      printer->ExtrudeTo(x + offset_x,
                         y + offset_y, height + layer_height * fraction);
    }

    if (height > 1.5 && !fan_is_on) {
      printer->SwitchFan(true);  // 1.5mm reached - fan on.
      fan_is_on = true;
    }
  }
}

Polygon ReadPolygon(const char *filename, double factor) {
  Polygon polygon;
  FILE *in = fopen(filename, "r");
  if (!in) {
    fprintf(stderr, "Can't open %s\n", filename);
    return polygon;
  }
  char buffer[256];
  while (fgets(buffer, sizeof(buffer), in)) {
    Point p;
    if (sscanf(buffer, "%lf %lf\n", &p.x, &p.y) == 2) {
      p.x *= factor;
      p.y *= factor;
      polygon.push_back(p);
    }
  }
  fclose(in);
  return polygon;
}

// Pump a polygon as if it was not arranged a dot but a circle of radius pump_r
Polygon RadialPumpPolygon(const Polygon& polygon, double pump_r) {
  Polygon result;
  for (std::size_t i = 0; i < polygon.size(); ++i) {
    const Point &p = polygon[i];
    double from_center = distance(p.x, p.y, 0);
    double stretch = (from_center + pump_r) / from_center;
    result.push_back(Point(p.x * stretch, p.y * stretch));
  }
  return result;
}

// Determine radius of circumscribed circle
double GetRadius(const Polygon &polygon) {
  double dist = -1;
  for (size_t i = 0; i < polygon.size(); ++i) {
    dist = std::max(dist, distance(polygon[i].x, polygon[i].y, 0));
  }
  return dist;
}

int main(int argc, char *argv[]) {
  const double start_x = 20;
  const double start_y = 20;

  // Some useful default values.
  double pitch = 30.0;
  double layer_height = 0.16;
  double nozzle_radius = 0.4 / 2;
  double filament_radius = 1.75 / 2;
  double total_height = -1;
  double machine_limit_x = 150, machine_limit_y = 150;
  int faces = 720;
  double initial_size = 10.0;
  double shell_increment = 1.6;
  double initial_shell = 0;
  double head_offset_x = 45;
  double head_offset_y = 45;
  double feed_mm_per_sec = 100;
  double min_layer_time = 4.5;
  double thread_depth = -1;
  double extrusion_fudge_factor = 1.9;  // empiric...
  int screw_count = 2;
  double twist = 0.0;
  double pump = 0.0;
  bool do_postscript = false;
  bool matryoshka = false;

  const char *fun_init = "AABBBAABBBAABBB";
  const char *data_file = NULL;

  int opt;
  while ((opt = getopt(argc, argv, "t:h:n:s:R:i:d:l:f:p:T:L:o:w:PD:u:m")) != -1) {
    switch (opt) {
    case 't': fun_init = strdup(optarg); break;
    case 'D': data_file = strdup(optarg); break;
    case 'h': total_height = atof(optarg); break;
    case 'n': screw_count = atoi(optarg); break;
    case 's': initial_size = atof(optarg); break;
    case 'i': initial_shell = atof(optarg); break;
    case 'R': shell_increment = atof(optarg); break;
    case 'd': thread_depth = atof(optarg); break;
    case 'l': layer_height = atof(optarg); break;
    case 'f': feed_mm_per_sec = atof(optarg); break;
    case 'T': min_layer_time = atof(optarg); break;
    case 'w': twist = atof(optarg); break;
    case 'u': pump = atof(optarg); break;
    case 'm': matryoshka = true; break;
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
    thread_depth = initial_size / 5;

  if (matryoshka && !do_postscript) {
    fprintf(stderr, "Matryoshka mode only valid with postscript\n");
    return usage(argv[0]);
  }

  matryoshka &= do_postscript;   // Anyway, let's formulate it also this way

  // Get polygon we'll be working on.
  Polygon polygon = data_file
    ? ReadPolygon(data_file, initial_size)
    : RotationalPolygon(fun_init, initial_size, thread_depth, twist);

  if (polygon.empty()) {
    fprintf(stderr, "Polygon empty\n");
    return 1;
  }

  if (pump > 0) {
    polygon = RadialPumpPolygon(polygon, pump);
  }

  if (initial_shell != 0) {
    polygon = PolygonOffset(polygon, initial_shell);
  }

  double radius = GetRadius(polygon);

  if (matryoshka) {
    machine_limit_x = 2 * radius + 2 * start_x;
    machine_limit_y = 2 * radius + 2 * start_y;
  }

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

  printer->Comment("https://github.com/hzeller/gcode-multi-shell-extrude\n");
  printer->Comment("\n");
  printer->Comment(" ");
  for (int i = 0; i < argc; ++i) printf("%s ", argv[i]);
  printf("\n");
  printer->Comment("\n");
  printer->Comment("screw template '%s'\n", fun_init);
  printer->Comment("size=%.1fmm h=%.1fmm n=%d (shell-increment=%.1fmm)\n",
                   initial_size, total_height, screw_count, shell_increment);
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

  printer->Init(machine_limit_x, machine_limit_y, feed_mm_per_sec);

  // How much the whole system should rotate per mm height.
  const double rotation_per_mm = (pitch == 0) ? 10000000.0 : 1.0 / pitch;

  double total_time = 0;
  double total_travel = 0;

  double x = std::max(start_x, radius + 5);
  double y = std::max(start_y, radius + 5);
  printer->SetSpeed(feed_mm_per_sec);  // initial speed.
  for (int i = 0; i < screw_count; ++i) {
    if (x + radius + 5 > machine_limit_x
        || y + radius + 5 > machine_limit_y) {
      fprintf(stderr, "With currently configured bedsize and printhead-offset, "
              "only %d screws fit.\n"
              "Configure your machine constraints with -L <x/y> -o < dx,dy> "
              "(currently -L %.0f,%.0f -o %.0f,%.0f)\n", i,
              machine_limit_x, machine_limit_y, head_offset_x, head_offset_y);
      break;
    }
    printer->MoveTo(x, y, i > 0 ? total_height + 5 : 5);
    double layer_feedrate = CalcPolygonLen(polygon) / min_layer_time;
    layer_feedrate = std::min(layer_feedrate, feed_mm_per_sec);
    printer->ResetExtrude();
    printer->SetSpeed(layer_feedrate);
    CreateExtrusion(polygon, printer, x, y, layer_height, total_height,
                    rotation_per_mm);
    polygon = PolygonOffset(polygon, shell_increment);
    double travel = printer->GetExtrusionLength();
    total_travel += travel;
    total_time += travel / layer_feedrate;  // roughly (without acceleration)
    printer->SetSpeed(feed_mm_per_sec);
    printer->Retract(3);
    printer->GoZPos(total_height + 5);
    double new_radius = GetRadius(polygon);
    if (!matryoshka) {
      x += head_offset_x + radius + new_radius;
      y += head_offset_y + radius + new_radius;
    }
    radius = new_radius;
  }

  printer->Postamble();
  fprintf(stderr, "Total time about %.0f seconds; %.2fm filament\n",
          total_time, total_travel * filament_extrusion_factor / 1000);
}
