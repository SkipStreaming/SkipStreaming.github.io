#pragma once
struct redundant_clip
{
    int query_video_begin_frame;
    int query_video_end_frame;
    //int query_audio_begin_frame;
    //int query_audio_end_frame;

    int reference_video_begin_frame;
    int reference_video_end_frame;
    //int reference_audio_begin_frame;
    //int reference_audio_end_frame;

    bool is_from_partial_gop;
};

struct video_frame_packet
{
    __int64 pts;
    __int64 dts;
    int is_key_frame;
    __int64 duration;
    __int64 pos;
};

bool sort_video_frame_packet_by_pts(const video_frame_packet& p1, const video_frame_packet& p2)
{
    if (p1.pts != p2.pts)
    {
        return p1.pts < p2.pts;
    }
    else if (p1.dts != p2.dts)
    {
        return p1.dts < p2.dts;
    }
    else
    {
        return p1.pos < p2.pos;
    }
}
