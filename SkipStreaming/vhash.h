#pragma once
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavformat/avio.h"
#include "libavutil/pixfmt.h"
#include "libswscale/swscale.h"
}

#define MAX_DCT_SIZE 64
#define PI 3.1415926535

#define SCALE_HEIGHT 16
#define SCALE_WIDTH 16
#define DCT_LOW_FREQUENCY_HEIGHT 16
#define DCT_LOW_FREQUENCY_WIDTH 16
#define HAMMING_THRESHOLD 0.16

double dct_in_mat[MAX_DCT_SIZE][MAX_DCT_SIZE];
double dct_tmp_mat[MAX_DCT_SIZE][MAX_DCT_SIZE];
double dct_trans_mat[MAX_DCT_SIZE][MAX_DCT_SIZE];

struct vhash_item
{
	unsigned char sz;
	unsigned char h[256];
	__int64 pkt_pos;
	unsigned int frame_index;

	vhash_item()
	{
		sz = 0;
		memset(h, 0, 31);
		pkt_pos = 0;
		frame_index = -1;
	}

	bool operator < (const vhash_item& rhs) const
	{
		if (sz != rhs.sz)
			return sz < rhs.sz;

		for (int i = 0; i < sz; i++)
			if (h[i] != rhs.h[i])
				return h[i] < rhs.h[i];

		return false;
	}

	bool operator > (const vhash_item& rhs) const
	{
		if (sz != rhs.sz)
			return sz > rhs.sz;

		for (int i = 0; i < sz; i++)
			if (h[i] != rhs.h[i])
				return h[i] > rhs.h[i];

		return false;
	}
};

int get_hamming_distance(const vhash_item& h1, const vhash_item& h2)
{
	int dis = 0;
	for (int i = 0; i < h1.sz; i++)
	{
		unsigned char hd = h1.h[i] ^ h2.h[i];
		while (hd)
		{
			if (hd & 1)
				dis++;
			hd >>= 1;
		}
	}
	return dis;
}

void scale_video_frame_yuv(AVCodecContext* pCodecCtx, AVFrame* in_frame, AVFrame* out_frame, int out_h, int out_w)
{
	int src_h = pCodecCtx->height;
	int src_w = pCodecCtx->width;
	struct SwsContext* pSwsContext;

	/*out_frame->linesize[0] = out_w;
	out_frame->linesize[1] = out_w / 2;
	out_frame->linesize[2] = out_w / 2;*/

	pSwsContext = sws_getContext(
		src_w,
		src_h,
		AV_PIX_FMT_YUV420P,
		out_w,
		out_h,
		AV_PIX_FMT_YUV420P,
		SWS_FAST_BILINEAR,
		NULL, NULL, NULL
	);

	sws_scale(pSwsContext, in_frame->data, in_frame->linesize, 0, pCodecCtx->height, out_frame->data, out_frame->linesize);

	sws_freeContext(pSwsContext);
}

vhash_item video_average_hash_yuv(AVCodecContext* pCodecContext, AVFrame* in_frame)
{
	int scale_height = 6, scale_width = 6;
	AVFrame* frame = av_frame_alloc();
	avpicture_alloc((AVPicture*)frame, AV_PIX_FMT_YUV420P, scale_width, scale_height);
	scale_video_frame_yuv(pCodecContext, in_frame, frame, scale_height, scale_width);
	frame->height = scale_height;
	frame->width = scale_width;

	vhash_item item;
	item.sz = 0;
	//	item.timestamp = frame->best_effort_timestamp;

	int avg_Y = 0;
	int linesz = frame->linesize[0];

	for (int i = 0; i < linesz * frame->height; i++)
		avg_Y += frame->data[0][i];

	avg_Y /= linesz * frame->height;

	unsigned char hs = 0;
	int cnt = 0;

	for (int i = 0; i < linesz * frame->height; i++)
	{
		if (cnt == 8)
		{
			item.h[item.sz++] = hs;
			cnt = 0;
			hs = 0;
		}

		hs <<= 1;

		if (frame->data[0][i] >= avg_Y)
			hs |= 0x01;
		else
			hs &= 0xfe;

		cnt++;
	}

	if (cnt != 0)
		item.h[item.sz++] = hs;

	avpicture_free((AVPicture*)frame);
	av_frame_free(&frame);
	return item;
}

