#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raylib.h"
#include "raymath.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#define NUMBER_OF_CHANNELS 2
#define SAMPLE_RATE 44100

#define FRAMES_PER_SETTING (SAMPLE_RATE / 5)

typedef struct {
    size_t element_id;
    Vector2 initial_value;
    Vector2 initial_mouse_position;
} DraggingState;

typedef struct {
    float wave1;
    float wave2;
    float wave3;
} Setting;

typedef struct {
    float wave1;
    float wave2;
    float wave3;
} Phases;

typedef struct {
    ma_device audio_device;

    Setting *settings;
    size_t settings_count;
    Phases phases;

    DraggingState dragging_state;

    bool is_playing_sound;
    size_t playback_frame_counter;
} State;

static State *state = NULL;

// AUDIO

float sine_wave(double frequency, double amplitude, int i) {
    return amplitude * sin(2.0 * PI * frequency * i / SAMPLE_RATE);
}

void audio_callback(ma_device *device, void *output, const void *input, ma_uint32 frame_count) {
    (void)device;
    (void)input;

    if (!state->is_playing_sound) {
        memset(output, 0, sizeof(float) * frame_count * NUMBER_OF_CHANNELS);
        return;
    }

    for (size_t i = 0; i < frame_count; i++) {
        float signal1 = sinf(state->phases.wave1);
        float signal2 = sinf(state->phases.wave2) * 0.5f + 0.5f;
        float signal3 = sinf(state->phases.wave3) * 0.5f + 0.5f;
        float value = signal1 * signal2;

        // set output for both channels
        ((float*)output)[i * NUMBER_OF_CHANNELS + 0] = value;
        ((float*)output)[i * NUMBER_OF_CHANNELS + 1] = value;

        int setting_position = ((state->playback_frame_counter + i) / FRAMES_PER_SETTING) % state->settings_count;
        Setting setting_this_frame = state->settings[setting_position];

        // waves formula
        float wave1 = setting_this_frame.wave1 * signal3;
        float wave2 = setting_this_frame.wave2;
        float wave3 = setting_this_frame.wave3;

        state->phases.wave1 += 2.0f * PI * wave1 / SAMPLE_RATE;
        state->phases.wave2 += 2.0f * PI * wave2 / SAMPLE_RATE;
        state->phases.wave3 += 2.0f * PI * wave3 / SAMPLE_RATE;

        if (state->phases.wave1 >= 2.0f * PI) state->phases.wave1 -= 2.0f * PI;
        if (state->phases.wave2 >= 2.0f * PI) state->phases.wave2 -= 2.0f * PI;
        if (state->phases.wave3 >= 2.0f * PI) state->phases.wave3 -= 2.0f * PI;
    }

    state->playback_frame_counter += frame_count;
}

void init_audio_device(void) {
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = NUMBER_OF_CHANNELS;
    config.sampleRate = SAMPLE_RATE;
    config.dataCallback = audio_callback;

    if (ma_device_init(NULL, &config, &state->audio_device) != MA_SUCCESS) {
        printf("Error initializing audio device.\n");
        return;
    }

    ma_device_start(&state->audio_device);
    printf("Audio device initialized and started.\n");
}

// UI

bool is_hovering(Rectangle frame) {
    Vector2 mouse_position = GetMousePosition();
    return CheckCollisionPointRec(mouse_position, frame);
}

bool HandleDragging(Vector2 *output, DraggingState *dragging_state, size_t element_id, bool is_hovering_draggable_element) {
    assert(element_id != 0);

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && dragging_state->element_id == 0 && is_hovering_draggable_element) {
        dragging_state->element_id = element_id;
        dragging_state->initial_value = *output;
        dragging_state->initial_mouse_position = GetMousePosition();
    }
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && dragging_state->element_id == element_id) {
        *output = Vector2Add(Vector2Subtract(dragging_state->initial_value, GetMousePosition()), dragging_state->initial_mouse_position);
        return true;
    }
    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT) && dragging_state->element_id == element_id) {
        dragging_state->element_id = 0;
    }
    return false;
}

