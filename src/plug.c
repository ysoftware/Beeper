#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int __temp;
} State;

static State *state = NULL;

void plug_init(void) {
    state = malloc(sizeof(*state));
}

void plug_cleanup(void) {
    free(state);
    state = NULL;
}

void *plug_pre_reload(void) {
    return state;
}

void plug_post_reload(void *old_state) {
    state = old_state;
}

void plug_update(void) {
    SetExitKey(KEY_Q);
    BeginDrawing();
    ClearBackground(BLACK);
    DrawText("Hello from Plugin!", 10, 10, 20, WHITE);
    EndDrawing();
}
