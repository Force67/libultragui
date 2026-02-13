#pragma once

#include <ultragui/core/types.h>

#include <string>
#include <vector>

namespace ugui {

/// Minimal JSON value for .uganim parsing. Supports objects, arrays,
/// strings, numbers, booleans, and null.
struct JsonValue {
    enum Type : u8 { Null, Bool, Number, String, Array, Object };

    Type type = Null;
    bool bool_val = false;
    f64 number_val = 0;
    std::string string_val;

    // Array storage
    std::vector<JsonValue> array_val;

    // Object storage (parallel arrays to avoid std::pair<string, incomplete type>)
    std::vector<std::string> object_keys;
    std::vector<JsonValue> object_vals;

    const JsonValue* get(const char* key) const;
    const JsonValue* at(usize index) const;
    f64 as_number(f64 def = 0) const;
    f32 as_float(f32 def = 0) const;
    const char* as_string(const char* def = "") const;
    bool as_bool(bool def = false) const;
    usize size() const;
};

/// Parse a JSON string. Returns true on success.
bool parse_json(const char* data, usize length, JsonValue& out);

} // namespace ugui
