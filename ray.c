#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include "raylib.h"
#include <pthread.h>
#include <rlgl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "audio.h"

typedef struct AVList {
    AVPacket       self;
    struct AVList *next;
} AVList;

typedef struct {
    AVList         *head;
    AVList         *last;
    int             size;
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
    AVCodecContext *codecCtx;
} PQueue;

PQueue pq;

void init_pq() {
    pq.head     = NULL;
    pq.last     = NULL;
    pq.size     = 0;
    pq.codecCtx = NULL;
    pthread_mutex_init(&pq.mutex, NULL);
    pthread_cond_init(&pq.cond, NULL);
}

bool pq_empty() {
    return pq.size == 0;
}

void pq_put(AVPacket packet) {
    printf("putting...\n");
    pthread_mutex_lock(&pq.mutex);
    AVList *node = malloc(sizeof(AVList));
    node->self   = packet;
    node->next   = NULL;
    if (pq_empty()) {
        pq.head = node;
        pq.last = node;
    } else {
        pq.last->next = node;
        pq.last       = node;
    }
    pq.size++;
    pthread_cond_signal(&pq.cond);
    pthread_mutex_unlock(&pq.mutex);
}

AVPacket pq_get() {
    printf("getting...\n");
    pthread_mutex_lock(&pq.mutex);
    while (pq_empty()) {
        pthread_cond_wait(&pq.cond, &pq.mutex);
    }
    AVList *node = pq.head;
    pq.head      = pq.head->next;
    AVPacket p   = node->self;
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
    AVList *node = pq.head;
    while (node != NULL) {
        AVList *next = node->next;
        av_packet_unref(&node->self);
        free(node);
        node = next;
    }
}

int audio_decode_frame(uint8_t *buf) {
    AVPacket packet = pq_get();
    int      ret    = avcodec_send_packet(pq.codecCtx, &packet);
    if (ret < 0) {
        printf("Error sending a packet for decoding\n");
        av_packet_unref(&packet);
        return -1;
    }
    AVFrame *frame = av_frame_alloc();

    ret = avcodec_receive_frame(pq.codecCtx, frame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        av_frame_free(&frame);
        av_packet_unref(&packet);
        return -1;
    } else if (ret < 0) {
        printf("Error during decoding\n");
        av_frame_free(&frame);
        av_packet_unref(&packet);
        return -1;
    }
    // Got frame
    uint f_size = frame->linesize[0];
    uint b_ch = f_size / av_get_channel_layout_nb_channels(pq.codecCtx->channel_layout);
   int  res    = audio_resampling(pq.codecCtx, frame, AV_SAMPLE_FMT_FLT, 2, 44100, buf);

    av_frame_free(&frame);
    av_packet_unref(&packet);
    return res;
}

