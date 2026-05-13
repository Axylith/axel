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

void editor_insert_utf8(Editor& e, const char* bytes, int n){
    if (n <= 0) return;
    if (n == 1) {
        unsigned char c = (unsigned char)bytes[0];
        if(c < 0x20 && c != '\t') return;
        if (c == 0x7F) return;
    }

    e.last_input = std::chrono::steady_clock::now();
    e.measure_pending = true;
    e.text.append(bytes, (size_t)n);
    e.dirty = true;
    e.modified = true;
};

void editor_backspace(Editor& e) {
    if (e.text.empty()) return;
    e.last_input      = std::chrono::steady_clock::now();
    e.measure_pending = true;
    size_t i = e.text.size();
    do { i--; } while (i > 0 && ((unsigned char)e.text[i] & 0xC0) == 0x80);
    e.text.erase(i);
    e.dirty    = true;
    e.modified = true;
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
    editor_set_status(e, buf);
    return true;
}


void editor_newline(Editor& e) {
    e.last_input      = std::chrono::steady_clock::now();
    e.measure_pending = true;
    e.text.push_back('\n');
    e.dirty    = true;
    e.modified = true;
}