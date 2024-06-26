/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 * (c) 2014 Henner Zeller <h.zeller@acm.org>
 * Creative commons BY-SA
 */

#include <assert.h>
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
#include "config-values.h"

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

// Get temperature for layer. Right now, this is a simple sin(), but
// could be something more pleasingly erratic, such as Perlin noise.
static float GetLayerTemperature(float base_temp, float variation,
                                 float height, float noise_feature) {
  return sin(2 * M_PI * height / noise_feature) * variation + base_temp;
}

static void CreateBottomPlate(const Polygon &target_polygon,
                              Printer *printer,
                              const Vector2D &center_offset,
                              float outer_distance, float inner_distance,
                              float spiral_distance) {
  bool is_first = true;
  // Initial height.
  const float z_height = spiral_distance/2;
  const Vector2D centroid = Centroid(target_polygon);
  for (float poffset = outer_distance;
       poffset > inner_distance; poffset -= spiral_distance) {
    Polygon p = PolygonOffset(target_polygon, poffset);
    if (p.size() == 0)
      return;   // Natural end of moving towards center.
    float run_len = 0;
    const float polygon_len = CalcPolygonLen(p);
    // fudging a spiral: we want that the distance from the center
    // is one spiral_distance less in the end.
    float outer_distance = (p[0] - centroid).magnitude();
    for (int i = 0; i < (int) p.size(); ++i) {
      if (i == 0) {
        run_len = 0;
      } else {
        run_len += (p[i] - p[i-1]).magnitude();
      }
      Vector2D current_point_from_center = p[i] - centroid;
      const double fraction = run_len / polygon_len;
      float spiral_adjust = (outer_distance - fraction*spiral_distance)/outer_distance;
      current_point_from_center = current_point_from_center * spiral_adjust;
      Vector2D next_pos = center_offset + centroid + current_point_from_center;
      if (is_first)
        printer->MoveTo(next_pos, z_height);
      else
        printer->ExtrudeTo(next_pos, z_height, 1.0);
      is_first = false;
    }
  }
}

struct ExtrusionParams {
  double feedrate;
  double layer_height;
  double total_height;
  double rotation_per_mm;
  double lock_offset;
  double fan_on_height;
  double elephant_foot_multiplier;
  double first_layer_feedrate_multiplier;

  float base_temp;
  float temp_variation;
};

