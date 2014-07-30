/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 * (c) 2014 Henner Zeller <h.zeller@acm.org>
 * GNU General Public License Version 3.0
 */

#include <assert.h>
#include <getopt.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>
#include <algorithm>

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

static double distance(double dx, double dy, double dz) {
  return sqrt(dx*dx + dy*dy + dz*dz);
}

static int quantize_up(int x, int q) {
  return q * ((x + q - 1) / q);
}

static int usage(const char *progname) {
  fprintf(stderr, "Usage: %s -t <template> [-p <height-per-rotation-mm>]\n",
          progname);
  fprintf(stderr,
          "Template describes shape. The letters in that string describe the\n"
          "screw depth for a full turn.\n"
          "A template 'AAZZZAAZZZAAZZZ' is a screw with three parallel threads,"
          "\nwith 'inner parts' (the one with the lower letter 'A') being 2/3\n"
          "the width of the outer parts. 'AAZZZ' would have one thread per turn."
          "\nThe string-length represents a full turn, so 'AAZZZZZZZZ' would\n"
          "have one narrow thread.\n"
          "The range of letters the depth. Try 'AAZZMMZZZZ'.\n"
          "Required parameter: -h <height>\n\n"
          "\t -t <template>     : template string, described above\n"
          "\t -h <height>       : Total height to be printed\n"
          "\t -n <number-of-screws> : number of screws to be printed\n"
          "\t -r <radius>       : radius of the smallest screw\n"
          "\t -R <radius-increment> : increment between screws\n"
          "\t -d <thread-depth> : depth of thread (default: radius/5)\n"
          "\t -l <layer-height> : Height of each layer\n"
          "\t -f <feed-rate>    : maximum, in mm/s\n"
          "\t -T <layer-time>   : min time per layer; dynamically influences -f\n"
          "\t -p <pitch>        : how many mm height a full screw-turn takes\n");
  return 1;
}

// Returns total amount of travel by nozzle.
static double CreateExtrusion(PolarFunction *fun, double thread_depth,
                              double offset_x, double offset_y, double radius,
                              double height_step, double total_height,
                              double angle_step, double extrusion_factor) {
  printf("; Screw center X=%.1f Y=%.1f r=%.1f thread-depth=%.1f\n",
         offset_x, offset_y, radius, thread_depth);
  bool fan_is_on = false;
  printf("M106 S0 ; fan off initially\n");
  double angle = 0;
  double last_x = radius, last_y = 0, last_h = 0;
  double total_dist = 0;
  double height = 0;
  for (height=0, angle=0; height < total_height;
       height+=height_step, angle+=angle_step) {
    const double r = radius + thread_depth * fun->value(angle, height);
    const double x = r * cos(angle * 2 * M_PI);
    const double y = r * sin(angle * 2 * M_PI);
    total_dist += distance(x - last_x, y - last_y, height - last_h);
    printf("G1 X%.3f Y%.3f Z%.3f E%.3f\n",
           x + offset_x, y + offset_y, height,
           total_dist * extrusion_factor);
    last_x = x; last_y = y; last_h = height;
    if (height > 1.5 && !fan_is_on) {
      printf("M106 S255 ; 1.5mm reached - fan on\n");
      fan_is_on = true;
    }
  }
  return total_dist;
}

