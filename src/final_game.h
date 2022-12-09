#pragma once

// Simple Frogger Game
// Brings together major engine systems to make a very simple "frogger."

typedef struct final_game_t final_game_t;

typedef struct fs_t fs_t;
typedef struct heap_t heap_t;
typedef struct render_t render_t;
typedef struct wm_window_t wm_window_t;

// Create an instance of simple test game.
final_game_t* final_game_create(heap_t* heap, fs_t* fs, wm_window_t* window, render_t* render, int argc, const char** argv);

// Destroy an instance of simple test game.
void final_game_destroy(final_game_t* game);

// Per-frame update for our simple test game.
void final_game_update(final_game_t* game);
