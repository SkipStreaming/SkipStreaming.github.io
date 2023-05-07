#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <utility>
#include "pscommon/getopt.h"
#include "pscommon/vhash.h"
#include "pscommon/mih.h"
#include "SkipStreaming.h"
#include "scene_hierarchy.h"

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavformat/avio.h"
#include "libavutil/pixfmt.h"
#include "libswscale/swscale.h"
#include <cassert>
}

// needs further tuning
#define SCENE_GRANULARITY 32
#define SCENE_SEARCH_TRY 8
#define CONST_STR_LEN 1024

#define VIDEO_FRAME_PROTECT_RANGE 25 * 60 * 10
#define VIDEO_FRAME_SKIP_CHECK_THRESHOLD 50
#define VIDEO_SCENE_PROTECT_SKIP 60
#define TIME_DELTA_THRESHOLD 1

#define VIDEO_BLACK_PIXEL_THRESHOLD 25
#define VIDEO_BLACK_FRAME_THRESHOLD 0.95
#define VIDEO_BLACK_EPISODE_FRAME_COUNT 40
#define VIDEO_BLACK_FRAME_CHECK_THRESHOLD 0.5

#define AUDIO_SILENT_RMS_THRESHOLD 0.001
#define AUDIO_SILENT_EPISODE_FRAME_COUNT 40

#define GOP_REDUNDANCY_MULTIPLIER 1.8

#define GOP_SAME 1
#define GOP_DIFFERENT 0
#define GOP_UNCERTAIN -1

char reference_video_path[CONST_STR_LEN];
char reference_video_index_path[CONST_STR_LEN];
char query_video_path[CONST_STR_LEN];
char query_video_audio_t_path[CONST_STR_LEN];
char out_report_path[CONST_STR_LEN];
char ground_truth_path[CONST_STR_LEN];
clock_t run_begin_time, run_end_time;

AVFormatContext* query_video_format_context = NULL;
AVFormatContext* query_audio_format_context = NULL;
AVStream* query_video_stream = NULL;
AVStream* query_audio_stream = NULL;
int query_video_index = 0;
int query_audio_index = 0;

AVFormatContext* reference_video_format_context = NULL;
AVStream* reference_video_stream = NULL;
int reference_video_index = 0;

int query_video_frame_number = 0, query_video_key_frame_number = 0, reference_video_frame_number = 0;
std::vector<video_frame_packet> query_video_frames;
std::vector<int> query_video_key_frame_index;
std::vector<video_frame_packet> reference_video_frames;
std::map<__int64, int> reference_video_pkt_pos_to_frame_index;
std::map<int, vhash_item> query_video_key_frame_hash_cache;
std::map<unsigned int, std::vector<int>> query_video_vhash_compare_cache;
std::map<int, std::vector<vhash_item>> query_video_gop_decompress;

int query_video_audio_scene_leaf_number = 0, query_video_audio_sr = 0, query_video_audio_n_fft = 0, query_video_audio_hop_length = 0;
std::vector<std::pair<int, int>> query_video_audio_scene_hierarchy;
std::vector<int> query_video_base_scene_node_id;
std::vector<scene_segment> query_video_base_scenes;

std::vector<redundant_clip> redundant_clips;

vhash_item(*get_img_hash)(AVCodecContext* pCodecContext, AVFrame* in_frame);

int frame_examined_number = 0;

void init_args(int argc, char* argv[]);
int init_reference_av_params();
int init_query_av_params();
int init_reference_video_frame_index();
int init_query_video_base_scene();
int mark();
double pts2actualtime(__int64 pts, AVStream* stream);
double leaf2actualtime(int leaf_id);
vhash_item get_key_frame_hash(int key_frame_no);
void show_progress(int p, int t);
int is_similar_gop_strict(int query_video_key_frame_no, std::vector<int>& reference_video_find_at);
int redundancy_in_gop(int reference_based_on, int check_key_frame_no);
int redundancy_in_gop_reverse(int reference_based_on, int check_key_frame_no);
int find_redundancy_end_query_key_frame_no(int query_video_key_frame_no, int reference_based_on);
int find_redundancy_begin_query_key_frame_no_prev_scene(int query_video_key_frame_no, int reference_based_on);
int find_redundancy_begin_query_key_frame_no_cur_scene(int query_video_key_frame_no, int reference_based_on);
int get_key_frame_no_near_timestamp(double actual_time);
bool is_frame_black(AVFrame* frame);
bool is_frame_silent(AVFrame* frame);
std::vector<int> find_vhash(const vhash_item& item);
void concat_interval();
void show_results();
std::vector<std::pair<int, int>> get_overlapped_frame(const std::vector<std::pair<int, int>>& ground_truth);
std::vector<std::pair<int, int>> get_non_overlapped_frame(const std::vector<std::pair<int, int>>& ground_truth);
bool is_time_delta_correct(double query_time_delta, const std::vector<int> similar_frames, int renference_based_on);
void swap_scene(scene_segment& left, scene_segment& right);

int main(int argc, char* argv[])
{
    //std::cout << avcodec_configuration() << std::endl;
    av_log_set_level(AV_LOG_ERROR);
    run_begin_time = clock();
    get_img_hash = video_dct_hash_yuv;

    init_args(argc, argv);
    init_reference_av_params();
    init_query_av_params();
    init_reference_video_frame_index();
    init_query_video_base_scene();

    mark();

    run_end_time = clock();
    show_results();
    return 0;
}

void init_args(int argc, char* argv[])
{
    int ch;
    while ((ch = getopt(argc, argv, "r:x:q:a:o:g:")) != -1)
    {
        switch (ch)
        {
        case 'r':
            strcpy(reference_video_path, optarg);
            break;
        case 'x':
            strcpy(reference_video_index_path, optarg);
            break;
        case 'q':
            strcpy(query_video_path, optarg);
            break;
        case 'a':
            strcpy(query_video_audio_t_path, optarg);
            break;
        case 'o':
            strcpy(out_report_path, optarg);
            break;
        case 'g':
            strcpy(ground_truth_path, optarg);
            break;
        default:
            break;
        }
    }

}

int init_reference_av_params()
{
    if (avformat_open_input(&reference_video_format_context, reference_video_path, NULL, NULL) < 0)
    {
        printf("Could not open video file %s\n", reference_video_path);
        exit(1);
    }

    if (avformat_find_stream_info(reference_video_format_context, NULL) < 0)
    {
        printf("Could not find stream information\n");
        exit(1);
    }

    av_dump_format(reference_video_format_context, 0, reference_video_path, 0);

    reference_video_index = av_find_best_stream(reference_video_format_context, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);

    if (reference_video_index < 0)
    {
        fprintf(stderr, "Could not find %s stream in input file '%s'\n",
            av_get_media_type_string(AVMEDIA_TYPE_VIDEO), reference_video_path);
        return reference_video_index;
    }

    reference_video_stream = reference_video_format_context->streams[reference_video_index];

    AVPacket packet;
    av_init_packet(&packet);

    while (av_read_frame(reference_video_format_context, &packet) >= 0)
    {
        if (packet.stream_index == reference_video_index)
        {
            video_frame_packet p;
            p.pts = packet.pts;
            p.dts = packet.dts;
            p.is_key_frame = packet.flags & 0x01;
            p.duration = packet.duration;
            p.pos = packet.pos;
            reference_video_frames.push_back(p);
        }
        av_packet_unref(&packet);
    }

    std::sort(reference_video_frames.begin(), reference_video_frames.end(), sort_video_frame_packet_by_pts);
    reference_video_frame_number = reference_video_frames.size();
    for (int i = 0; i < reference_video_frame_number; i++)
    {
        reference_video_pkt_pos_to_frame_index[reference_video_frames[i].pos] = i;
    }

    return 0;
}

int init_query_av_params()
{
    // video params
    if (avformat_open_input(&query_video_format_context, query_video_path, NULL, NULL) < 0)
    {
        printf("Could not open video file %s\n", query_video_path);
        exit(1);
    }

    if (avformat_find_stream_info(query_video_format_context, NULL) < 0)
    {
        printf("Could not find stream information\n");
        exit(1);
    }

    av_dump_format(query_video_format_context, 0, query_video_path, 0);

    query_video_index = av_find_best_stream(query_video_format_context, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);

    if (query_video_index < 0)
    {
        fprintf(stderr, "Could not find %s stream in input file '%s'\n",
            av_get_media_type_string(AVMEDIA_TYPE_VIDEO), query_video_path);
        return query_video_index;
    }

    query_video_stream = query_video_format_context->streams[query_video_index];

    AVPacket packet;
    av_init_packet(&packet);

    while (av_read_frame(query_video_format_context, &packet) >= 0)
    {
        if (packet.stream_index == query_video_index)
        {
            video_frame_packet p;
            p.pts = packet.pts;
            p.dts = packet.dts;
            p.is_key_frame = packet.flags & 0x01;
            p.duration = packet.duration;
            p.pos = packet.pos;
            query_video_frames.push_back(p);
        }
        av_packet_unref(&packet);
    }

    std::sort(query_video_frames.begin(), query_video_frames.end(), sort_video_frame_packet_by_pts);
    query_video_frame_number = query_video_frames.size();

    for (int i = 0; i < query_video_frame_number; i++)
    {
        if (query_video_frames[i].is_key_frame)
        {
            query_video_key_frame_index.push_back(i);
        }
    }

    query_video_key_frame_number = query_video_key_frame_index.size();

    FILE* fp = fopen(query_video_audio_t_path, "r+");

    char sharp;

    fscanf(fp, "%c %d %d %d %d", &sharp, &query_video_audio_scene_leaf_number, &query_video_audio_sr, &query_video_audio_n_fft, &query_video_audio_hop_length);

    int node_id, left, right;

    while (fscanf(fp, "%d %d %d", &node_id, &left, &right) != EOF)
    {
        std::pair<int, int> p;
        p.first = left;
        p.second = right;
        query_video_audio_scene_hierarchy.push_back(p);
    }


    // audio params
    if (avformat_open_input(&query_audio_format_context, query_video_path, NULL, NULL) < 0)
    {
        printf("Could not open video file %s\n", query_video_path);
        exit(1);
    }

    if (avformat_find_stream_info(query_audio_format_context, NULL) < 0)
    {
        printf("Could not find stream information\n");
        exit(1);
    }

    av_dump_format(query_audio_format_context, 0, query_video_path, 0);

    query_audio_index = av_find_best_stream(query_audio_format_context, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);

    if (query_audio_index < 0)
    {
        fprintf(stderr, "Could not find %s stream in input file '%s'\n",
            av_get_media_type_string(AVMEDIA_TYPE_AUDIO), query_video_path);
        return query_audio_index;
    }

    query_audio_stream = query_audio_format_context->streams[query_audio_index];

    return 0;
}

