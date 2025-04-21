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

// TODO: add lerp to settings

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
    Setting *settings;
    Phases phases;
} Track;

typedef struct {
    ma_device audio_device;

    Track track1;
    Track track2;
    Track track3;
    size_t settings_count;

    DraggingState dragging_state;

    bool is_playing_sound;
    size_t playback_frame_counter;
} State;

static State *state = NULL;

// AUDIO

float sine_wave(float phase) {
    return sinf(phase);
}

float triangle_wave(float phase) {
    return 2.0f * fabsf(2.0f * (phase / (2.0f * PI) - floorf(phase / (2.0f * PI) + 0.5f))) - 1.0f;
}

float square_wave(float phase) {
    return (sinf(phase) >= 0.0f) ? 1.0f : -1.0f;
}

float track1_produce_sample(Track *track, int setting_position) {
    float signal1 = sine_wave(track->phases.wave1);
    float signal2 = sine_wave(track->phases.wave2) * 0.5f + 0.5f;
    float signal3 = sine_wave(track->phases.wave3) * 0.5f + 0.5f;
    float value = signal1 * signal2;

    // produce next phase value

    float wave1 = track->settings[setting_position].wave1 * signal3;
    float wave2 = track->settings[setting_position].wave2;
    float wave3 = track->settings[setting_position].wave3;

    track->phases.wave1 += 2.0f * PI * wave1 / SAMPLE_RATE;
    track->phases.wave2 += 2.0f * PI * wave2 / SAMPLE_RATE;
    track->phases.wave3 += 2.0f * PI * wave3 / SAMPLE_RATE;

    if (track->phases.wave1 >= 2.0f * PI) track->phases.wave1 -= 2.0f * PI;
    if (track->phases.wave2 >= 2.0f * PI) track->phases.wave2 -= 2.0f * PI;
    if (track->phases.wave3 >= 2.0f * PI) track->phases.wave3 -= 2.0f * PI;

    return value;
}

float track2_produce_sample(Track *track, int setting_position) {
    float signal1 = sine_wave(track->phases.wave1);
    float signal2 = sinf(track->phases.wave2) * 0.5f + 0.5f;
    float value = signal1 * signal2;

    // produce next phase value

    float wave1 = track->settings[setting_position].wave1;
    float wave2 = track->settings[setting_position].wave2;
    float wave3 = track->settings[setting_position].wave3;

    track->phases.wave1 += 2.0f * PI * wave1 / SAMPLE_RATE;
    track->phases.wave2 += 2.0f * PI * wave2 / SAMPLE_RATE;
    track->phases.wave3 += 2.0f * PI * wave3 / SAMPLE_RATE;

    if (track->phases.wave1 >= 2.0f * PI) track->phases.wave1 -= 2.0f * PI;
    if (track->phases.wave2 >= 2.0f * PI) track->phases.wave2 -= 2.0f * PI;
    if (track->phases.wave3 >= 2.0f * PI) track->phases.wave3 -= 2.0f * PI;

    return value;
}

float track3_produce_sample(Track *track, int setting_position) {
    float signal1 = triangle_wave(track->phases.wave1);
    float signal2 = sinf(track->phases.wave2) * 0.5f + 0.5f;
    float value = signal1 * signal2;

    // produce next phase value

    float wave1 = track->settings[setting_position].wave1;
    float wave2 = track->settings[setting_position].wave2;
    float wave3 = track->settings[setting_position].wave3;

    track->phases.wave1 += 2.0f * PI * wave1 / SAMPLE_RATE;
    track->phases.wave2 += 2.0f * PI * wave2 / SAMPLE_RATE;
    track->phases.wave3 += 2.0f * PI * wave3 / SAMPLE_RATE;

    if (track->phases.wave1 >= 2.0f * PI) track->phases.wave1 -= 2.0f * PI;
    if (track->phases.wave2 >= 2.0f * PI) track->phases.wave2 -= 2.0f * PI;
    if (track->phases.wave3 >= 2.0f * PI) track->phases.wave3 -= 2.0f * PI;

    return value;
}

