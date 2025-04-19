#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raylib.h"
#include "raymath.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#define NUMBER_OF_CHANNELS 2
#define SAMPLE_RATE 44100

typedef struct {
    size_t element_id;
    Vector2 initial_value;
    Vector2 initial_mouse_position;
} DraggingState;

typedef struct {
    ma_device audio_device;

    float freq1;
    float freq2;
    float carrier_phase;
    float modulator_phase;

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
        float carrier = sinf(state->carrier_phase);
        float modulator = sinf(state->modulator_phase) * 0.5f + 0.5f;
        float value = carrier * modulator;

        for (int s = 0; s < NUMBER_OF_CHANNELS; s++) {
            ((float*)output)[i * NUMBER_OF_CHANNELS + s] = value;
        }

        state->carrier_phase += 2.0f * PI * state->freq1 / SAMPLE_RATE;
        state->modulator_phase += 2.0f * PI * state->freq2 / SAMPLE_RATE;

        if (state->carrier_phase >= 2.0f * PI) state->carrier_phase -= 2.0f * PI;
        if (state->modulator_phase >= 2.0f * PI) state->modulator_phase -= 2.0f * PI;
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

bool DrawSlider(float from, float to, float step, float *value, int element_id, Rectangle frame) {
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
    DrawRectangleRec(selected_frame, BLUE);

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

// PLUGIN

void plug_init(void) {
    state = malloc(sizeof(*state));
    memset(state, 0, sizeof(*state));

    state->freq1 = 600;
    state->freq2 = 1;

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
    return state;
}

void plug_post_reload(void *old_state) {
    state = old_state;
    init_audio_device();
}

void plug_update(void) {
    char text[64] = {0};

    BeginDrawing();
    ClearBackground(BLACK);

    if (IsKeyPressed(KEY_SPACE)) {
        state->is_playing_sound = !state->is_playing_sound;
        if (state->is_playing_sound) state->playback_frame_counter = 0;
    }

    float carrier = sinf(state->carrier_phase) * 0.5f + 0.5f;
    float modulator = sinf(state->modulator_phase) * 0.5f + 0.5f;

    Rectangle rec = { 200+400*modulator, 200+400*carrier, 50, 50 };
    Color color = { 255, 0, 0, 255 };
    DrawRectangleRec(rec, color);

    const int slider_width = 600;

    DrawSlider(1, 200, 1, &state->freq1, 1, ((Rectangle) { 10, 10, slider_width, 10 }));
    DrawSlider(1, 200, 1, &state->freq2, 2, ((Rectangle) { 10, 40, slider_width, 10 }));

    sprintf(text, "%.0f Hz", state->freq1);
    DrawText(text, slider_width + 20, 10, 20, WHITE);

    sprintf(text, "%.0f Hz", state->freq2);
    DrawText(text, slider_width + 20, 40, 20, WHITE);

    DrawFPS(GetScreenWidth() - 100, GetScreenHeight() - 30);

    EndDrawing();
}

