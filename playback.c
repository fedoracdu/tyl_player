#include <string.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <unistd.h>
#include <ncurses.h>

#include "control.h"
#include "sndcard.h"

static double pts;
static char	time_arr[12];
static unsigned int bytes_per_sec;
static int	flg = 0;
static uint8_t *audio_buf;
unsigned int	buf_size;
static int	audio_idx;	
static AVFormatContext	*ic;
static AVCodecContext	*avctx;
static AVCodec			*codec;
static struct SwrContext *swr_ctx;
static AVFrame			*frame;

static int row, col;

static int stream_open(const char *filename)
{
	int	ret = 0;

	AVDictionary	*opts = NULL;
	AVDictionaryEntry	*t = NULL;

	audio_idx = -1;
	ic = NULL;
	avctx = NULL;
	codec = NULL;
	swr_ctx = NULL;
	frame = NULL;

	ret = avformat_open_input(&ic, filename, NULL, NULL);
	if (ret < 0) {
		printw("open '%s' failed: %s\n", filename, strerror(ret));
		refresh();
		goto fail;
	}

	ret = avformat_find_stream_info(ic, NULL);
	if (ret < 0) {
		printw("'%s': could not find codec parameters\n", filename);
		refresh();
		goto fail;
	}

	if (ic->pb)
		ic->pb->eof_reached = 0;

	audio_idx = av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
	if (audio_idx < 0) {
		printw("could NOT find audio stream\n");
		refresh();
		ret = -EINVAL;
		goto fail;
	}

	avctx = ic->streams[audio_idx]->codec;

	codec = avcodec_find_decoder(avctx->codec_id);
	if (!codec) {
		printw("codec NOT found\n");
		refresh();
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
		printw("could NOT open codec\n");
		refresh();
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
			bytes_per_sec = frame->sample_rate * frame->channels *
							av_get_bytes_per_sample(frame->format);

			double tmp = ((double) len)  * av_get_bytes_per_sample(frame->format) * frame->channels;
			pts += tmp / bytes_per_sec;
			sprintf(time_arr, "%.2lf", pts);
			int secs = atoi(time_arr);
			int	hour = secs / 3600;
			int	min = secs / 60;
			int	sec = secs % 60;
			memset(time_arr, 0, sizeof(time_arr));
			sprintf(time_arr, "%02d:%02d:%02d", hour, min, sec);
			mvprintw(row / 2 - 1, (col - strlen(time_arr)) / 2, "%s\r", time_arr);
			mvprintw(row / 2, (col - strlen("playing\n")) / 2, "%s", "playing\n"); 
			refresh();
			write_sndcard(audio_buf, len);
		}
	}
	return ;
}

/*
static void display_metadata(void)
{
	int	i = row / 2 + 2;
	AVDictionaryEntry	*tag = NULL;

	while ((tag = av_dict_get(ic->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
		mvprintw(i, (col - strlen(tag->value)) / 2 - 5, "%s = %s\n", tag->key, tag->value);
		i++;
	}
	refresh();

	return ;
}
*/
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

	getmaxyx(stdscr, row, col);

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

	clear();

	while (1) {
		if (control.end_flg) {
			fflush(stdout);
			break;
		}

		if (control.pre_next_flg)
			break;

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

	clear();
	usleep(100);
	return ;
}