bool DrawSlider(float from, float to, float step, float *value, int element_id, Rectangle frame, Color color) {
    assert(to > from);
    assert(step >= 0);
    assert(value != NULL);

    float notch_size = frame.height;

    bool is_hovering_slider = is_hovering(frame);
    Vector2 dragging_output = (Vector2) { .y = 0 };
    bool is_dragging = HandleDragging(&dragging_output, &state->dragging_state, element_id, is_hovering_slider);

    float progress = 0;
    float active_x = frame.x + (float)10/2;
    float active_width = frame.width - 10;

    if (is_dragging) {
        float dragging_progress = fmax(0, fmin(1, (-(dragging_output.x - state->dragging_state.initial_mouse_position.x) - active_x) / active_width));
        float new_value = from + ((to - from) * dragging_progress);
        new_value -= fmod(new_value, step);
        *value = new_value;
        progress = (new_value - from) / (to - from);
    } else {
        progress = (*value - from) / (to - from);
    }

    DrawRectangleRec(frame, DARKGRAY);

    Rectangle selected_frame = frame;
    selected_frame.width = active_width;
    selected_frame.width *= progress;
    DrawRectangleRec(selected_frame, color);

    Rectangle notch_frame = frame;
    notch_frame.width = notch_size;
    notch_frame.x = active_x + selected_frame.width - notch_size / 2;

    if (is_dragging) {
        DrawRectangleRec(notch_frame, WHITE);
    } else {
        DrawRectangleRec(notch_frame, WHITE);
    }

    return is_dragging;
}

#define ADD(s1, s2, s3) state->settings[c++] = (Setting) { s1, s2, s3 };

void setup_settings(void) {
    if (state->settings != NULL) {
        free(state->settings);
        state->settings = NULL;
    }
    state->settings_count = 64;
    state->settings = malloc(state->settings_count * sizeof(Setting));

    size_t c = 0;

    // 0

    ADD(200, 0, 60);
    ADD(200, 0, 70);
    ADD(200, 0, 80);
    ADD(100, 0, 900);

    ADD(0,   0, 40);
    ADD(0,   0, 40);
    ADD(400, 0, 9);
    ADD(100, 0, 9);

    ADD(200, 0, 600);
    ADD(200, 0, 70);
    ADD(200, 0, 80);
    ADD(900, 0, 0);

    ADD(200, 0, 0);
    ADD(100, 0, 0);
    ADD(600, 0, 0);
    ADD(0,   0, 0);

    // 16

    ADD(200, 0, 600);
    ADD(200, 0, 700);
    ADD(200, 0, 80);
    ADD(100, 0, 90);

    ADD(0,   0, 0);
    ADD(0,   0, 0);
    ADD(400, 0, 0);
    ADD(100, 0, 0);

    ADD(200, 0, 600);
    ADD(200, 0, 700);
    ADD(200, 0, 80);
    ADD(200, 0, 90);

    ADD(200, 0, 0);
    ADD(100, 0, 0);
    ADD(400, 0, 0);
    ADD(0,   0, 0);

    // 32

    ADD(200, 0, 60);
    ADD(200, 0, 70);
    ADD(200, 0, 80);
    ADD(100, 0, 900);

    ADD(0,   0, 0);
    ADD(0,   0, 0);
    ADD(400, 0, 900);
    ADD(100, 0, 900);

    ADD(200, 0, 600);
    ADD(200, 0, 70);
    ADD(200, 0, 80);
    ADD(900, 0, 0);

    ADD(200, 0, 0);
    ADD(100, 0, 0);
    ADD(600, 0, 0);
    ADD(0,   0, 0);

    // 48

    ADD(200, 0, 600);
    ADD(200, 0, 700);
    ADD(200, 0, 80);
    ADD(100, 0, 90);

    ADD(0,   0, 40);
    ADD(0,   0, 40);
    ADD(400, 0, 9);
    ADD(100, 0, 9);

    ADD(200, 0, 600);
    ADD(200, 0, 700);
    ADD(200, 0, 80);
    ADD(200, 0, 90);

    ADD(200, 0, 400);
    ADD(100, 0, 400);
    ADD(400, 0, 400);
    ADD(0,   0, 0);
}

