#ifndef __R_AUDIO__
#define __R_AUDIO__
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
int audio_resampling(AVCodecContext *audio_decode_ctx, AVFrame *decoded_audio_frame,
                     enum AVSampleFormat out_sample_fmt, int out_channels, int out_sample_rate,
                     uint8_t *out_buf);
#endif
