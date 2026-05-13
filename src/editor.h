#pragma once
#include <string>
#include <chrono>

struct Editor {
    std::string text;
    bool dirty = true;
    bool modified = false;
    std::string path = "../data/untitled.axl";

    std::string status;
    std::chrono::steady_clock::time_point status_set;


    std::chrono::steady_clock::time_point last_input;
    bool measure_pending = false;

};

void editor_insert_utf8(Editor& e, const char* bytes, int n);
void editor_backspace(Editor& e);

bool editor_save (Editor& e);
bool editor_load (Editor& e);

void editor_set_status(Editor& e, const char* msg);
const char* editor_get_status(const Editor& e, double max_age_seconds = 3.0);


void editor_newline(Editor& e);
void editor_ensure_data_dir(const Editor& e);