#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <stdio.h>
#include <ao/ao.h>
#include "../audio.h"

int main(void) {
    // AO
    ao_initialize();
    int              driver = ao_default_driver_id();
    ao_sample_format sformat;
    //
    AVFormatContext *pFormatCtx = avformat_alloc_context();
    avformat_open_input(&pFormatCtx, "video.mp4", NULL, NULL);
    avformat_find_stream_info(pFormatCtx, NULL);
    AVStream          *stream = NULL;
    AVCodecParameters *par    = NULL;
    for (int i = 0; i < pFormatCtx->nb_streams; i++) {
        stream = pFormatCtx->streams[i];
        par    = stream->codecpar;
        if (par->codec_type == AVMEDIA_TYPE_VIDEO) {
            continue;
        }
        if (par->codec_type != AVMEDIA_TYPE_VIDEO) {
            break;
        }
    }
    const AVCodec *codec = avcodec_find_decoder(par->codec_id);

    AVCodecContext *codecCtx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codecCtx, par);
    avcodec_open2(codecCtx, codec, NULL);

    sformat.byte_format = AO_FMT_NATIVE;
    sformat.channels    = 1;
    sformat.rate        = 44100;
    sformat.bits        = 32;
    ao_device *adevice  = ao_open_live(driver, &sformat, NULL);

    AVFrame  *frame  = av_frame_alloc();
    AVPacket *packet = av_packet_alloc();
    // char     *tmp    = malloc(sizeof(float) * 2 * 4096);
    static uint8_t buf[1 << 16];

    while (av_read_frame(pFormatCtx, packet) >= 0) {
        if (packet->stream_index == stream->index) {
            // Getting frame from video
            int packet_rec = avcodec_send_packet(codecCtx, packet);
            av_packet_unref(packet);
            if (packet_rec < 0) {
                // Error
                av_packet_unref(packet);
                continue;
            }
            while (avcodec_receive_frame(codecCtx, frame) >= 0) {
                printf("tt: %d, %d\n", frame->extended_data[0][0], frame->extended_data[0][1]);
                int ret = audio_resampling(codecCtx, frame, AV_SAMPLE_FMT_S32, 1, 44100, buf);
                printf("re: %d, %d\n", buf[0], buf[1]);
                ao_play(adevice, (char *)buf, ret);
            }
        }
        av_packet_unref(packet);
    }
    av_frame_free(&frame);
    av_packet_unref(packet);
    av_packet_free(&packet);
    avcodec_free_context(&codecCtx);
    avformat_close_input(&pFormatCtx);
    ao_shutdown();
    return 0;
}
