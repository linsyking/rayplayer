#include "raylib.h"
#include <pthread.h>
#include <rlgl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "theoraplay.h"

typedef struct PList {
    const THEORAPLAY_AudioPacket *packet;
    struct PList                 *next;
} PList;

typedef struct {
    PList          *head;
    PList          *last;
    int             size;
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
} PQueue;

PQueue pq;

void init_pq() {
    pq.head = NULL;
    pq.last = NULL;
    pq.size = 0;
    pthread_mutex_init(&pq.mutex, NULL);
    pthread_cond_init(&pq.cond, NULL);
}

bool pq_empty() {
    return pq.size == 0;
}

void pq_put(const THEORAPLAY_AudioPacket *packet) {
    pthread_mutex_lock(&pq.mutex);
    PList *pl  = malloc(sizeof(PList));
    pl->packet = packet;
    pl->next   = NULL;
    if (pq_empty()) {
        pq.head = pl;
        pq.last = pl;
    } else {
        pq.last->next = pl;
        pq.last       = pl;
    }
    pq.size++;
    pthread_cond_signal(&pq.cond);
    pthread_mutex_unlock(&pq.mutex);
}

const THEORAPLAY_AudioPacket *pq_get() {
    pthread_mutex_lock(&pq.mutex);
    while (pq_empty()) {
        pthread_cond_wait(&pq.cond, &pq.mutex);
    }
    PList *node                     = pq.head;
    pq.head                         = pq.head->next;
    const THEORAPLAY_AudioPacket *p = node->packet;
    free(node);
    if (pq.head == NULL) {
        pq.last = NULL;
    }
    pq.size--;
    pthread_mutex_unlock(&pq.mutex);
    return p;
}

void pq_free() {
    pthread_mutex_destroy(&pq.mutex);
    pthread_cond_destroy(&pq.cond);
    // Free all nodes
    printf("total %d node found, destroying them...\n", pq.size);
    PList *node = pq.head;
    while (node != NULL) {
        PList *next = node->next;
        THEORAPLAY_freeAudio(node->packet);
        free(node);
        node = next;
    }
}

int audio_decode_frame(uint8_t *buf) {
    float                        *db     = (float *)buf;
    const THEORAPLAY_AudioPacket *packet = pq_get();
    size_t                        sz     = packet->frames * sizeof(float) * 2;
    // printf("size %ld\n", sz);
    memcpy((float *)buf, packet->samples, sz);
    // for (uint j = 0; j < packet->frames * 2; ++j) {
    //     db[j] = packet->samples[j];
    // }

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
    int                 len             = frames * sizeof(float) * 2;  // Stereo
    static int jj = 0;
    ++jj;
    printf("frames %d\n", jj);
    if (jj >= 100) {
        return;
    }
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
    init_pq();

    if (argc >= 3) {
        const char *option = argv[2];
        if (strcmp(option, "-f") == 0) {
            const int monitor = GetCurrentMonitor();
            // SetWindowSize(1920, 1080);
            screenWidth  = GetMonitorWidth(monitor);
            screenHeight = GetMonitorHeight(monitor);
            // ToggleFullscreen();
            SetWindowState(FLAG_FULLSCREEN_MODE);
        }
    }
    THEORAPLAY_Decoder           *decoder = NULL;
    const THEORAPLAY_VideoFrame  *video   = NULL;
    const THEORAPLAY_AudioPacket *audio   = NULL;
    decoder = THEORAPLAY_startDecodeFile(argv[1], 30, THEORAPLAY_VIDFMT_RGB);

    Texture texture     = {0};
    texture.format      = PIXELFORMAT_UNCOMPRESSED_R8G8B8;
    texture.mipmaps     = 1;
    bool texture_status = 0;

    SetTargetFPS(60);

    AudioStream rayAStream = LoadAudioStream(44100, 32, 2);
    SetAudioStreamCallback(rayAStream, audio_callback);
    PlayAudioStream(rayAStream);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(WHITE);
        while (THEORAPLAY_isDecoding(decoder)) {
            video = THEORAPLAY_getVideo(decoder);
            if (video) {
                if (texture_status == 0) {
                    // Not Loaded
                    texture.width  = video->width;
                    texture.height = video->height;
                    texture.id = rlLoadTexture(NULL, texture.width, texture.height, texture.format,
                                               texture.mipmaps);
                    texture_status = 1;
                    SetTargetFPS(video->fps);
                }
                UpdateTexture(texture, video->pixels);
                THEORAPLAY_freeVideo(video);
            }

            audio = THEORAPLAY_getAudio(decoder);
            if (audio) {
                // printf("putting...\n");
                pq_put(audio);
                break;
            }
        }
        if (THEORAPLAY_decodingError(decoder)) {
            TraceLog(LOG_ERROR, "CODEC: Cannot decode file");
            break;
        }
        DrawTexturePro(texture, (Rectangle){0, 0, texture.width, texture.height},
                       (Rectangle){0, 0, screenWidth, screenHeight}, (Vector2){0, 0}, 0, WHITE);

        DrawFPS(0, 0);

        EndDrawing();
    }
    UnloadTexture(texture);
    UnloadAudioStream(rayAStream);

    CloseWindow();
    CloseAudioDevice();

    THEORAPLAY_stopDecode(decoder);
    pq_free();
    return 0;
}
