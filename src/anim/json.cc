#include <ultragui/anim/json.h>

#include <cstdlib>
#include <cstring>

namespace ugui {

// ---------------------------------------------------------------------------
// JsonValue accessors
// ---------------------------------------------------------------------------

const JsonValue* JsonValue::get(const char* key) const {
  if (type != kObject) return nullptr;
  for (usize i = 0; i < object_keys.size(); ++i)
    if (object_keys[i] == key) return &object_vals[i];
  return nullptr;
}

const JsonValue* JsonValue::at(usize index) const {
  if (type != kArray || index >= array_val.size()) return nullptr;
  return &array_val[index];
}

f64 JsonValue::AsNumber(f64 def) const {
  return type == kNumber ? number_val : def;
}

f32 JsonValue::AsFloat(f32 def) const {
  return type == kNumber ? static_cast<f32>(number_val) : def;
}

const char* JsonValue::AsString(const char* def) const {
  return type == kString ? string_val.c_str() : def;
}

bool JsonValue::AsBool(bool def) const {
  return type == kBool ? bool_val : def;
}

usize JsonValue::size() const {
  if (type == kArray) return array_val.size();
  if (type == kObject) return object_keys.size();
  return 0;
}

// ---------------------------------------------------------------------------
// JSON parser
// ---------------------------------------------------------------------------

struct JsonParser {
  const char* p;
  const char* end;

  bool eof() const { return p >= end; }

  void skip_ws() {
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
      ++p;
  }

  bool parse_value(JsonValue& out) {
    skip_ws();
    if (eof()) return false;

    char c = *p;
    if (c == '"') return parse_string(out);
    if (c == '{') return parse_object(out);
    if (c == '[') return parse_array(out);
    if (c == 't' || c == 'f') return parse_bool(out);
    if (c == 'n') return parse_null(out);
    if (c == '-' || (c >= '0' && c <= '9')) return parse_number(out);
    return false;
  }

  bool parse_string(JsonValue& out) {
    out.type = JsonValue::kString;
    out.string_val.clear();
    ++p;  // skip opening "
    while (p < end && *p != '"') {
      if (*p == '\\') {
        ++p;
        if (p >= end) return false;
        switch (*p) {
          case '"':
            out.string_val += '"';
            break;
          case '\\':
            out.string_val += '\\';
            break;
          case '/':
            out.string_val += '/';
            break;
          case 'n':
            out.string_val += '\n';
            break;
          case 't':
            out.string_val += '\t';
            break;
          case 'r':
            out.string_val += '\r';
            break;
          default:
            out.string_val += *p;
            break;
        }
      } else {
        out.string_val += *p;
      }
      ++p;
    }
    if (p < end) ++p;  // skip closing "
    return true;
  }

  bool parse_number(JsonValue& out) {
    out.type = JsonValue::kNumber;
    char* num_end = nullptr;
    out.number_val = std::strtod(p, &num_end);
    if (num_end == p) return false;
    p = num_end;
    return true;
  }

  bool parse_bool(JsonValue& out) {
    out.type = JsonValue::kBool;
    if (end - p >= 4 && std::memcmp(p, "true", 4) == 0) {
      out.bool_val = true;
      p += 4;
      return true;
    }
    if (end - p >= 5 && std::memcmp(p, "false", 5) == 0) {
      out.bool_val = false;
      p += 5;
      return true;
    }
    return false;
  }

  bool parse_null(JsonValue& out) {
    out.type = JsonValue::kNull;
    if (end - p >= 4 && std::memcmp(p, "null", 4) == 0) {
      p += 4;
      return true;
    }
    return false;
  }

  bool parse_object(JsonValue& out) {
    out.type = JsonValue::kObject;
    out.object_keys.clear();
    out.object_vals.clear();
    ++p;  // skip '{'
    skip_ws();
    if (p < end && *p == '}') {
      ++p;
      return true;
    }

    while (p < end) {
      skip_ws();
      if (*p != '"') return false;
      JsonValue key;
      if (!parse_string(key)) return false;

      skip_ws();
      if (p >= end || *p != ':') return false;
      ++p;

      JsonValue val;
      if (!parse_value(val)) return false;
      out.object_keys.push_back(std::move(key.string_val));
      out.object_vals.push_back(std::move(val));

      skip_ws();
      if (p < end && *p == ',') {
        ++p;
        continue;
      }
      if (p < end && *p == '}') {
        ++p;
        return true;
      }
      return false;
    }
    return false;
  }

  bool parse_array(JsonValue& out) {
    out.type = JsonValue::kArray;
    out.array_val.clear();
    ++p;  // skip '['
    skip_ws();
    if (p < end && *p == ']') {
      ++p;
      return true;
    }

    while (p < end) {
      JsonValue val;
      if (!parse_value(val)) return false;
      out.array_val.push_back(std::move(val));

      skip_ws();
      if (p < end && *p == ',') {
        ++p;
        continue;
      }
      if (p < end && *p == ']') {
        ++p;
        return true;
      }
      return false;
    }
    return false;
  }
};

bool ParseJson(const char* data, usize length, JsonValue& out) {
  JsonParser parser{data, data + length};
  return parser.parse_value(out);
}

}  // namespace ugui
