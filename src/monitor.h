#pragma once
#include <cstdio>
#include <cstring>
#include <cstdlib>

struct ResourceSample {
    double timestamp_ms;
    long rss_kb; // physical ram
    long vm_kb; // virtual memory
    double cpu_percent;
};

inline long get_rss_kb() {
    FILE* f = fopen("/proc/self/statm", "r");
    if (!f) return 0;
    long pages = 0, resident = 0;
    fscanf(f, "%ld %ld", &pages, &resident);
    fclose(f);
    return resident * 4;  // pages are 4KB each
}

inline long get_vm_kb() {
    FILE* f = fopen("/proc/self/statm", "r");
    if (!f) return 0;
    long pages = 0;
    fscanf(f, "%ld", &pages);
    fclose(f);
    return pages * 4;
}


struct ResourceMonitor {
    FILE* csv;
    long prev_utime = 0;
    long prev_stime = 0;
    long prev_time_ms = 0;
    
    bool open(const char* path) {
        csv = fopen(path, "w");
        if (!csv) return false;
        fprintf(csv, "timestamp_ms,rss_kb,vm_kb,cpu_percent\n");
        
        // read initial CPU ticks
        FILE* f = fopen("/proc/self/stat", "r");
        if (f) {
            fscanf(f, "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %ld %ld",
                   &prev_utime, &prev_stime);
            fclose(f);
        }
        return true;
    }
    
    void sample(double timestamp_ms) {
        if (!csv) return;
        
        long rss = get_rss_kb();
        long vm = get_vm_kb();
        
        // CPU percent since last sample
        long utime = 0, stime = 0;
        FILE* f = fopen("/proc/self/stat", "r");
        if (f) {
            fscanf(f, "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %ld %ld",
                   &utime, &stime);
            fclose(f);
        }
        
        long tick_delta = (utime + stime) - (prev_utime + prev_stime);
        double time_delta = timestamp_ms - prev_time_ms;
        double cpu = 0.0;
        if (time_delta > 0) {
            // clock ticks are in units of 1/sysconf(_SC_CLK_TCK) seconds (usually 10ms)
            cpu = (tick_delta * 10.0) / time_delta * 100.0;
        }
        
        prev_utime = utime;
        prev_stime = stime;
        prev_time_ms = timestamp_ms;
        
        fprintf(csv, "%.1f,%ld,%ld,%.2f\n", timestamp_ms, rss, vm, cpu);
    }
    
    void close() {
        if (csv) {
            fflush(csv);
            fclose(csv);
            csv = nullptr;
        }
    }
};