int init_reference_video_frame_index()
{
    FILE* fp = fopen(reference_video_index_path, "rb+");

    vhash_item item;
    mih_codes_n = 0;
    while (fread(&item.sz, sizeof(item.sz), 1, fp) > 0)
    {
        fread(item.h, sizeof(unsigned char), item.sz, fp);
        fread(&item.pkt_pos, sizeof(item.pkt_pos), 1, fp);
        fread(&item.frame_index, sizeof(item.frame_index), 1, fp);
        item.frame_index = reference_video_pkt_pos_to_frame_index[item.pkt_pos];

        vhash_by_frame_index[item.frame_index] = item;
        mih_insert_table(item);
        mih_codes_n++;
    }

    fclose(fp);

    return 0;
}

int init_query_video_base_scene()
{
    query_video_base_scene_node_id = sh_cut(SCENE_GRANULARITY, query_video_audio_scene_leaf_number, query_video_audio_scene_hierarchy);

    //std::vector<int> label(query_video_audio_scene_leaf_number, -1);

    for (int i = 0; i < query_video_base_scene_node_id.size(); i++)
    {
        scene_segment segment = sh_get_scene_segment_by_node_id(query_video_base_scene_node_id[i], query_video_audio_scene_leaf_number, query_video_audio_scene_hierarchy);
        query_video_base_scenes.push_back(segment);
    }

    std::sort(query_video_base_scenes.begin(), query_video_base_scenes.end(), sort_scene_segment_by_end_time);

    return 0;
}

int mark()
{
    bool last_is_redundant = false;
    int key_frame_left_ptr = 0;
    show_progress(0, query_video_key_frame_number);
    int last_partial_gop = -2;
    int prev_gop_sim = GOP_UNCERTAIN;
    while (key_frame_left_ptr < query_video_key_frame_number)
    {
        show_progress(key_frame_left_ptr, query_video_key_frame_number);
        // Both video frames & audio frames are necessary here,
        // since it is not easy to figure out where redundancy starts only depending on randomly sampled audio frames,
        // and the video sync frames at client side & server side may not be aligned.

        std::vector<int> find_at = find_vhash(get_key_frame_hash(key_frame_left_ptr));
        int gop_comp = is_similar_gop_strict(key_frame_left_ptr, find_at);
        if (gop_comp == GOP_SAME)
        {
            int local_prob_matched_head_idx = find_at.front();
            int backup_key_frame_left_ptr = key_frame_left_ptr;
            int backup_local_prob_matched_head_idx = local_prob_matched_head_idx;
            if (key_frame_left_ptr > 0) {

                int new_key_frame_left_ptr;
                if (prev_gop_sim == GOP_UNCERTAIN)
                {
                    new_key_frame_left_ptr = find_redundancy_begin_query_key_frame_no_cur_scene(key_frame_left_ptr, local_prob_matched_head_idx);
                }
                else
                {
                    new_key_frame_left_ptr = find_redundancy_begin_query_key_frame_no_prev_scene(key_frame_left_ptr, local_prob_matched_head_idx);
                }
                local_prob_matched_head_idx -= (query_video_key_frame_index[key_frame_left_ptr] - query_video_key_frame_index[new_key_frame_left_ptr]);
                local_prob_matched_head_idx = max(local_prob_matched_head_idx, 0);
                key_frame_left_ptr = new_key_frame_left_ptr;

                if (key_frame_left_ptr > 0)
                {
                    int gop_check_delta = 1;
                    int ret = redundancy_in_gop_reverse(local_prob_matched_head_idx, key_frame_left_ptr - gop_check_delta);
                    int frame_num = ret;

                    while (key_frame_left_ptr - gop_check_delta - 1 >= 0 &&
                        ret == query_video_key_frame_index[key_frame_left_ptr - gop_check_delta + 1] - query_video_key_frame_index[key_frame_left_ptr - gop_check_delta])
                    {
                        gop_check_delta++;
                        ret = redundancy_in_gop_reverse(local_prob_matched_head_idx - frame_num, key_frame_left_ptr - gop_check_delta);
                        frame_num += ret;
                    }

                    if (frame_num > 0) {
                        redundant_clip clip;
                        clip.is_from_partial_gop = true;

                        clip.query_video_begin_frame = query_video_key_frame_index[key_frame_left_ptr] - frame_num;
                        //piece.cloud_video_end_frame = video_sync_sample_index[iframe_right_ptr + 1] - 1;
                        clip.query_video_end_frame = query_video_key_frame_index[key_frame_left_ptr] - 1;

                        clip.reference_video_begin_frame = local_prob_matched_head_idx - frame_num;
                        clip.reference_video_end_frame = local_prob_matched_head_idx - 1;

                        if (clip.reference_video_end_frame >= reference_video_frame_number)
                            clip.reference_video_end_frame = reference_video_frame_number - 1;

                        redundant_clips.push_back(clip);
                    }
                }
            }

            int checked_tail = find_redundancy_end_query_key_frame_no(backup_key_frame_left_ptr, backup_local_prob_matched_head_idx);

            // here, key_frame_right_ptr indicates the beginning I frame in the last surely (not partial) redundant gop

            int key_frame_right_ptr = checked_tail == query_video_key_frame_number - 1 ? checked_tail : checked_tail - 1;
            //int key_frame_right_ptr = checked_tail;

            redundant_clip clip;
            clip.is_from_partial_gop = false;

            clip.query_video_begin_frame = query_video_key_frame_index[key_frame_left_ptr];
            //piece.cloud_video_end_frame = video_sync_sample_index[iframe_right_ptr + 1] - 1;
            clip.query_video_end_frame = query_video_key_frame_index[key_frame_right_ptr] - 1;

            /*unsigned int cloud_begin_audio_frame = find_audio_sample_index_upper_bound_by_time((double)get_video_timestamp_by_sample_index(video_sync_sample_index[iframe_left_ptr]) / video_time_scale);
            int audio_delta = hit_at.back().front() - hit_at.front().front();*/

            clip.reference_video_begin_frame = local_prob_matched_head_idx;
            //piece.local_video_end_frame = -1 + local_prob_matched_head_idx + video_sync_sample_index[iframe_right_ptr + 1] - video_sync_sample_index[iframe_left_ptr];
            clip.reference_video_end_frame = clip.reference_video_begin_frame + clip.query_video_end_frame - clip.query_video_begin_frame;

            if (clip.reference_video_end_frame >= reference_video_frame_number)
                clip.reference_video_end_frame = reference_video_frame_number - 1;

            key_frame_left_ptr = key_frame_right_ptr;

            redundant_clips.push_back(clip);


            if (key_frame_left_ptr < query_video_key_frame_number) {
                int ret = redundancy_in_gop(redundant_clips.back().reference_video_end_frame + 1, key_frame_left_ptr);
                int frame_num = ret;
                int gop_check_delta = 1;

                while (key_frame_left_ptr + gop_check_delta < query_video_key_frame_number &&
                    ret == query_video_key_frame_index[key_frame_left_ptr + gop_check_delta] - query_video_key_frame_index[key_frame_left_ptr + gop_check_delta - 1])
                {
                    ret = redundancy_in_gop(redundant_clips.back().reference_video_end_frame + 1 + frame_num, key_frame_left_ptr + gop_check_delta);
                    frame_num += ret;
                    gop_check_delta++;
                }

                if (frame_num > 0) {
                    redundant_clip clip;
                    clip.is_from_partial_gop = true;

                    clip.query_video_begin_frame = query_video_key_frame_index[key_frame_left_ptr];
                    //piece.cloud_video_end_frame = video_sync_sample_index[iframe_right_ptr + 1] - 1;
                    clip.query_video_end_frame = clip.query_video_begin_frame + frame_num - 1;

                    clip.reference_video_begin_frame = redundant_clips.back().reference_video_end_frame + 1;
                    clip.reference_video_end_frame = clip.reference_video_begin_frame + clip.query_video_end_frame - clip.query_video_begin_frame;

                    if (clip.reference_video_end_frame >= reference_video_frame_number)
                        clip.reference_video_end_frame = reference_video_frame_number - 1;

                    redundant_clips.push_back(clip);
                }

                key_frame_left_ptr += (gop_check_delta - 1);
            }
        }

        key_frame_left_ptr++;
        if (gop_comp == GOP_SAME || gop_comp == GOP_DIFFERENT)
        {
            if (key_frame_left_ptr < query_video_key_frame_number)
            {
                double query_video_key_frame_time_second = pts2actualtime(query_video_frames[query_video_key_frame_index[key_frame_left_ptr]].pts, query_video_stream);
                scene_segment scene;
                int i;
                for (i = 0; i < query_video_base_scenes.size(); i++)
                {
                    if (leaf2actualtime(query_video_base_scenes[i].right_leaf_id) > query_video_key_frame_time_second)
                    {
                        scene = query_video_base_scenes[i];
                        break;
                    }
                }
                if (i < query_video_base_scenes.size())
                {
                    int bak_key_frame = key_frame_left_ptr;
                    key_frame_left_ptr = get_key_frame_no_near_timestamp(leaf2actualtime(scene.right_leaf_id));
                    key_frame_left_ptr = min(key_frame_left_ptr + 1, query_video_key_frame_number - 1);

                    if (abs(pts2actualtime(query_video_frames[query_video_key_frame_index[key_frame_left_ptr]].pts, query_video_stream) - pts2actualtime(query_video_frames[query_video_key_frame_index[bak_key_frame - 1]].pts, query_video_stream)) > VIDEO_SCENE_PROTECT_SKIP)
                    {
                        key_frame_left_ptr = get_key_frame_no_near_timestamp(pts2actualtime(query_video_frames[query_video_key_frame_index[bak_key_frame - 1]].pts, query_video_stream) + VIDEO_SCENE_PROTECT_SKIP);
                        if (key_frame_left_ptr < bak_key_frame)
                        {
                            key_frame_left_ptr = bak_key_frame;
                        }
                        key_frame_left_ptr = min(key_frame_left_ptr, query_video_key_frame_number - 1);
                        gop_comp = GOP_UNCERTAIN;
                    }
                }
            }
        }
        prev_gop_sim = gop_comp;
        show_progress(key_frame_left_ptr, query_video_key_frame_number);
    }
    concat_interval();
    return 0;
}

