#include <string.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <unistd.h>

#include "control.h"
#include "sndcard.h"

static double 			pts;
static char 			time_total[12];
static char				time_arr[24];
static unsigned int 	bytes_per_sec;
static int				flg = 0;
static uint8_t 			*audio_buf;
unsigned int			buf_size;
static int				audio_idx;	
static AVFormatContext	*ic;
static AVCodecContext	*avctx;
static AVCodec			*codec;
static AVFrame			*frame;
static int				hours, mins, secs;
static struct SwrContext	*swr_ctx;

static void display_metadata(void)
{
	AVDictionaryEntry	*tag = NULL;

	putchar('\n');
	putchar('\n');
	while ((tag = av_dict_get(ic->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
		fprintf(stderr, "  %s = %s\n", tag->key, tag->value);
	}

	return ;
}

static int stream_open(const char *filename)
{
	int	ret = 0;

	AVDictionary	*opts = NULL;
	AVDictionaryEntry	*t = NULL;

	audio_idx	= -1;
	ic 			= NULL;
	avctx	 	= NULL;
	codec 		= NULL;
	swr_ctx 	= NULL;
	frame 		= NULL;

	ret = avformat_open_input(&ic, filename, NULL, NULL);
	if (ret < 0) {
		fprintf(stderr, "open '%s' failed: %s\n", filename, strerror(ret));
		goto fail;
	}

	ret = avformat_find_stream_info(ic, NULL);
	if (ret < 0) {
		fprintf(stderr, "'%s': could not find codec parameters\n", filename);
		goto fail;
	}

	if (ic->pb)
		ic->pb->eof_reached = 0;

	audio_idx = av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
	if (audio_idx < 0) {
		fprintf(stderr, "could NOT find audio stream\n");
		ret = -EINVAL;
		goto fail;
	}

	display_metadata();

	if (ic->duration != AV_NOPTS_VALUE) {
		int64_t	duration = ic->duration + 5000;
		secs = duration / AV_TIME_BASE;
		mins = secs / 60;
		secs %= 60;
		hours = mins / 60;
		mins %= 60;

		memset(time_total, 0, sizeof(time_total));
		snprintf(time_total, 9, "%02d:%02d:%02d", hours, mins, secs);
	}
	avctx = ic->streams[audio_idx]->codec;

	codec = avcodec_find_decoder(avctx->codec_id);
	if (!codec) {
		fprintf(stderr, "codec NOT found\n");
		ret = -1;
		goto fail;
	}

	if (avctx->lowres > codec->max_lowres) {
		av_log(avctx, AV_LOG_WARNING, "The maximum value for lowres supported "
								"by the decoder is %d\n", codec->max_lowres);
		avctx->lowres = codec->max_lowres;
	}

	av_dict_set(&opts, "threads", "auto", 0);
	if (avcodec_open2(avctx, codec, &opts) < 0) {
		fprintf(stderr, "could NOT open codec\n");
		ret = -1;
		goto fail;
	}
	if ((t = av_dict_get(opts, "", NULL, AV_DICT_IGNORE_SUFFIX))) {
		av_log(NULL, AV_LOG_ERROR, "Option %s not found.\n", t->key);
		ret = AVERROR_OPTION_NOT_FOUND;
		goto fail;
	}

	return ret;
fail:
	if (ic)
		avformat_close_input(&ic);
	return ret;
}

static void audio_decode_frame(AVPacket *pkt)
{
	int	got_frame = 0;
	int	len = 0;

	while (pkt->size > 0) {
		len = avcodec_decode_audio4(avctx, frame, &got_frame, pkt);
		if (len < 0)
			break;

		pkt->data += len;
		pkt->size -= len;

		if (!got_frame) {
			if (!pkt->data && avctx->codec->capabilities & CODEC_CAP_DELAY)
				break;
			continue;
		}

		bytes_per_sec = frame->sample_rate * frame->channels *
							av_get_bytes_per_sample(frame->format);

		if (!av_sample_fmt_is_planar(frame->format)) {
			int size = frame->linesize[0];

			pts += (double)size / bytes_per_sec;
			snprintf(time_arr, sizeof(time_arr) - 1, "%.2lf", pts);
			int secs = atoi(time_arr);
			int hours = secs / 3600;
			int mins = secs / 60;
			secs %= 60;

			memset(time_arr, 0, sizeof(time_arr));
			snprintf(time_arr, 18, "%02d:%02d:%02d/%s", hours, mins, secs, time_total);
			size = size / av_get_bytes_per_sample(frame->format) / frame->channels;
			write_sndcard(frame->data[0], size);
			fprintf(stderr, "    %s\r", time_arr);
			continue;
		}

		if (!swr_ctx) {
			swr_ctx = swr_alloc_set_opts(NULL, audio_params_t.channel_layout,
							audio_params_t.av_format, audio_params_t.sample_rate,
							frame->channel_layout, frame->format,
							frame->sample_rate, 0, NULL);
			if (!swr_ctx || swr_init(swr_ctx) < 0)
				break;
		}
		if (swr_ctx) {
			const uint8_t	**in = (const uint8_t **)frame->extended_data;
			uint8_t **out = &audio_buf;
			int out_cnt = frame->nb_samples * audio_params_t.sample_rate /
						   frame->sample_rate + 256;
			int out_size = av_samples_get_buffer_size(NULL, 2, out_cnt,
							frame->format, 0);
			av_fast_malloc(&audio_buf, &buf_size, out_size);
			len = swr_convert(swr_ctx, out, out_cnt, in, frame->nb_samples);
			if (len < 0)
				break;

			double tmp = ((double) len)  * av_get_bytes_per_sample(frame->format) * frame->channels;
			pts += tmp / bytes_per_sec;
			sprintf(time_arr, "%.2lf", pts);
			int secs = atoi(time_arr);
			int	hour = secs / 3600;
			int	min = secs / 60;
			int	sec = secs % 60;
			memset(time_arr, 0, sizeof(time_arr));
			snprintf(time_arr, sizeof(time_arr) - 1, "%02d:%02d:%02d/%s", hour, min, sec, time_total);
			write_sndcard(audio_buf, len);
			fprintf(stderr, "    %s\r", time_arr);
		}
	}
	return ;
}

void init_libav(void)
{
	av_log_set_flags(AV_LOG_SKIP_REPEATED);

	av_register_all();

	avcodec_register_all();

	return ;
}

void play(const char *filename)
{
	int	ret;

	AVPacket	pkt1, *pkt = &pkt1;
	AVPacket	tmp_pkt;

	if (control.end_flg)
		return ;

	control.begin_flg = 0;
	memset(pkt, 0, sizeof(AVPacket));

	ret = stream_open(filename);
	if (ret != 0)
		return ;

	ret = init_sndcard(avctx->sample_rate, avctx->channels,
				 avctx->channel_layout, avctx->sample_fmt);
	if (ret != 0)
		return ;

	frame = avcodec_alloc_frame();
	if (!frame)
		return ;

	flg = 0;
	control.begin_flg = 1;
	pts = 0;
	memset(time_arr, 0, sizeof(time_arr));


	while (1) {
		if (control.end_flg) {
			fflush(stdout);
			break;
		}

		if (control.pre_next_flg) {
			control.pause_flg = 0;
			break;
		}

		if (control.pause_flg) {
			if (!flg)
				pause_sndcard(1);
			flg = 1;
			usleep(20000);
			continue;
		}
		if (flg) {
			flg = 0;
			pause_sndcard(0);
		}

		ret = av_read_frame(ic, pkt);
		if (ret < 0) {
			if (ret == AVERROR_EOF || url_feof(ic->pb))
				break;
			if (ic->pb && ic->pb->error)
				break;
			usleep(10);
			break;
		}
		if (pkt->stream_index != audio_idx) {
			av_free_packet(pkt);
			memset(pkt, 0, sizeof(AVPacket));
			continue;
		}

		if (av_dup_packet(pkt) < 0)
			continue;

		tmp_pkt = *pkt;
		audio_decode_frame(&tmp_pkt);
		av_free_packet(pkt);
	}

	avformat_close_input(&ic);
	avcodec_free_frame(&frame);

	close_sndcard();

	if (swr_ctx) {
		swr_free(&swr_ctx);
		swr_ctx = NULL;
	}

	usleep(100);
	return ;
}
