#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <stdio.h>

void save_frame(unsigned char *buf, int wrap, int xsize, int ysize, char *filename) {
    FILE *f = NULL;
    int   i = 0;
    f       = fopen(filename, "wb");

    printf("Writing %dx%d image, wrap %d\n", xsize, ysize, wrap);
    fprintf(f, "P6\n%d %d\n%d\n", xsize, ysize, 255);

    for (i = 0; i < ysize; i++) {
        fwrite(buf + i * wrap, 1, xsize * 3, f);
    }
    fclose(f);
}

int main(void) {
    AVFormatContext   *pFormatCtx      = avformat_alloc_context();
    struct SwsContext *img_convert_ctx = sws_alloc_context();
    struct SwsContext *sws_ctx         = NULL;
    avformat_open_input(&pFormatCtx, "video.mp4", NULL, NULL);
    printf("Format %s\n", pFormatCtx->iformat->long_name);
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
            printf("Video Codec: resolution %d x %d, type: %d\n", par->width, par->height,
                   par->codec_id);
            break;
        }
    }
    const AVCodec *codec = avcodec_find_decoder(par->codec_id);
    printf("Codec %s ID %d bit_rate %ld\n", codec->name, codec->id, par->bit_rate);

    AVCodecContext *codecCtx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codecCtx, par);
    avcodec_open2(codecCtx, codec, NULL);

    AVFrame  *frame  = av_frame_alloc();
    AVPacket *packet = av_packet_alloc();
    sws_ctx = sws_getContext(codecCtx->width, codecCtx->height, codecCtx->pix_fmt, codecCtx->width,
                             codecCtx->height, AV_PIX_FMT_RGB24, SWS_FAST_BILINEAR, 0, 0, 0);
    while (av_read_frame(pFormatCtx, packet) >= 0) {
        if (packet->stream_index == stream->index)
            break;
        av_packet_unref(packet);
    }

    //
    // Start decoding
    //

    avcodec_send_packet(codecCtx, packet);
    avcodec_receive_frame(codecCtx, frame);
    // Convert the image from its native format to RGB
    // You must create new buffer for RGB data
    pRGBFrame = av_frame_alloc();

    pRGBFrame->format = AV_PIX_FMT_RGB24;
    pRGBFrame->width  = frame->width;
    pRGBFrame->height = frame->height;
    av_frame_get_buffer(pRGBFrame, 0);

    sws_scale(sws_ctx, (uint8_t const *const *)frame->data, frame->linesize, 0, frame->height,
              pRGBFrame->data, pRGBFrame->linesize);
    save_frame(pRGBFrame->data[0], pRGBFrame->linesize[0], pRGBFrame->width, pRGBFrame->height,
               "aa.ppm");
    av_frame_free(&pRGBFrame);

    //
    // End decoding
    //

    // Clean up
    av_frame_free(&frame);
    av_packet_unref(packet);
    av_packet_free(&packet);
    avcodec_free_context(&codecCtx);
    sws_freeContext(sws_ctx);
    avformat_close_input(&pFormatCtx);
    return 0;
}