int main(int argc, char *argv[]) {
  // Some useful default values.
  double pitch = 30.0;
  double layer_height = 0.16;
  double nozzle_radius = 0.4 / 2;
  double filament_radius = 1.75 / 2;
  double total_height = -1;
  int faces = 720;
  double radius = 10.0;
  double radius_increment = 1.6;
  double start_x = 40;
  double start_y = 40;
  double offset_x = 50;
  double offset_y = 50;
  double feed_mm_per_sec = 100;
  double min_layer_time = 4.5;
  double thread_depth = -1;
  double extrusion_fudge_factor = 1.9;  // empiric...
  int screw_count = 3;

  const char *fun_init = "AABBBAABBBAABBB";

  int opt;
  while ((opt = getopt(argc, argv, "t:h:n:r:R:d:l:f:p:T:")) != -1) {
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
    case 'p': pitch = atof(optarg); break;
    default:
      return usage(argv[0]);
    }
  }

  if (total_height < 0)
    return usage(argv[0]);

  if (thread_depth < 0)
    thread_depth = radius / 5;

  if (thread_depth >= radius) {
    fprintf(stderr, "Thread-depth larger than radius won't work :)\n");
    return 1;
  }

  const double filament_extrusion_factor = extrusion_fudge_factor *
    (nozzle_radius * (layer_height/2)) / (filament_radius*filament_radius);

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
  faces = quantize_up(faces, strlen(fun_init));   // same sampling per letter.
  printf("; https://github.com/hzeller/gcode-multi-shell-extrude\n");
  printf(";\n; ");
  for (int i = 0; i < argc; ++i) printf("%s ", argv[i]);
  printf("\n;\n");
  printf("; screw template '%s'\n"
         "; r=%.1fmm h=%.1fmm n=%d (radius-increment=%.1fmm)\n"
         "; thread-depth=%.1fmm faces=%d\n"
         "; feed=%.1fmm/s (maximum; layer time at least %.1f s)\n"
         "; pitch=%.1fmm/turn layer-height=%.3f\n"
         ";----\n",
         fun_init,
         radius, total_height, screw_count, radius_increment,
         thread_depth, faces,
         feed_mm_per_sec, min_layer_time, pitch,
         layer_height);

  printf("G28\nG1 F%.1f\n", feed_mm_per_sec * 60);
  printf("G1 X150 Y10 Z30\n");
  printf("M109 S190\nM116\n");
  printf("M82 ; absolute extrusion\n");
  double rotation_per_mm = (pitch == 0) ? 10000000.0 : 1.0 / pitch;
  PolarFunction f((unsigned char*) fun_init, rotation_per_mm);

  printf("G92 E0  ; nozzle clean extrusion\n");
  printf("G1 X250 Y10 Z0\nG1 X100 Y10 E%.3f F1000\n",
         150 /*mm*/ * filament_extrusion_factor);
  printf("G1 Z5\n");
  printf("M83\nG1 E-3 ; retract\nM82\n");  // relative, retract, absolute

  double total_time = 0;
  double total_travel = 0;
  // We want to overshoot with the number of faces one rotation a bit so
  // that we are matching up with the start of the new rotation of the polar
  // function. That way, we can have few faces without alias problems.
  const double angle_step = (1.0 + (rotation_per_mm * layer_height)) / faces;
  const double height_step = layer_height * angle_step;
  double x = start_x + radius;
  double y = start_y + radius;
  printf("G1 F%.1f\n", feed_mm_per_sec * 60);  // initial speed.
  for (int i = 0; i < screw_count; ++i) {
    printf("G1 X%.3f Y%.3f\n", x, y);  // got to center
    double layer_feedrate = 2 * M_PI * radius / min_layer_time;
    layer_feedrate = std::min(layer_feedrate, feed_mm_per_sec);
    printf("G92 E0  ; start extrusion\n");
    printf("G1 F%.1f  ; feedrate = %.1fmm/s\n", layer_feedrate * 60,
           layer_feedrate);
    double travel = CreateExtrusion(&f, thread_depth,
                                    x, y, radius, height_step, total_height,
                                    angle_step, filament_extrusion_factor);
    total_travel += travel;
    total_time += travel / layer_feedrate;  // roughly (without acceleration)
    printf("G1 F%.1f\n", feed_mm_per_sec * 60);
    printf("M83\nG1 E-3 ; retract\nM82\n");
    printf("G1 Z%.3f\n", total_height + 5);
    x += offset_x + radius;
    y += offset_y + radius;
    radius += radius_increment;
  }

  printf("M104 S0\nM84\n");
  printf("M106 S0 ; fan off\n");
  printf("G1 Z%.3f\n", total_height + 5);
  printf("G28 X0 Y0\n");
  fprintf(stderr, "Total time about %.0f seconds; %.2fm filament\n",
          total_time, total_travel * filament_extrusion_factor / 1000);
}
