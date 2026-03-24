#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include "svg_types.h"

namespace ugui {
namespace svg {

// ============================================================================
// Constants
// ============================================================================

static constexpr i32 AA_LEVEL = 8;         // sub-scanlines per pixel row
static constexpr f32 FLATTEN_TOL = 0.25f;  // bezier flatten tolerance (pixels)
static constexpr f32 INV_AA = 1.0f / AA_LEVEL;

// ============================================================================
// Edge representation
// ============================================================================

struct Edge {
  f32 x0, y0, x1, y1;  // y0 < y1 guaranteed
  i32 dir;             // +1 going down, -1 going up (for winding number)
};

// ============================================================================
// Path flattening: convert cubics to line segments
// ============================================================================

static void flatten_cubic(Vector<Vec2>& out, Vec2 p0, Vec2 c1, Vec2 c2, Vec2 p3,
                          f32 tol) {
  // Adaptive subdivision based on flatness
  f32 dx = p3.x - p0.x;
  f32 dy = p3.y - p0.y;
  f32 d2 = std::fabs((c1.x - p3.x) * dy - (c1.y - p3.y) * dx);
  f32 d3 = std::fabs((c2.x - p3.x) * dy - (c2.y - p3.y) * dx);

  f32 flatness = (d2 + d3);
  f32 len_sq = dx * dx + dy * dy;
  f32 tol_sq = tol * tol * len_sq;

  if (flatness * flatness <= tol_sq || len_sq < 0.25f) {
    out.push_back(p3);
    return;
  }

  // de Casteljau split at t=0.5
  Vec2 m01 = (p0 + c1) * 0.5f;
  Vec2 m12 = (c1 + c2) * 0.5f;
  Vec2 m23 = (c2 + p3) * 0.5f;
  Vec2 m012 = (m01 + m12) * 0.5f;
  Vec2 m123 = (m12 + m23) * 0.5f;
  Vec2 mid = (m012 + m123) * 0.5f;

  flatten_cubic(out, p0, m01, m012, mid, tol);
  flatten_cubic(out, mid, m123, m23, p3, tol);
}

static void flatten_path(const Path& path, const Transform& xform,
                         Vector<Vec2>& points, Vector<u32>& subpath_starts) {
  Vec2 cur = {0, 0};
  Vec2 subpath_start = {0, 0};

  for (auto& entry : path.entries) {
    switch (entry.cmd) {
      case PathCmd::kMoveTo: {
        Vec2 p = xform.Apply(entry.pts[0]);
        subpath_starts.push_back(static_cast<u32>(points.size()));
        points.push_back(p);
        cur = p;
        subpath_start = p;
        break;
      }
      case PathCmd::kLineTo: {
        Vec2 p = xform.Apply(entry.pts[0]);
        points.push_back(p);
        cur = p;
        break;
      }
      case PathCmd::kCubicTo: {
        Vec2 c1 = xform.Apply(entry.pts[0]);
        Vec2 c2 = xform.Apply(entry.pts[1]);
        Vec2 p3 = xform.Apply(entry.pts[2]);
        flatten_cubic(points, cur, c1, c2, p3, FLATTEN_TOL);
        cur = p3;
        break;
      }
      case PathCmd::kClose: {
        if (cur != subpath_start) points.push_back(subpath_start);
        cur = subpath_start;
        break;
      }
    }
  }
}

// ============================================================================
// Build edge list from flattened points
// ============================================================================

static void build_edges(const Vector<Vec2>& points,
                        const Vector<u32>& subpath_starts,
                        Vector<Edge>& edges) {
  for (usize s = 0; s < subpath_starts.size(); ++s) {
    u32 start = subpath_starts[s];
    u32 end = (s + 1 < subpath_starts.size()) ? subpath_starts[s + 1]
                                              : static_cast<u32>(points.size());
    if (end - start < 2) continue;

    for (u32 i = start; i + 1 < end; ++i) {
      Vec2 p0 = points[i];
      Vec2 p1 = points[i + 1];
      if (p0.y == p1.y) continue;  // skip horizontal edges

      Edge e;
      if (p0.y < p1.y) {
        e = {p0.x, p0.y, p1.x, p1.y, 1};
      } else {
        e = {p1.x, p1.y, p0.x, p0.y, -1};
      }
      edges.push_back(e);
    }
  }
}

// ============================================================================
// Stroke expansion: convert stroke to fill outline
// ============================================================================

static void stroke_to_fill(const Vector<Vec2>& points,
                           const Vector<u32>& subpath_starts, f32 width,
                           Vector<Vec2>& out_points, Vector<u32>& out_starts) {
  f32 half_w = width * 0.5f;

  for (usize s = 0; s < subpath_starts.size(); ++s) {
    u32 start = subpath_starts[s];
    u32 end = (s + 1 < subpath_starts.size()) ? subpath_starts[s + 1]
                                              : static_cast<u32>(points.size());
    u32 count = end - start;
    if (count < 2) continue;

    // Check if subpath is closed
    bool closed = (points[start] == points[end - 1]) && count > 2;
    u32 seg_count = closed ? count - 1 : count - 1;

    // Compute normals for each segment
    struct SegNorm {
      Vec2 n;  // left normal
    };
    Vector<SegNorm> norms(seg_count);
    for (u32 i = 0; i < seg_count; ++i) {
      Vec2 d = points[start + i + 1] - points[start + i];
      f32 len = d.length();
      if (len > 0) {
        d = d * (1.0f / len);
        norms[i].n = {-d.y, d.x};
      }
    }

    // Build left and right offset polylines
    Vector<Vec2> left, right;

    auto avg_normal = [&](u32 seg_a, u32 seg_b) -> Vec2 {
      Vec2 n = (norms[seg_a].n + norms[seg_b].n);
      f32 len = n.length();
      if (len > 0.001f)
        n = n * (1.0f / len);
      else
        n = norms[seg_a].n;

      // Miter factor
      f32 dot = norms[seg_a].n.Dot(n);
      if (dot > 0.1f) n = n * (1.0f / dot);
      return n;
    };

    for (u32 i = 0; i <= seg_count; ++i) {
      u32 idx = start + (closed ? (i % (count - 1)) : i);
      Vec2 p = points[idx];
      Vec2 n;

      if (!closed) {
        if (i == 0)
          n = norms[0].n;
        else if (i == seg_count)
          n = norms[seg_count - 1].n;
        else
          n = avg_normal(i - 1, i);
      } else {
        u32 prev = (i == 0) ? seg_count - 1 : i - 1;
        u32 cur = i % seg_count;
        n = avg_normal(prev, cur);
      }

      left.push_back(p + n * half_w);
      right.push_back(p - n * half_w);
    }

    // Build closed outline: left forward + right reverse
    out_starts.push_back(static_cast<u32>(out_points.size()));
    for (auto& lp : left) out_points.push_back(lp);
    for (auto it = right.rbegin(); it != right.rend(); ++it)
      out_points.push_back(*it);
    // Close
    if (!out_points.empty())
      out_points.push_back(out_points[out_starts.back()]);
  }
}

// ============================================================================
// Gradient sampling
// ============================================================================

static Color sample_gradient(const Gradient& grad, f32 t) {
  if (grad.stops.empty()) return Color::Black();

  // Apply spread method
  switch (grad.spread) {
    case SpreadMethod::kPad:
      t = Clamp(t, 0.0f, 1.0f);
      break;
    case SpreadMethod::kReflect: {
      t = std::fmod(t, 2.0f);
      if (t < 0) t += 2.0f;
      if (t > 1.0f) t = 2.0f - t;
      break;
    }
    case SpreadMethod::kRepeat:
      t = t - std::floor(t);
      break;
  }

  if (t <= grad.stops.front().offset) return grad.stops.front().color;
  if (t >= grad.stops.back().offset) return grad.stops.back().color;

  for (usize i = 1; i < grad.stops.size(); ++i) {
    if (t <= grad.stops[i].offset) {
      f32 range = grad.stops[i].offset - grad.stops[i - 1].offset;
      f32 local_t = (range > 0) ? (t - grad.stops[i - 1].offset) / range : 0;
      return Lerp(grad.stops[i - 1].color, grad.stops[i].color, local_t);
    }
  }
  return grad.stops.back().color;
}

static Color eval_gradient_at(const Gradient& grad, f32 px, f32 py, f32 bbox_x,
                              f32 bbox_y, f32 bbox_w, f32 bbox_h) {
  f32 t = 0;

  if (grad.type == GradientType::kLinear) {
    f32 gx1, gy1, gx2, gy2;
    if (grad.user_space) {
      gx1 = grad.x1;
      gy1 = grad.y1;
      gx2 = grad.x2;
      gy2 = grad.y2;
    } else {
      gx1 = bbox_x + grad.x1 * bbox_w;
      gy1 = bbox_y + grad.y1 * bbox_h;
      gx2 = bbox_x + grad.x2 * bbox_w;
      gy2 = bbox_y + grad.y2 * bbox_h;
    }

    // Apply gradient transform
    Vec2 p1 = grad.transform.Apply({gx1, gy1});
    Vec2 p2 = grad.transform.Apply({gx2, gy2});

    f32 dx = p2.x - p1.x;
    f32 dy = p2.y - p1.y;
    f32 len_sq = dx * dx + dy * dy;
    if (len_sq > 0) t = ((px - p1.x) * dx + (py - p1.y) * dy) / len_sq;
  } else {
    // Radial
    f32 gcx, gcy, gr;
    if (grad.user_space) {
      gcx = grad.cx;
      gcy = grad.cy;
      gr = grad.r;
    } else {
      gcx = bbox_x + grad.cx * bbox_w;
      gcy = bbox_y + grad.cy * bbox_h;
      gr = grad.r * std::max(bbox_w, bbox_h);
    }

    Vec2 center = grad.transform.Apply({gcx, gcy});
    f32 dist = std::sqrt((px - center.x) * (px - center.x) +
                         (py - center.y) * (py - center.y));
    t = (gr > 0) ? dist / gr : 0;
  }

  return sample_gradient(grad, t);
}

// ============================================================================
// Bounding box computation
// ============================================================================

struct BBox {
  f32 x0 = 1e30f, y0 = 1e30f, x1 = -1e30f, y1 = -1e30f;

