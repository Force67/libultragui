#include <ugui/render/renderer2d.h>
#include <ugui/style/style.h>

#include <cmath>
#include <cstring>

namespace ugui {

bool Renderer2D::Init(RHI* rhi) {
  rhi_ = rhi;
  vertices_.reserve(4096);
  indices_.reserve(8192);
  batches_.reserve(64);
  text_vertices_.reserve(4096);
  text_indices_.reserve(8192);
  text_batches_.reserve(16);
  draw_order_.reserve(128);
  return true;
}

void Renderer2D::Shutdown() {
  rhi_ = nullptr;
  vertices_.clear();
  indices_.clear();
  batches_.clear();
  text_vertices_.clear();
  text_indices_.clear();
  text_batches_.clear();
  draw_order_.clear();
}

void Renderer2D::BeginFrame() {
  vertices_.clear();
  indices_.clear();
  batches_.clear();
  text_vertices_.clear();
  text_indices_.clear();
  text_batches_.clear();
  draw_order_.clear();
  scissor_stack_.clear();
  current_texture_ = kNullTextureId;
  current_text_atlas_ = kNullTextureId;
  Vec2 vp = rhi_ ? rhi_->display_size() : display_size_;
  current_scissor_ = Rect{0, 0, vp.x, vp.y};
}

const DrawData& Renderer2D::GetDrawData() {
  // Finalize pending geometry into batches, exactly as EndFrame() does before
  // submitting, but expose it instead of issuing RHI draw calls.
  FlushBatch();
  FlushTextBatch();

  draw_cmds_.clear();
  draw_cmds_.reserve(draw_order_.size());
  for (const auto& cmd : draw_order_) {
    DrawCmd out;
    if (cmd.kind == DrawKind::kQuad) {
      const auto& batch = batches_[cmd.batch_index];
      out.clip_rect = batch.scissor;
      out.index_offset = batch.index_offset;
      out.elem_count = batch.index_count;
      out.is_text = false;
      out.texture_id = batch.texture;
    } else {
      const auto& batch = text_batches_[cmd.batch_index];
      out.clip_rect = batch.scissor;
      out.index_offset = batch.index_offset;
      out.elem_count = batch.index_count;
      out.is_text = true;
      out.texture_id = kFontTextureId;
    }
    draw_cmds_.push_back(out);
  }

  draw_data_.quad_vertices = vertices_.data();
  draw_data_.quad_vertex_count = static_cast<u32>(vertices_.size());
  draw_data_.quad_indices = indices_.data();
  draw_data_.quad_index_count = static_cast<u32>(indices_.size());
  draw_data_.text_vertices = text_vertices_.data();
  draw_data_.text_vertex_count = static_cast<u32>(text_vertices_.size());
  draw_data_.text_indices = text_indices_.data();
  draw_data_.text_index_count = static_cast<u32>(text_indices_.size());
  draw_data_.commands = draw_cmds_.data();
  draw_data_.command_count = static_cast<u32>(draw_cmds_.size());
  draw_data_.display_size = display_size_;
  draw_data_.valid = true;
  return draw_data_;
}

void Renderer2D::EndFrame() {
  FlushBatch();
  FlushTextBatch();

  // Walk submission order so quads and text are interleaved exactly
  // as the widget tree emitted them. Without this, all text was drawn
  // after all quads in a global pass - meaning a settings modal would
  // be covered by text from any panel painted earlier in the tree.
  for (const auto& cmd : draw_order_) {
    if (cmd.kind == DrawKind::kQuad) {
      const auto& batch = batches_[cmd.batch_index];
      rhi_->SetScissor(batch.scissor);
      rhi_->DrawTriangles(vertices_.data(), static_cast<u32>(vertices_.size()),
                          indices_.data() + batch.index_offset,
                          batch.index_count,
                          RhiHandleFromTextureId(batch.texture));
    } else {
      const auto& batch = text_batches_[cmd.batch_index];
      rhi_->SetScissor(batch.scissor);
      rhi_->DrawTextTriangles(text_vertices_.data(),
                              static_cast<u32>(text_vertices_.size()),
                              text_indices_.data() + batch.index_offset,
                              batch.index_count,
                              RhiHandleFromTextureId(batch.texture));
    }
  }
}

void Renderer2D::DrawRect(Rect rect, Color color, u32 corner_radii) {
  u32 packed = Vertex2D::PackColor(color.r, color.g, color.b, color.a);
  EmitQuad(rect, packed, packed, corner_radii, 0.0f, 0.0f, 0, kNullTextureId);
}

void Renderer2D::DrawTexturedRect(Rect rect, TextureId texture, Color tint,
                                  u32 corner_radii) {
  u32 packed = Vertex2D::PackColor(tint.r, tint.g, tint.b, tint.a);
  EmitQuad(rect, packed, packed, corner_radii, 0.0f, 0.0f, 0, texture);
}

void Renderer2D::DrawRectGradient(Rect rect, Color start_color, Color end_color,
                                  u32 corner_radii, f32 angle_deg) {
  // Batch management
  if (kNullTextureId != current_texture_) {
    FlushBatch();
    current_texture_ = kNullTextureId;
  }

  // DPI pixel snapping
  f32 dpi = rhi_ ? rhi_->dpi_scale() : 1.0f;
  if (dpi != 1.0f) {
    f32 inv = 1.0f / dpi;
    f32 x0 = std::round(rect.x * dpi) * inv;
    f32 y0 = std::round(rect.y * dpi) * inv;
    f32 x1 = std::round((rect.x + rect.w) * dpi) * inv;
    f32 y1 = std::round((rect.y + rect.h) * dpi) * inv;
    rect = {x0, y0, x1 - x0, y1 - y0};
  }

  f32 hw = rect.w * 0.5f;
  f32 hh = rect.h * 0.5f;

  // Gradient direction (CSS: 0deg=to-top, 90deg=to-right, 180deg=to-bottom)
  f32 angle_rad = angle_deg * (3.14159265f / 180.0f);
  f32 dx = std::sin(angle_rad);
  f32 dy = -std::cos(angle_rad);

  // Max projection onto gradient axis (for normalization to [0,1])
  f32 max_proj = std::abs(hw * dx) + std::abs(hh * dy);
  if (max_proj < 0.001f) max_proj = 1.0f;

  // Corner offsets from center: TL, TR, BR, BL
  constexpr f32 kCorners[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};

  u32 colors[4];
  for (int i = 0; i < 4; ++i) {
    f32 cx = kCorners[i][0] * hw;
    f32 cy = kCorners[i][1] * hh;
    f32 t = Clamp((cx * dx + cy * dy) / max_proj * 0.5f + 0.5f, 0.0f, 1.0f);
    Color c = {start_color.r + (end_color.r - start_color.r) * t,
               start_color.g + (end_color.g - start_color.g) * t,
               start_color.b + (end_color.b - start_color.b) * t,
               start_color.a + (end_color.a - start_color.a) * t};
    colors[i] = Vertex2D::PackColor(c.r, c.g, c.b, c.a);
  }

  // Emit quad with per-vertex gradient colors (color == color2 per vertex,
  // so the shader's mix() is a no-op and GPU bilinear interpolation produces
  // the gradient)
  u32 base = static_cast<u32>(vertices_.size());
  vertices_.push_back({{rect.x, rect.y},
                       {0, 0},
                       colors[0],
                       colors[0],
                       corner_radii,
                       0.0f,
                       {hw, hh},
                       0.0f,
                       0});
  vertices_.push_back({{rect.x + rect.w, rect.y},
                       {1, 0},
                       colors[1],
                       colors[1],
                       corner_radii,
                       0.0f,
                       {hw, hh},
                       0.0f,
                       0});
  vertices_.push_back({{rect.x + rect.w, rect.y + rect.h},
                       {1, 1},
                       colors[2],
                       colors[2],
                       corner_radii,
                       0.0f,
                       {hw, hh},
                       0.0f,
                       0});
  vertices_.push_back({{rect.x, rect.y + rect.h},
                       {0, 1},
                       colors[3],
                       colors[3],
                       corner_radii,
                       0.0f,
                       {hw, hh},
                       0.0f,
                       0});

  indices_.push_back(base + 0);
  indices_.push_back(base + 1);
  indices_.push_back(base + 2);
  indices_.push_back(base + 0);
  indices_.push_back(base + 2);
  indices_.push_back(base + 3);
}

TextureId Renderer2D::GetRadialGradientTexture(Color center, Color edge) {
  if (!texture_backend_) return kNullTextureId;  // no sink: fall back to flat
  u32 c1 = Vertex2D::PackColor(center.r, center.g, center.b, center.a);
  u32 c2 = Vertex2D::PackColor(edge.r, edge.g, edge.b, edge.a);
  u64 key = static_cast<u64>(c1) | (static_cast<u64>(c2) << 32);

  auto it = gradient_cache_.find(key);
  if (it != gradient_cache_.end()) return it->second;

  constexpr u32 kSize = 64;
  u8 pixels[kSize * kSize * 4];
  f32 half = kSize * 0.5f;
  for (u32 y = 0; y < kSize; ++y) {
    for (u32 x = 0; x < kSize; ++x) {
      f32 dx = (static_cast<f32>(x) + 0.5f - half) / half;
      f32 dy = (static_cast<f32>(y) + 0.5f - half) / half;
      f32 t = Clamp(std::sqrt(dx * dx + dy * dy), 0.0f, 1.0f);
      u32 idx = (y * kSize + x) * 4;
      pixels[idx + 0] = static_cast<u8>(
          Clamp(center.r + (edge.r - center.r) * t, 0.0f, 1.0f) * 255.0f);
      pixels[idx + 1] = static_cast<u8>(
          Clamp(center.g + (edge.g - center.g) * t, 0.0f, 1.0f) * 255.0f);
      pixels[idx + 2] = static_cast<u8>(
          Clamp(center.b + (edge.b - center.b) * t, 0.0f, 1.0f) * 255.0f);
      pixels[idx + 3] = static_cast<u8>(
          Clamp(center.a + (edge.a - center.a) * t, 0.0f, 1.0f) * 255.0f);
    }
  }

  auto tex = texture_backend_->CreateTexture(kSize, kSize,
                                             RHIFormat::kRgba8Unorm, pixels);
  gradient_cache_[key] = tex;
  return tex;
}

void Renderer2D::DrawRadialGradient(Rect rect, Color center_color,
                                    Color edge_color, u32 corner_radii) {
  auto tex = GetRadialGradientTexture(center_color, edge_color);
  DrawTexturedRect(rect, tex, Color::White(), corner_radii);
}

static Color SampleGradient(const GradientStop* stops, u32 count, f32 t) {
  if (count == 0) return Color::Transparent();
  if (t <= stops[0].position || count == 1) return stops[0].color;
  if (t >= stops[count - 1].position) return stops[count - 1].color;

  for (u32 i = 0; i < count - 1; ++i) {
    if (t >= stops[i].position && t <= stops[i + 1].position) {
      f32 range = stops[i + 1].position - stops[i].position;
      f32 local_t = (range > 0.001f) ? (t - stops[i].position) / range : 0.0f;
      return Color{
          stops[i].color.r +
              (stops[i + 1].color.r - stops[i].color.r) * local_t,
          stops[i].color.g +
              (stops[i + 1].color.g - stops[i].color.g) * local_t,
          stops[i].color.b +
              (stops[i + 1].color.b - stops[i].color.b) * local_t,
          stops[i].color.a +
              (stops[i + 1].color.a - stops[i].color.a) * local_t,
      };
    }
  }
  return stops[count - 1].color;
}

TextureId Renderer2D::GetMultiStopGradientTexture(const GradientStop* stops,
                                                  u32 count, GradientType type,
                                                  f32 angle_deg) {
  if (!texture_backend_) return kNullTextureId;  // no sink: fall back to flat
  // Hash the gradient parameters for caching
  u64 hash = static_cast<u64>(count) ^ (static_cast<u64>(type) << 8);
  for (u32 i = 0; i < count; ++i) {
    u32 pc = Vertex2D::PackColor(stops[i].color.r, stops[i].color.g,
                                 stops[i].color.b, stops[i].color.a);
    hash ^= static_cast<u64>(pc) << ((i * 7) % 32);
    u32 pos_bits;
    std::memcpy(&pos_bits, &stops[i].position, sizeof(u32));
    hash ^= static_cast<u64>(pos_bits) << ((i * 13) % 32);
  }
  u32 angle_bits;
  std::memcpy(&angle_bits, &angle_deg, sizeof(u32));
  hash ^= static_cast<u64>(angle_bits) << 48;

  auto it = gradient_cache_.find(hash);
  if (it != gradient_cache_.end()) return it->second;

  // Always generate a 64x64 2D texture for both linear and radial
  constexpr u32 kSize = 64;
  u8 pixels[kSize * kSize * 4];

  f32 angle_rad = angle_deg * (3.14159265f / 180.0f);
  f32 dx = std::sin(angle_rad);
  f32 dy = -std::cos(angle_rad);

  for (u32 y = 0; y < kSize; ++y) {
    for (u32 x = 0; x < kSize; ++x) {
      f32 u = (static_cast<f32>(x) + 0.5f) / kSize;
      f32 v = (static_cast<f32>(y) + 0.5f) / kSize;
      f32 t;
      if (type == GradientType::kRadial) {
        f32 cx = u - 0.5f;
        f32 cy = v - 0.5f;
        t = Clamp(std::sqrt(cx * cx + cy * cy) * 2.0f, 0.0f, 1.0f);
      } else {
        f32 max_proj = std::abs(0.5f * dx) + std::abs(0.5f * dy);
        if (max_proj > 0.001f)
          t = Clamp(
              ((u - 0.5f) * dx + (v - 0.5f) * dy) / (max_proj * 2.0f) + 0.5f,
              0.0f, 1.0f);
        else
          t = 0.5f;
      }
      Color c = SampleGradient(stops, count, t);
      u32 idx = (y * kSize + x) * 4;
      pixels[idx + 0] = static_cast<u8>(Clamp(c.r, 0.0f, 1.0f) * 255.0f);
      pixels[idx + 1] = static_cast<u8>(Clamp(c.g, 0.0f, 1.0f) * 255.0f);
      pixels[idx + 2] = static_cast<u8>(Clamp(c.b, 0.0f, 1.0f) * 255.0f);
      pixels[idx + 3] = static_cast<u8>(Clamp(c.a, 0.0f, 1.0f) * 255.0f);
    }
  }

  auto tex = texture_backend_->CreateTexture(kSize, kSize,
                                             RHIFormat::kRgba8Unorm, pixels);
  gradient_cache_[hash] = tex;
  return tex;
}

void Renderer2D::DrawMultiStopGradient(Rect rect, const GradientStop* stops,
                                       u32 stop_count, GradientType type,
                                       f32 angle_deg, u32 corner_radii) {
  auto tex = GetMultiStopGradientTexture(stops, stop_count, type, angle_deg);
  DrawTexturedRect(rect, tex, Color::White(), corner_radii);
}

void Renderer2D::DrawShadow(Rect rect, Color shadow_color, f32 blur, f32 spread,
                            Vec2 offset, u32 corner_radii) {
  Rect shadow_rect = {
      rect.x + offset.x - spread - blur,
      rect.y + offset.y - spread - blur,
      rect.w + (spread + blur) * 2.0f,
      rect.h + (spread + blur) * 2.0f,
  };
  u32 packed = Vertex2D::PackColor(shadow_color.r, shadow_color.g,
                                   shadow_color.b, shadow_color.a);
  u32 tl = (corner_radii & 0xFFu);
  u32 tr = ((corner_radii >> 8) & 0xFFu);
  u32 br = ((corner_radii >> 16) & 0xFFu);
  u32 bl = ((corner_radii >> 24) & 0xFFu);
  u32 shadow_radii = Vertex2D::PackRadii(
      static_cast<f32>(tl) + spread, static_cast<f32>(tr) + spread,
      static_cast<f32>(br) + spread, static_cast<f32>(bl) + spread);
  EmitQuad(shadow_rect, packed, packed, shadow_radii, blur, 0.0f, 0,
           kNullTextureId);
}

void Renderer2D::DrawInsetShadow(Rect rect, Color shadow_color, f32 blur,
                                 f32 spread, Vec2 offset, u32 corner_radii) {
  // Inset shadow: shrink the SDF rect by spread, offset it, and use negative
  // softness to signal the shader to fade from edge inward
  Rect shadow_rect = {rect.x + offset.x + spread, rect.y + offset.y + spread,
                      rect.w - spread * 2.0f, rect.h - spread * 2.0f};
  if (shadow_rect.w <= 0.0f || shadow_rect.h <= 0.0f) return;
  u32 packed = Vertex2D::PackColor(shadow_color.r, shadow_color.g,
                                   shadow_color.b, shadow_color.a);
  // Shrink corner radii by spread
  u32 tl = (corner_radii & 0xFFu);
  u32 tr = ((corner_radii >> 8) & 0xFFu);
  u32 br = ((corner_radii >> 16) & 0xFFu);
  u32 bl = ((corner_radii >> 24) & 0xFFu);
  u32 inner_radii =
      Vertex2D::PackRadii(std::max(0.0f, static_cast<f32>(tl) - spread),
                          std::max(0.0f, static_cast<f32>(tr) - spread),
                          std::max(0.0f, static_cast<f32>(br) - spread),
                          std::max(0.0f, static_cast<f32>(bl) - spread));
  // Negative softness signals inset mode to the shader
  EmitQuad(shadow_rect, packed, packed, inner_radii, -blur, 0.0f, 0,
           kNullTextureId);
}

void Renderer2D::DrawBorderedRect(Rect rect, Color fill, Color border_color,
                                  f32 border_width, u32 corner_radii) {
  u32 fill_packed = Vertex2D::PackColor(fill.r, fill.g, fill.b, fill.a);
  u32 border_packed = Vertex2D::PackColor(border_color.r, border_color.g,
                                          border_color.b, border_color.a);
  EmitQuad(rect, fill_packed, fill_packed, corner_radii, 0.0f, border_width,
           border_packed, kNullTextureId);
}

void Renderer2D::PushScissor(Rect rect) {
  FlushBatch();
  FlushTextBatch();
  scissor_stack_.push_back(current_scissor_);
  current_scissor_ = current_scissor_.Intersected(rect);
}

void Renderer2D::PopScissor() {
  FlushBatch();
  FlushTextBatch();
  if (!scissor_stack_.empty()) {
    current_scissor_ = scissor_stack_.back();
    scissor_stack_.pop_back();
  }
}

void Renderer2D::EmitQuad(Rect rect, u32 color, u32 color2, u32 corner_radii,
                          f32 softness, f32 border_width, u32 border_color,
                          TextureId texture) {
  // If there are pending text indices that haven't been flushed yet,
  // close them off as their own batch first so the upcoming quad
  // renders ON TOP of them rather than getting batched with earlier
  // quads (which would all draw before any text). This is what makes
  // a settings modal cover panels with text behind it.
  {
    u32 expected_text_end = 0;
    if (!text_batches_.empty()) {
      const auto& prev = text_batches_.back();
      expected_text_end = prev.index_offset + prev.index_count;
    }
    if (static_cast<u32>(text_indices_.size()) > expected_text_end)
      FlushTextBatch();
  }

  // Start a new batch if texture changed
  if (texture != current_texture_) {
    FlushBatch();
    current_texture_ = texture;
  }

  // Snap rect edges to physical pixel boundaries for crisp edges on fractional
  // DPI. Without this, a panel at x=10.3 on 1.65x DPI straddles framebuffer
  // pixels, causing the SDF anti-aliasing to blur across an extra pixel.
  f32 dpi = rhi_ ? rhi_->dpi_scale() : 1.0f;
  if (dpi != 1.0f) {
    f32 inv = 1.0f / dpi;
    f32 x0 = std::round(rect.x * dpi) * inv;
    f32 y0 = std::round(rect.y * dpi) * inv;
    f32 x1 = std::round((rect.x + rect.w) * dpi) * inv;
    f32 y1 = std::round((rect.y + rect.h) * dpi) * inv;
    rect = {x0, y0, x1 - x0, y1 - y0};
  }

  f32 hw = rect.w * 0.5f;
  f32 hh = rect.h * 0.5f;

  u32 base = static_cast<u32>(vertices_.size());

  // Top-left
  vertices_.push_back({{rect.x, rect.y},
                       {0, 0},
                       color,
                       color2,
                       corner_radii,
                       softness,
                       {hw, hh},
                       border_width,
                       border_color});
  // Top-right
  vertices_.push_back({{rect.x + rect.w, rect.y},
                       {1, 0},
                       color,
                       color2,
                       corner_radii,
                       softness,
                       {hw, hh},
                       border_width,
                       border_color});
  // Bottom-right
  vertices_.push_back({{rect.x + rect.w, rect.y + rect.h},
                       {1, 1},
                       color,
                       color2,
                       corner_radii,
                       softness,
                       {hw, hh},
                       border_width,
                       border_color});
  // Bottom-left
  vertices_.push_back({{rect.x, rect.y + rect.h},
                       {0, 1},
                       color,
                       color2,
                       corner_radii,
                       softness,
                       {hw, hh},
                       border_width,
                       border_color});

  // Two triangles: 0-1-2, 0-2-3
  indices_.push_back(base + 0);
  indices_.push_back(base + 1);
  indices_.push_back(base + 2);
  indices_.push_back(base + 0);
  indices_.push_back(base + 2);
  indices_.push_back(base + 3);
}

void Renderer2D::FlushBatch() {
  u32 idx_offset = 0;
  if (!batches_.empty()) {
    auto& prev = batches_.back();
    idx_offset = prev.index_offset + prev.index_count;
  }

  u32 total_indices = static_cast<u32>(indices_.size());
  if (total_indices <= idx_offset) return;

  batches_.push_back({
      current_texture_,
      current_scissor_,
      idx_offset,
      total_indices - idx_offset,
  });
  draw_order_.push_back(
      {DrawKind::kQuad, static_cast<u32>(batches_.size() - 1)});
}

void Renderer2D::FlushTextBatch() {
  u32 idx_offset = 0;
  if (!text_batches_.empty()) {
    auto& prev = text_batches_.back();
    idx_offset = prev.index_offset + prev.index_count;
  }

  u32 total_indices = static_cast<u32>(text_indices_.size());
  if (total_indices <= idx_offset) return;

  text_batches_.push_back({
      current_text_atlas_,
      current_scissor_,
      idx_offset,
      total_indices - idx_offset,
  });
  draw_order_.push_back(
      {DrawKind::kText, static_cast<u32>(text_batches_.size() - 1)});
}

void Renderer2D::DrawText(Vec2 pos, const TextRun& run, Color color,
                          TextureId atlas_texture) {
  // If there are pending quad indices that haven't been flushed yet,
  // close them off as their own batch first so the upcoming text
  // renders ON TOP of those quads (and BELOW any later quads). See
  // the symmetric guard in EmitQuad for the rationale.
  {
    u32 expected_quad_end = 0;
    if (!batches_.empty()) {
      const auto& prev = batches_.back();
      expected_quad_end = prev.index_offset + prev.index_count;
    }
    if (static_cast<u32>(indices_.size()) > expected_quad_end) FlushBatch();
  }

  if (atlas_texture != current_text_atlas_) {
    FlushTextBatch();
    current_text_atlas_ = atlas_texture;
  }

  u32 packed_color = Vertex2D::PackColor(color.r, color.g, color.b, color.a);
  f32 cursor_x = pos.x;
  f32 baseline_y = pos.y + run.ascent;

  // Snap glyph positions to physical pixel boundaries to prevent sub-pixel
  // blur. On fractional DPI (e.g., 1.65x), unsnapped positions straddle
  // framebuffer pixels, causing the GPU to bilinear-filter the glyph texture ->
  // fuzz.
  f32 dpi = rhi_ ? rhi_->dpi_scale() : 1.0f;
  f32 inv_dpi = 1.0f / dpi;

  for (u32 i = 0; i < run.glyph_count; ++i) {
    auto& g = run.glyphs[i];
    if (g.bmp_w <= 0 || g.bmp_h <= 0) {
      cursor_x += g.x_advance;
      continue;
    }

    // Snap glyph position AND size to physical pixel grid.
    // Position snap prevents sub-pixel blur from bilinear interpolation.
    // Size snap ensures the glyph quad covers an integer number of physical
    // pixels.
    f32 x = std::round((cursor_x + g.bearing_x + g.x_offset) * dpi) * inv_dpi;
    f32 y = std::round((baseline_y - g.bearing_y + g.y_offset) * dpi) * inv_dpi;
    f32 w = std::round(g.bmp_w * dpi) * inv_dpi;
    f32 h = std::round(g.bmp_h * dpi) * inv_dpi;

    u32 base = static_cast<u32>(text_vertices_.size());
    text_vertices_.push_back(
        {{x, y}, {g.u0, g.v0}, packed_color, packed_color, 0, 0, {0, 0}, 0, 0});
    text_vertices_.push_back({{x + w, y},
                              {g.u1, g.v0},
                              packed_color,
                              packed_color,
                              0,
                              0,
                              {0, 0},
                              0,
                              0});
    text_vertices_.push_back({{x + w, y + h},
                              {g.u1, g.v1},
                              packed_color,
                              packed_color,
                              0,
                              0,
                              {0, 0},
                              0,
                              0});
    text_vertices_.push_back({{x, y + h},
                              {g.u0, g.v1},
                              packed_color,
                              packed_color,
                              0,
                              0,
                              {0, 0},
                              0,
                              0});

    text_indices_.push_back(base + 0);
    text_indices_.push_back(base + 1);
    text_indices_.push_back(base + 2);
    text_indices_.push_back(base + 0);
    text_indices_.push_back(base + 2);
    text_indices_.push_back(base + 3);

    cursor_x += g.x_advance;
  }
}

void Renderer2D::DrawTextLayout(Vec2 pos, const TextRun& run,
                                const TextLayout& layout, Color color,
                                TextureId atlas_texture, f32 max_width) {
  if (layout.line_count == 0) return;

  for (u32 li = 0; li < layout.line_count; ++li) {
    auto& line = layout.lines[li];

    f32 x_offset = 0.0f;
    if (max_width > 0.0f) {
      f32 slack = max_width - line.width;
      x_offset = 0.0f;
      (void)slack;
    }

    TextRun line_run = run;
    line_run.glyphs = run.glyphs + line.glyph_start;
    line_run.glyph_count = line.glyph_count;

    DrawText(Vec2{pos.x + x_offset, pos.y + line.y_offset}, line_run, color,
             atlas_texture);
  }
}

}  // namespace ugui
