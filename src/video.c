#include "video.h"
#include "render.h"   /* WINDOW_W, WINDOW_H */

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>

#include <stdlib.h>
#include <string.h>

/* ── Internal struct ─────────────────────────────────────────────────────── */

struct VideoPlayer {
    AVFormatContext  *fmt_ctx;
    AVCodecContext   *vid_ctx;
    AVCodecContext   *aud_ctx;
    int               vid_stream;
    int               aud_stream;

    struct SwsContext *sws_ctx;
    SwrContext        *swr_ctx;

    SDL_Texture      *frame_tex;     /* current decoded video frame          */
    SDL_AudioStream  *audio_out;     /* SDL3 audio stream for playback       */

    AVFrame          *av_frame;      /* reused across decode calls            */
    AVPacket         *av_packet;     /* reused across read calls              */

    uint8_t          *rgba_buf;      /* RGBA pixel buffer for swscale output  */
    int               rgba_stride;   /* bytes per row                         */

    int               width;
    int               height;

    double            vid_tb;        /* video stream time_base as double      */

    double            elapsed;       /* seconds since playback started        */
    int               eof;           /* 1 once all packets have been read     */
    int               done;          /* 1 once playback is fully finished     */
};

/* ── Internal helpers ───────────────────────────────────────────────────── */

/* Convert the current av_frame to RGBA and upload to frame_tex. */
static void upload_video_frame(VideoPlayer *vp)
{
    if (!vp->sws_ctx || !vp->frame_tex) return;

    uint8_t *dst_data[4]  = { vp->rgba_buf, NULL, NULL, NULL };
    int      dst_ls[4]    = { vp->rgba_stride, 0, 0, 0 };

    sws_scale(vp->sws_ctx,
              (const uint8_t * const *)vp->av_frame->data,
              vp->av_frame->linesize,
              0, vp->height,
              dst_data, dst_ls);

    SDL_UpdateTexture(vp->frame_tex, NULL, vp->rgba_buf, vp->rgba_stride);
}

/* Decode an audio packet and push the converted float32 stereo samples to
 * the SDL audio stream. */
static void decode_push_audio(VideoPlayer *vp)
{
    if (!vp->aud_ctx || !vp->swr_ctx || !vp->audio_out) return;

    if (avcodec_send_packet(vp->aud_ctx, vp->av_packet) < 0) return;

    while (avcodec_receive_frame(vp->aud_ctx, vp->av_frame) >= 0) {
        /* max output samples for this block */
        int out_samples = (int)av_rescale_rnd(
            swr_get_delay(vp->swr_ctx, vp->aud_ctx->sample_rate)
                + vp->av_frame->nb_samples,
            44100, vp->aud_ctx->sample_rate, AV_ROUND_UP);

        int buf_size = out_samples * 2 /* channels */ * sizeof(float);
        uint8_t *out_buf = (uint8_t *)malloc((size_t)buf_size);
        if (!out_buf) continue;

        uint8_t *out_planes[1] = { out_buf };
        int converted = swr_convert(vp->swr_ctx,
                                    out_planes, out_samples,
                                    (const uint8_t **)vp->av_frame->data,
                                    vp->av_frame->nb_samples);
        if (converted > 0) {
            int bytes = converted * 2 * (int)sizeof(float);
            SDL_PutAudioStreamData(vp->audio_out, out_buf, bytes);
        }
        free(out_buf);
        av_frame_unref(vp->av_frame);
    }
}

/* ── Public API ──────────────────────────────────────────────────────────── */