// Requires: Polygon with centroid on (0,0)
static void CreateExtrusion(const Polygon &extrusion_polygon, Printer *printer,
                            const Vector2D &center,
                            const ExtrusionParams &params) {
  printer->Comment("Center X=%.1f Y=%.1f\n", center.x, center.y);
  printer->SetColor(0, 0, 0);
  const float z_bottom_offset = params.layer_height / 2;
  const double rotation_per_layer =
      params.layer_height * params.rotation_per_mm * 2 * M_PI;
  bool fan_is_on = false;
  printer->SwitchFan(false);
  double height = 0;
  double angle = 0;
  double run_len = 0;
  const bool do_lock = (params.lock_offset > 0);
  double polygon_len = 0;
  Polygon p; // active polygon.
  static const int kLockOverlap = 3;
  enum State { START, WIDE_LOCK, NORMAL, NARROW_LOCK };
  enum State state = START;
  enum State prev_state;
  for (height = 0, angle = 0; height < params.total_height;
       height += params.layer_height, angle += rotation_per_layer) {
    printer->SetTemperature(GetLayerTemperature(
        params.base_temp, params.temp_variation, height, 30));
    prev_state = state;

    // Experimental. Locking screws do have smaller/larger diameter at their
    // ends. This goes through the state transitions.
    // What to print. For locking screw we're very simple: we just offset the
    // polygon, but don't do any transition for now.
    // TODO: re-arrange polygon to start at same angle.
    switch (state) {
    case START:
      if (do_lock) {
        state = WIDE_LOCK;
        p = PolygonOffset(extrusion_polygon, params.lock_offset);
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
      if (do_lock && height > params.total_height - kLockOverlap) {
        p = PolygonOffset(extrusion_polygon, -params.lock_offset);
        state = NARROW_LOCK;
      }
      break;
    case NARROW_LOCK: /* terminal state */
      break;
    }

    if (state != prev_state) {
      polygon_len = CalcPolygonLen(p);
      // First move slowly, so that we wipe potential nozzle leak extrusion
      printer->SetSpeed(std::min(params.feedrate / 3, 15.0));
      printer->MoveTo(p[0] + center, height + z_bottom_offset);
    }

    for (int i = 0; i < (int)p.size(); ++i) {
      if (i == 0) {
        run_len = 0;
      } else {
        run_len += distance(p[i].x - p[i - 1].x, p[i].y - p[i - 1].y, 0);
      }
      const double fraction = run_len / polygon_len;
      const double a = angle + fraction * rotation_per_layer;
      const Vector2D point = rotate(p[i], a);
      const double z = height + params.layer_height * fraction;
      const bool is_initial_layers = z < 2 * params.layer_height;
      // Speed: keep slow while initial layers, then lerp-ing up to full
      // speed within 4 more layers
      if (is_initial_layers) {
        printer->SetSpeed(params.feedrate *
                          params.first_layer_feedrate_multiplier);
      } else if (z < 4 * params.layer_height) {
        const double range = 1.0 - params.first_layer_feedrate_multiplier;
        const double lerp = (z - 2 *  params.layer_height)
          / ((4 - 2) * params.layer_height);
        printer->SetSpeed(params.feedrate *
                          (params.first_layer_feedrate_multiplier
                           + lerp * range));
      } else {
        printer->SetSpeed(params.feedrate);
      }
      // Start only extruding when min z-offset reached and also stop extruding
      // at the top to wipe off excess
      if (z > z_bottom_offset / 2 &&
          z < params.total_height - 0.30 * params.layer_height) {
        printer->ExtrudeTo(point + center, z,
                           (is_initial_layers)
                           ? params.elephant_foot_multiplier
                           : 1.0);
      } else {
        // In the last layer, we stop extruding to have a smooth finish.
        printer->MoveTo(point + center, z);
      }
    }

    if (height > params.fan_on_height && !fan_is_on) {
      printer->SwitchFan(true); // reached fan-on height: switch on.
      fan_is_on = true;
    }
  }
}

Polygon OffsetCenter(const Polygon& polygon, double x_offset, double y_offset) {
  Polygon result;
  for (const Vector2D &p : polygon) {
    result.push_back(Vector2D(p.x + x_offset, p.y + y_offset));
  }
  return result;
}

// Read very simple polygon from file: essentially a sequence of x y
// coordinates.
Polygon ReadPolygon(const std::string &filename, double factor) {
  Polygon polygon;
  FILE *in = fopen(filename.c_str(), "r");
  if (!in) {
    fprintf(stderr, "Can't open %s\n", filename.c_str());
    return polygon;
  }
  char buffer[256];
  int line = 0;
  while (fgets(buffer, sizeof(buffer), in)) {
    ++line;
    const char *start = buffer;
    while (*start && isspace(*start))
      start++;
    if (*start == '\0' || *start == '#')
      continue;
    Vector2D p;
    if (sscanf(start, "%lf %lf", &p.x, &p.y) == 2) {
      p.x *= factor;
      p.y *= factor;
      polygon.push_back(p);
    } else {
      for (char *end = buffer + strlen(buffer) - 1; isspace(*end); end--) {
        *end = '\0';
      }
      fprintf(stderr, "%s:%d not a comment and not coordinates: '%s'\n",
              filename.c_str(), line, start);
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
  for (const Vector2D &p : polygon) {
    double from_center = distance(p.x, p.y, 0);
    double stretch = (from_center + pump_r) / from_center;
    result.push_back(Vector2D(p.x * stretch, p.y * stretch));
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
  ParamHeadline h1("Screw-data from template");
  StringParam fun_init    ("AABBBAABBBAABBB", "screw-template", 't', "Template string for screw.");
  FloatParam thread_depth (-1, "thread-depth", 'd',   "Depth of thread, initial-size/5 if negative");
  FloatParam twist        (0.0, "twist",        0,    "Twist ratio of angle per radius fraction (good -0.3..0.3)");

  ParamHeadline h2("Screw-data from polygon file");
  StringParam polygon_file("", "polygon-file", 'D',  "File describing polygon. Files with x y pairs");

  ParamHeadline h3("General Parameters");
  FloatParam total_height (-1,    "height", 'h', "Total height to be printed (must set)");
  FloatParam pitch        (30.0,  "pitch",  'p', "Millimeter height a full turn takes. "
                           "Negative for left-turning screw; 0 for straight hull.");
  FloatParam initial_size (10.0, "size",    's', "Polygon sizing parameter. Means radius if from "
                           "--screw-template, factor for --polygon-file");
  Vector2DParam center_offset(Vector2D(0.0, 0.0), "center-offset", 0, "Rotation-center offset into polygon.");
  BoolParam  auto_center(false, "auto-center", 0, "Automatically center around centroid.");
  FloatParam pump         (0.0,   "pump",    0, "Pump polygon as if the center was not a dot, but a circle of this radius");
  IntParam screw_count    (2,     "number", 'n', "Number of screws to be printed");
  FloatParam initial_shell(0,     "start-offset", 0, "Initial offset for first polygon");
  FloatParam shell_increment(1.2, "offset", 'R', "Offset increment between screws - the clearance");
  FloatParam lock_offset  (-1,    "lock-offset", 0, "EXPERIMENTAL offset to stop screw at end; Approx value: (offset - shell_thickness)/2 + 0.05");
  FloatParam brim(0, "brim", 0, "Add brim of this size on the bottom for better stability");
  FloatParam brim_spiral_factor(0.55, "brim-spiral-factor", 0,
                               "Distance between spirals in brim as factor of shell-thickness");
  FloatParam brim_smooth_radius(0, "brim-smooth-radius", 0, "Smoothing of brim connection to polygon to not get lost in inner details");
  BoolParam vessel(false, "vessel", 0, "Make a vessel with closed bottom");

  ParamHeadline h4("Quality");
  FloatParam layer_height (0.16,  "layer-height", 'l', "Height of each layer");
  FloatParam shell_thickness(0.8, "shell-thickness", 0, "Thickness of shell");
  FloatParam feed_mm_per_sec(100, "feed-rate",    'f', "maximum, in mm/s");
  FloatParam min_layer_time(3,    "layer-time",   'T', "Min time per layer; upper bound for feed-rate");
  FloatParam fan_on (0.3,  "fan-on-height", 0, "Height to switch on fan");
  FloatParam elephant_foot_multiplier (0.9,  "slender-elephant", 0, "Extrusion multiplier at first two layer heights to prevent elephant foot");
  FloatParam retract_amount (1.2, "retract", 0, "Millimeter of retract");
  FloatParam first_layer_feed_multiplier (0.7, "first-layer-speed", 0, "Feedrate multiplier for first layer");
  ParamHeadline h5("Printer Parameters");
  FloatParam nozzle_diameter(0.4, "nozzle-diameter", 0, "Diameter of extruder nozzle");
  FloatParam bed_temp(-1, "bed-temp", 0, "Bed temperature.");
  FloatParam temperature(190, "temperature", 0, "Extrusion temperature.");
  FloatParam temp_variation(0, "temperature-variation", 0, "Temperature variation around --temperature, e.g. to get dark lines in wood filament.");
  FloatParam filament_diameter(1.75, "filament-diameter", 0, "Diameter of filament");
  Vector2DParam machine_limit(Vector2D(150.0,150.0), "bed-size",    'L',  "x/y size limit of your printbed.");
  Vector2DParam head_offset(Vector2D(45.0,45.0),"head-offset", 'o', "dx/dy offset per print.");
  Vector2DParam edge_offset(Vector2D(5.0,5.0), "edge-offset",  0,  "Offset from the edge of the bed (bottom left origin).");

  // Output options
  ParamHeadline h6("Output Options");
  BoolParam do_postscript(false, "postscript", 'P', "PostScript output instead of GCode output");
  FloatParam postscript_thick_factor(1.0, "ps-thick-factor", 0, "Line thickness factor for shell size. Chooser smaller (e.g. 0.1) to better see overlaps");
  BoolParam matryoshka(false,    "nested",      0, "For PostScript: show nested (Matryoshka doll style)");

  if (!SetParametersFromCommandline(argc, argv)) {
    return ParameterUsage(argv[0]);
  }

  if (total_height < 0) {
    fprintf(stderr, "\n--height needs to be set\n\n");
    return ParameterUsage(argv[0]);
  }

  if (thread_depth < 0)
    thread_depth = initial_size / 5;

  if (matryoshka && !do_postscript) {
    fprintf(stderr, "Matryoshka mode only valid with postscript\n");
    return ParameterUsage(argv[0]);
  }

  // Calculated values from input parameters.
  const double nozzle_radius = nozzle_diameter / 2;
  const double filament_radius = filament_diameter / 2;
  const double shell_thickness_factor = shell_thickness / nozzle_diameter;

  matryoshka = matryoshka & do_postscript;   // Formulate it this way.

  // Get polygon we'll be working on; either from rotational input or file.
  Polygon input_polygon = (polygon_file.get().empty()
                           ? RotationalPolygon(fun_init.get().c_str(),
                                               initial_size,
                                               thread_depth, twist)
                           : ReadPolygon(polygon_file, initial_size));

  // Add pump if needed.
  if (pump > 0) {
    input_polygon = RadialPumpPolygon(input_polygon, pump);
  }

  if (auto_center) {
    center_offset = Centroid(input_polygon);
    center_offset = Vector2D(0,0) - center_offset;
  }

  // .. and offsetting
  if (center_offset->x != 0 || center_offset->y != 0) {
    input_polygon = OffsetCenter(input_polygon,
                                 center_offset->x, center_offset->y);
  }

  const Polygon base_polygon = input_polygon;
  if (base_polygon.empty()) {
    fprintf(stderr, "Polygon empty\n");
    return 1;
  }
  if (base_polygon.size() < 3) {
    fprintf(stderr, "Polygon is a %sgon :) Need at least 3 vertices.\n",
            base_polygon.size() == 1 ? "Mono" : "Duo");
    return 1;
  }

  // Determine limits
  if (matryoshka) {
    Polygon biggst_polygon
      = PolygonOffset(base_polygon,
                      initial_shell + (screw_count-1) * shell_increment);
    double max_radius = GetRadius(biggst_polygon) + brim;
    Vector2D poly_radius(max_radius + 5, max_radius + 5);
    machine_limit = poly_radius * 2;
    edge_offset = poly_radius;  // In matryoshka-case, edge_offset is center
  } else {
    const Vector2D max_machine = machine_limit - edge_offset;
    Vector2D pos = edge_offset;
    float radius = GetRadius(PolygonOffset(base_polygon, initial_shell));
    Vector2D screw_dimension(2 * (radius + brim), 2*(radius + brim));
    for (int i = 0; i < screw_count; ++i) {
      Vector2D new_pos = pos + screw_dimension;
      if (new_pos.x > max_machine.x || new_pos.y > max_machine.y) {
        fprintf(stderr, "With currently configured bedsize and printhead-offset, "
                "only %d screws fit (radius is %.1fmm)\n"
                "Configure your machine constraints with -L <x/y> -o < dx,dy> "
                "(currently -L %.0f,%.0f -o %.0f,%.0f)\n", i, radius,
                machine_limit->x, machine_limit->y,
                head_offset->x, head_offset->y);
        screw_count = i;
        break;
      }
      pos = new_pos + head_offset;
      screw_dimension = screw_dimension
        + Vector2D(shell_increment, shell_increment) * 2;
    }
    pos = pos - head_offset;
    // Now, pos is the largest corner. We can offset
    // the edge_offset now to the difference to center things.
    edge_offset = edge_offset + (max_machine - pos - edge_offset) / 2;
  }

  const double filament_extrusion_factor = shell_thickness_factor *
    (nozzle_radius * (layer_height/2)) / (filament_radius*filament_radius);

  Printer *printer = NULL;
  if (do_postscript) {
    total_height = std::min(total_height.get(),
                            3 * layer_height); // not needed more.
    // no move lines w/ Matryoshka
    printer = CreatePostscriptPrinter(!matryoshka,
                                      postscript_thick_factor * shell_thickness);
  } else {
    printer = CreateGCodePrinter(filament_extrusion_factor, retract_amount,
                                 temperature, bed_temp);
  }
  printer->Preamble(machine_limit, feed_mm_per_sec);

  printer->Comment("https://github.com/hzeller/gcode-multi-shell-extrude\n");
  printer->Comment("\n");
  printer->Comment(" ");
  for (int i = 0; i < argc; ++i)
    printf("%s ", argv[i]);
  printf("\n");
  printer->Comment("\n");
  if (!polygon_file.get().empty()) {
    printer->Comment("Polygon from polygon-file '%s'\n",
                     polygon_file.get().c_str());
    printer->Comment("size-factor=%.1f\n", initial_size.get());
  } else {
    printer->Comment("Polygon from screw template '%s'\n",
                     fun_init.get().c_str());
    printer->Comment("thread-depth=%.1fmm size=%.1fmm (radius)\n",
                     thread_depth.get(), initial_size.get());
  }
  printer->Comment("h=%.1fmm n=%d (shell-increment=%.1fmm)\n",
                   total_height.get(), screw_count.get(), shell_increment.get());
  printer->Comment("feed=%.1fmm/s (maximum; layer time at least %.1f s)\n",
                   feed_mm_per_sec.get(), min_layer_time.get());
  printer->Comment("pitch=%.1fmm/turn layer-height=%.3f\n",
                   pitch.get(), layer_height.get());
  printer->Comment("machine limits: bed: (%.0f/%.0f):  "
                  "head-offset: (%.0f,%.0f)\n",
                   machine_limit->x, machine_limit->y,
                   head_offset->x, head_offset->y);
  printer->Comment("----\n");

  printer->Init(machine_limit, feed_mm_per_sec);

  // How much the whole system should rotate per mm height.
  const double rotation_per_mm = (fabs(pitch) < 0.1) ? 0 : 1.0 / pitch;

  double total_time = 0;
  double total_travel = 0;

  constexpr float kHoverPos = 10.0;  // Hovering over screws while moving
  Vector2D center = edge_offset;
  printer->SetSpeed(feed_mm_per_sec);  // initial speed.
  for (int i = 0; i < screw_count; ++i) {
    const float current_offset = initial_shell + i * shell_increment;
    Polygon polygon = PolygonOffset(base_polygon, current_offset);
    if (polygon.size() == 0) {
      fprintf(stderr, "Polygon offset %.1f results in empty polygon\n",
              initial_shell + i * shell_increment);
      continue;
    }
    const double radius = GetRadius(polygon);
    Vector2D screw_radius(radius + brim, radius + brim);
    if (!matryoshka) {
      // We start here.
      center = center + screw_radius;
    }
    printer->MoveTo(center, i > 0 ? total_height + kHoverPos : kHoverPos);
    const float polygon_len = CalcPolygonLen(polygon);
    const float area = polygon_len * total_height * 2;  // inside and out.
    float layer_feedrate =  polygon_len / min_layer_time;
    layer_feedrate = std::min(layer_feedrate, feed_mm_per_sec.get());
    printer->ResetExtrude();
    printer->SetSpeed(layer_feedrate);
    printer->Comment("Screw #%d, polygon-offset=%.1f\n",
                     i+1, initial_shell + i * shell_increment);
    if (vessel) {
      const float spiral_layer_distance = shell_thickness * brim_spiral_factor;
      printer->Comment("Create vessel-bottom\n");
      printer->SetColor(0.5, 0, 0.5);
      CreateBottomPlate(polygon, printer, center,
                        0, -radius, spiral_layer_distance);
      // TODO: make this multi-layer.
      printer->GoZPos(2);
    }

    if (brim > 0) {
      const float spiral_layer_distance = shell_thickness * brim_spiral_factor;
      int layers = (int) ceil(brim / spiral_layer_distance);
      Polygon brim_polygon = polygon;
      if (brim_smooth_radius > 0)
        brim_polygon = PolygonOffset(PolygonOffset(polygon, brim_smooth_radius), -brim_smooth_radius);
      printer->Comment("Create brim\n");
      printer->SetColor(0, 0.5, 0);
      CreateBottomPlate(brim_polygon, printer, center,
                        layers * spiral_layer_distance, spiral_layer_distance/2,
                        spiral_layer_distance);
    }
    ExtrusionParams params = {
      .feedrate = layer_feedrate,
      .layer_height = layer_height,
      .total_height = total_height,
      .rotation_per_mm = rotation_per_mm,
      .lock_offset = lock_offset,
      .fan_on_height = fan_on,
      .elephant_foot_multiplier = elephant_foot_multiplier,
      .first_layer_feedrate_multiplier = first_layer_feed_multiplier,
      .base_temp = temperature,
      .temp_variation = temp_variation
    };

    CreateExtrusion(polygon, printer, center, params);
    const double travel = printer->GetExtrusionDistance();  // since last reset.
    total_travel += travel;
    total_time += travel / layer_feedrate;  // roughly (without acceleration)
    printer->SetSpeed(feed_mm_per_sec);
    printer->Retract();
    printer->GoZPos(total_height + kHoverPos);
    if (!matryoshka) {
      center = center + screw_radius + head_offset;
    }
    if (!do_postscript) {
      fprintf(stderr, "Screw-surface (out+in) for offset %.1f: ~%.1f cm²\n",
              current_offset, area / 100);
    }
  }

  printer->Postamble();
  if (!do_postscript) {  // doesn't make sense to print for PostScript
    int t = (int)total_time;
    const int hours = t / 3600;
    t %= 3600;
    const int minutes = t / 60;
    const int seconds = t % 60;
    fprintf(stderr, "Total time >= %02d:%02d:%02d; %.2fm filament\n", hours,
            minutes, seconds, total_travel * filament_extrusion_factor / 1000);
  }
}
