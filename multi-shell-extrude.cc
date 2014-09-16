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

#include <algorithm>
#include <fstream>
#include <iostream>
#include <vector>

#include "multi-shell-extrude.h"
#include "printer.h"

static int usage(const char *progname) {
  fprintf(stderr, "Usage: %s -h <height> [<options>]\n",
          progname);
  fprintf(stderr,
          "Required parameter: -h <height>\n"
          "\n [ Screw from a string template]\n"
          "\t -t <template>     : template string, described above\n"
          "\t                     (default \"AABBBAABBBAABBB\")\n"
          "\t                   The following options -d and -w work for this:\n"
          "\t -d <thread-depth> : depth of thread (default: radius/5)\n"
          "\t -w <twist>        : tWist ratio of angle per radius fraction\n"
          "\t                     (good range: -0.3...0.3)\n"
          "\n [ Screw from a polygon data file ]\n"
          "\t -D <data>         : data file with polygon. Lines with x y "
          "pairs.\n"
          "\n [ General parameters ]\n"
          "\t -h <height>       : Total height to be printed\n"
          "\t -s <initial-size> : Polygon sizing parameter.\n"
          "\t                     Means radius if from -t, factor for -D.\n"
          "\t -u <pump>         : Pump up polygon as if the center was not a\n"
          "\t                     dot but a circle of <pump> radius\n"
          "\t -n <number-of-screws> : number of screws to be printed\n"
          "\t -R <radius-increment> : increment between screws, the clearance.\n"
          "\t -S <stop-offset>  : EXPERIMENTAL offset to stop screw\n"
          "\t                     around (radius_increment - 0.8)/2 + 0.05\n"
          "\t -l <layer-height> : Height of each layer.\n"
          "\t -f <feed-rate>    : maximum, in mm/s\n"
          "\t -T <layer-time>   : min time per layer; dynamically influences -f\n"
          "\t -p <pitch>        : how many mm height a full screw-turn takes.\n"
          "\t                     negative for left screw. 0 for straight hull\n"
          "\t -L <x,y>          : x/y size limit of your printbed.\n"
          "\t -o <dx,dy>        : dx/dy offset per print. Clearance needed from\n"
          "\t                     hotend-tip to left and front essentially.\n"
          "\n [ Output options ]\n"
          "\t -P                : PostScript output instead of GCode output\n"
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
static void CreateExtrusion(const Polygon &extrusion_polygon,
                            Printer *printer,
                            double offset_x, double offset_y,
                            double layer_height, double total_height,
                            double rotation_per_mm,
                            double lock_offset) {
  printer->Comment("Center X=%.1f Y=%.1f\n", offset_x, offset_y);
  const double rotation_per_layer = layer_height * rotation_per_mm * 2 * M_PI;
  bool fan_is_on = false;
  printer->SwitchFan(false);
  double height = 0;
  double angle = 0;
  double run_len = 0;
  const bool do_lock = (lock_offset > 0);
  double polygon_len = 0;
  Polygon p;  // active polygon.
  static const int kLockOverlap = 3;
  enum State {
    START, WIDE_LOCK, NORMAL, NARROW_LOCK
  };
  enum State state = START;
  enum State prev_state;
  for (height = 0, angle=0; height < total_height;
       height += layer_height, angle += rotation_per_layer) {
    prev_state = state;

    // What to print. For lock we're very simple: we just offset the
    // polygon, but don't do any transition for now.
    switch (state) {
    case START:
      if (do_lock) {
        state = WIDE_LOCK;
        p = PolygonOffset(extrusion_polygon, lock_offset);
      } else {
        state = NORMAL;
        p = extrusion_polygon;
      }
      break;

    case WIDE_LOCK:
      if (do_lock && height > kLockOverlap) {
        p = extrusion_polygon;
        state = NORMAL;
      }
      break;

    case NORMAL:
      if (do_lock && height > total_height - kLockOverlap) {
        p = PolygonOffset(extrusion_polygon, -lock_offset);
        state = NARROW_LOCK;
      }
      break;
    case NARROW_LOCK: /* terminal state */ break;
    }

    if (state != prev_state) {
      polygon_len = CalcPolygonLen(p);
      printer->MoveTo(p[0].x + offset_x, p[0].y + offset_y, height);
    }

    for (int i = 0; i < (int) p.size(); ++i) {
      if (i == 0) {
        run_len = 0;
      } else {
        run_len += distance(p[i].x - p[i-1].x, p[i].y - p[i-1].y, 0);
      }
      const double fraction = run_len / polygon_len;
      const double a = angle + fraction * rotation_per_layer;
      const double x = p[i].x * cos(a) - p[i].y * sin(a);
      const double y = p[i].y * cos(a) + p[i].x * sin(a);
      const double z = height + layer_height * fraction;
      if (z < total_height - 0.20 * layer_height) {
        printer->ExtrudeTo(x + offset_x, y + offset_y, z);
      } else {
        // In the last layer, we stop extruding to have a smooth finish.
        printer->MoveTo(x + offset_x, y + offset_y, z);
      }
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
    Point2D p;
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
  if (pump_r <= 0)
    return polygon;
  Polygon result;
  for (std::size_t i = 0; i < polygon.size(); ++i) {
    const Point2D &p = polygon[i];
    double from_center = distance(p.x, p.y, 0);
    double stretch = (from_center + pump_r) / from_center;
    result.push_back(Point2D(p.x * stretch, p.y * stretch));
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
  double start_x = 5;  // Initial edge offset.
  double start_y = 5;
  // Some useful default values.
  double pitch = 30.0;
  double layer_height = 0.16;
  double nozzle_radius = 0.4 / 2;
  double filament_radius = 1.75 / 2;
  double total_height = -1;
  double machine_limit_x = 150, machine_limit_y = 150;
  int faces = 720;
  double initial_size = 10.0;
  double shell_increment = 1.2;
  double initial_shell = 0;
  double head_offset_x = 45;
  double head_offset_y = 45;
  double feed_mm_per_sec = 100;
  double min_layer_time = 6;
  double thread_depth = -1;
  double shell_thickness_factor = 1.9;  // ~2*nozzzle = ~0.8mm shell thickness.
  int screw_count = 2;
  double twist = 0.0;
  double pump = 0.0;
  bool do_postscript = false;
  bool matryoshka = false;
  double lock_offset = -1;

  const char *fun_init = "AABBBAABBBAABBB";
  const char *data_file = NULL;

  int opt;
  while ((opt = getopt(argc, argv, "t:h:n:s:R:i:d:l:f:p:T:L:o:w:PD:u:mr:S:")) != -1) {
    switch (opt) {
    case 't': fun_init = strdup(optarg); break;
    case 'D': data_file = strdup(optarg); break;
    case 'h': total_height = atof(optarg); break;
    case 'n': screw_count = atoi(optarg); break;
    case 's': initial_size = atof(optarg); break;
    case 'i': initial_shell = atof(optarg); break;
    case 'R': shell_increment = atof(optarg); break;
    case 'S': lock_offset = atof(optarg); break;
    case 'd': thread_depth = atof(optarg); break;
    case 'l': layer_height = atof(optarg); break;
    case 'f': feed_mm_per_sec = atof(optarg); break;
    case 'T': min_layer_time = atof(optarg); break;
    case 'w': twist = atof(optarg); break;
    case 'u': pump = atof(optarg); break;
    case 'm': matryoshka = true; break;
    case 'r':
      fprintf(stderr,
              "Hello old-time user that knows about this option:\n"
              "The option -r does not exist anymore. You "
              "now want to look at -s\n");
      return 1;
      break;
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

  // Get polygon we'll be working on. Add pump if needed.
  const Polygon base_polygon =
    RadialPumpPolygon(data_file
                      ? ReadPolygon(data_file, initial_size)
                      : RotationalPolygon(fun_init, initial_size,
                                          thread_depth, twist),
                      pump);

  if (base_polygon.empty()) {
    fprintf(stderr, "Polygon empty\n");
    return 1;
  }

  if (matryoshka) {
    Polygon biggst_polygon
      = PolygonOffset(base_polygon,
                      initial_shell + (screw_count-1) * shell_increment);
    double max_radius = GetRadius(biggst_polygon);
    machine_limit_x = 2 * (max_radius + 5);
    machine_limit_y = 2 * (max_radius + 5);
    start_x = max_radius + 5;
    start_y = max_radius + 5;
  }

  const double filament_extrusion_factor = shell_thickness_factor *
    (nozzle_radius * (layer_height/2)) / (filament_radius*filament_radius);

  Printer *printer = NULL;
  if (do_postscript) {
    total_height = std::min(total_height, 3 * layer_height); // not needed more.
    // no move lines w/ Matryoshka
    printer = CreatePostscriptPrinter(!matryoshka,
                                      shell_thickness_factor * 2*nozzle_radius);
  } else {
    printer = CreateGCodePrinter(filament_extrusion_factor);
  }
  printer->Preamble(machine_limit_x, machine_limit_y, feed_mm_per_sec);

  printer->Comment("https://github.com/hzeller/gcode-multi-shell-extrude\n");
  printer->Comment("\n");
  printer->Comment(" ");
  for (int i = 0; i < argc; ++i)
    printf("%s ", argv[i]);
  printf("\n");
  printer->Comment("\n");
  if (data_file) {
    printer->Comment("Polygon from datafile '%s'\n", data_file);
  } else {
    printer->Comment("Polygon from screw template '%s'\n", fun_init);
  }
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
  const double rotation_per_mm = (fabs(pitch) < 0.1) ? 0 : 1.0 / pitch;

  double total_time = 0;
  double total_travel = 0;

  double x = start_x;
  double y = start_y;
  printer->SetSpeed(feed_mm_per_sec);  // initial speed.
  for (int i = 0; i < screw_count; ++i) {
    Polygon polygon = PolygonOffset(base_polygon,
                                    initial_shell + i * shell_increment);
    double radius = GetRadius(polygon);
    if (!matryoshka) {
      // New center.
      x += radius;
      y += radius;
    }
    if (x + radius + 5 > machine_limit_x || y + radius + 5 > machine_limit_y) {
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
    printer->Comment("Screw #%d, polygon-offset=%.1f\n",
                     i+1, initial_shell + i * shell_increment);
    CreateExtrusion(polygon, printer, x, y, layer_height, total_height,
                    rotation_per_mm, lock_offset);
    const double travel = printer->GetExtrusionDistance();  // since last reset.
    total_travel += travel;
    total_time += travel / layer_feedrate;  // roughly (without acceleration)
    printer->SetSpeed(feed_mm_per_sec);
    printer->Retract();
    printer->GoZPos(total_height + 5);
    if (!matryoshka) {
      x += head_offset_x + radius;
      y += head_offset_y + radius;
    }
  }

  printer->Postamble();
  if (total_time > 0) {  // doesn't make sense to print for PostScript
    fprintf(stderr, "Total time >= %.0f seconds; %.2fm filament\n",
            total_time, total_travel * filament_extrusion_factor / 1000);
  }
}