double pts2actualtime(__int64 pts, AVStream* stream)
{
    return pts * av_q2d(stream->time_base);
}

double leaf2actualtime(int leaf_id)
{
    return (query_video_audio_n_fft / 2 + query_video_audio_hop_length * leaf_id) / (double)query_video_audio_sr;
}

vhash_item get_key_frame_hash(int key_frame_no)
{
    if (query_video_key_frame_hash_cache.find(key_frame_no) != query_video_key_frame_hash_cache.end())
    {
        return query_video_key_frame_hash_cache[key_frame_no];
    }
    vhash_item item;
    AVCodec* video_codec = avcodec_find_decoder(query_video_stream->codecpar->codec_id);

    AVCodecContext* video_codec_context = avcodec_alloc_context3(NULL);
    avcodec_parameters_to_context(video_codec_context, query_video_stream->codecpar);
    avcodec_open2(video_codec_context, video_codec, NULL);
    AVFrame* frame = av_frame_alloc();

    AVPacket packet;
    av_init_packet(&packet);
    bool flag = true;
    static int frame_count = 1;
    video_frame_packet frame_packet = query_video_frames[query_video_key_frame_index[key_frame_no]];
    av_seek_frame(query_video_format_context, query_video_index, max(0, frame_packet.pts - frame_packet.duration * 16), AVSEEK_FLAG_FRAME);
    while (av_read_frame(query_video_format_context, &packet) >= 0 && flag)
    {
        if (packet.stream_index == query_video_index)
        {
            int re = avcodec_send_packet(video_codec_context, &packet);
            if (re < 0)
            {
                continue;
            }
            avcodec_send_packet(video_codec_context, NULL);
            while (avcodec_receive_frame(video_codec_context, frame) == 0)
            {
                //char buf[1024];
                //sprintf(buf, "%spic/%d.jpg", "../test_case/", query_video_key_frame_index[key_frame_no]);
                //save_jpg(frame, buf);
                item = get_img_hash(video_codec_context, frame);
                item.frame_index = query_video_key_frame_index[key_frame_no];
                flag = false;
                break;
            }
        }
        av_packet_unref(&packet);
    }

    avcodec_close(video_codec_context);
    avcodec_free_context(&video_codec_context);
    av_frame_free(&frame);
    av_packet_unref(&packet);
    return item;
}

void show_progress(int p, int t)
{
    char buf[256] = { 0 };
    buf[0] = '[';
    int pixel = t == 0 ? 100 : 100.0 * p / t;
    int percent = pixel;
    pixel /= 2;
    for (int i = 1; i <= pixel; i++)
        buf[i] = i == pixel ? '>' : '=';
    for (int i = pixel + 1; i <= 50; i++)
        buf[i] = ' ';
    buf[51] = ']';
    sprintf(buf + 52, " %d %%", percent);
    printf("%s", buf);
    putchar('\r');
    fflush(stdout);
}

int is_similar_gop_strict(int query_video_key_frame_no, std::vector<int>& reference_video_find_at)
{
    if (query_video_key_frame_no + 1 < query_video_key_frame_number)
    {
        std::vector<int> next_find_at = find_vhash(get_key_frame_hash(query_video_key_frame_no + 1));

        if (reference_video_find_at.size() >= VIDEO_FRAME_SKIP_CHECK_THRESHOLD || next_find_at.size() >= VIDEO_FRAME_SKIP_CHECK_THRESHOLD)
            return GOP_UNCERTAIN;

        std::vector<int> current_val_find_at, next_val_find_at;

        for (int i = 0; i < reference_video_find_at.size(); i++)
        {
            for (int j = 0; j < next_find_at.size(); j++)
            {
                double query_video_time_delta = pts2actualtime(query_video_frames[query_video_key_frame_index[query_video_key_frame_no + 1]].pts, query_video_stream)
                    - pts2actualtime(query_video_frames[query_video_key_frame_index[query_video_key_frame_no]].pts, query_video_stream);
                double reference_video_time_delta = pts2actualtime(reference_video_frames[next_find_at[j]].pts, reference_video_stream)
                    - pts2actualtime(reference_video_frames[reference_video_find_at[i]].pts, reference_video_stream);
                if (abs(query_video_time_delta - reference_video_time_delta) <= TIME_DELTA_THRESHOLD)
                {
                    current_val_find_at.push_back(reference_video_find_at[i]);
                    next_val_find_at.push_back(next_find_at[j]);
                }
            }
        }

        if (!current_val_find_at.empty() && !next_val_find_at.empty())
        {
            reference_video_find_at = current_val_find_at;
            if (reference_video_find_at.size() > 1)
            {
                for (int i = 1; i < reference_video_find_at.size(); i++)
                {
                    if (reference_video_find_at[i] == reference_video_find_at[i - 1])
                    {
                        reference_video_find_at.erase(reference_video_find_at.begin() + i);
                        i--;
                    }
                }
            }
            return GOP_SAME;
        }
        return GOP_DIFFERENT;
    }
    return GOP_DIFFERENT;
}

int redundancy_in_gop(int reference_based_on, int check_key_frame_no)
{
    std::vector<vhash_item> hashes;
    std::vector<double> video_timestamps;
    std::vector<bool> audio_silents;
    std::vector<double> audio_timestamps;
    video_frame_packet frame_packet = query_video_frames[query_video_key_frame_index[check_key_frame_no]];

    if (query_video_gop_decompress.find(check_key_frame_no) == query_video_gop_decompress.end())
    {
        AVCodec* video_codec = avcodec_find_decoder(query_video_stream->codecpar->codec_id);
        AVCodec* audio_codec = avcodec_find_decoder(query_audio_stream->codecpar->codec_id);

        AVCodecContext* video_codec_context = avcodec_alloc_context3(NULL);
        avcodec_parameters_to_context(video_codec_context, query_video_stream->codecpar);
        avcodec_open2(video_codec_context, video_codec, NULL);

        AVCodecContext* audio_codec_context = avcodec_alloc_context3(NULL);
        avcodec_parameters_to_context(audio_codec_context, query_audio_stream->codecpar);
        avcodec_open2(audio_codec_context, audio_codec, NULL);

        AVFrame* frame = av_frame_alloc();
        AVFrame* audio_frame = av_frame_alloc();

        AVPacket packet;
        av_init_packet(&packet);

        int frame_count = 0;
        bool flag = true;

        //std::vector< std::vector<int> > find_ats;

        int stop = 0;
        int black_frame_count = 0;
        int silent_frame_count = 0;
        double audio_silent_begin_time = 0;


        av_seek_frame(query_video_format_context, query_video_index, max(0, frame_packet.pts - frame_packet.duration * 16), AVSEEK_FLAG_FRAME);
        //av_seek_frame(audio_format_context, audio_index, (int64_t)(pts_time * AV_TIME_BASE), AVSEEK_FLAG_BACKWARD);
        while (av_read_frame(query_video_format_context, &packet) >= 0)
        {
            if (packet.stream_index == query_video_index)
            {
                int re = avcodec_send_packet(video_codec_context, &packet);
                if (re < 0)
                {
                    continue;
                }
                while (avcodec_receive_frame(video_codec_context, frame) == 0)
                {
                    if (frame->key_frame)
                    {
                        stop++;
                        if (stop == 2)
                        {
                            break;
                        }
                    }

                    if (reference_based_on <= VIDEO_BLACK_FRAME_CHECK_THRESHOLD * reference_video_frame_number && is_frame_black(frame))
                    {
                        black_frame_count++;
                    }
                    else
                    {
                        black_frame_count = 0;
                    }

                    if (black_frame_count >= VIDEO_BLACK_EPISODE_FRAME_COUNT)
                    {
                        break;
                    }

                    vhash_item item = get_img_hash(video_codec_context, frame);

                    hashes.push_back(item);
                    video_timestamps.push_back((double)frame->pts * (double)query_video_stream->time_base.num / query_video_stream->time_base.den);

                    frame_count++;
                }
            }
            else if (packet.stream_index == query_audio_index)
            {
                int re = avcodec_send_packet(audio_codec_context, &packet);
                if (re < 0)
                {
                    continue;
                }
                while (avcodec_receive_frame(audio_codec_context, audio_frame) == 0)
                {
                    //printf("%f %f\n", *d1, *d2);
                    audio_silents.push_back(is_frame_silent(audio_frame));
                    audio_timestamps.push_back((double)audio_frame->pts * (double)query_audio_stream->time_base.num / query_audio_stream->time_base.den);
                }
            }
            av_packet_unref(&packet);
            if (!flag)
            {
                break;
            }
            if (stop == 2)
            {
                break;
            }
            if (black_frame_count >= VIDEO_BLACK_EPISODE_FRAME_COUNT)
            {
                for (int i = 0; i < black_frame_count - 1; i++)
                {
                    hashes.pop_back();
                }
                break;
            }
        }

        if (black_frame_count < VIDEO_BLACK_EPISODE_FRAME_COUNT && reference_based_on <= VIDEO_BLACK_FRAME_CHECK_THRESHOLD * reference_video_frame_number)
        {
            silent_frame_count = 0;
            for (int i = 0; i < audio_silents.size(); i++)
            {
                if (audio_silents[i])
                {
                    if (silent_frame_count == 0)
                    {
                        audio_silent_begin_time = audio_timestamps[i];
                    }
                    silent_frame_count++;
                }
                else
                {
                    silent_frame_count = 0;
                }

                if (silent_frame_count >= AUDIO_SILENT_EPISODE_FRAME_COUNT)
                {
                    break;
                }
            }

            if (silent_frame_count >= AUDIO_SILENT_EPISODE_FRAME_COUNT)
            {
                while (hashes.size() > 0 && audio_silent_begin_time < video_timestamps.back())
                {
                    hashes.pop_back();
                    video_timestamps.pop_back();
                }
            }
        }
        avcodec_close(video_codec_context);
        avcodec_free_context(&video_codec_context);
        avcodec_close(audio_codec_context);
        avcodec_free_context(&audio_codec_context);
        //avpicture_free((AVPicture*)frame);
        av_frame_free(&frame);
        av_frame_free(&audio_frame);
        av_packet_unref(&packet);

        query_video_gop_decompress[check_key_frame_no] = hashes;

        frame_examined_number += hashes.size();
    }
    else
    {
        hashes = query_video_gop_decompress[check_key_frame_no];
    }

    int pass_count = 0;

    for (int i = 0; i < hashes.size(); i++)
    {
        pass_count++;
        vhash_item vh = hashes[i];
        int index = min(max(reference_based_on + pass_count - 1, 0), reference_video_frame_number - 1);

        int dis = get_hamming_distance(vh, vhash_by_frame_index[index]);

        if (dis > HAMMING_THRESHOLD * GOP_REDUNDANCY_MULTIPLIER * SCALE_WIDTH * SCALE_HEIGHT)
        {
            pass_count--;
            break;
        }
    }

    return pass_count;
}