void scale_video_frame_rgb(AVCodecContext* pCodecCtx, AVFrame* in_frame, AVFrame* out_frame, int out_h, int out_w)
{
	int src_h = pCodecCtx->height;
	int src_w = pCodecCtx->width;
	struct SwsContext* pSwsContext;

	/*out_frame->linesize[0] = out_w;
	out_frame->linesize[1] = out_w / 2;
	out_frame->linesize[2] = out_w / 2;*/

	pSwsContext = sws_getContext(
		src_w,
		src_h,
		AV_PIX_FMT_YUV420P,
		out_w,
		out_h,
		AV_PIX_FMT_RGB24,
		SWS_FAST_BILINEAR,
		NULL, NULL, NULL
	);

	sws_scale(pSwsContext, in_frame->data, in_frame->linesize, 0, pCodecCtx->height, out_frame->data, out_frame->linesize);

	sws_freeContext(pSwsContext);
}

vhash_item video_average_hash_rgb(AVCodecContext* pCodecContext, AVFrame* in_frame, int scale_height, int scale_width)
{
	AVFrame* frame = av_frame_alloc();
	avpicture_alloc((AVPicture*)frame, AV_PIX_FMT_RGB24, scale_width, scale_height);
	scale_video_frame_rgb(pCodecContext, in_frame, frame, scale_height, scale_width);
	frame->height = scale_height;
	frame->width = scale_width;

	vhash_item item;
	int sz = frame->linesize[0] * frame->height / 3;
	int* gray = new int[sz];

	int average_gray = 0;

	for (int i = 0; i < sz; i++)
	{
		gray[i] = (frame->data[0][i * 3] * 19595 + frame->data[0][i * 3 + 1] * 38469 + frame->data[0][i * 3 + 2] * 7472) >> 16;
		average_gray += gray[i];
	}

	average_gray /= sz;

	unsigned char hs = 0;
	int cnt = 0;

	for (int i = 0; i < sz; i++)
	{
		if (cnt == 8)
		{
			item.h[item.sz++] = hs;
			cnt = 0;
			hs = 0;
		}
		hs <<= 1;
		if (gray[i] >= average_gray)
			hs |= 0x01;
		else
			hs &= 0xfe;
		cnt++;
	}
	if (cnt != 0)
		item.h[item.sz++] = hs;
	delete[] gray;
	avpicture_free((AVPicture*)frame);
	av_frame_free(&frame);
	return item;
}

void init_dct_trans_mat(int sz)
{
	for (int i = 0; i < sz; i++)
	{
		for (int j = 0; j < sz; j++)
		{
			double a = 0;
			if (i == 0)
				a = sqrt((float)1 / sz);
			else
				a = sqrt((float)2 / sz);
			dct_trans_mat[i][j] = a * cos((j + 0.5) * PI * i / sz);
		}
	}
}

void dct(int sz)
{
	double t = 0;
	int i, j, k;
	for (i = 0; i < sz; i++)
	{
		for (j = 0; j < sz; j++)
		{
			t = 0;
			for (k = 0; k < sz; k++)
			{
				t += dct_trans_mat[i][k] * dct_in_mat[k][j];
			}
			dct_tmp_mat[i][j] = t;
		}
	}
	for (i = 0; i < sz; i++)
	{
		for (j = 0; j < sz; j++)
		{
			t = 0;
			for (k = 0; k < sz; k++)
			{
				t += dct_tmp_mat[i][k] * dct_trans_mat[j][k];
			}
			dct_in_mat[i][j] = t;
		}
	}
}