// PLUGIN

void plug_init(void) {
    state = malloc(sizeof(*state));
    memset(state, 0, sizeof(*state));

    setup_settings();

    init_audio_device();
    SetExitKey(KEY_Q);
}

void plug_cleanup(void) {
    ma_device_uninit(&state->audio_device);
    free(state);
    state = NULL;
}

void *plug_pre_reload(void) {
    ma_device_uninit(&state->audio_device);
    state->is_playing_sound = false;
    return state;
}

void plug_post_reload(void *old_state) {
    state = old_state;
    init_audio_device();
    setup_settings();
}

void plug_update(void) {
    char text[64] = {0};

    BeginDrawing();
    ClearBackground(BLACK);

    if (IsKeyPressed(KEY_SPACE)) {
        state->is_playing_sound = !state->is_playing_sound;
        if (state->is_playing_sound) {
            state->playback_frame_counter = 0;
            state->phases = (Phases) {0};
        }
    }

    int setting_position = (state->playback_frame_counter / FRAMES_PER_SETTING) % state->settings_count;
    Setting setting_this_frame = state->settings[setting_position];

    float signal1 = sinf(state->phases.wave1) * 0.5f + 0.5f;
    float signal2 = sinf(state->phases.wave2) * 0.5f + 0.5f;

    Rectangle rec = { 200+400*signal1, 200+400*signal2, 50, 50 };
    Color color = { signal1*255, signal2*255, 0, 255 };
    DrawRectangleRec(rec, color);

    sprintf(text, "signal1 %f", signal1);
    DrawText(text, 20, GetScreenHeight() - 50, 20, WHITE);

    const int slider_width = 800;
    int y = 10;

    DrawSlider(0, 800, 1, &setting_this_frame.wave1, 1, ((Rectangle) { 10, y, slider_width, 10 }), BLUE);
    sprintf(text, "%.0f Hz", setting_this_frame.wave1);
    DrawText(text, slider_width + 20, y, 20, WHITE);
    y += 15;
    DrawSlider(0, 2*PI, 1, &state->phases.wave1, 1, ((Rectangle) { 10, y, slider_width, 10 }), YELLOW);
    sprintf(text, "%.3f", state->phases.wave1/(2*PI));
    DrawText(text, slider_width + 20, y, 20, WHITE);
    y += 40;

    DrawSlider(0, 10, 1, &setting_this_frame.wave2, 1, ((Rectangle) { 10, y, slider_width, 10 }), BLUE);
    sprintf(text, "%.0f Hz", setting_this_frame.wave2);
    DrawText(text, slider_width + 20, y, 20, WHITE);
    y += 15;
    DrawSlider(0, 2*PI, 1, &state->phases.wave2, 1, ((Rectangle) { 10, y, slider_width, 10 }), YELLOW);
    sprintf(text, "%.3f", state->phases.wave2/(2*PI));
    DrawText(text, slider_width + 20, y, 20, WHITE);
    y += 40;

    DrawSlider(0, 900, 1, &setting_this_frame.wave3, 1, ((Rectangle) { 10, y, slider_width, 10 }), BLUE);
    sprintf(text, "%.0f Hz", setting_this_frame.wave3);
    DrawText(text, slider_width + 20, y, 20, WHITE);
    y += 15;
    DrawSlider(0, 2*PI, 1, &state->phases.wave3, 1, ((Rectangle) { 10, y, slider_width, 10 }), YELLOW);
    sprintf(text, "%.3f", state->phases.wave3/(2*PI));
    DrawText(text, slider_width + 20, y, 20, WHITE);
    y += 40;

    sprintf(text, "Setting %d", setting_position);
    DrawText(text, 20, GetScreenHeight() - 30, 20, WHITE);

    DrawFPS(GetScreenWidth() - 100, GetScreenHeight() - 30);

    EndDrawing();
}

