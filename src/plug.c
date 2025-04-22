#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raylib.h"
#include "ffmpeg_linux.c"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#define NUMBER_OF_CHANNELS 2
#define SAMPLE_RATE 44100

#define FRAMES_PER_SETTING (SAMPLE_RATE / 5)

#define VIDEO_WIDTH 1280
#define VIDEO_HEIGHT 720
#define VIDEO_FPS 60

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
    float track3_circle_size_target;
    float track3_circle_size_value;
        
    float track3_circle_color_target;
    float track3_circle_color_value;

} UI;

typedef struct {
    ma_device audio_device;

    Track track1;
    Track track2;
    Track track3;
    size_t settings_count;

    UI ui;

    bool is_playing_sound;
    size_t playback_frame_counter;

    FFMPEG *ffmpeg;
    RenderTexture2D render_target;
    float *audio_buffer;
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
    float signal2 = square_wave(track->phases.wave2) * 0.5f + 0.5f;
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
    float signal2 = sine_wave(track->phases.wave2) * 0.5f + 0.5f;
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

    if (device != NULL && !state->is_playing_sound) {
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
        value += track2_produce_sample(&state->track2, setting_position) * 0.2f;
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

#define SET(s1, s2, s3) current_track->settings[c++] = (Setting) { s1, s2, s3 };

void setup_settings(void) {
    state->settings_count = 128;

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

    for (int i = 0; i < 4; i++) {
        SET(0, 0, 0); SET(0, 0, 0); SET(0, 0, 0); SET(0, 0, 0);
        SET(0, 0, 0); SET(0, 0, 0); SET(0, 0, 0); SET(0, 0, 0);
        SET(0, 0, 0); SET(0, 0, 0); SET(0, 0, 0); SET(0, 0, 0);
        SET(0, 0, 0); SET(0, 0, 0); SET(0, 0, 0); SET(0, 0, 0);
    }

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
    assert(c == state->settings_count);
    current_track = &state->track2;
    state->track2.settings = malloc(state->settings_count * sizeof(Setting));
    assert(current_track->settings != NULL && "Uninitialized");
    c = 0;

    for (int i = 0; i < 4; i++) {
        SET(0, 0, 0); SET(0, 0, 0); SET(0, 0, 0); SET(0, 0, 0);
        SET(0, 0, 0); SET(0, 0, 0); SET(0, 0, 0); SET(0, 0, 0);
        SET(0, 0, 0); SET(0, 0, 0); SET(0, 0, 0); SET(0, 0, 0);
        SET(0, 0, 0); SET(0, 0, 0); SET(0, 0, 0); SET(0, 0, 0);
    }

    SET(  0,  0.01, 0); SET(  0,  0.05, 0); SET(  0,  0.05, 0); SET(  0,  0.05, 0);
    SET(  0,  0.05, 0); SET(  0,  0.05, 0); SET(  0,  0.05, 0); SET(  0,  0.05, 0);
    SET(  0,  0.10, 0); SET(  0,  0.10, 0); SET(400,  0.10, 0); SET(400,  0.10, 0);
    SET(400,  10.0, 0); SET(  0,  10.0, 0); SET(400,  10.0, 0); SET(  0,  10.0, 0);

    SET(400,  10.0, 0); SET(  0,  10.0, 0); SET(400,  10.0, 0); SET(  0,  10.0, 0);
    SET(400,  0.10, 0); SET(  0,  0.10, 0); SET(  0,  0.10, 0); SET(  0,  0.10, 0);
    SET(  0,  0.05, 0); SET(  0,  0.05, 0); SET(  0,  0.05, 0); SET(400,  0.05, 0);
    SET(400,  10.0, 0); SET(  0,  10.0, 0); SET(400,  10.0, 0); SET(  0,  10.0, 0);

    SET(0, 0, 0); SET(0, 0, 0); SET(0, 0, 0); SET(0, 0, 0);
    SET(0, 0, 0); SET(0, 0, 0); SET(0, 0, 0); SET(0, 0, 0);
    SET(0, 0, 0); SET(0, 0, 0); SET(0, 0, 0); SET(0, 0, 0);
    SET(0, 0, 0); SET(0, 0, 0); SET(0, 0, 0); SET(0, 0, 0);

    SET(0, 0, 0); SET(0, 0, 0); SET(0, 0, 0); SET(0, 0, 0);
    SET(0, 0, 0); SET(0, 0, 0); SET(0, 0, 0); SET(0, 0, 0);
    SET(0, 0, 0); SET(0, 0, 0); SET(0, 0, 0); SET(0, 0, 0);
    SET(0, 0, 0); SET(0, 0, 0); SET(0, 0, 0); SET(0, 0, 0);

    // TRACK 3
    assert(c == state->settings_count);
    current_track = &state->track3;
    state->track3.settings = malloc(state->settings_count * sizeof(Setting));
    assert(current_track->settings != NULL && "Uninitialized");
    c = 0;

    for (int i = 0; i < 8; i++) {
        SET( 20, 50, 0); SET(0,  0, 0); SET(  0, 20, 0); SET(  0, 80, 0);
        SET( 20,  0, 0); SET(0,  0, 0); SET(  0,  0, 0); SET(  0,  0, 0);
        SET( 20, 20, 0); SET(0,  0, 0); SET(  0,  0, 0); SET(  0, 80, 0);
        SET( 50, 50, 0); SET(0,  0, 0); SET( 90,  0, 0); SET(  0,  0, 0);
    }

    assert(c == state->settings_count);
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

    SetExitKey(KEY_Q);
    SetWindowSize(VIDEO_WIDTH, VIDEO_HEIGHT);
    state->render_target = LoadRenderTexture(VIDEO_WIDTH, VIDEO_HEIGHT);
    init_audio_device();
    setup_settings();
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

// UI

void animate_ease_in(float *current, float start, float target, float elapsed, float duration) {
    float t = elapsed / duration;
    if (t >= 1.0f) {
        *current = target;
    } else {
        *current = start + (target - start) * /* quad ease in */ (t * t);
    }
}

void DrawFrame(int setting_position, float delta_time) {
    { // TRACK 3 - the beat
        Track track = state->track3;
        float freq1 = track.settings[setting_position].wave1;
        float freq2 = track.settings[setting_position].wave2;

        state->ui.track3_circle_size_target = 20 + (freq1 * 5);
        state->ui.track3_circle_color_target = 200 + (freq2);

        animate_ease_in(&state->ui.track3_circle_size_value, state->ui.track3_circle_size_value, state->ui.track3_circle_size_target, delta_time, 0.025f);
        animate_ease_in(&state->ui.track3_circle_color_value, state->ui.track3_circle_color_value, state->ui.track3_circle_color_target, delta_time, 0.045f);

        DrawCircle(GetScreenWidth()/2, GetScreenHeight()/2, 
                state->ui.track3_circle_size_value, 
                (Color) { state->ui.track3_circle_color_value, 0, 0, 255 });
    }
}

void plug_update(void) {
    char text[256] = {0};
    bool is_rendering = state->ffmpeg != NULL;

    if (!is_rendering && IsKeyPressed(KEY_SPACE)) {
        if (!state->is_playing_sound) {
            playback_play();
        } else {
            playback_stop();
        }
    }

    if (!is_rendering && IsKeyPressed(KEY_R)) {
        playback_stop();

        // render audio
        state->playback_frame_counter = 0;
        size_t buffer_size = sizeof(float) * state->settings_count * FRAMES_PER_SETTING * NUMBER_OF_CHANNELS;
        state->audio_buffer = malloc(buffer_size);
        audio_callback(NULL, state->audio_buffer, NULL, state->settings_count * FRAMES_PER_SETTING);

        FFMPEG *audio_ffmpeg = ffmpeg_start_rendering_audio("output.wav");
        ffmpeg_send_sound_samples(audio_ffmpeg, state->audio_buffer, buffer_size);
        ffmpeg_end_rendering(audio_ffmpeg, false);

        // render video
        state->playback_frame_counter = 0;
        state->ffmpeg = ffmpeg_start_rendering_video("output.mp4", VIDEO_WIDTH, VIDEO_HEIGHT, VIDEO_FPS);
        SetTargetFPS(500);
    }

    BeginDrawing();
    ClearBackground(BLACK);

    int latency_adjustment = state->ffmpeg == NULL ? 850 : 0;
    int setting_position = ((state->playback_frame_counter + latency_adjustment) / FRAMES_PER_SETTING) % state->settings_count;

    BeginTextureMode(state->render_target);
    ClearBackground(BLACK);
    DrawFrame(setting_position, is_rendering ? (float)1/VIDEO_FPS : GetFrameTime());
    EndTextureMode();

    DrawTexture(state->render_target.texture, 0, 0, WHITE);

    if (state->is_playing_sound) {
        sprintf(text, "%d", setting_position);
        DrawText(text, 20, GetScreenHeight() - 30, 20, WHITE);
    }

    if (is_rendering) {
        DrawText(text, 20, GetScreenHeight() - 30, 20, WHITE);

        Image image = LoadImageFromTexture(state->render_target.texture);
        if (!ffmpeg_send_frame_flipped(state->ffmpeg, image.data, image.width, image.height)) {
            ffmpeg_end_rendering(state->ffmpeg, true);
            state->ffmpeg = NULL;
        }

        state->playback_frame_counter += SAMPLE_RATE / VIDEO_FPS;
        if (state->playback_frame_counter >= state->settings_count * FRAMES_PER_SETTING) {
            ffmpeg_end_rendering(state->ffmpeg, false);
            SetTargetFPS(90);
        }
    }

    DrawFPS(GetScreenWidth() - 100, GetScreenHeight() - 30);
    EndDrawing();
}

