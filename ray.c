#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raylib.h"

int main(void) {
    const int screenWidth  = 1280;
    const int screenHeight = 720;

    InitWindow(screenWidth, screenHeight, "FFMPEG Video Player");
    SetWindowState(FLAG_WINDOW_RESIZABLE);

    Image img                          = {0};
    img.format                         = PIXELFORMAT_UNCOMPRESSED_R8G8B8;
    img.mipmaps                        = 1;
    Texture            texture         = {0};
    AVFormatContext   *pFormatCtx      = avformat_alloc_context();
    struct SwsContext *img_convert_ctx = sws_alloc_context();
    struct SwsContext *sws_ctx         = NULL;
    avformat_open_input(&pFormatCtx, "video.mp4", NULL, NULL);
    TraceLog(LOG_INFO, "CODEC: Format %s", pFormatCtx->iformat->long_name);
    avformat_find_stream_info(pFormatCtx, NULL);
    AVStream          *stream    = NULL;
    AVCodecParameters *par       = NULL;
    AVFrame           *pRGBFrame = NULL;
    for (int i = 0; i < pFormatCtx->nb_streams; i++) {
        stream = pFormatCtx->streams[i];
        par    = stream->codecpar;
        if (par->codec_type != AVMEDIA_TYPE_VIDEO) {
            continue;
        }
        if (par->codec_type == AVMEDIA_TYPE_VIDEO) {
            TraceLog(LOG_INFO, "CODEC: Resolution %d x %d, Type: %d", par->width, par->height,
                     par->codec_id);
            break;
        }
    }
    const AVCodec *codec = avcodec_find_decoder(par->codec_id);
    TraceLog(LOG_INFO, "CODEC: %s ID %d, Bit rate %ld", codec->name, codec->id, par->bit_rate);
    TraceLog(LOG_INFO, "FPS: %d/%d, TBR: %d/%d, TimeBase: %d/%d", stream->avg_frame_rate.num,
             stream->avg_frame_rate.den, stream->r_frame_rate.num, stream->r_frame_rate.den,
             stream->time_base.num, stream->time_base.den);

    AVCodecContext *codecCtx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codecCtx, par);
    avcodec_open2(codecCtx, codec, NULL);

    AVFrame  *frame  = av_frame_alloc();
    AVPacket *packet = av_packet_alloc();
    sws_ctx = sws_getContext(codecCtx->width, codecCtx->height, codecCtx->pix_fmt, codecCtx->width,
                             codecCtx->height, AV_PIX_FMT_RGB24, SWS_FAST_BILINEAR, 0, 0, 0);
    img.width  = codecCtx->width;
    img.height = codecCtx->height;
    img.data   = malloc(img.width * img.height * 3);

    texture = LoadTextureFromImage(img);
    UnloadImage(img);

    SetTargetFPS(stream->avg_frame_rate.num / stream->avg_frame_rate.den);

    while (!WindowShouldClose()) {
        while (av_read_frame(pFormatCtx, packet) >= 0) {
            if (packet->stream_index == stream->index) {
                // Getting frame from video
                int packet_rec = avcodec_send_packet(codecCtx, packet);
                int frame_rec  = avcodec_receive_frame(codecCtx, frame);
                if (packet_rec < 0 || frame_rec < 0) {
                    // Error
                    av_packet_unref(packet);
                    continue;
                }
                // Convert the image from its native format to RGB
                // You must create new buffer for RGB data
                // TraceLog(LOG_INFO, "CODEC: Frame PTS: %ld", frame->pts);
                pRGBFrame = av_frame_alloc();

                pRGBFrame->format = AV_PIX_FMT_RGB24;
                pRGBFrame->width  = frame->width;
                pRGBFrame->height = frame->height;
                av_frame_get_buffer(pRGBFrame, 0);

                sws_scale(sws_ctx, (uint8_t const *const *)frame->data, frame->linesize, 0,
                          frame->height, pRGBFrame->data, pRGBFrame->linesize);
                UpdateTexture(texture, pRGBFrame->data[0]);
                av_frame_free(&pRGBFrame);
                break;
            }
            av_packet_unref(packet);
        }

        BeginDrawing();
        ClearBackground(WHITE);

        DrawTexturePro(texture, (Rectangle){0, 0, texture.width, texture.height},
                       (Rectangle){0, 0, GetScreenWidth(), GetScreenHeight()}, (Vector2){0, 0}, 0,
                       WHITE);
        DrawFPS(0, 0);

        EndDrawing();
    }

    UnloadTexture(texture);

    CloseWindow();

    av_frame_free(&frame);
    av_packet_unref(packet);
    av_packet_free(&packet);
    avcodec_free_context(&codecCtx);
    sws_freeContext(sws_ctx);
    avformat_close_input(&pFormatCtx);
    return 0;
}
