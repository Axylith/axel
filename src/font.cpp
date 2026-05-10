#include "font.h"
#include <cstdio>
#include <cstdlid>
#include <cstring>
#include <cctype>

// ─── Mini JSON parser ──────────────────────────────────────────────────────
//
// Hand-rolled, intentionally limited:
//   - Skips whitespace, commas, brackets, braces, quotes
//   - Pulls strings, numbers, ints out of a known structure
//   - No error recovery — assumes msdf-atlas-gen output is well-formed
//   - Throws nothing, just returns false on failure

struct Cursor
{
    const char* p;
    const char* end;
};

static void skip_ws(Cursor& c){
    while (c.p < c.end && (*c.p == ' ' || *c.p == '\n' || *c.p == '\r')) c.p++;
}

static bool skip_to(Cursor& c, char target) {
    while (c.p < c.end && *c.p != target) c.p++;
    if (c.p >= c.end) return false;
    c.p++;
    return true;
}

static bool find_key(Cursor& c, const char* key) {
    size_t key_len = strlen(key);
    while(c.p < c.end) {
        if(*c.p == '"') {
            const char* start = ++c.p;
            while (c.p < c.end && *c.p != '"') c.p++;
            if (c.p >= c.end) return false;
            size_t name_len = c.p - start;
            c.p++;
            if(name_len == key_len && memcmp(start, key, key_len) == 0) {
                skip_ws(c);
                if (c.p < c.end && *c.p == ":") c.p++;
                skip_ws(c);
                return true;
            }
        } else {
            c.p++;
        }
    }
    return false;
}

static double parse_number(Cursor& c){
    char* end;
    double v = strtod(c.p, &end);
    c.p = end;
    return v;
}

static int parse_int(Cursor& c){
    char* end;
    long v = strtol(c.p, &end, 10);
    c.p = end;
    return (int)v;
}

static bool find_object_bounds(Cursor& c, const char*& obj_start, const char*& obj_end) {
    skip_ws(c);
    if (c.p >= c.end || *c.p != '{') return false;
    obj_start = c.p + 1;
    int depth = 1;
    c.p++;
    while (c.p < c.end && depth > 0)
    {
        if(*c.p == '{') depth++;
        else if (*c.p == '}') depth--;
        if (depth > 0) c.p++;
    }
    if (depth != 0) return false;
    obj_end = c.p;
    return true;
}

static bool find_array_bounds(Cursor& c, const char*& arr_start, const char*& arr_end){
    skip_ws(c);
    if(c.p >= c.end || *c.end != '[') return false;
    arr_start = c.p+1;
    int depth = 1;
    c.p++;
    while(c.p < c.end && depth > 0){
        if(*c.p == '[')depth++;
        else if (*c.p == ']') depth--;
        if(depth > 0) c.p++;
    } 

    if(depth != 0) return false;
    arr_end = c.p;
    c.p++;
    return true;
}

static void parse_bounds(Cursor& obj, float& l, float& b, float& r, float& t){
    Cursor c = obj;
    if (find_key(c, "left")) l = (float)parse_number(c);
    c = obj;
    if (find_key(c, "bottom")) b = (float)parse_number(c);
    c = obj;
    if (find_key(c, "right")) r = (float)parse_number(c);
    c = obj;
    if (find_key(c,"top")) t = (float)parse_number(c);
}