VideoPlayer *video_player_open(SDL_Renderer *renderer, const char *path)
{
    VideoPlayer *vp = (VideoPlayer *)calloc(1, sizeof(VideoPlayer));
    if (!vp) return NULL;

    vp->vid_stream = -1;
    vp->aud_stream = -1;

    /* Open container */
    if (avformat_open_input(&vp->fmt_ctx, path, NULL, NULL) < 0) {
        SDL_Log("video_player_open: cannot open '%s'", path);
        free(vp);
        return NULL;
    }
    if (avformat_find_stream_info(vp->fmt_ctx, NULL) < 0) {
        SDL_Log("video_player_open: cannot read stream info for '%s'", path);
        goto fail;
    }

    /* Find best video and audio streams */
    vp->vid_stream = av_find_best_stream(vp->fmt_ctx, AVMEDIA_TYPE_VIDEO,
                                         -1, -1, NULL, 0);
    vp->aud_stream = av_find_best_stream(vp->fmt_ctx, AVMEDIA_TYPE_AUDIO,
                                         -1, -1, NULL, 0);

    if (vp->vid_stream < 0) {
        SDL_Log("video_player_open: no video stream in '%s'", path);
        goto fail;
    }

    /* Open video decoder */
    {
        AVStream         *st  = vp->fmt_ctx->streams[vp->vid_stream];
        const AVCodec    *dec = avcodec_find_decoder(st->codecpar->codec_id);
        if (!dec) {
            SDL_Log("video_player_open: no decoder for video codec %d",
                    st->codecpar->codec_id);
            goto fail;
        }
        vp->vid_ctx = avcodec_alloc_context3(dec);
        if (!vp->vid_ctx) goto fail;
        avcodec_parameters_to_context(vp->vid_ctx, st->codecpar);
        if (avcodec_open2(vp->vid_ctx, dec, NULL) < 0) {
            SDL_Log("video_player_open: avcodec_open2 failed for video");
            goto fail;
        }
        vp->width   = vp->vid_ctx->width;
        vp->height  = vp->vid_ctx->height;
        vp->vid_tb  = (double)st->time_base.num / (double)st->time_base.den;
    }

    /* Open audio decoder (optional) */
    if (vp->aud_stream >= 0) {
        AVStream         *st  = vp->fmt_ctx->streams[vp->aud_stream];
        const AVCodec    *dec = avcodec_find_decoder(st->codecpar->codec_id);
        if (dec) {
            vp->aud_ctx = avcodec_alloc_context3(dec);
            if (vp->aud_ctx) {
                avcodec_parameters_to_context(vp->aud_ctx, st->codecpar);
                if (avcodec_open2(vp->aud_ctx, dec, NULL) < 0) {
                    avcodec_free_context(&vp->aud_ctx);
                    vp->aud_ctx = NULL;
                }
            }
        }
    }

    /* swscale: convert any pixel format → RGBA */
    vp->sws_ctx = sws_getContext(
        vp->width, vp->height, vp->vid_ctx->pix_fmt,
        vp->width, vp->height, AV_PIX_FMT_RGBA,
        SWS_BILINEAR, NULL, NULL, NULL);
    if (!vp->sws_ctx) {
        SDL_Log("video_player_open: sws_getContext failed");
        goto fail;
    }

    /* RGBA pixel buffer */
    vp->rgba_stride = vp->width * 4;
    vp->rgba_buf    = (uint8_t *)malloc((size_t)(vp->rgba_stride * vp->height));
    if (!vp->rgba_buf) goto fail;

    /* SDL texture for the current video frame */
    vp->frame_tex = SDL_CreateTexture(renderer,
                                      SDL_PIXELFORMAT_RGBA8888,
                                      SDL_TEXTUREACCESS_STREAMING,
                                      vp->width, vp->height);
    if (!vp->frame_tex) {
        SDL_Log("video_player_open: SDL_CreateTexture failed: %s",
                SDL_GetError());
        goto fail;
    }

    /* swresample + SDL audio stream (only when audio decoder opened) */
    if (vp->aud_ctx) {
        AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
        if (swr_alloc_set_opts2(&vp->swr_ctx,
                                &stereo, AV_SAMPLE_FMT_FLT, 44100,
                                &vp->aud_ctx->ch_layout,
                                vp->aud_ctx->sample_fmt,
                                vp->aud_ctx->sample_rate,
                                0, NULL) < 0 || swr_init(vp->swr_ctx) < 0) {
            swr_free(&vp->swr_ctx);
            vp->swr_ctx = NULL;
        }

        if (vp->swr_ctx) {
            SDL_AudioSpec spec = { SDL_AUDIO_F32, 2, 44100 };
            vp->audio_out = SDL_OpenAudioDeviceStream(
                SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
            if (vp->audio_out)
                SDL_ResumeAudioStreamDevice(vp->audio_out);
        }
    }

    /* Reusable frame / packet */
    vp->av_frame  = av_frame_alloc();
    vp->av_packet = av_packet_alloc();
    if (!vp->av_frame || !vp->av_packet) goto fail;

    return vp;

fail:
    video_player_close(vp);
    return NULL;
}

void video_player_update(VideoPlayer *vp, float dt)
{
    if (!vp || vp->done) return;

    vp->elapsed += (double)dt;

    /* Read and process packets until we've displayed a video frame whose PTS
     * is at or past the current elapsed time (or until EOF). */
    while (!vp->eof) {
        int ret = av_read_frame(vp->fmt_ctx, vp->av_packet);
        if (ret < 0) {
            /* EOF – flush video decoder */
            vp->eof = 1;
            avcodec_send_packet(vp->vid_ctx, NULL);
            while (avcodec_receive_frame(vp->vid_ctx, vp->av_frame) >= 0) {
                upload_video_frame(vp);
                av_frame_unref(vp->av_frame);
            }
            vp->done = 1;
            break;
        }

        if (vp->av_packet->stream_index == vp->vid_stream) {
            if (avcodec_send_packet(vp->vid_ctx, vp->av_packet) >= 0) {
                int got = 0;
                while (avcodec_receive_frame(vp->vid_ctx, vp->av_frame) >= 0) {
                    double pts = (double)vp->av_frame->best_effort_timestamp
                                 * vp->vid_tb;
                    upload_video_frame(vp);
                    av_frame_unref(vp->av_frame);
                    got = 1;
                    if (pts >= vp->elapsed) {
                        /* Frame is at or ahead of current time; stop here. */
                        av_packet_unref(vp->av_packet);
                        return;
                    }
                }
                if (got) {
                    /* Displayed at least one frame this tick. */
                    av_packet_unref(vp->av_packet);
                    return;
                }
            }
        } else if (vp->av_packet->stream_index == vp->aud_stream) {
            decode_push_audio(vp);
        }

        av_packet_unref(vp->av_packet);
    }
}

void video_player_render(VideoPlayer *vp, SDL_Renderer *renderer)
{
    if (!vp || !vp->frame_tex) return;
    SDL_FRect dst = { 0.0f, 0.0f, (float)WINDOW_W, (float)WINDOW_H };
    SDL_RenderTexture(renderer, vp->frame_tex, NULL, &dst);
}

int video_player_is_done(const VideoPlayer *vp)
{
    return vp ? vp->done : 1;
}

void video_player_close(VideoPlayer *vp)
{
    if (!vp) return;
    if (vp->audio_out)  SDL_DestroyAudioStream(vp->audio_out);
    if (vp->frame_tex)  SDL_DestroyTexture(vp->frame_tex);
    if (vp->rgba_buf)   free(vp->rgba_buf);
    if (vp->sws_ctx)    sws_freeContext(vp->sws_ctx);
    if (vp->swr_ctx)    swr_free(&vp->swr_ctx);
    if (vp->av_frame)   av_frame_free(&vp->av_frame);
    if (vp->av_packet)  av_packet_free(&vp->av_packet);
    if (vp->aud_ctx)    avcodec_free_context(&vp->aud_ctx);
    if (vp->vid_ctx)    avcodec_free_context(&vp->vid_ctx);
    if (vp->fmt_ctx)    avformat_close_input(&vp->fmt_ctx);
    free(vp);
}