void audio_callback(void *buffer, unsigned int frames) {
    uint8_t            *origin = (uint8_t *)buffer;
    uint8_t            *dbuf   = (uint8_t *)buffer;
    static uint8_t      audio_buf[19200];
    static unsigned int audio_buf_size  = 0;
    static unsigned int audio_buf_index = 0;
    int                 len1            = -1;
    int                 audio_size      = -1;
    int                 len             = frames * sizeof(float) * 2;  // Stereo
    static int          jj              = 0;
    ++jj;
    printf("AFrame: %d, %d\n", jj, pq.size);
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

    Texture            texture         = {0};
    AVFormatContext   *pFormatCtx      = avformat_alloc_context();
    struct SwsContext *img_convert_ctx = sws_alloc_context();
    struct SwsContext *sws_ctx         = NULL;
    avformat_open_input(&pFormatCtx, argv[1], NULL, NULL);
    TraceLog(LOG_INFO, "CODEC: Format %s", pFormatCtx->iformat->long_name);
    avformat_find_stream_info(pFormatCtx, NULL);
    AVStream          *videoStream = NULL;
    AVStream          *audioStream = NULL;
    AVCodecParameters *videoPar    = NULL;
    AVCodecParameters *audioPar    = NULL;
    AVFrame           *pRGBFrame   = NULL;
    for (int i = 0; i < pFormatCtx->nb_streams; i++) {
        AVStream          *tmpStream = pFormatCtx->streams[i];
        AVCodecParameters *tmpPar    = tmpStream->codecpar;
        if (tmpPar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioStream = tmpStream;
            audioPar    = tmpPar;
            TraceLog(LOG_INFO, "CODEC: Audio sample rate %d, channels: %d", audioPar->sample_rate,
                     av_get_channel_layout_nb_channels(audioPar->channel_layout));
            continue;
        }
        if (tmpPar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStream = tmpStream;
            videoPar    = tmpPar;
            TraceLog(LOG_INFO, "CODEC: Resolution %d x %d, type: %d", videoPar->width,
                     videoPar->height, videoPar->codec_id);
            continue;
        }
    }
    if (!videoStream) {
        printf("Could not find video stream.\n");
        return -1;
    }

    // return with error in case no audio stream was found
    if (!audioStream) {
        printf("Could not find audio stream.\n");
        return -1;
    }
    const AVCodec *videoCodec = avcodec_find_decoder(videoPar->codec_id);
    const AVCodec *audioCodec = avcodec_find_decoder(audioPar->codec_id);
    TraceLog(LOG_INFO, "CODEC: %s ID %d, Bit rate %ld", videoCodec->name, videoCodec->id,
             videoPar->bit_rate);
    TraceLog(LOG_INFO, "FPS: %d/%d, TBR: %d/%d, TimeBase: %d/%d", videoStream->avg_frame_rate.num,
             videoStream->avg_frame_rate.den, videoStream->r_frame_rate.num,
             videoStream->r_frame_rate.den, videoStream->time_base.num, videoStream->time_base.den);

    AVCodecContext *audioCodecCtx = avcodec_alloc_context3(audioCodec);
    AVCodecContext *videoCodecCtx = avcodec_alloc_context3(videoCodec);
    pq.codecCtx                   = audioCodecCtx;
    avcodec_parameters_to_context(videoCodecCtx, videoPar);
    avcodec_parameters_to_context(audioCodecCtx, audioPar);
    avcodec_open2(videoCodecCtx, videoCodec, NULL);
    avcodec_open2(audioCodecCtx, audioCodec, NULL);

    AVFrame  *frame  = av_frame_alloc();
    AVPacket *packet = av_packet_alloc();
    sws_ctx = sws_getContext(videoCodecCtx->width, videoCodecCtx->height, videoCodecCtx->pix_fmt,
                             videoCodecCtx->width, videoCodecCtx->height, AV_PIX_FMT_RGB24,
                             SWS_FAST_BILINEAR, 0, 0, 0);
    texture.height  = videoCodecCtx->height;
    texture.width   = videoCodecCtx->width;
    texture.format  = PIXELFORMAT_UNCOMPRESSED_R8G8B8;
    texture.mipmaps = 1;
    texture.id =
        rlLoadTexture(NULL, texture.width, texture.height, texture.format, texture.mipmaps);
    SetTargetFPS(videoStream->avg_frame_rate.num / videoStream->avg_frame_rate.den);

    AudioStream rayAStream = LoadAudioStream(44100, 32, 2);
    SetAudioStreamCallback(rayAStream, audio_callback);
    PlayAudioStream(rayAStream);
    pRGBFrame         = av_frame_alloc();
    pRGBFrame->format = AV_PIX_FMT_RGB24;
    pRGBFrame->width  = videoCodecCtx->width;
    pRGBFrame->height = videoCodecCtx->height;
    av_frame_get_buffer(pRGBFrame, 0);
    int vframe = 0;
    while (!WindowShouldClose()) {
        vframe++;
        printf("VFrame %d\n", vframe);
        while (av_read_frame(pFormatCtx, packet) >= 0) {
            if (packet->stream_index == videoStream->index) {
                printf("Video frame\n");
                // Getting frame from video
                int ret = avcodec_send_packet(videoCodecCtx, packet);
                av_packet_unref(packet);
                if (ret < 0) {
                    // Error
                    printf("Error sending packet\n");
                    continue;
                }
                while (ret >= 0) {
                    ret = avcodec_receive_frame(videoCodecCtx, frame);
                    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                        break;
                    }
                    sws_scale(sws_ctx, (uint8_t const *const *)frame->data, frame->linesize, 0,
                              frame->height, pRGBFrame->data, pRGBFrame->linesize);
                    UpdateTexture(texture, pRGBFrame->data[0]);
                }
                break;
            } else if (packet->stream_index == audioStream->index) {
                // Getting audio data from audio
                printf("Audio frame\n");
                AVPacket *cloned = av_packet_clone(packet);
                pq_put(*cloned);
            }
            av_packet_unref(packet);
        }

        BeginDrawing();
        ClearBackground(WHITE);

        DrawTexturePro(texture, (Rectangle){0, 0, texture.width, texture.height},
                       (Rectangle){0, 0, screenWidth, screenHeight}, (Vector2){0, 0}, 0, WHITE);

        DrawFPS(0, 0);

        EndDrawing();
    }
    UnloadTexture(texture);
    UnloadAudioStream(rayAStream);

    CloseWindow();
    CloseAudioDevice();

    av_frame_free(&frame);
    av_frame_free(&pRGBFrame);
    av_packet_unref(packet);
    av_packet_free(&packet);
    avcodec_free_context(&videoCodecCtx);
    sws_freeContext(sws_ctx);
    avformat_close_input(&pFormatCtx);
    pq_free();
    return 0;
}
