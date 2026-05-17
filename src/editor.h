#pragma once
#include <string>
#include <chrono>

enum class SaveFormat { Plain, Axl };


struct Editor {
    std::string text;
    bool dirty = true;
    bool modified = false;
    size_t cursor = 0;
    float scroll_y = 0.0f;

    size_t sel_anchor = 0;
    size_t sel_active = 0;
    bool   has_selection = false;

    std::string path = "../data/untitled.axl";
    
    std::string status;
    std::chrono::steady_clock::time_point status_set;


    std::chrono::steady_clock::time_point last_input;
    bool measure_pending = false;
    SaveFormat format = SaveFormat::Axl;

};

void editor_insert_utf8(Editor& e, const char* bytes, int n);
void editor_backspace(Editor& e);

bool editor_save (Editor& e);
bool editor_load (Editor& e);

void editor_set_status(Editor& e, const char* msg);
const char* editor_get_status(const Editor& e, double max_age_seconds = 3.0);


void editor_newline(Editor& e);
void editor_ensure_data_dir(const Editor& e);

void editor_move_left  (Editor& e);
void editor_move_right (Editor& e);
void editor_move_up    (Editor& e);
void editor_move_down  (Editor& e);
void editor_move_home  (Editor& e);
void editor_move_end   (Editor& e);

void editor_scroll_to_cursor(Editor& e,
                              float viewport_top_px,
                              float viewport_height_px,
                              float line_height_px);

void editor_scroll_lines(Editor& e, int n_lines, float line_height_px,
                         float viewport_top_px, float viewport_height_px,
                         float text_total_height_px);

void editor_page_up   (Editor& e, float viewport_height_px, float line_height_px);
void editor_page_down (Editor& e, float viewport_height_px, float line_height_px);

void editor_selection_range(const Editor& e, size_t& lo, size_t& hi);
void editor_clear_selection(Editor& e);
void editor_select_to(Editor& e, size_t new_cursor);
void editor_delete_selection(Editor& e);

void editor_move_word_left(Editor& e);
void editor_move_word_right(Editor& e);

void editor_delete_word_left(Editor& e);