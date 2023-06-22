#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <stdio.h>
#include <ao/ao.h>
int audio_resampling(  // 1
    AVCodecContext *audio_decode_ctx, AVFrame *decoded_audio_frame,
    enum AVSampleFormat out_sample_fmt, int out_channels, int out_sample_rate, uint8_t *out_buf) {
    SwrContext     *swr_ctx             = NULL;
    int             ret                 = 0;
    AVChannelLayout in_channel_layout   = audio_decode_ctx->ch_layout;
    AVChannelLayout out_channel_layout  = AV_CHANNEL_LAYOUT_STEREO;
    int             out_nb_channels     = 0;
    int             out_linesize        = 0;
    int             in_nb_samples       = 0;
    int             out_nb_samples      = 0;
    int             max_out_nb_samples  = 0;
    uint8_t       **resampled_data      = NULL;
    int             resampled_data_size = 0;

    swr_ctx = swr_alloc();

    if (!swr_ctx) {
        printf("swr_alloc error.\n");
        return -1;
    }

    // set output audio channels based on the input audio channels
    if (out_channels == 1) {
        AVChannelLayout tmp = AV_CHANNEL_LAYOUT_MONO;
        out_channel_layout  = tmp;
    } else if (out_channels == 2) {
        AVChannelLayout tmp = AV_CHANNEL_LAYOUT_STEREO;
        out_channel_layout  = tmp;
    } else {
        AVChannelLayout tmp = AV_CHANNEL_LAYOUT_SURROUND;
        out_channel_layout  = tmp;
    }

    // retrieve number of audio samples (per channel)
    in_nb_samples = decoded_audio_frame->nb_samples;
    if (in_nb_samples <= 0) {
        printf("in_nb_samples error.\n");
        return -1;
    }

    swr_alloc_set_opts2(&swr_ctx,  // 2
                        &out_channel_layout, out_sample_fmt, out_sample_rate, &in_channel_layout,
                        audio_decode_ctx->sample_fmt, audio_decode_ctx->sample_rate, 0, NULL);

    // Once all values have been set for the SwrContext, it must be initialized
    // with swr_init().
    ret = swr_init(swr_ctx);
    ;
    if (ret < 0) {
        printf("Failed to initialize the resampling context.\n");
        return -1;
    }

    max_out_nb_samples = out_nb_samples =
        av_rescale_rnd(in_nb_samples, out_sample_rate, audio_decode_ctx->sample_rate, AV_ROUND_UP);

    // check rescaling was successful
    if (max_out_nb_samples <= 0) {
        printf("av_rescale_rnd error.\n");
        return -1;
    }

    // get number of output audio channels
    out_nb_channels = out_channel_layout.nb_channels;

    ret = av_samples_alloc_array_and_samples(&resampled_data, &out_linesize, out_nb_channels,
                                             out_nb_samples, out_sample_fmt, 0);

    if (ret < 0) {
        printf(
            "av_samples_alloc_array_and_samples() error: Could not allocate destination "
            "samples.\n");
        return -1;
    }

    // retrieve output samples number taking into account the progressive delay
    out_nb_samples =
        av_rescale_rnd(swr_get_delay(swr_ctx, audio_decode_ctx->sample_rate) + in_nb_samples,
                       out_sample_rate, audio_decode_ctx->sample_rate, AV_ROUND_UP);

    // check output samples number was correctly retrieved
    if (out_nb_samples <= 0) {
        printf("av_rescale_rnd error\n");
        return -1;
    }

    if (out_nb_samples > max_out_nb_samples) {
        // free memory block and set pointer to NULL
        av_free(resampled_data[0]);

        // Allocate a samples buffer for out_nb_samples samples
        ret = av_samples_alloc(resampled_data, &out_linesize, out_nb_channels, out_nb_samples,
                               out_sample_fmt, 1);

        // check samples buffer correctly allocated
        if (ret < 0) {
            printf("av_samples_alloc failed.\n");
            return -1;
        }

        max_out_nb_samples = out_nb_samples;
    }

    if (swr_ctx) {
        // do the actual audio data resampling
        ret = swr_convert(swr_ctx, resampled_data, out_nb_samples,
                          (const uint8_t **)decoded_audio_frame->data,
                          decoded_audio_frame->nb_samples);

        // check audio conversion was successful
        if (ret < 0) {
            printf("swr_convert_error.\n");
            return -1;
        }

        // Get the required buffer size for the given audio parameters
        resampled_data_size =
            av_samples_get_buffer_size(&out_linesize, out_nb_channels, ret, out_sample_fmt, 1);

        // check audio buffer size
        if (resampled_data_size < 0) {
            printf("av_samples_get_buffer_size error.\n");
            return -1;
        }
    } else {
        printf("swr_ctx null error.\n");
        return -1;
    }

    // copy the resampled data to the output buffer
    memcpy(out_buf, resampled_data[0], resampled_data_size);

    /*
     * Memory Cleanup.
     */
    if (resampled_data) {
        // free memory block and set pointer to NULL
        av_freep(&resampled_data[0]);
    }

    av_freep(&resampled_data);
    resampled_data = NULL;

    if (swr_ctx) {
        // Free the given SwrContext and set the pointer to NULL
        swr_free(&swr_ctx);
    }

    return resampled_data_size;
}
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
    sformat.channels    = 2;
    sformat.rate        = codecCtx->sample_rate;
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
                int ret = audio_resampling(codecCtx, frame, AV_SAMPLE_FMT_S32, 2, 44100, buf);
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
