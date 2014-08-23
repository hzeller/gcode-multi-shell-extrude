/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 * (c) 2014 Henner Zeller <h.zeller@acm.org>
 * Creative commons BY-SA
 */

#include "multi-shell-extrude.h"

#include <stdio.h>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Polygon_2.h>
#include <CGAL/create_offset_polygons_2.h>

typedef CGAL::Exact_predicates_inexact_constructions_kernel CG_K ;
typedef CG_K::FT                        CG_FT;
typedef CG_K::Point_2                   CG_Point ;
typedef CGAL::Polygon_2<CG_K>           CG_Polygon_2 ;
typedef CGAL::Straight_skeleton_2<CG_K> CG_Ss ;
typedef boost::shared_ptr<CG_Polygon_2> CG_PolygonPtr ;
typedef std::vector<CG_PolygonPtr> CG_PolygonVector;

// Essentially Minkowsky, but simplified using skeletons
// http://doc.cgal.org/latest/Straight_skeleton_2/index.html
Polygon PolygonOffset(const Polygon &polygon, double offset) {
  CG_Polygon_2 cg_poly;
  for (std::size_t i = 0; i < polygon.size(); ++i) {
    const Point &p = polygon[i];
    cg_poly.push_back(CG_Point(p.x, p.y));
  }
  cg_poly.push_back(CG_Point(polygon[0].x, polygon[0].y));
  CG_FT cg_offset = offset;
  
  CG_PolygonVector cg_outer
    = CGAL::create_exterior_skeleton_and_offset_polygons_2(cg_offset, cg_poly);
  
  Polygon result;
  CG_Polygon_2 &cg_out_poly = *cg_outer[1];  // [0] contains bounding box (?)
  // For some reason, the resulting polygon turns around right; to get a left
  // turning polygon, reverse it.
  for (int i = (int)cg_out_poly.size() - 1; i >= 0; --i) {
    result.push_back(Point(cg_out_poly[i].x(), cg_out_poly[i].y()));
  }

  return result;
}