int redundancy_in_gop_reverse(int reference_based_on, int check_key_frame_no)
{
    std::vector<vhash_item> hashes;
    video_frame_packet frame_packet = query_video_frames[query_video_key_frame_index[check_key_frame_no]];

    if (query_video_gop_decompress.find(check_key_frame_no) == query_video_gop_decompress.end())
    {

        AVCodec* video_codec = avcodec_find_decoder(query_video_stream->codecpar->codec_id);

        AVCodecContext* video_codec_context = avcodec_alloc_context3(NULL);
        avcodec_parameters_to_context(video_codec_context, query_video_stream->codecpar);
        avcodec_open2(video_codec_context, video_codec, NULL);

        AVFrame* frame = av_frame_alloc();

        AVPacket packet;
        av_init_packet(&packet);

        int frame_count = 0;
        bool flag = true;

        int stop = 0;

        //std::vector< std::vector<int> > find_ats;
        av_seek_frame(query_video_format_context, query_video_index, max(0, frame_packet.pts - frame_packet.duration * 16), AVSEEK_FLAG_FRAME);
        while (av_read_frame(query_video_format_context, &packet) >= 0)
        {
            if (packet.stream_index == query_video_index)
            {
                int re = avcodec_send_packet(video_codec_context, &packet);
                if (re < 0)
                {
                    continue;
                }
                while (avcodec_receive_frame(video_codec_context, frame) == 0) {
                    if (frame->key_frame)
                    {
                        stop++;
                        if (stop == 2)
                        {
                            break;
                        }
                    }
                    vhash_item item = get_img_hash(video_codec_context, frame);
                    hashes.push_back(item);
                    frame_count++;

                }
            }
            av_packet_unref(&packet);
            if (stop == 2)
            {
                break;
            }
        }

        avcodec_close(video_codec_context);
        avcodec_free_context(&video_codec_context);
        //avpicture_free((AVPicture*)frame);
        av_frame_free(&frame);
        av_packet_unref(&packet);

        query_video_gop_decompress[check_key_frame_no] = hashes;
        frame_examined_number += hashes.size();
    }
    else
    {
        hashes = query_video_gop_decompress[check_key_frame_no];
    }

    int pass_count = 0;

    for (int i = hashes.size() - 1; i >= 0; i--)
    {
        pass_count++;
        vhash_item vh = hashes[i];

        int index = max(reference_based_on - pass_count, 0);

        int dis = get_hamming_distance(vh, vhash_by_frame_index[index]);

        if (dis > HAMMING_THRESHOLD * GOP_REDUNDANCY_MULTIPLIER * SCALE_WIDTH * SCALE_HEIGHT)
        {
            pass_count--;
            break;
        }
    }

    return pass_count;
}

