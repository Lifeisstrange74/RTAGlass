// A small JSON reader: objects, arrays, strings (without escapes but \" and \\), numbers and literals. The save
// files (game/save/save_file.cpp) and controls.json (platform/controls.cpp) are read with it.
#pragma once

#include <ctype.h>

#include <string>
#include <vector>

namespace util {

struct Json {
    enum Kind { kNull, kNumber, kString, kArray, kObject } kind = kNull;
    std::string text;   // a string, or a number as written
    std::vector<Json> items;
    std::vector<std::string> keys;   // an object's, beside its items

    const Json* Get(const char* key) const {
        for (size_t i = 0; i < keys.size(); ++i)
            if (keys[i] == key) return &items[i];
        return nullptr;
    }
};

struct JsonParser {
    const char* s;
    const char* end;
    bool ok = true;

    void Space() {
        while (s < end && (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')) ++s;
    }
    bool Eat(char c) {
        Space();
        if (s < end && *s == c) {
            ++s;
            return true;
        }
        return false;
    }
    std::string String() {
        std::string out;
        ++s;   // the opening quote
        while (s < end && *s != '"') {
            if (*s == '\\' && s + 1 < end) ++s;
            out += *s++;
        }
        if (s < end) ++s;
        else ok = false;
        return out;
    }
    Json Value(int depth) {
        Json v;
        Space();
        if (s >= end || depth > 16) {
            ok = false;
            return v;
        }
        if (*s == '{') {
            ++s;
            v.kind = Json::kObject;
            if (Eat('}')) return v;
            do {
                Space();
                if (s >= end || *s != '"') {
                    ok = false;
                    return v;
                }
                v.keys.push_back(String());
                if (!Eat(':')) {
                    ok = false;
                    return v;
                }
                v.items.push_back(Value(depth + 1));
            } while (ok && Eat(','));
            if (!Eat('}')) ok = false;
        } else if (*s == '[') {
            ++s;
            v.kind = Json::kArray;
            if (Eat(']')) return v;
            do v.items.push_back(Value(depth + 1));
            while (ok && Eat(','));
            if (!Eat(']')) ok = false;
        } else if (*s == '"') {
            v.kind = Json::kString;
            v.text = String();
        } else {
            const char* start = s;
            while (s < end && (isalnum((unsigned char)*s) || *s == '-' || *s == '+' || *s == '.')) ++s;
            if (s == start) ok = false;
            v.text.assign(start, s);
            v.kind = v.text == "null" || v.text == "true" || v.text == "false" ? Json::kNull : Json::kNumber;
        }
        return v;
    }
};

}  // namespace util