vhash_item video_dct_hash_yuv(AVCodecContext* pCodecContext, AVFrame* in_frame)
{
	int scale_height = SCALE_HEIGHT, scale_width = SCALE_WIDTH;

	static bool dct_init = false;
	if (!dct_init)
	{
		init_dct_trans_mat(scale_height);
		dct_init = true;
	}

	AVFrame* frame = av_frame_alloc();
	avpicture_alloc((AVPicture*)frame, AV_PIX_FMT_YUV420P, scale_width, scale_height);
	scale_video_frame_yuv(pCodecContext, in_frame, frame, scale_height, scale_width);
	frame->height = scale_height;
	frame->width = scale_width;

	vhash_item item;
	item.sz = 0;
	//	item.timestamp = frame->best_effort_timestamp;

	double avg_Y_dct = 0;
	int linesz = frame->linesize[0];

	for (int i = 0; i < scale_height; i++)
	{
		for (int j = 0; j < scale_width; j++)
		{
			dct_in_mat[i][j] = frame->data[0][i * scale_width + j];
		}
	}

	dct(scale_height);

	// DCT_LOW_FREQUENCY_HEIGHT = SCALE_HEIGHT
	// DCT_LOW_FREQUENCY_WIDTH = SCALE_WIDTH
	// here we extract all DCT featrue rather than low-frequency feature
	// to distinguish frames
	// since the neighbouring two frames are quite similar
	for (int i = 0; i < DCT_LOW_FREQUENCY_HEIGHT; i++)
	{
		for (int j = 0; j < DCT_LOW_FREQUENCY_WIDTH; j++)
		{
			avg_Y_dct += dct_in_mat[i][j];
		}
	}

	avg_Y_dct /= (DCT_LOW_FREQUENCY_HEIGHT * DCT_LOW_FREQUENCY_WIDTH);

	unsigned char hs = 0;
	int cnt = 0;

	for (int i = 0; i < DCT_LOW_FREQUENCY_HEIGHT; i++)
	{
		for (int j = 0; j < DCT_LOW_FREQUENCY_WIDTH; j++)
		{
			if (cnt == 8)
			{
				item.h[item.sz++] = hs;
				cnt = 0;
				hs = 0;
			}

			hs <<= 1;

			if (dct_in_mat[i][j] >= avg_Y_dct)
				hs |= 0x01;
			else
				hs &= 0xfe;

			cnt++;
		}
	}

	if (cnt != 0)
		item.h[item.sz++] = hs;

	avpicture_free((AVPicture*)frame);
	av_frame_free(&frame);
	return item;
}

vhash_item video_dct_hash_yuv_scaled(AVCodecContext* pCodecContext, AVFrame* frame)
{
	static bool dct_init = false;
	if (!dct_init)
	{
		init_dct_trans_mat(frame->height);
		dct_init = true;
	}

	vhash_item item;
	item.sz = 0;
	//	item.timestamp = frame->best_effort_timestamp;

	double avg_Y_dct = 0;
	int linesz = frame->linesize[0];

	for (int i = 0; i < frame->height; i++)
	{
		for (int j = 0; j < frame->width; j++)
		{
			dct_in_mat[i][j] = frame->data[0][i * frame->width + j];
		}
	}

	dct(frame->height);

	// DCT_LOW_FREQUENCY_HEIGHT = SCALE_HEIGHT
	// DCT_LOW_FREQUENCY_WIDTH = SCALE_WIDTH
	// here we extract all DCT featrue rather than low-frequency feature
	// to distinguish frames
	// since the neighbouring two frames are quite similar
	for (int i = 0; i < DCT_LOW_FREQUENCY_HEIGHT; i++)
	{
		for (int j = 0; j < DCT_LOW_FREQUENCY_WIDTH; j++)
		{
			avg_Y_dct += dct_in_mat[i][j];
		}
	}

	avg_Y_dct /= (DCT_LOW_FREQUENCY_HEIGHT * DCT_LOW_FREQUENCY_WIDTH);

	unsigned char hs = 0;
	int cnt = 0;

	for (int i = 0; i < DCT_LOW_FREQUENCY_HEIGHT; i++)
	{
		for (int j = 0; j < DCT_LOW_FREQUENCY_WIDTH; j++)
		{
			if (cnt == 8)
			{
				item.h[item.sz++] = hs;
				cnt = 0;
				hs = 0;
			}

			hs <<= 1;

			if (dct_in_mat[i][j] >= avg_Y_dct)
				hs |= 0x01;
			else
				hs &= 0xfe;

			cnt++;
		}
	}

	if (cnt != 0)
		item.h[item.sz++] = hs;

	//avpicture_free((AVPicture*)frame);
	//av_frame_free(&frame);
	return item;
}