int find_redundancy_end_query_key_frame_no(int query_video_key_frame_no, int reference_based_on)
{
    int tries = 0;
    double query_video_key_frame_time_second = pts2actualtime(query_video_frames[query_video_key_frame_index[query_video_key_frame_no]].pts, query_video_stream);
    double reference_video_frame_time_second = pts2actualtime(reference_video_frames[reference_based_on].pts, reference_video_stream);
    int begin_at = reference_based_on + 1, end_at = min(reference_video_frame_number, reference_based_on + VIDEO_FRAME_PROTECT_RANGE);

    //int redundancy_end_key_frame_no = query_video_key_frame_no;

    int current_scene_index = query_video_base_scenes.size() - 1;
    scene_segment current_scene = query_video_base_scenes.back();

    int last_same_gop_key_frame_no = query_video_key_frame_no;

    for (int i = 0; i < query_video_base_scenes.size(); i++)
    {
        if (leaf2actualtime(query_video_base_scenes[i].right_leaf_id) > query_video_key_frame_time_second)
        {
            current_scene_index = i;
            current_scene = query_video_base_scenes[i];
            break;
        }
    }

    int left_node_id = -1, right_node_id = -1;
    bool find_break_scene = false;
    while (!find_break_scene)
    {
        int key_frame_no_left = get_key_frame_no_near_timestamp(leaf2actualtime(current_scene.right_leaf_id));

        if (key_frame_no_left + 1 < query_video_key_frame_number)
        {
            vhash_item query_video_hash_left = get_key_frame_hash(key_frame_no_left);
            //vhash_item reference_video_hash_left = vhash_by_frame_index[query_video_key_frame_index[key_frame_no_left] - query_video_key_frame_index[query_video_key_frame_no] + reference_based_on];

            //int dis_left = get_hamming_distance(query_video_hash_left, reference_video_hash_left);

            //if (dis_left > HAMMING_THRESHOLD * SCALE_HEIGHT * SCALE_WIDTH)

            std::vector<int> matched_frames = find_vhash(query_video_hash_left);
            if (matched_frames.size() <= 0 || !is_time_delta_correct(pts2actualtime(query_video_frames[query_video_key_frame_index[key_frame_no_left]].pts, query_video_stream)
                - pts2actualtime(query_video_frames[query_video_key_frame_index[query_video_key_frame_no]].pts, query_video_stream), matched_frames, reference_based_on))
            {
                // left key frame is not redundant
                left_node_id = current_scene.hierarchy_node_id < query_video_audio_scene_leaf_number ?
                    current_scene.hierarchy_node_id : query_video_audio_scene_hierarchy[current_scene.hierarchy_node_id - query_video_audio_scene_leaf_number].first;
                right_node_id = current_scene.hierarchy_node_id < query_video_audio_scene_leaf_number ?
                    current_scene.hierarchy_node_id : query_video_audio_scene_hierarchy[current_scene.hierarchy_node_id - query_video_audio_scene_leaf_number].second;
                find_break_scene = true;
                continue;
            }

            vhash_item query_video_hash_right = get_key_frame_hash(key_frame_no_left + 1);
            //vhash_item reference_video_hash_right = vhash_by_frame_index[query_video_key_frame_index[key_frame_no_left + 1] - query_video_key_frame_index[query_video_key_frame_no] + reference_based_on];

            //int dis_right = get_hamming_distance(query_video_hash_right, reference_video_hash_right);

            //if (dis_right > HAMMING_THRESHOLD * SCALE_HEIGHT * SCALE_WIDTH)

            matched_frames = find_vhash(query_video_hash_right);
            if (matched_frames.size() <= 0 || !is_time_delta_correct(pts2actualtime(query_video_frames[query_video_key_frame_index[key_frame_no_left + 1]].pts, query_video_stream)
                - pts2actualtime(query_video_frames[query_video_key_frame_index[query_video_key_frame_no]].pts, query_video_stream), matched_frames, reference_based_on))
            {
                // left key frame is redundant, while the right is not
                return key_frame_no_left;
            }

            // left and right key frames are both redundant

            assert(current_scene_index < query_video_base_scenes.size() - 1);

            current_scene_index++;
            current_scene = query_video_base_scenes[current_scene_index];
            last_same_gop_key_frame_no = key_frame_no_left;
        }
        else
        {
            vhash_item query_video_hash_left = get_key_frame_hash(key_frame_no_left);
            //vhash_item reference_video_hash_left = vhash_by_frame_index[query_video_key_frame_index[key_frame_no_left] - query_video_key_frame_index[query_video_key_frame_no] + reference_based_on];

            //int dis_left = get_hamming_distance(query_video_hash_left, reference_video_hash_left);

            //if (dis_left > HAMMING_THRESHOLD * SCALE_HEIGHT * SCALE_WIDTH)
            std::vector<int> matched_frames = find_vhash(query_video_hash_left);
            if (matched_frames.size() <= 0 || !is_time_delta_correct(pts2actualtime(query_video_frames[query_video_key_frame_index[key_frame_no_left]].pts, query_video_stream)
                - pts2actualtime(query_video_frames[query_video_key_frame_index[query_video_key_frame_no]].pts, query_video_stream), matched_frames, reference_based_on))
            {
                // left key frame is not redundant
                left_node_id = current_scene.hierarchy_node_id < query_video_audio_scene_leaf_number ?
                    current_scene.hierarchy_node_id : query_video_audio_scene_hierarchy[current_scene.hierarchy_node_id - query_video_audio_scene_leaf_number].first;
                right_node_id = current_scene.hierarchy_node_id < query_video_audio_scene_leaf_number ?
                    current_scene.hierarchy_node_id : query_video_audio_scene_hierarchy[current_scene.hierarchy_node_id - query_video_audio_scene_leaf_number].second;
                find_break_scene = true;
                continue;
            }

            // left key frame is redundant

            return key_frame_no_left;
        }
    }

    assert(left_node_id != -1 && right_node_id != -1);
    scene_segment left_scene = sh_get_scene_segment_by_node_id(left_node_id, query_video_audio_scene_leaf_number, query_video_audio_scene_hierarchy);
    scene_segment right_scene = sh_get_scene_segment_by_node_id(right_node_id, query_video_audio_scene_leaf_number, query_video_audio_scene_hierarchy);
    swap_scene(left_scene, right_scene);
    int key_frame_no_left = get_key_frame_no_near_timestamp(leaf2actualtime(left_scene.right_leaf_id));

    while (key_frame_no_left < query_video_key_frame_no)
    {
        left_node_id = right_scene.hierarchy_node_id < query_video_audio_scene_leaf_number ?
            right_scene.hierarchy_node_id : query_video_audio_scene_hierarchy[right_scene.hierarchy_node_id - query_video_audio_scene_leaf_number].first;
        right_node_id = right_scene.hierarchy_node_id < query_video_audio_scene_leaf_number ?
            right_scene.hierarchy_node_id : query_video_audio_scene_hierarchy[right_scene.hierarchy_node_id - query_video_audio_scene_leaf_number].second;
        left_scene = sh_get_scene_segment_by_node_id(left_node_id, query_video_audio_scene_leaf_number, query_video_audio_scene_hierarchy);
        right_scene = sh_get_scene_segment_by_node_id(right_node_id, query_video_audio_scene_leaf_number, query_video_audio_scene_hierarchy);
        swap_scene(left_scene, right_scene);
        key_frame_no_left = get_key_frame_no_near_timestamp(leaf2actualtime(left_scene.right_leaf_id));
    }

    bool can_loop = true;
    while (can_loop)
    {
        if (key_frame_no_left + 1 < query_video_key_frame_number)
        {
            vhash_item query_video_hash_left = get_key_frame_hash(key_frame_no_left);
            //vhash_item reference_video_hash_left = vhash_by_frame_index[query_video_key_frame_index[key_frame_no_left] - query_video_key_frame_index[query_video_key_frame_no] + reference_based_on];

            //int dis_left = get_hamming_distance(query_video_hash_left, reference_video_hash_left);

            //if (dis_left > HAMMING_THRESHOLD * SCALE_HEIGHT * SCALE_WIDTH)

            std::vector<int> matched_frames = find_vhash(query_video_hash_left);
            if (matched_frames.size() <= 0 || !is_time_delta_correct(pts2actualtime(query_video_frames[query_video_key_frame_index[key_frame_no_left]].pts, query_video_stream)
                - pts2actualtime(query_video_frames[query_video_key_frame_index[query_video_key_frame_no]].pts, query_video_stream), matched_frames, reference_based_on))
            {
                // left key frame is not redundant, so search left scene
                left_node_id = left_scene.hierarchy_node_id < query_video_audio_scene_leaf_number ?
                    left_scene.hierarchy_node_id : query_video_audio_scene_hierarchy[left_scene.hierarchy_node_id - query_video_audio_scene_leaf_number].first;
                right_node_id = left_scene.hierarchy_node_id < query_video_audio_scene_leaf_number ?
                    left_scene.hierarchy_node_id : query_video_audio_scene_hierarchy[left_scene.hierarchy_node_id - query_video_audio_scene_leaf_number].second;

                left_scene = sh_get_scene_segment_by_node_id(left_node_id, query_video_audio_scene_leaf_number, query_video_audio_scene_hierarchy);
                right_scene = sh_get_scene_segment_by_node_id(right_node_id, query_video_audio_scene_leaf_number, query_video_audio_scene_hierarchy);
                swap_scene(left_scene, right_scene);

                int new_key_frame_no_left = get_key_frame_no_near_timestamp(leaf2actualtime(left_scene.right_leaf_id));

                tries = SCENE_SEARCH_TRY;
                while (new_key_frame_no_left == key_frame_no_left && tries-- > 0)
                {
                    left_node_id = left_scene.hierarchy_node_id < query_video_audio_scene_leaf_number ?
                        left_scene.hierarchy_node_id : query_video_audio_scene_hierarchy[left_scene.hierarchy_node_id - query_video_audio_scene_leaf_number].first;
                    right_node_id = left_scene.hierarchy_node_id < query_video_audio_scene_leaf_number ?
                        left_scene.hierarchy_node_id : query_video_audio_scene_hierarchy[left_scene.hierarchy_node_id - query_video_audio_scene_leaf_number].second;

                    assert(left_node_id != -1 && right_node_id != -1);
                    left_scene = sh_get_scene_segment_by_node_id(left_node_id, query_video_audio_scene_leaf_number, query_video_audio_scene_hierarchy);
                    right_scene = sh_get_scene_segment_by_node_id(right_node_id, query_video_audio_scene_leaf_number, query_video_audio_scene_hierarchy);
                    swap_scene(left_scene, right_scene);
                    new_key_frame_no_left = get_key_frame_no_near_timestamp(leaf2actualtime(left_scene.right_leaf_id));
                }

                if (new_key_frame_no_left == key_frame_no_left)
                {
                    // cannot go deeper any more
                    can_loop = false;
                    break;
                }

                key_frame_no_left = new_key_frame_no_left;

                continue;
            }

            vhash_item query_video_hash_right = get_key_frame_hash(key_frame_no_left + 1);
            //vhash_item reference_video_hash_right = vhash_by_frame_index[query_video_key_frame_index[key_frame_no_left + 1] - query_video_key_frame_index[query_video_key_frame_no] + reference_based_on];

            //int dis_right = get_hamming_distance(query_video_hash_right, reference_video_hash_right);

            //if (dis_right > HAMMING_THRESHOLD * SCALE_HEIGHT * SCALE_WIDTH)

            matched_frames = find_vhash(query_video_hash_right);
            if (matched_frames.size() >= VIDEO_FRAME_SKIP_CHECK_THRESHOLD || // avoid false positives at the end of the intro
                matched_frames.size() <= 0 || !is_time_delta_correct(pts2actualtime(query_video_frames[query_video_key_frame_index[key_frame_no_left + 1]].pts, query_video_stream)
                    - pts2actualtime(query_video_frames[query_video_key_frame_index[query_video_key_frame_no]].pts, query_video_stream), matched_frames, reference_based_on))
            {
                // left key frame is redundant, while the right is not
                return key_frame_no_left;
            }

            // left and right key frames are both redundant, search right scene

            last_same_gop_key_frame_no = key_frame_no_left;

            left_node_id = right_scene.hierarchy_node_id < query_video_audio_scene_leaf_number ?
                right_scene.hierarchy_node_id : query_video_audio_scene_hierarchy[right_scene.hierarchy_node_id - query_video_audio_scene_leaf_number].first;
            right_node_id = right_scene.hierarchy_node_id < query_video_audio_scene_leaf_number ?
                right_scene.hierarchy_node_id : query_video_audio_scene_hierarchy[right_scene.hierarchy_node_id - query_video_audio_scene_leaf_number].second;

            left_scene = sh_get_scene_segment_by_node_id(left_node_id, query_video_audio_scene_leaf_number, query_video_audio_scene_hierarchy);
            right_scene = sh_get_scene_segment_by_node_id(right_node_id, query_video_audio_scene_leaf_number, query_video_audio_scene_hierarchy);
            swap_scene(left_scene, right_scene);

            int new_key_frame_no_left = get_key_frame_no_near_timestamp(leaf2actualtime(left_scene.right_leaf_id));

            tries = SCENE_SEARCH_TRY;
            while (new_key_frame_no_left == key_frame_no_left && tries-- > 0)
            {
                left_node_id = right_scene.hierarchy_node_id < query_video_audio_scene_leaf_number ?
                    right_scene.hierarchy_node_id : query_video_audio_scene_hierarchy[right_scene.hierarchy_node_id - query_video_audio_scene_leaf_number].first;
                right_node_id = right_scene.hierarchy_node_id < query_video_audio_scene_leaf_number ?
                    right_scene.hierarchy_node_id : query_video_audio_scene_hierarchy[right_scene.hierarchy_node_id - query_video_audio_scene_leaf_number].second;

                assert(left_node_id != -1 && right_node_id != -1);
                left_scene = sh_get_scene_segment_by_node_id(left_node_id, query_video_audio_scene_leaf_number, query_video_audio_scene_hierarchy);
                right_scene = sh_get_scene_segment_by_node_id(right_node_id, query_video_audio_scene_leaf_number, query_video_audio_scene_hierarchy);
                swap_scene(left_scene, right_scene);
                new_key_frame_no_left = get_key_frame_no_near_timestamp(leaf2actualtime(left_scene.right_leaf_id));
            }

            if (new_key_frame_no_left == key_frame_no_left)
            {
                // cannot go deeper any more
                can_loop = false;
                break;
            }

            key_frame_no_left = new_key_frame_no_left;
        }
        else
        {
            vhash_item query_video_hash_left = get_key_frame_hash(key_frame_no_left);
            //vhash_item reference_video_hash_left = vhash_by_frame_index[query_video_key_frame_index[key_frame_no_left] - query_video_key_frame_index[query_video_key_frame_no] + reference_based_on];

            //int dis_left = get_hamming_distance(query_video_hash_left, reference_video_hash_left);

            //if (dis_left > HAMMING_THRESHOLD * SCALE_HEIGHT * SCALE_WIDTH)
            std::vector<int> matched_frames = find_vhash(query_video_hash_left);
            if (matched_frames.size() <= 0 || !is_time_delta_correct(pts2actualtime(query_video_frames[query_video_key_frame_index[key_frame_no_left]].pts, query_video_stream)
                - pts2actualtime(query_video_frames[query_video_key_frame_index[query_video_key_frame_no]].pts, query_video_stream), matched_frames, reference_based_on))
            {
                // left key frame is not redundant, so search left scene
                left_node_id = left_scene.hierarchy_node_id < query_video_audio_scene_leaf_number ?
                    left_scene.hierarchy_node_id : query_video_audio_scene_hierarchy[left_scene.hierarchy_node_id - query_video_audio_scene_leaf_number].first;
                right_node_id = left_scene.hierarchy_node_id < query_video_audio_scene_leaf_number ?
                    left_scene.hierarchy_node_id : query_video_audio_scene_hierarchy[left_scene.hierarchy_node_id - query_video_audio_scene_leaf_number].second;

                left_scene = sh_get_scene_segment_by_node_id(left_node_id, query_video_audio_scene_leaf_number, query_video_audio_scene_hierarchy);
                right_scene = sh_get_scene_segment_by_node_id(right_node_id, query_video_audio_scene_leaf_number, query_video_audio_scene_hierarchy);
                swap_scene(left_scene, right_scene);

                int new_key_frame_no_left = get_key_frame_no_near_timestamp(leaf2actualtime(left_scene.right_leaf_id));
                if (new_key_frame_no_left == key_frame_no_left)
                {
                    // cannot go deeper any more
                    can_loop = false;
                    break;
                }

                key_frame_no_left = new_key_frame_no_left;

                continue;
            }

            return key_frame_no_left;
        }
    }
    return last_same_gop_key_frame_no;
}