void audio_callback(ma_device *device, void *output, const void *input, ma_uint32 frames_count) {
    (void)device;
    (void)input;

    if (!state->is_playing_sound) {
        memset(output, 0, sizeof(float) * frames_count * NUMBER_OF_CHANNELS);
        return;
    }

    for (size_t i = 0; i < frames_count; i++) {
        size_t frame = state->playback_frame_counter + i;
        int setting_position = (frame / FRAMES_PER_SETTING) % state->settings_count;

        // reset phases to 0 on repeat
        int previous_setting_position = ((frame-1) / FRAMES_PER_SETTING) % state->settings_count;
        if (previous_setting_position > setting_position) {
            state->track1.phases = (Phases) {0};
            state->track2.phases = (Phases) {0};
        }

        float value = 0;
        value += track1_produce_sample(&state->track1, setting_position) * 0.4f;
        value += track2_produce_sample(&state->track2, setting_position) * 0.1f;
        value += track3_produce_sample(&state->track3, setting_position) * 0.9f;

        ((float*)output)[i * NUMBER_OF_CHANNELS + 0] = value;
        ((float*)output)[i * NUMBER_OF_CHANNELS + 1] = value;
    }

    state->playback_frame_counter += frames_count;
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

#define SET(s1, s2, s3) current_track->settings[c++] = (Setting) { s1, s2, s3 };

void setup_settings(void) {
    state->settings_count = 64;

    if (state->track1.settings != NULL) {
        free(state->track1.settings);
        state->track1.settings = NULL;
    }
    if (state->track2.settings != NULL) {
        free(state->track2.settings);
        state->track2.settings = NULL;
    }
    if (state->track3.settings != NULL) {
        free(state->track3.settings);
        state->track3.settings = NULL;
    }

    // TRACK 1
    Track *current_track = &state->track1;
    state->track1.settings = malloc(state->settings_count * sizeof(Setting));
    assert(current_track->settings != NULL && "Uninitialized");
    size_t c = 0;

    SET(200, 0, 60);   SET(200, 0, 70);   SET(200, 0, 80);  SET(100, 0, 900);
    SET(0,   0, 40);   SET(0,   0, 40);   SET(400, 0, 9);   SET(100, 0, 9);
    SET(200, 0, 600);  SET(200, 0, 70);   SET(200, 0, 80);  SET(900, 0, 0);
    SET(200, 0, 0);    SET(100, 0, 0);    SET(600, 0, 0);   SET(0,   0, 0);

    SET(200, 0, 600);  SET(200, 0, 700);  SET(200, 0, 80);  SET(100, 0, 90);
    SET(0,   0, 0);    SET(0,   0, 0);    SET(400, 0, 0);   SET(100, 0, 0);
    SET(200, 0, 600);  SET(200, 0, 700);  SET(200, 0, 80);  SET(200, 0, 90);
    SET(200, 0, 0);    SET(100, 0, 0);    SET(400, 0, 0);   SET(0,   0, 0);

    SET(200, 0, 60);   SET(200, 0, 70);   SET(200, 0, 80);  SET(100, 0, 900);
    SET(0,   0, 0);    SET(0,   0, 0);    SET(400, 0, 900); SET(100, 0, 900);
    SET(200, 0, 600);  SET(200, 0, 70);   SET(200, 0, 80);  SET(900, 0, 0);
    SET(200, 0, 0);    SET(100, 0, 0);    SET(600, 0, 0);   SET(0,   0, 0);

    SET(200, 0, 600);  SET(200, 0, 700);  SET(200, 0, 80);  SET(100, 0, 90);
    SET(0,   0, 40);   SET(0,   0, 40);   SET(400, 0, 9);   SET(100, 0, 9);
    SET(200, 0, 600);  SET(200, 0, 700);  SET(200, 0, 80);  SET(200, 0, 90);
    SET(200, 0, 400);  SET(100, 0, 400);  SET(400, 0, 400); SET(0,   0, 0);

    // TRACK 2
    current_track = &state->track2;
    state->track2.settings = malloc(state->settings_count * sizeof(Setting));
    assert(current_track->settings != NULL && "Uninitialized");
    c = 0;

    SET(600,  4, 0); SET(603,  4, 0); SET(606,  4, 0); SET(609,  4, 0);
    SET(612,  4, 0); SET(  0,  4, 0); SET(615,  4, 0); SET(  0,  4, 0);
    SET(  0,  4, 0); SET(  0,  4, 0); SET(200,  4, 0); SET(  0,  4, 0);
    SET(400,  4, 0); SET(  0,  4, 0); SET(400,  4, 0); SET(  0,  4, 0);

    SET(600,  4, 0); SET(  0,  4, 0); SET(600,  4, 0); SET(  0,  4, 0);
    SET(600,  4, 0); SET(  0,  4, 0); SET(  0,  4, 0); SET(  0,  4, 0);
    SET(600,  4, 0); SET(603,  4, 0); SET(606,  4, 0); SET(609,  4, 0);
    SET(400,  4, 0); SET(  0,  4, 0); SET(  0,  4, 0); SET(  0,  4, 0);

    SET(0, 0, 0); SET(0, 0, 0); SET(0, 0, 0); SET(0, 0, 0);
    SET(0, 0, 0); SET(0, 0, 0); SET(0, 0, 0); SET(0, 0, 0);
    SET(0, 0, 0); SET(0, 0, 0); SET(0, 0, 0); SET(0, 0, 0);
    SET(0, 0, 0); SET(0, 0, 0); SET(0, 0, 0); SET(0, 0, 0);

    SET(0, 0, 0); SET(0, 0, 0); SET(0, 0, 0); SET(0, 0, 0);
    SET(0, 0, 0); SET(0, 0, 0); SET(0, 0, 0); SET(0, 0, 0);
    SET(0, 0, 0); SET(0, 0, 0); SET(0, 0, 0); SET(0, 0, 0);
    SET(0, 0, 0); SET(0, 0, 0); SET(0, 0, 0); SET(0, 0, 0);

    // TRACK 3
    current_track = &state->track3;
    state->track3.settings = malloc(state->settings_count * sizeof(Setting));
    assert(current_track->settings != NULL && "Uninitialized");
    c = 0;

    for (int i = 0; i < 4; i++) {
        SET( 20, 50, 0); SET(0,  0, 0); SET(  0, 20, 0); SET(  0, 80, 0);
        SET( 20,  0, 0); SET(0,  0, 0); SET(  0,  0, 0); SET(  0,  0, 0);
        SET( 20, 20, 0); SET(0,  0, 0); SET(  0,  0, 0); SET(  0, 80, 0);
        SET( 50, 50, 0); SET(0,  0, 0); SET( 90,  0, 0); SET(  0,  0, 0);
    }
}

void playback_reset(void) {
    state->playback_frame_counter = 0;
    state->track1.phases = (Phases) {0};
    state->track2.phases = (Phases) {0};
    state->track3.phases = (Phases) {0};
}

void playback_play(void) {
    playback_reset();
    state->is_playing_sound = true;
}

void playback_stop(void) {
    state->is_playing_sound = false;
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
    return state;
}

void plug_post_reload(void *old_state) {
    state = old_state;
    init_audio_device();
    setup_settings();
    playback_reset();
}

void plug_update(void) {
    char text[64] = {0};

    BeginDrawing();
    ClearBackground(BLACK);

    if (IsKeyPressed(KEY_SPACE)) {
        if (!state->is_playing_sound) {
            playback_play();
        } else {
            playback_stop();
        }
    }

    int setting_position = (state->playback_frame_counter / FRAMES_PER_SETTING) % state->settings_count;
    Setting setting_this_frame = state->track1.settings[setting_position];

    float signal1 = sinf(state->track1.phases.wave1) * 0.5f + 0.5f;
    float signal2 = sinf(state->track1.phases.wave2) * 0.5f + 0.5f;

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
    DrawSlider(0, 2*PI, 1, &state->track1.phases.wave1, 1, ((Rectangle) { 10, y, slider_width, 10 }), YELLOW);
    sprintf(text, "%.3f", state->track1.phases.wave1/(2*PI));
    DrawText(text, slider_width + 20, y, 20, WHITE);
    y += 40;

    DrawSlider(0, 10, 1, &setting_this_frame.wave2, 1, ((Rectangle) { 10, y, slider_width, 10 }), BLUE);
    sprintf(text, "%.0f Hz", setting_this_frame.wave2);
    DrawText(text, slider_width + 20, y, 20, WHITE);
    y += 15;
    DrawSlider(0, 2*PI, 1, &state->track1.phases.wave2, 1, ((Rectangle) { 10, y, slider_width, 10 }), YELLOW);
    sprintf(text, "%.3f", state->track1.phases.wave2/(2*PI));
    DrawText(text, slider_width + 20, y, 20, WHITE);
    y += 40;

    DrawSlider(0, 900, 1, &setting_this_frame.wave3, 1, ((Rectangle) { 10, y, slider_width, 10 }), BLUE);
    sprintf(text, "%.0f Hz", setting_this_frame.wave3);
    DrawText(text, slider_width + 20, y, 20, WHITE);
    y += 15;
    DrawSlider(0, 2*PI, 1, &state->track1.phases.wave3, 1, ((Rectangle) { 10, y, slider_width, 10 }), YELLOW);
    sprintf(text, "%.3f", state->track1.phases.wave3/(2*PI));
    DrawText(text, slider_width + 20, y, 20, WHITE);
    y += 40;

    sprintf(text, "Setting %d", setting_position);
    DrawText(text, 20, GetScreenHeight() - 30, 20, WHITE);

    DrawFPS(GetScreenWidth() - 100, GetScreenHeight() - 30);

    EndDrawing();
}

