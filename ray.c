#include "raylib.h"
#include <pthread.h>
#include <rlgl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "theoraplay.h"
#include "apvf.h"

#define MAX_LEADING_FRAMES    20
#define AUDIO_FETCH_PER_FRAME 2

int audio_decode_frame(uint8_t *buf) {
    float                        *db     = (float *)buf;
    const THEORAPLAY_AudioPacket *packet = aq_get();
    size_t                        sz     = packet->frames * sizeof(float) * aq.channels;
    memcpy((float *)buf, packet->samples, sz);
    THEORAPLAY_freeAudio(packet);
    return sz;
}

void audio_callback(void *buffer, unsigned int frames) {
    uint8_t            *dbuf = (uint8_t *)buffer;
    static uint8_t      audio_buf[19200];
    static unsigned int audio_buf_size  = 0;
    static unsigned int audio_buf_index = 0;
    int                 len1            = -1;
    int                 audio_size      = -1;
    int                 len             = frames * sizeof(float) * aq.channels;  // Stereo
    while (len > 0) {
        if (audio_buf_index >= audio_buf_size) {
            audio_size = audio_decode_frame(audio_buf);
            if (audio_size < 0) {
                // output silence
                printf("Skipped one frame.\n");
                continue;
            } else {
                audio_buf_size = audio_size;
            }
            audio_buf_index = 0;
        }
        len1 = audio_buf_size - audio_buf_index;

        if (len1 > len) {
            len1 = len;
        }

        memcpy(dbuf, audio_buf + audio_buf_index, len1);

        len -= len1;
        dbuf += len1;
        audio_buf_index += len1;
    }
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return 0;
    }
    int screenWidth  = 1280;
    int screenHeight = 720;

    InitWindow(screenWidth, screenHeight, "RayPlayer");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    InitAudioDevice();
    init_aq();

    if (argc >= 3) {
        const char *option = argv[2];
        if (strcmp(option, "-f") == 0) {
            const int monitor = GetCurrentMonitor();
            screenWidth       = GetMonitorWidth(monitor);
            screenHeight      = GetMonitorHeight(monitor);
            SetWindowState(FLAG_FULLSCREEN_MODE);
        }
    }
    THEORAPLAY_Decoder           *decoder = NULL;
    const THEORAPLAY_VideoFrame  *video   = NULL;
    const THEORAPLAY_AudioPacket *audio   = NULL;
    decoder = THEORAPLAY_startDecodeFile(argv[1], 30, THEORAPLAY_VIDFMT_RGB);
    // Decoding audio and video

    while (aq_empty() && THEORAPLAY_isDecoding(decoder)) {
        // Initializing AQ
        audio = THEORAPLAY_getAudio(decoder);
        if (audio) {
            aq.freq     = audio->freq;
            aq.channels = audio->channels;
            aq_put(audio);
        }
    }

    int video_initialized = 0;
    // Setting constants
    Texture texture = {0};
    texture.format  = PIXELFORMAT_UNCOMPRESSED_R8G8B8;
    texture.mipmaps = 1;

    AudioStream rayAStream = LoadAudioStream(aq.freq, 32, aq.channels);
    SetAudioStreamCallback(rayAStream, audio_callback);
    PlayAudioStream(rayAStream);
    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(WHITE);
        printf("decoding\n");
        if (THEORAPLAY_isDecoding(decoder)) {
            video = THEORAPLAY_getVideo(decoder);
            if (video) {
                printf("video\n");
                if (!video_initialized) {
                    // Initialize
                    video_initialized = 1;
                    texture.width     = video->width;
                    texture.height    = video->height;
                    texture.id = rlLoadTexture(NULL, texture.width, texture.height, texture.format,
                                               texture.mipmaps);
                    SetTargetFPS(video->fps);
                }
                UpdateTexture(texture, video->pixels);
                THEORAPLAY_freeVideo(video);
            }

            while (aq.size < 3 && THEORAPLAY_isDecoding(decoder)) {
                audio = THEORAPLAY_getAudio(decoder);
                if (audio) {
                    printf("audio put\n");
                    aq_put(audio);
                }
            }
        } else {
            break;
        }
        printf("a: %d\n", aq.size);

        if (THEORAPLAY_decodingError(decoder)) {
            TraceLog(LOG_ERROR, "CODEC: Cannot decode file");
            break;
        }
        DrawTexturePro(texture, (Rectangle){0, 0, texture.width, texture.height},
                       (Rectangle){0, 0, screenWidth, screenHeight}, (Vector2){0, 0}, 0, WHITE);

        DrawFPS(0, 0);

        EndDrawing();
    }
    quit = 1;
    pthread_cond_signal(&aq.cond);

    UnloadTexture(texture);
    UnloadAudioStream(rayAStream);

    CloseWindow();
    CloseAudioDevice();

    THEORAPLAY_stopDecode(decoder);
    aq_free();
    return 0;
}