int find_redundancy_begin_query_key_frame_no_prev_scene(int query_video_key_frame_no, int reference_based_on)
{
    int tries = 0;
    double query_video_key_frame_time_second = pts2actualtime(query_video_frames[query_video_key_frame_index[query_video_key_frame_no]].pts, query_video_stream);
    double reference_video_frame_time_second = pts2actualtime(reference_video_frames[reference_based_on].pts, reference_video_stream);

    int current_scene_index = 0;
    scene_segment current_scene = query_video_base_scenes.front();

    int last_same_gop_key_frame_no = query_video_key_frame_no;

    for (int i = 0; i < query_video_base_scenes.size(); i++)
    {
        if (leaf2actualtime(query_video_base_scenes[i].right_leaf_id) > query_video_key_frame_time_second)
        {
            current_scene_index = i;
            break;
        }
    }

    current_scene_index = max(current_scene_index - 1, 0);
    current_scene = query_video_base_scenes[current_scene_index];

    int left_node_id = current_scene.hierarchy_node_id < query_video_audio_scene_leaf_number ?
        current_scene.hierarchy_node_id : query_video_audio_scene_hierarchy[current_scene.hierarchy_node_id - query_video_audio_scene_leaf_number].first;
    int right_node_id = current_scene.hierarchy_node_id < query_video_audio_scene_leaf_number ?
        current_scene.hierarchy_node_id : query_video_audio_scene_hierarchy[current_scene.hierarchy_node_id - query_video_audio_scene_leaf_number].second;

    assert(left_node_id != -1 && right_node_id != -1);
    scene_segment left_scene = sh_get_scene_segment_by_node_id(left_node_id, query_video_audio_scene_leaf_number, query_video_audio_scene_hierarchy);
    scene_segment right_scene = sh_get_scene_segment_by_node_id(right_node_id, query_video_audio_scene_leaf_number, query_video_audio_scene_hierarchy);
    swap_scene(left_scene, right_scene);
    int key_frame_no_left = get_key_frame_no_near_timestamp(leaf2actualtime(left_scene.right_leaf_id));

    bool can_loop = true;
    while (can_loop)
    {
        if (key_frame_no_left + 1 < query_video_key_frame_number)
        {
            vhash_item query_video_hash_right = get_key_frame_hash(key_frame_no_left + 1);
            //vhash_item reference_video_hash_right = vhash_by_frame_index[query_video_key_frame_index[key_frame_no_left + 1] - query_video_key_frame_index[query_video_key_frame_no] + reference_based_on];

            //int dis_right = get_hamming_distance(query_video_hash_right, reference_video_hash_right);

            //if (dis_right > HAMMING_THRESHOLD * SCALE_HEIGHT * SCALE_WIDTH)
            std::vector<int> matched_frames = find_vhash(query_video_hash_right);
            if (matched_frames.size() <= 0 || !is_time_delta_correct(pts2actualtime(query_video_frames[query_video_key_frame_index[key_frame_no_left + 1]].pts, query_video_stream)
                - pts2actualtime(query_video_frames[query_video_key_frame_index[query_video_key_frame_no]].pts, query_video_stream), matched_frames, reference_based_on))
            {
                // right key frame is not redundant, so search right scene
                left_node_id = right_scene.hierarchy_node_id < query_video_audio_scene_leaf_number ?
                    right_scene.hierarchy_node_id : query_video_audio_scene_hierarchy[right_scene.hierarchy_node_id - query_video_audio_scene_leaf_number].first;
                right_node_id = right_scene.hierarchy_node_id < query_video_audio_scene_leaf_number ?
                    right_scene.hierarchy_node_id : query_video_audio_scene_hierarchy[right_scene.hierarchy_node_id - query_video_audio_scene_leaf_number].second;

                left_scene = sh_get_scene_segment_by_node_id(left_node_id, query_video_audio_scene_leaf_number, query_video_audio_scene_hierarchy);
                right_scene = sh_get_scene_segment_by_node_id(right_node_id, query_video_audio_scene_leaf_number, query_video_audio_scene_hierarchy);
                swap_scene(left_scene, right_scene);

                int new_key_frame_no_left = get_key_frame_no_near_timestamp(leaf2actualtime(left_scene.right_leaf_id));

                tries = SCENE_SEARCH_TRY;
                while (new_key_frame_no_left == key_frame_no_left && tries-- > 0)
                {
                    left_node_id = right_scene.hierarchy_node_id < query_video_audio_scene_leaf_number ?
                        right_scene.hierarchy_node_id : query_video_audio_scene_hierarchy[right_scene.hierarchy_node_id - query_video_audio_scene_leaf_number].first;
                    right_node_id = right_scene.hierarchy_node_id < query_video_audio_scene_leaf_number ?
                        right_scene.hierarchy_node_id : query_video_audio_scene_hierarchy[right_scene.hierarchy_node_id - query_video_audio_scene_leaf_number].second;

                    assert(left_node_id != -1 && right_node_id != -1);
                    left_scene = sh_get_scene_segment_by_node_id(left_node_id, query_video_audio_scene_leaf_number, query_video_audio_scene_hierarchy);
                    right_scene = sh_get_scene_segment_by_node_id(right_node_id, query_video_audio_scene_leaf_number, query_video_audio_scene_hierarchy);
                    swap_scene(left_scene, right_scene);
                    new_key_frame_no_left = get_key_frame_no_near_timestamp(leaf2actualtime(left_scene.right_leaf_id));
                }

                if (new_key_frame_no_left == key_frame_no_left)
                {
                    // cannot go deeper any more
                    can_loop = false;
                    break;
                }

                key_frame_no_left = new_key_frame_no_left;

                continue;
            }

            vhash_item query_video_hash_left = get_key_frame_hash(key_frame_no_left);
            //vhash_item reference_video_hash_left = vhash_by_frame_index[query_video_key_frame_index[key_frame_no_left] - query_video_key_frame_index[query_video_key_frame_no] + reference_based_on];

            //int dis_left = get_hamming_distance(query_video_hash_left, reference_video_hash_left);

            //if (dis_left > HAMMING_THRESHOLD * SCALE_HEIGHT * SCALE_WIDTH)
            matched_frames = find_vhash(query_video_hash_left);
            if (matched_frames.size() <= 0 || !is_time_delta_correct(pts2actualtime(query_video_frames[query_video_key_frame_index[key_frame_no_left]].pts, query_video_stream)
                - pts2actualtime(query_video_frames[query_video_key_frame_index[query_video_key_frame_no]].pts, query_video_stream), matched_frames, reference_based_on))
            {
                // right key frame is redundant, while the left is not
                return key_frame_no_left + 1;
            }

            // left and right key frames are both redundant, search left scene

            last_same_gop_key_frame_no = key_frame_no_left;

            left_node_id = left_scene.hierarchy_node_id < query_video_audio_scene_leaf_number ?
                left_scene.hierarchy_node_id : query_video_audio_scene_hierarchy[left_scene.hierarchy_node_id - query_video_audio_scene_leaf_number].first;
            right_node_id = left_scene.hierarchy_node_id < query_video_audio_scene_leaf_number ?
                left_scene.hierarchy_node_id : query_video_audio_scene_hierarchy[left_scene.hierarchy_node_id - query_video_audio_scene_leaf_number].second;

            left_scene = sh_get_scene_segment_by_node_id(left_node_id, query_video_audio_scene_leaf_number, query_video_audio_scene_hierarchy);
            right_scene = sh_get_scene_segment_by_node_id(right_node_id, query_video_audio_scene_leaf_number, query_video_audio_scene_hierarchy);
            swap_scene(left_scene, right_scene);

            int new_key_frame_no_left = get_key_frame_no_near_timestamp(leaf2actualtime(left_scene.right_leaf_id));

            tries = SCENE_SEARCH_TRY;
            while (new_key_frame_no_left == key_frame_no_left && tries-- > 0)
            {
                left_node_id = left_scene.hierarchy_node_id < query_video_audio_scene_leaf_number ?
                    left_scene.hierarchy_node_id : query_video_audio_scene_hierarchy[left_scene.hierarchy_node_id - query_video_audio_scene_leaf_number].first;
                right_node_id = left_scene.hierarchy_node_id < query_video_audio_scene_leaf_number ?
                    left_scene.hierarchy_node_id : query_video_audio_scene_hierarchy[left_scene.hierarchy_node_id - query_video_audio_scene_leaf_number].second;

                assert(left_node_id != -1 && right_node_id != -1);
                left_scene = sh_get_scene_segment_by_node_id(left_node_id, query_video_audio_scene_leaf_number, query_video_audio_scene_hierarchy);
                right_scene = sh_get_scene_segment_by_node_id(right_node_id, query_video_audio_scene_leaf_number, query_video_audio_scene_hierarchy);
                swap_scene(left_scene, right_scene);
                new_key_frame_no_left = get_key_frame_no_near_timestamp(leaf2actualtime(left_scene.right_leaf_id));
            }

            if (new_key_frame_no_left == key_frame_no_left)
            {
                // cannot go deeper any more
                can_loop = false;
                break;
            }

            key_frame_no_left = new_key_frame_no_left;
        }
        else
        {
            return query_video_key_frame_no;
        }
    }
    return last_same_gop_key_frame_no;
}

