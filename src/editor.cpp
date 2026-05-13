#include "editor.h"
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

static constexpr size_t AXL_HEADER_LEN = 16;
static const unsigned char AXL_MAGIC[4] = {'A', 'X', 'L', '\0'};

void editor_ensure_data_dir(const Editor& e) {
    size_t slash = e.path.find_last_of('/');
    if (slash == std::string::npos) return;
    std::string dir = e.path.substr(0, slash);
    mkdir(dir.c_str(), 0755);   // ignore EEXIST
}

void editor_set_status(Editor& e, const char* msg){
    e.status = msg;
    e.status_set = std::chrono::steady_clock::now();
}

const char* editor_get_status(const Editor& e, double max_age_seconds){
    if(e.status.empty()) return nullptr;
    auto now = std::chrono::steady_clock::now();
    double age = std::chrono::duration<double>(now - e.status_set).count();
    if(age > max_age_seconds) return nullptr;
    return e.status.c_str();
}
void editor_insert_utf8(Editor& e, const char* bytes, int n){
    if (n <= 0) return;
    if (n == 1) {
        unsigned char c = (unsigned char)bytes[0];
        if (c < 0x20 && c != '\t' && c != '\n') return;   // <-- allow newline
        if (c == 0x7F) return;
    }

    e.last_input = std::chrono::steady_clock::now();
    e.measure_pending = true;
    e.text.insert(e.cursor, bytes, (size_t)n);
    e.cursor += (size_t)n;
    e.dirty = true;
    e.modified = true;
};



static size_t utf8_prev(const std::string& s, size_t pos){
    if (pos == 0) return 0;
    do {pos--;} while (pos > 0 && ((unsigned char)s[pos] & 0xC0) == 0x80);
    return pos;
    
}

static size_t utf8_next(const std::string& s, size_t pos){
    if(pos >= s.size()) return s.size();
    pos++;
    while(pos < s.size() && ((unsigned char)s[pos] & 0xC0) == 0x80) pos++;
    return pos;
}


static size_t line_start(const std::string& s, size_t pos) {
    while (pos > 0 && s[pos - 1] != '\n') pos--;
    return pos;
}

static size_t line_end(const std::string& s, size_t pos) {
    while (pos < s.size() && s[pos] != '\n') pos++;
    return pos;
}

// --- Mutations now operate at cursor ---



void editor_backspace(Editor& e) {
    if (e.cursor > e.text.size()) e.cursor = e.text.size();

    if (e.cursor == 0) return;
    e.last_input      = std::chrono::steady_clock::now();
    e.measure_pending = true;
    size_t prev = utf8_prev(e.text, e.cursor);
    e.text.erase(prev, e.cursor - prev);
    e.cursor = prev;
    e.dirty    = true;
    e.modified = true;
}

void editor_newline(Editor& e) {
    editor_insert_utf8(e, "\n", 1);
}

// --- Cursor movement (no buffer change, only cursor + dirty) ---

void editor_move_left(Editor& e) {
    if (e.cursor > e.text.size()) e.cursor = e.text.size();

    if (e.cursor == 0) return;
    e.cursor = utf8_prev(e.text, e.cursor);
    e.dirty = true;
}

void editor_move_right(Editor& e) {
    if (e.cursor > e.text.size()) e.cursor = e.text.size();

    if (e.cursor >= e.text.size()) return;
    e.cursor = utf8_next(e.text, e.cursor);
    e.dirty = true;
}

void editor_move_home(Editor& e) {
    if (e.cursor > e.text.size()) e.cursor = e.text.size();

    size_t ls = line_start(e.text, e.cursor);
    if (e.cursor != ls) { e.cursor = ls; e.dirty = true; }
}

void editor_move_end(Editor& e) {
    if (e.cursor > e.text.size()) e.cursor = e.text.size();

    size_t le = line_end(e.text, e.cursor);
    if (e.cursor != le) { e.cursor = le; e.dirty = true; }
}

void editor_move_up(Editor& e) {
    if (e.cursor > e.text.size()) e.cursor = e.text.size();

    size_t ls  = line_start(e.text, e.cursor);
    if (ls == 0) return;                         // already on first line
    size_t col = e.cursor - ls;                  // byte column on current line
    size_t prev_end   = ls - 1;                  // points at the \n separating lines
    size_t prev_start = line_start(e.text, prev_end);
    size_t prev_len   = prev_end - prev_start;
    e.cursor = prev_start + (col < prev_len ? col : prev_len);
    e.dirty = true;
}