  void add(f32 x, f32 y) {
    if (x < x0) x0 = x;
    if (y < y0) y0 = y;
    if (x > x1) x1 = x;
    if (y > y1) y1 = y;
  }
  f32 width() const { return x1 - x0; }
  f32 height() const { return y1 - y0; }
};

static BBox compute_bbox(const Vector<Edge>& edges) {
  BBox bb;
  for (auto& e : edges) {
    bb.add(e.x0, e.y0);
    bb.add(e.x1, e.y1);
  }
  return bb;
}

// ============================================================================
// Resolve paint to a color at a given pixel
// ============================================================================

static Color resolve_paint(const Paint& paint, f32 px, f32 py, f32 alpha,
                           const Document& doc, const BBox& bbox) {
  if (paint.type == Paint::kNone) return Color::Transparent();
  if (paint.type == Paint::kSolid)
    return paint.color.WithAlpha(paint.color.a * alpha);
  if (paint.type == Paint::kGradientRef) {
    auto it = doc.gradients.find(paint.gradient_id);
    if (it == doc.gradients.end()) return Color::Black().WithAlpha(alpha);
    Color c = eval_gradient_at(it->second, px, py, bbox.x0, bbox.y0,
                               bbox.width(), bbox.height());
    return c.WithAlpha(c.a * alpha);
  }
  return Color::Transparent();
}

// ============================================================================
// Alpha compositing (source over, straight alpha)
// ============================================================================

static inline void composite_pixel(u8* dst, Color src) {
  if (src.a <= 0.0f) return;

  f32 da = dst[3] / 255.0f;
  f32 sa = src.a;
  f32 out_a = sa + da * (1.0f - sa);

  if (out_a <= 0.0f) {
    dst[0] = dst[1] = dst[2] = dst[3] = 0;
    return;
  }

  f32 inv_out_a = 1.0f / out_a;
  f32 dr = dst[0] / 255.0f;
  f32 dg = dst[1] / 255.0f;
  f32 db = dst[2] / 255.0f;

  dst[0] = static_cast<u8>(
      Clamp((src.r * sa + dr * da * (1.0f - sa)) * inv_out_a, 0.0f, 1.0f) *
          255.0f +
      0.5f);
  dst[1] = static_cast<u8>(
      Clamp((src.g * sa + dg * da * (1.0f - sa)) * inv_out_a, 0.0f, 1.0f) *
          255.0f +
      0.5f);
  dst[2] = static_cast<u8>(
      Clamp((src.b * sa + db * da * (1.0f - sa)) * inv_out_a, 0.0f, 1.0f) *
          255.0f +
      0.5f);
  dst[3] = static_cast<u8>(Clamp(out_a, 0.0f, 1.0f) * 255.0f + 0.5f);
}

// ============================================================================
// Scanline rasterizer with multi-sample AA
// ============================================================================

struct ActiveEdge {
  f32 x;    // current x intersection
  f32 dx;   // x increment per sub-scanline (1/AA_LEVEL pixel)
  f32 y1;   // bottom y of edge
  i32 dir;  // winding direction
};

static void rasterize_edges(const Vector<Edge>& edges, FillRule rule,
                            u8* pixels, u32 width, u32 height,
                            const Paint& paint, f32 opacity,
                            const Document& doc) {
  if (edges.empty()) return;

  BBox bbox = compute_bbox(edges);

  // Sort edges by min-Y (reuse scratch to avoid per-call allocation)
  static thread_local Vector<usize> sorted;
  sorted.resize(edges.size());
  for (usize i = 0; i < edges.size(); ++i) sorted[i] = i;
  std::sort(sorted.begin(), sorted.end(),
            [&](usize a, usize b) { return edges[a].y0 < edges[b].y0; });

  // Coverage buffer for one pixel row
  static thread_local Vector<f32> coverage;
  coverage.assign(width, 0.0f);
  static thread_local Vector<ActiveEdge> active;
  active.clear();

  usize edge_idx = 0;  // next edge to activate

  f32 fwidth = static_cast<f32>(width);

  for (u32 py = 0; py < height; ++py) {
    std::memset(coverage.data(), 0, width * sizeof(f32));

    for (i32 sub = 0; sub < AA_LEVEL; ++sub) {
      f32 scan_y = static_cast<f32>(py) + (sub + 0.5f) * INV_AA;

      // Add new edges that start at or before this scanline
      while (edge_idx < sorted.size() && edges[sorted[edge_idx]].y0 <= scan_y) {
        auto& e = edges[sorted[edge_idx]];
        if (e.y1 > scan_y) {
          f32 dy = e.y1 - e.y0;
          f32 dxdy = (dy > 0) ? (e.x1 - e.x0) / dy : 0;
          f32 x = e.x0 + dxdy * (scan_y - e.y0);
          active.push_back({x, dxdy * INV_AA, e.y1, e.dir});
        }
        ++edge_idx;
      }

      // Remove expired edges
      active.erase(std::remove_if(active.begin(), active.end(),
                                  [scan_y](const ActiveEdge& ae) {
                                    return ae.y1 <= scan_y;
                                  }),
                   active.end());

      // Sort active edges by x
      std::sort(
          active.begin(), active.end(),
          [](const ActiveEdge& a, const ActiveEdge& b) { return a.x < b.x; });

      // Fill spans based on fill rule
      if (rule == FillRule::kEvenOdd) {
        for (usize i = 0; i + 1 < active.size(); i += 2) {
          f32 x0 = Clamp(active[i].x, 0.0f, fwidth);
          f32 x1 = Clamp(active[i + 1].x, 0.0f, fwidth);

          i32 ix0 = static_cast<i32>(x0);
          i32 ix1 = static_cast<i32>(std::ceil(x1));
          ix0 = std::max(ix0, 0);
          ix1 = std::min(ix1, static_cast<i32>(width));

          for (i32 x = ix0; x < ix1; ++x) {
            f32 left = std::max(static_cast<f32>(x), x0);
            f32 right = std::min(static_cast<f32>(x + 1), x1);
            coverage[x] += (right - left) * INV_AA;
          }
        }
      } else {
        // Non-zero winding
        i32 winding = 0;
        usize i = 0;
        while (i < active.size()) {
          i32 old_winding = winding;
          winding += active[i].dir;

          if (old_winding == 0 && winding != 0) {
            // Start of filled span
            f32 span_start = active[i].x;
            ++i;
            while (i < active.size()) {
              winding += active[i].dir;
              if (winding == 0) {
                // End of filled span
                f32 span_end = active[i].x;
                f32 x0 = Clamp(span_start, 0.0f, fwidth);
                f32 x1 = Clamp(span_end, 0.0f, fwidth);
                i32 ix0 = static_cast<i32>(x0);
                i32 ix1 = static_cast<i32>(std::ceil(x1));
                ix0 = std::max(ix0, 0);
                ix1 = std::min(ix1, static_cast<i32>(width));

                for (i32 x = ix0; x < ix1; ++x) {
                  f32 left = std::max(static_cast<f32>(x), x0);
                  f32 right = std::min(static_cast<f32>(x + 1), x1);
                  coverage[x] += (right - left) * INV_AA;
                }
                ++i;
                break;
              }
              ++i;
            }
          } else {
            ++i;
          }
        }
      }

      // Step active edge x values
      for (auto& ae : active) ae.x += ae.dx;
    }

    // Apply coverage to pixels
    u8* row = pixels + py * width * 4;
    for (u32 px = 0; px < width; ++px) {
      f32 cov = Clamp(coverage[px], 0.0f, 1.0f);
      if (cov > 0.001f) {
        Color c = resolve_paint(paint, px + 0.5f, py + 0.5f, opacity * cov, doc,
                                bbox);
        composite_pixel(row + px * 4, c);
      }
    }
  }
}

// ============================================================================
// Public rasterize entry point
// ============================================================================

void Rasterize(const Document& doc, u8* pixels, u32 width, u32 height) {
  std::memset(pixels, 0, width * height * 4);

  // Compute viewBox -> target transform
  Transform view_xform = Transform::Identity();
  if (doc.view_w > 0 && doc.view_h > 0) {
    f32 sx = static_cast<f32>(width) / doc.view_w;
    f32 sy = static_cast<f32>(height) / doc.view_h;
    view_xform = Transform::Scale(sx, sy) *
                 Transform::Translate(-doc.view_x, -doc.view_y);
  } else if (doc.width > 0 && doc.height > 0) {
    f32 sx = static_cast<f32>(width) / doc.width;
    f32 sy = static_cast<f32>(height) / doc.height;
    view_xform = Transform::Scale(sx, sy);
  }

  // Scratch buffers: reused across shapes (clear preserves capacity)
  Vector<Vec2> points;
  Vector<u32> subpath_starts;
  Vector<Edge> edges;
  Vector<Vec2> stroke_points;
  Vector<u32> stroke_starts;

  for (auto& shape : doc.shapes) {
    Transform xform = view_xform * shape.transform;

    // --- Fill ---
    if (shape.fill.type != Paint::kNone) {
      points.clear();
      subpath_starts.clear();
      edges.clear();

      flatten_path(shape.path, xform, points, subpath_starts);
      build_edges(points, subpath_starts, edges);

      if (!edges.empty()) {
        rasterize_edges(edges, shape.fill_rule, pixels, width, height,
                        shape.fill, shape.opacity * shape.fill_opacity, doc);
      }
    }

    // --- Stroke ---
    if (shape.stroke.type != Paint::kNone && shape.stroke_width > 0) {
      points.clear();
      subpath_starts.clear();
      edges.clear();

      flatten_path(shape.path, xform, points, subpath_starts);

      // Compute stroke width in transformed space (approximate)
      f32 scale = std::sqrt(std::fabs(xform.a * xform.d - xform.b * xform.c));
      f32 sw = shape.stroke_width * scale;

      // Expand stroke to fill outline
      stroke_points.clear();
      stroke_starts.clear();
      stroke_to_fill(points, subpath_starts, sw, stroke_points, stroke_starts);

      edges.clear();
      build_edges(stroke_points, stroke_starts, edges);

      if (!edges.empty()) {
        rasterize_edges(edges, FillRule::kNonZero, pixels, width, height,
                        shape.stroke, shape.opacity * shape.stroke_opacity,
                        doc);
      }
    }
  }
}

}  // namespace svg
}  // namespace ugui