int find_redundancy_begin_query_key_frame_no_cur_scene(int query_video_key_frame_no, int reference_based_on)
{
    int tries = 0;
    double query_video_key_frame_time_second = pts2actualtime(query_video_frames[query_video_key_frame_index[query_video_key_frame_no]].pts, query_video_stream);
    double reference_video_frame_time_second = pts2actualtime(reference_video_frames[reference_based_on].pts, reference_video_stream);

    int current_scene_index = 0;
    scene_segment current_scene = query_video_base_scenes.front();

    int last_same_gop_key_frame_no = query_video_key_frame_no;

    for (int i = 0; i < query_video_base_scenes.size(); i++)
    {
        if (leaf2actualtime(query_video_base_scenes[i].right_leaf_id) > query_video_key_frame_time_second)
        {
            current_scene_index = i;
            current_scene = query_video_base_scenes[i];
            break;
        }
    }

    int left_node_id = current_scene.hierarchy_node_id < query_video_audio_scene_leaf_number ?
        current_scene.hierarchy_node_id : query_video_audio_scene_hierarchy[current_scene.hierarchy_node_id - query_video_audio_scene_leaf_number].first;
    int right_node_id = current_scene.hierarchy_node_id < query_video_audio_scene_leaf_number ?
        current_scene.hierarchy_node_id : query_video_audio_scene_hierarchy[current_scene.hierarchy_node_id - query_video_audio_scene_leaf_number].second;

    assert(left_node_id != -1 && right_node_id != -1);
    scene_segment left_scene = sh_get_scene_segment_by_node_id(left_node_id, query_video_audio_scene_leaf_number, query_video_audio_scene_hierarchy);
    scene_segment right_scene = sh_get_scene_segment_by_node_id(right_node_id, query_video_audio_scene_leaf_number, query_video_audio_scene_hierarchy);
    swap_scene(left_scene, right_scene);
    int key_frame_no_left = get_key_frame_no_near_timestamp(leaf2actualtime(left_scene.right_leaf_id));

    tries = SCENE_SEARCH_TRY;
    while (key_frame_no_left >= query_video_key_frame_no && tries-- > 0)
    {
        left_node_id = left_scene.hierarchy_node_id < query_video_audio_scene_leaf_number ?
            left_scene.hierarchy_node_id : query_video_audio_scene_hierarchy[left_scene.hierarchy_node_id - query_video_audio_scene_leaf_number].first;
        right_node_id = left_scene.hierarchy_node_id < query_video_audio_scene_leaf_number ?
            left_scene.hierarchy_node_id : query_video_audio_scene_hierarchy[left_scene.hierarchy_node_id - query_video_audio_scene_leaf_number].second;

        assert(left_node_id != -1 && right_node_id != -1);
        left_scene = sh_get_scene_segment_by_node_id(left_node_id, query_video_audio_scene_leaf_number, query_video_audio_scene_hierarchy);
        right_scene = sh_get_scene_segment_by_node_id(right_node_id, query_video_audio_scene_leaf_number, query_video_audio_scene_hierarchy);
        swap_scene(left_scene, right_scene);
        key_frame_no_left = get_key_frame_no_near_timestamp(leaf2actualtime(left_scene.right_leaf_id));
    }

    if (key_frame_no_left >= query_video_key_frame_no)
    {
        return query_video_key_frame_no;
    }

    bool can_loop = true;
    while (can_loop)
    {
        if (key_frame_no_left + 1 < query_video_key_frame_number)
        {
            vhash_item query_video_hash_right = get_key_frame_hash(key_frame_no_left + 1);
            //vhash_item reference_video_hash_right = vhash_by_frame_index[query_video_key_frame_index[key_frame_no_left + 1] - query_video_key_frame_index[query_video_key_frame_no] + reference_based_on];

            //int dis_right = get_hamming_distance(query_video_hash_right, reference_video_hash_right);

            //if (dis_right > HAMMING_THRESHOLD * SCALE_HEIGHT * SCALE_WIDTH)
            std::vector<int> matched_frames = find_vhash(query_video_hash_right);
            if (matched_frames.size() <= 0 || !is_time_delta_correct(pts2actualtime(query_video_frames[query_video_key_frame_index[key_frame_no_left + 1]].pts, query_video_stream)
                - pts2actualtime(query_video_frames[query_video_key_frame_index[query_video_key_frame_no]].pts, query_video_stream), matched_frames, reference_based_on))
            {
                // right key frame is not redundant, so search right scene
                left_node_id = right_scene.hierarchy_node_id < query_video_audio_scene_leaf_number ?
                    right_scene.hierarchy_node_id : query_video_audio_scene_hierarchy[right_scene.hierarchy_node_id - query_video_audio_scene_leaf_number].first;
                right_node_id = right_scene.hierarchy_node_id < query_video_audio_scene_leaf_number ?
                    right_scene.hierarchy_node_id : query_video_audio_scene_hierarchy[right_scene.hierarchy_node_id - query_video_audio_scene_leaf_number].second;

                left_scene = sh_get_scene_segment_by_node_id(left_node_id, query_video_audio_scene_leaf_number, query_video_audio_scene_hierarchy);
                right_scene = sh_get_scene_segment_by_node_id(right_node_id, query_video_audio_scene_leaf_number, query_video_audio_scene_hierarchy);
                swap_scene(left_scene, right_scene);

                int new_key_frame_no_left = get_key_frame_no_near_timestamp(leaf2actualtime(left_scene.right_leaf_id));
                tries = SCENE_SEARCH_TRY;
                while ((new_key_frame_no_left >= query_video_key_frame_no || new_key_frame_no_left == key_frame_no_left) && tries-- > 0)
                {
                    left_node_id = left_scene.hierarchy_node_id < query_video_audio_scene_leaf_number ?
                        left_scene.hierarchy_node_id : query_video_audio_scene_hierarchy[left_scene.hierarchy_node_id - query_video_audio_scene_leaf_number].first;
                    right_node_id = left_scene.hierarchy_node_id < query_video_audio_scene_leaf_number ?
                        left_scene.hierarchy_node_id : query_video_audio_scene_hierarchy[left_scene.hierarchy_node_id - query_video_audio_scene_leaf_number].second;

                    assert(left_node_id != -1 && right_node_id != -1);
                    left_scene = sh_get_scene_segment_by_node_id(left_node_id, query_video_audio_scene_leaf_number, query_video_audio_scene_hierarchy);
                    right_scene = sh_get_scene_segment_by_node_id(right_node_id, query_video_audio_scene_leaf_number, query_video_audio_scene_hierarchy);
                    swap_scene(left_scene, right_scene);
                    new_key_frame_no_left = get_key_frame_no_near_timestamp(leaf2actualtime(left_scene.right_leaf_id));
                }

                if (new_key_frame_no_left >= query_video_key_frame_no)
                {
                    return last_same_gop_key_frame_no;
                }

                if (new_key_frame_no_left == key_frame_no_left)
                {
                    can_loop = false;
                    break;
                }

                key_frame_no_left = new_key_frame_no_left;

                continue;
            }

            vhash_item query_video_hash_left = get_key_frame_hash(key_frame_no_left);
            //vhash_item reference_video_hash_left = vhash_by_frame_index[query_video_key_frame_index[key_frame_no_left] - query_video_key_frame_index[query_video_key_frame_no] + reference_based_on];

            //int dis_left = get_hamming_distance(query_video_hash_left, reference_video_hash_left);

            //if (dis_left > HAMMING_THRESHOLD * SCALE_HEIGHT * SCALE_WIDTH)
            matched_frames = find_vhash(query_video_hash_left);
            if (matched_frames.size() <= 0 || !is_time_delta_correct(pts2actualtime(query_video_frames[query_video_key_frame_index[key_frame_no_left]].pts, query_video_stream)
                - pts2actualtime(query_video_frames[query_video_key_frame_index[query_video_key_frame_no]].pts, query_video_stream), matched_frames, reference_based_on))
            {
                // right key frame is redundant, while the left is not
                return key_frame_no_left + 1;
            }

            // left and right key frames are both redundant, search left scene

            last_same_gop_key_frame_no = key_frame_no_left;

            left_node_id = left_scene.hierarchy_node_id < query_video_audio_scene_leaf_number ?
                left_scene.hierarchy_node_id : query_video_audio_scene_hierarchy[left_scene.hierarchy_node_id - query_video_audio_scene_leaf_number].first;
            right_node_id = left_scene.hierarchy_node_id < query_video_audio_scene_leaf_number ?
                left_scene.hierarchy_node_id : query_video_audio_scene_hierarchy[left_scene.hierarchy_node_id - query_video_audio_scene_leaf_number].second;

            left_scene = sh_get_scene_segment_by_node_id(left_node_id, query_video_audio_scene_leaf_number, query_video_audio_scene_hierarchy);
            right_scene = sh_get_scene_segment_by_node_id(right_node_id, query_video_audio_scene_leaf_number, query_video_audio_scene_hierarchy);
            swap_scene(left_scene, right_scene);

            int new_key_frame_no_left = get_key_frame_no_near_timestamp(leaf2actualtime(left_scene.right_leaf_id));
            tries = SCENE_SEARCH_TRY;
            while ((new_key_frame_no_left >= query_video_key_frame_no || new_key_frame_no_left == key_frame_no_left) && tries-- > 0)
            {
                left_node_id = left_scene.hierarchy_node_id < query_video_audio_scene_leaf_number ?
                    left_scene.hierarchy_node_id : query_video_audio_scene_hierarchy[left_scene.hierarchy_node_id - query_video_audio_scene_leaf_number].first;
                right_node_id = left_scene.hierarchy_node_id < query_video_audio_scene_leaf_number ?
                    left_scene.hierarchy_node_id : query_video_audio_scene_hierarchy[left_scene.hierarchy_node_id - query_video_audio_scene_leaf_number].second;

                assert(left_node_id != -1 && right_node_id != -1);
                left_scene = sh_get_scene_segment_by_node_id(left_node_id, query_video_audio_scene_leaf_number, query_video_audio_scene_hierarchy);
                right_scene = sh_get_scene_segment_by_node_id(right_node_id, query_video_audio_scene_leaf_number, query_video_audio_scene_hierarchy);
                swap_scene(left_scene, right_scene);
                new_key_frame_no_left = get_key_frame_no_near_timestamp(leaf2actualtime(left_scene.right_leaf_id));
            }

            if (new_key_frame_no_left >= query_video_key_frame_no)
            {
                return last_same_gop_key_frame_no;
            }

            if (new_key_frame_no_left == key_frame_no_left)
            {
                can_loop = false;
                break;
            }

            key_frame_no_left = new_key_frame_no_left;
        }
        else
        {
            return query_video_key_frame_no;
        }
    }
    return last_same_gop_key_frame_no;
}

int get_key_frame_no_near_timestamp(double actual_time)
{
    for (int i = query_video_key_frame_number - 1; i >= 0; i--)
    {
        if (pts2actualtime(query_video_frames[query_video_key_frame_index[i]].pts, query_video_stream) <= actual_time)
        {
            return i;
        }
    }
    return 0;
}

