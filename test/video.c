#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <stdio.h>

int main(void) {
    const int          screenWidth     = 1280;
    const int          screenHeight    = 720;
    AVFormatContext   *pFormatCtx      = avformat_alloc_context();
    struct SwsContext *img_convert_ctx = sws_alloc_context();
    struct SwsContext *sws_ctx         = NULL;
    avformat_open_input(&pFormatCtx, "../video.mp4", NULL, NULL);
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
            break;
        }
    }
    const AVCodec *codec = avcodec_find_decoder(par->codec_id);

    AVCodecContext *codecCtx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codecCtx, par);
    avcodec_open2(codecCtx, codec, NULL);

    AVFrame  *frame  = av_frame_alloc();
    AVPacket *packet = av_packet_alloc();
    sws_ctx = sws_getContext(codecCtx->width, codecCtx->height, codecCtx->pix_fmt, codecCtx->width,
                             codecCtx->height, AV_PIX_FMT_RGB24, SWS_FAST_BILINEAR, 0, 0, 0);

    pRGBFrame = av_frame_alloc();

    pRGBFrame->format = AV_PIX_FMT_RGB24;
    pRGBFrame->width  = codecCtx->width;
    pRGBFrame->height = codecCtx->height;
    av_frame_get_buffer(pRGBFrame, 0);
    while (av_read_frame(pFormatCtx, packet) >= 0) {
        if (packet->stream_index == stream->index) {
            // Getting frame from video
            int packet_rec = avcodec_send_packet(codecCtx, packet);
            av_packet_unref(packet);
            int frame_rec = avcodec_receive_frame(codecCtx, frame);

            if (packet_rec < 0 || frame_rec < 0) {
                // Error
                av_packet_unref(packet);
                continue;
            }
            // Convert the image from its native format to RGB
            // You must create new buffer for RGB data

            sws_scale(sws_ctx, (uint8_t const *const *)frame->data, frame->linesize, 0,
                      frame->height, pRGBFrame->data, pRGBFrame->linesize);
            break;
        }
        av_packet_unref(packet);
    }

    av_frame_free(&frame);
    av_frame_free(&pRGBFrame);
    av_packet_unref(packet);
    av_packet_free(&packet);
    avcodec_free_context(&codecCtx);
    sws_freeContext(sws_ctx);
    avformat_close_input(&pFormatCtx);
    return 0;
}