void editor_move_down(Editor& e) {
    if (e.cursor > e.text.size()) e.cursor = e.text.size();

    size_t le = line_end(e.text, e.cursor);
    if (le == e.text.size()) return;             // already on last line
    size_t ls   = line_start(e.text, e.cursor);
    size_t col  = e.cursor - ls;
    size_t next_start = le + 1;                  // skip the \n
    size_t next_end   = line_end(e.text, next_start);
    size_t next_len   = next_end - next_start;
    e.cursor = next_start + (col < next_len ? col : next_len);
    e.dirty = true;
}

bool editor_save(Editor &e){
    if(e.path.empty()){
        editor_set_status(e, "save failed: no path");
        return false;
    }

    std::string tmp = e.path + ".tmp";

    int fd = open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        char buf[128];
        snprintf(buf, sizeof(buf), "save failed: %s", strerror(errno));
        editor_set_status(e,buf);
        return false;
    }

    unsigned char header[AXL_HEADER_LEN] = {0};
    memcpy(header, AXL_MAGIC, 4);
    header[4] = 1;
    header[5] = 0;

    ssize_t wrote = write(fd, header, AXL_HEADER_LEN);
    if (wrote != (ssize_t)AXL_HEADER_LEN) goto write_err;

    if(!e.text.empty()){
        wrote = write(fd, e.text.data(), e.text.size());
        if(wrote != (ssize_t)e.text.size()) goto write_err;
    }

    if(fsync(fd) != 0) goto write_err;
    if(close(fd) != 0) {
        char buf[128];
        snprintf(buf, sizeof(buf), "save failed on close: %s", strerror(errno));
        editor_set_status(e, buf);
        unlink(tmp.c_str());
        return false;
    }

    if(rename(tmp.c_str(), e.path.c_str()) != 0){
        char buf[128];
        snprintf(buf, sizeof(buf), "save failed on rename: %s", strerror(errno));
        editor_set_status(e, buf);
        unlink(tmp.c_str());
        return false;
    }

    e.modified = false;
    char buf[128];
    snprintf(buf, sizeof(buf), "saved %s (%zu bytes)", e.path.c_str(), e.text.size());
    editor_set_status(e, buf);
    return true;

write_err:
    {
        char errbuf[128];
        snprintf(errbuf, sizeof(errbuf), "save failed: %s", strerror(errno));
        editor_set_status(e, errbuf);
    }
    close(fd);
    unlink(tmp.c_str());
    return false;
}

bool editor_load(Editor& e) {
    int fd = open(e.path.c_str(), O_RDONLY);
    if (fd < 0) {
        char buf[128];
        snprintf(buf, sizeof(buf), "load failed: %s", strerror(errno));
        editor_set_status(e, buf);
        return false;
    }

    struct stat st;
    if (fstat(fd, &st) != 0) {
        editor_set_status(e, "load failed: stat");
        close(fd);
        return false;
    }

    // Size sanity — refuse to load >64MB into a flat string for now.
    if (st.st_size > 64 * 1024 * 1024) {
        editor_set_status(e, "load failed: file >64MB");
        close(fd);
        return false;
    }

    std::string contents;
    contents.resize((size_t)st.st_size);
    ssize_t got = read(fd, contents.data(), (size_t)st.st_size);
    close(fd);
    if (got != st.st_size) {
        editor_set_status(e, "load failed: short read");
        return false;
    }

    // Detect AXL header. If present, skip it; otherwise load as plain text.
    bool has_header = (contents.size() >= AXL_HEADER_LEN) &&
                      (memcmp(contents.data(), AXL_MAGIC, 4) == 0);

    if (has_header) {
        e.text.assign(contents.begin() + AXL_HEADER_LEN, contents.end());
    } else {
        e.text = std::move(contents);
    }

    e.dirty    = true;
    e.modified = false;

    char buf[128];
    snprintf(buf, sizeof(buf), "loaded %s (%zu bytes%s)",
             e.path.c_str(), e.text.size(),
             has_header ? ", .axl" : ", plain text");
    e.cursor = 0;
    editor_set_status(e, buf);
    return true;
}