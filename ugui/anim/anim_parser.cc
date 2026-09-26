#include <ugui/anim/anim_types.h>
#include <ugui/core/from_chars_compat.h>
#include <ugui/anim/json.h>
#include <ugui/core/color.h>

#include <stdio.h>
#include <string.h>

namespace ugui {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static Color parse_anim_color(const char* s) {
  if (!s || !*s) return Color::Transparent();
  if (s[0] == '#') {
    u32 hex = 0;
    usize len = strlen(s);
    ugui::from_chars(s + 1, s + len, hex, 16);
    if (len == 7) return Color::FromHex(hex);
    if (len == 9) {
      u32 rgb = hex >> 8;
      f32 alpha = static_cast<f32>(hex & 0xFF) / 255.0f;
      return Color::FromHex(rgb, alpha);
    }
  }
  if (strcmp(s, "none") == 0 || strcmp(s, "transparent") == 0)
    return Color::Transparent();
  if (strcmp(s, "white") == 0) return Color::White();
  if (strcmp(s, "black") == 0) return Color::Black();
  return Color::Transparent();
}

static EasingType parse_anim_easing(const char* s) {
  if (!s) return EasingType::kLinear;
  if (strcmp(s, "ease-in-out") == 0) return EasingType::kEaseInOut;
  if (strcmp(s, "ease-in") == 0) return EasingType::kEaseIn;
  if (strcmp(s, "ease-out") == 0) return EasingType::kEaseOut;
  return EasingType::kLinear;
}

static AnimShapeType parse_shape_type(const char* s) {
  if (!s) return AnimShapeType::kRect;
  if (strcmp(s, "rect") == 0) return AnimShapeType::kRect;
  if (strcmp(s, "circle") == 0) return AnimShapeType::kCircle;
  if (strcmp(s, "ellipse") == 0) return AnimShapeType::kEllipse;
  if (strcmp(s, "path") == 0) return AnimShapeType::kPath;
  if (strcmp(s, "group") == 0) return AnimShapeType::kGroup;
  return AnimShapeType::kRect;
}

static AnimProperty parse_property_name(const char* s) {
  if (strcmp(s, "x") == 0) return AnimProperty::kX;
  if (strcmp(s, "y") == 0) return AnimProperty::kY;
  if (strcmp(s, "w") == 0 || strcmp(s, "width") == 0) return AnimProperty::kW;
  if (strcmp(s, "h") == 0 || strcmp(s, "height") == 0) return AnimProperty::kH;
  if (strcmp(s, "r") == 0) return AnimProperty::kR;
  if (strcmp(s, "cx") == 0) return AnimProperty::kCx;
  if (strcmp(s, "cy") == 0) return AnimProperty::kCy;
  if (strcmp(s, "rx") == 0) return AnimProperty::kRx;
  if (strcmp(s, "ry") == 0) return AnimProperty::kRy;
  if (strcmp(s, "rotation") == 0) return AnimProperty::kRotation;
  if (strcmp(s, "scale-x") == 0) return AnimProperty::kScaleX;
  if (strcmp(s, "scale-y") == 0) return AnimProperty::kScaleY;
  if (strcmp(s, "opacity") == 0) return AnimProperty::kOpacity;
  if (strcmp(s, "stroke-width") == 0) return AnimProperty::kStrokeWidth;
  if (strcmp(s, "corner-radius") == 0) return AnimProperty::kCornerRadius;
  if (strcmp(s, "fill") == 0) return AnimProperty::kFill;
  if (strcmp(s, "stroke") == 0) return AnimProperty::kStroke;
  return AnimProperty::kOpacity;  // fallback
}

static bool is_color_property(AnimProperty p) {
  return p == AnimProperty::kFill || p == AnimProperty::kStroke;
}

// ---------------------------------------------------------------------------
// Parse keyframes
// ---------------------------------------------------------------------------

static AnimPropertyTrack parse_track(const char* prop_name,
                                     const JsonValue& arr) {
  AnimPropertyTrack track;
  track.property = parse_property_name(prop_name);
  bool is_color = is_color_property(track.property);

  for (auto& kf_val : arr.array_val) {
    AnimKeyframe kf;
    if (auto* t = kf_val.get("t")) kf.t = t->AsFloat();
    if (is_color) {
      if (auto* v = kf_val.get("v")) kf.color = parse_anim_color(v->AsString());
    } else {
      if (auto* v = kf_val.get("v")) kf.value = v->AsFloat();
    }
    if (auto* e = kf_val.get("ease"))
      kf.easing = parse_anim_easing(e->AsString());
    track.keyframes.push_back(kf);
  }
  return track;
}

// ---------------------------------------------------------------------------
// Parse layer
// ---------------------------------------------------------------------------

static AnimLayer parse_layer(const JsonValue& obj) {
  AnimLayer layer;

  if (auto* v = obj.get("name")) layer.name = v->AsString();
  if (auto* v = obj.get("shape"))
    layer.shape_type = parse_shape_type(v->AsString());

  // Static properties
  if (auto* v = obj.get("x")) layer.x = v->AsFloat();
  if (auto* v = obj.get("y")) layer.y = v->AsFloat();
  if (auto* v = obj.get("w")) layer.w = v->AsFloat();
  if (auto* v = obj.get("h")) layer.h = v->AsFloat();
  if (auto* v = obj.get("r")) layer.r = v->AsFloat();
  if (auto* v = obj.get("cx")) layer.cx = v->AsFloat();
  if (auto* v = obj.get("cy")) layer.cy = v->AsFloat();
  if (auto* v = obj.get("rx")) layer.rx = v->AsFloat();
  if (auto* v = obj.get("ry")) layer.ry = v->AsFloat();
  if (auto* v = obj.get("corner-radius")) layer.corner_radius = v->AsFloat();
  if (auto* v = obj.get("rotation")) layer.rotation = v->AsFloat();
  if (auto* v = obj.get("scale-x")) layer.scale_x = v->AsFloat(1);
  if (auto* v = obj.get("scale-y")) layer.scale_y = v->AsFloat(1);
  if (auto* v = obj.get("opacity")) layer.opacity = v->AsFloat(1);
  if (auto* v = obj.get("stroke-width")) layer.stroke_width = v->AsFloat(1);

  if (auto* v = obj.get("d")) layer.path_data = v->AsString();
  if (auto* v = obj.get("fill")) layer.fill_str = v->AsString("none");
  if (auto* v = obj.get("stroke")) layer.stroke_str = v->AsString("none");
  if (auto* v = obj.get("mask")) layer.mask_layer = v->AsString();

  // Keyframes
  if (auto* kf = obj.get("keyframes")) {
    for (usize i = 0; i < kf->object_keys.size(); ++i) {
      if (kf->object_vals[i].type == JsonValue::kArray) {
        layer.tracks.push_back(
            parse_track(kf->object_keys[i].c_str(), kf->object_vals[i]));
      }
    }
  }

  // Children (for groups)
  if (auto* children = obj.get("children")) {
    for (auto& child : children->array_val) {
      layer.children.push_back(parse_layer(child));
    }
  }

  return layer;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool parse_anim_document(const JsonValue& root, AnimDocument& out) {
  if (root.type != JsonValue::kObject) return false;

  if (auto* v = root.get("version"))
    out.version = static_cast<u32>(v->AsNumber(1));
  if (auto* v = root.get("width")) out.width = v->AsFloat();
  if (auto* v = root.get("height")) out.height = v->AsFloat();
  if (auto* v = root.get("duration")) out.duration = v->AsFloat(1);
  if (auto* v = root.get("loop")) out.loop = v->AsBool(false);

  // Layers
  if (auto* layers = root.get("layers")) {
    for (auto& layer_val : layers->array_val) {
      out.layers.push_back(parse_layer(layer_val));
    }
  }

  return !out.layers.empty();
}

bool parse_anim_file(const char* path, AnimDocument& out) {
  FILE* file = fopen(path, "rb");
  if (!file) {
    fprintf(stderr, "ugui/anim: failed to open '%s'\n", path);
    return false;
  }
  fseek(file, 0, SEEK_END);
  auto size = ftell(file);
  fseek(file, 0, SEEK_SET);
  String data(static_cast<usize>(size), '\0');
  size_t read = fread(data.data(), 1, static_cast<usize>(size), file);
  (void)read;
  fclose(file);

  JsonValue root;
  if (!ParseJson(data.c_str(), data.size(), root)) {
    fprintf(stderr, "ugui/anim: JSON parse error in '%s'\n", path);
    return false;
  }

  return parse_anim_document(root, out);
}

bool parse_anim_data(const char* json_data, usize length, AnimDocument& out) {
  JsonValue root;
  if (!ParseJson(json_data, length, root)) return false;
  return parse_anim_document(root, out);
}

}  // namespace ugui