bool is_frame_black(AVFrame* frame)
{
    int black_pixel = 0;
    for (int i = 0; i < frame->height; i++)
    {
        for (int j = 0; j < frame->width; j++)
        {
            if (frame->data[0][i * frame->width + j] <= VIDEO_BLACK_PIXEL_THRESHOLD)
            {
                black_pixel++;
            }
        }
    }

    if (black_pixel >= VIDEO_BLACK_FRAME_THRESHOLD * frame->height * frame->width)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool is_frame_silent(AVFrame* frame)
{
    double rms = 0;
    int n = 1024;
    float* d1 = (float*)frame->data[0];
    float* d2 = (float*)frame->data[1];
    for (int i = 0; i < n; i++)
    {
        rms += ((d1[0] + d2[1]) / 2) * ((d1[0] + d2[1]) / 2);
    }

    rms /= n;
    rms = sqrt(rms);
    //printf("%f %f\n", rms, (double)frame->pts * (double)audio_stream->time_base.num / audio_stream->time_base.den);
    if (rms <= AUDIO_SILENT_RMS_THRESHOLD)
    {
        return true;
    }
    else
    {
        return false;
    }
}

std::vector<int> find_vhash(const vhash_item& item)
{
    if (item.frame_index >= 0 && query_video_vhash_compare_cache.find(item.frame_index) != query_video_vhash_compare_cache.end())
    {
        return query_video_vhash_compare_cache[item.frame_index];
    }
    std::vector<int> result = mih_query(item);
    if (item.frame_index >= 0)
    {
        query_video_vhash_compare_cache[item.frame_index] = result;
    }
    return result;
}

void concat_interval()
{
    std::vector<redundant_clip> new_clips;

    int ptr = 0;

    while (ptr < redundant_clips.size())
    {
        redundant_clip p = redundant_clips[ptr];
        ptr++;

        while (ptr < redundant_clips.size())
        {
            if (redundant_clips[ptr].query_video_begin_frame - redundant_clips[ptr - 1].query_video_end_frame < 30
                && redundant_clips[ptr].reference_video_begin_frame - redundant_clips[ptr - 1].reference_video_end_frame < 30)
            {
                p.query_video_end_frame = redundant_clips[ptr].query_video_end_frame;
                p.reference_video_end_frame = redundant_clips[ptr].reference_video_end_frame;
                ptr++;
            }
            else
            {
                break;
            }
        }

        p.reference_video_begin_frame = max(0, p.reference_video_end_frame - (p.query_video_end_frame - p.query_video_begin_frame));
        new_clips.push_back(p);
    }
    redundant_clips = new_clips;
}

void show_results()
{
    FILE* result_fp = fopen(out_report_path, "w");
    fprintf(result_fp, "time: %.5f s\n", (double)(run_end_time - run_begin_time) / CLOCKS_PER_SEC);
    fprintf(result_fp, "(query_frame_start,query_frame_end,query_time_start,query_time_end,ref_frame_start,reference_frame_end,reference_time_start,reference_time_end)\n");

    printf("\n[result]\n");
    for (int i = 0; i < redundant_clips.size(); i++)
    {
        if (redundant_clips[i].query_video_begin_frame < 0 || redundant_clips[i].query_video_end_frame < 0 ||
            redundant_clips[i].reference_video_begin_frame < 0 || redundant_clips[i].reference_video_end_frame < 0)
        {
            redundant_clips.erase(redundant_clips.begin() + i);
            i--;
            continue;
        }
        printf("\n");
        printf("\tquery video frame:\n\t\t%d - %d, timestamp: %.3f - %.3f\n",
            redundant_clips[i].query_video_begin_frame + 1,
            redundant_clips[i].query_video_end_frame + 1,
            pts2actualtime(query_video_frames[redundant_clips[i].query_video_begin_frame].pts, query_video_stream),
            pts2actualtime(query_video_frames[redundant_clips[i].query_video_end_frame].pts, query_video_stream));

        printf("\treference video frame:\n\t\t%d - %d, timestamp: %.3f - %.3f\n",
            redundant_clips[i].reference_video_begin_frame + 1,
            redundant_clips[i].reference_video_end_frame + 1,
            pts2actualtime(reference_video_frames[redundant_clips[i].reference_video_begin_frame].pts, reference_video_stream),
            pts2actualtime(reference_video_frames[redundant_clips[i].reference_video_end_frame].pts, reference_video_stream));

        fprintf(result_fp, "%d %d %.3f %.3f %d %d %.3f %.3f\n",
            redundant_clips[i].query_video_begin_frame + 1,
            redundant_clips[i].query_video_end_frame + 1,
            pts2actualtime(query_video_frames[redundant_clips[i].query_video_begin_frame].pts, query_video_stream),
            pts2actualtime(query_video_frames[redundant_clips[i].query_video_end_frame].pts, query_video_stream),
            redundant_clips[i].reference_video_begin_frame + 1,
            redundant_clips[i].reference_video_end_frame + 1,
            pts2actualtime(reference_video_frames[redundant_clips[i].reference_video_begin_frame].pts, reference_video_stream),
            pts2actualtime(reference_video_frames[redundant_clips[i].reference_video_end_frame].pts, reference_video_stream));
    }

    fprintf(result_fp, "query_frame_number: %d\n", query_video_frame_number);
    fprintf(result_fp, "examined_frame_number: %d\n", frame_examined_number + query_video_vhash_compare_cache.size());
    fprintf(result_fp, "query_key_frame_number: %d\n", query_video_key_frame_number);
    fprintf(result_fp, "examined_key_frame_number: %d\n", query_video_vhash_compare_cache.size());
    fprintf(result_fp, "reference_frame_number: %d\n", reference_video_frame_number);

    printf("\n[time]\n");
    printf("\telapsed time: %.3f s\n", (double)(run_end_time - run_begin_time) / CLOCKS_PER_SEC);

    printf("\n[evaluation]\n");

    std::vector<std::pair<int, int>> v_ground_truth; // local file true redundancy ranges (l-r inclusive)

    FILE* fp = fopen(ground_truth_path, "r+");

    int s, t;

    if (fp != NULL)
    {
        while ((fscanf(fp, "%d", &s)) != EOF)
        {
            fscanf(fp, "%d", &t);
            s--;
            t--;
            v_ground_truth.push_back(std::make_pair(max(0, s), min((int)query_video_frame_number - 1, t)));

            char c;
            fscanf(fp, "%c", &c);
            fscanf(fp, "%c", &c);
            fscanf(fp, "%c", &c);
        }

        fclose(fp);
    }
    else
    {
        printf("ground truth not found.\n");
    }

    printf("\tredundancy ground truth:\n");

    for (int i = 0; i < v_ground_truth.size(); i++)
    {
        printf("\t\tquery video frame: %d - %d, timestamp: %.3f - %.3f\n",
            v_ground_truth[i].first + 1, v_ground_truth[i].second + 1,
            pts2actualtime(query_video_frames[v_ground_truth[i].first].pts, query_video_stream),
            pts2actualtime(query_video_frames[v_ground_truth[i].second].pts, query_video_stream));
    }

    // true positive
    std::vector<std::pair<int, int>> true_positive = get_overlapped_frame(v_ground_truth);

    // false positive
    std::vector<std::pair<int, int>> false_positive = get_non_overlapped_frame(v_ground_truth);


    printf("\n");
    printf("\tkey frame number:           %15u\n", query_video_key_frame_number);
    //printf("\tI frame access:          %15d\n", video_frame_access_record.size());

    int fp_cnt = 0, tp_cnt = 0;
    for (int i = 0; i < true_positive.size(); i++)
        tp_cnt += true_positive[i].second - true_positive[i].first + 1;
    for (int i = 0; i < false_positive.size(); i++)
        fp_cnt += false_positive[i].second - false_positive[i].first + 1;

    printf("\n");
    printf("\ttrue positive number:     %15d\n", tp_cnt);
    printf("\tfalse positive number:    %15d\n", fp_cnt);
    printf("\tfalse positive duration: %13.2f s\n", fp_cnt / 25.0);
    //printf("\tcontrol size:			%12.2f KB\n", control_sz / 1024.0);

    //printf("\ttraffic on redundancy: %12.2f KB\n", traffic_on_false_positive_and_control / 1024.0);
    //printf("\tredundancy elimination:%13.2f %%\n", 100.0 * (1 - (double)traffic_on_redundancy / redundancy_sz));

    // EVAL == redundancy elimination

    // precision - recall

    int truth_cnt = 0, res_cnt = 0;

    for (int i = 0; i < v_ground_truth.size(); i++)
        truth_cnt += v_ground_truth[i].second - v_ground_truth[i].first + 1;

    for (int i = 0; i < redundant_clips.size(); i++)
        res_cnt += redundant_clips[i].query_video_end_frame - redundant_clips[i].query_video_begin_frame + 1;

    double precision = (double)tp_cnt / res_cnt;
    double recall = (double)tp_cnt / truth_cnt;

    printf("\n");
    printf("\tprecision:               %13.2f %%\n", precision * 100);
    printf("\trecall:                  %13.2f %%\n", recall * 100);
    printf("\tf1-score:                %15.4f\n", 2 * precision * recall / (precision + recall));

    printf("\n[all done]\n");
}

std::vector<std::pair<int, int>> get_overlapped_frame(const std::vector<std::pair<int, int>>& ground_truth)
{
    std::vector<std::pair<int, int>> res;

    for (int i = 0; i < ground_truth.size(); i++)
    {
        for (int j = 0; j < redundant_clips.size(); j++)
        {
            unsigned int s = max(ground_truth[i].first, redundant_clips[j].query_video_begin_frame);
            unsigned int t = min(ground_truth[i].second, redundant_clips[j].query_video_end_frame);
            if (s <= t)
                res.push_back(std::make_pair(s, t));
        }
    }

    return res;
}

std::vector<std::pair<int, int>> get_non_overlapped_frame(const std::vector<std::pair<int, int>>& ground_truth)
{
    bool* b = new bool[query_video_frame_number + 1];
    std::fill_n(b, query_video_frame_number + 1, false);
    std::vector<std::pair<int, int>> res;
    int from = query_video_frame_number + 1, to = 0;

    for (int i = 0; i < redundant_clips.size(); i++)
    {
        from = min(from, redundant_clips[i].query_video_begin_frame);
        to = max(to, redundant_clips[i].query_video_end_frame);
        for (int j = redundant_clips[i].query_video_begin_frame; j <= redundant_clips[i].query_video_end_frame; j++)
            b[j] = true;
    }

    for (int i = 0; i < ground_truth.size(); i++)
    {
        from = min(from, ground_truth[i].first);
        to = max(to, ground_truth[i].second);
        for (int j = ground_truth[i].first; j <= ground_truth[i].second; j++)
            b[j] = false;
    }


    unsigned int s, t;
    int i = from;
    while (i <= to)
    {
        if (!b[i])
        {
            i++;
            continue;
        }

        s = i;

        while (i <= to && b[i])
            i++;

        t = i - 1;

        res.push_back(std::make_pair(s, t));
    }

    delete[] b;

    return res;
}

bool is_time_delta_correct(double query_time_delta, const std::vector<int> similar_frames, int renference_based_on)
{
    for (int i = 0; i < similar_frames.size(); i++)
    {
        double delta = abs((pts2actualtime(reference_video_frames[similar_frames[i]].pts, reference_video_stream)
            - pts2actualtime(reference_video_frames[renference_based_on].pts, reference_video_stream)) - query_time_delta);
        if (delta <= TIME_DELTA_THRESHOLD)
            return true;
    }
    return false;
}

void swap_scene(scene_segment& left, scene_segment& right)
{
    if (left.right_leaf_id > right.left_leaf_id)
    {
        std::swap(left, right);
    }
}
