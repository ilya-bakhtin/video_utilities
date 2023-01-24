#include "stdafx.h"

#include "MaskUpdate.h"

#include <vector>

#include <stdio.h>

struct WaveEntry
{
    int x_;
    int y_;
};

class MaskUpdate : public GenericVideoFilter
{
public:
    MaskUpdate(PClip _child, int mask_w, int mask_h, IScriptEnvironment* env);
    virtual ~MaskUpdate();

    PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);

private:
    int x_limit_;
    int y_limit_;

    mutable std::vector<int> vizited_;
    mutable std::vector<std::pair<int, int> > vizited_p2_;

    mutable std::vector<bool> vizited_w_;
    mutable std::vector<size_t> vizited_p2_w_;

    bool analyze_neighbours(int x, int y, PVideoFrame& src, int level) const;
    bool analyze_neighbours_p2(int x, int y, PVideoFrame& src, int level, int& min_x, int& max_y, int dir, FILE* f);
    void minmax_to_neighbours(int x, int y, PVideoFrame& src, int min_x, int max_y);

    bool analyze_neighbours_wave(PVideoFrame& src, std::vector<WaveEntry>& wave, bool is_black /* area is already detected as black */) const;
    size_t analyze_neighbours_p2_wave(PVideoFrame& src, std::vector<WaveEntry>& wave, size_t prev_size) const;
};

MaskUpdate::MaskUpdate(PClip _child, int mask_w, int mask_h, IScriptEnvironment* env) :
    GenericVideoFilter(_child),
    x_limit_(mask_w),
    y_limit_(mask_h)
{
    vizited_.resize(x_limit_*y_limit_);
    vizited_w_.resize(x_limit_*y_limit_);
    vizited_p2_.resize(x_limit_*y_limit_);
    vizited_p2_w_.resize(x_limit_*y_limit_);
}

MaskUpdate::~MaskUpdate()
{
}

struct PointOffset
{
    int offs_x_;
    int offs_y_;
};

static const PointOffset point_offsets[8] = {
    { 1,  0},
    { 1,  1},
    { 0,  1},
    {-1, -1},
    {-1,  0},
    {-1, -1},
    { 0, -1},
    { 1, -1},
};

bool MaskUpdate::analyze_neighbours_wave(PVideoFrame& dst, std::vector<WaveEntry>& wave, bool is_black /* area is already detected as black */) const
{
    if (!vi.IsRGB32())
        return false;

    const int height = dst->GetHeight();
    std::vector<WaveEntry> w;

    for (std::vector<WaveEntry>::iterator iw = wave.begin(); iw != wave.end(); ++iw)
    {
        const int cx = iw->x_;
        const int cy = iw->y_;

        for (size_t i = 0; i < sizeof(point_offsets)/sizeof(point_offsets[0]); ++i)
        {
            const int x = cx + point_offsets[i].offs_x_;
            const int y = cy + point_offsets[i].offs_y_;

            if (x < 0 || y >= height)
                continue; // left or top border. implicitly white.

            if (x >= x_limit_ || y < height - y_limit_) // right or bottom border. implicitly black. if we get here then whole area is black.
            {
                is_black = true;
                continue;
            }

            const int block_y = y - (height - y_limit_);

            if (!vizited_w_[x + block_y*x_limit_])
            {
/*
FILE* f = fopen("C:\\Users\\ilx\\wrk\\video\\Super8\\log.txt", "a");
if (f != nullptr)
{
    fprintf(f, "* %d, %d (%d)\n", x, block_y, y);
    fflush(f); fclose(f);
}
*/
                vizited_w_[x + block_y*x_limit_] = true;

                const unsigned char* const dstp = dst->GetReadPtr() + dst->GetPitch() * y;
                if (dstp[x*4 + 0] == 0 && dstp[x*4 + 1] == 0 && dstp[x*4 + 2] == 0)
                {
                    WaveEntry e = {x, y};
                    w.push_back(e);
                }
            }
        }
    }

    const bool res = w.empty() ? !is_black : analyze_neighbours_wave(dst, w, is_black);

    if (res)
    {
        for (std::vector<WaveEntry>::iterator iw = wave.begin(); iw != wave.end(); ++iw)
        {
            const int x = iw->x_;
            const int y = iw->y_;

            unsigned char* const dstp = dst->GetWritePtr() + dst->GetPitch() * y;
            dstp[x*4 + 0] = 255;
            dstp[x*4 + 1] = 255;
            dstp[x*4 + 2] = 255;
            dstp[x*4 + 3] = 255;
        }
    }

    return res;
}

size_t MaskUpdate::analyze_neighbours_p2_wave(PVideoFrame& dst, std::vector<WaveEntry>& wave, size_t prev_size) const
{
    if (!vi.IsRGB32())
        return 0;

    if (wave.empty())
        return prev_size;

    const int height = dst->GetHeight();
    std::vector<WaveEntry> w;

    for (std::vector<WaveEntry>::iterator iw = wave.begin(); iw != wave.end(); ++iw)
    {
        const int cx = iw->x_;
        const int cy = iw->y_;

        for (size_t i = 0; i < sizeof(point_offsets)/sizeof(point_offsets[0]); ++i)
        {
            const int x = cx + point_offsets[i].offs_x_;
            const int y = cy + point_offsets[i].offs_y_;

            if (x < 0 || y >= height || x >= x_limit_ || y < height - y_limit_) // border is reached.
                continue;

            const int block_y = y - (height - y_limit_);

            if (vizited_p2_w_[x + block_y*x_limit_] == 0)
            {
                vizited_p2_w_[x + block_y*x_limit_] = 1;

                const unsigned char* const dstp = dst->GetReadPtr() + dst->GetPitch() * y;
                if (dstp[x*4 + 0] != 0 || dstp[x*4 + 1] != 0 || dstp[x*4 + 2] != 0)
                {
                    WaveEntry e = {x, y};
                    w.push_back(e);
                }
            }
        }
    }

    const size_t res = analyze_neighbours_p2_wave(dst, w, prev_size + w.size());

    for (std::vector<WaveEntry>::iterator iw = wave.begin(); iw != wave.end(); ++iw)
    {
        const int x = iw->x_;
        const int y = iw->y_;
        const int block_y = y - (height - y_limit_);

        vizited_p2_w_[x + block_y*x_limit_] = res + 1;
    }

    return res;
}

bool MaskUpdate::analyze_neighbours(int x, int y, PVideoFrame& src, int level) const
{
    if (!vi.IsRGB32())
        return false;
/*
FILE* f = fopen("D:\\Users\\ilx\\wrk\\video\\Super8\\1983\\1983_720_133\\log.txt", "a");
if (f != nullptr)
{
    fprintf(f, "%d, %d, %d", level, x, y);
}
*/
    const int height = src->GetHeight();

    if (x < 0 || y < 0 || y >= height || x >= src->GetRowSize()/4)
    {
//        fprintf(f, ", out true\n");
//        fclose(f);
        return true;
    }

    if (x >= x_limit_ || y < height - y_limit_)
    {
//        fprintf(f, ", false\n");
//        fclose(f);
        return false;
    }

    const int v = vizited_[x + (y - (height - y_limit_))*x_limit_];
    if (v != -1)
    {
//        fprintf(f, ", vizited true\n");
//        fclose(f);
        return v != 0;
    }

    const unsigned char* const srcp = src->GetReadPtr() + src->GetPitch() * y;
    if (srcp[x*4 + 0] != 0 || srcp[x*4 + 1] != 0 || srcp[x*4 + 2] != 0)
    {
//        fprintf(f, ", white true\n");
//        fclose(f);
        vizited_[x + (y - (height - y_limit_))*x_limit_] = true;
        return true;
    }

    vizited_[x + (y - (height - y_limit_))*x_limit_] = true;

//    fprintf(f, ", deeper");

    const bool res = analyze_neighbours(x - 1, y + 1, src, level + 1) &&
                     analyze_neighbours(x, y + 1, src, level + 1) &&
                     analyze_neighbours(x + 1, y + 1, src, level + 1) &&
                     analyze_neighbours(x - 1, y, src, level + 1) &&
                     analyze_neighbours(x + 1, y, src, level + 1) &&
                     analyze_neighbours(x - 1, y - 1, src, level + 1) &&
                     analyze_neighbours(x, y - 1, src, level + 1) &&
                     analyze_neighbours(x + 1, y - 1, src, level + 1);

    vizited_[x + (y - (height - y_limit_))*x_limit_] = res;

//    fprintf(f, ", result %d\n", res);
//    fclose(f);

    return res;
}

void MaskUpdate::minmax_to_neighbours(int x, int y, PVideoFrame& src, int min_x, int max_y)
{
    const int height = src->GetHeight();

    if (x < 0 || y < 0 || y >= height || x >= src->GetRowSize()/4)
        return;

    if (x >= x_limit_ || y < height - y_limit_)
        return;

    const unsigned char* const srcp = src->GetReadPtr() + src->GetPitch() * y;
    if (srcp[x*4 + 0] == 0 && srcp[x*4 + 1] == 0 && srcp[x*4 + 2] == 0)
        return;

    const size_t viz_idx = x + (y - (height - y_limit_))*x_limit_;
    int& viz_mx = vizited_p2_[viz_idx].first;
    int& viz_my = vizited_p2_[viz_idx].second;

    viz_mx = min_x;
    viz_my = max_y;
}

bool MaskUpdate::analyze_neighbours_p2(int x, int y, PVideoFrame& src, int level, int& min_x, int& max_y, int dir, FILE* f)
{
    if (!vi.IsRGB32())
        return false;

    if (f != nullptr)
    {
        fprintf(f, "%d, %d, %d", level, x, y);
        fflush(f);
    }
    const int height = src->GetHeight();

    if (x < 0 || y < 0 || y >= height || x >= src->GetRowSize()/4)
    {
        if (f != nullptr)
        {
            fprintf(f, ", out true\n");
            fflush(f);
        }
        return true;
    }

    if (x >= x_limit_ || y < height - y_limit_)
    {
        if (f != nullptr)
        {
            fprintf(f, ", limit true\n");
            fflush(f);
        }
        return true;
    }

    const unsigned char* const srcp = src->GetReadPtr() + src->GetPitch() * y;
    if (srcp[x*4 + 0] == 0 && srcp[x*4 + 1] == 0 && srcp[x*4 + 2] == 0)
    {
        if (f != nullptr)
        {
            fprintf(f, ", black true\n");
            fflush(f);
        }
        return true;
    }

    const size_t viz_idx = x + (y - (height - y_limit_))*x_limit_;
    int& viz_mx = vizited_p2_[viz_idx].first;
    int& viz_my = vizited_p2_[viz_idx].second;

    if (viz_mx == -1 || viz_my == -1)
    {
        viz_mx = min_x;
        viz_my = max_y;
    }
    else
    {
        if (f != nullptr)
        {
            if (viz_mx == 0 && viz_my == height - 1)
                fprintf(f, ", vizited false, %d %d\n", viz_mx, viz_my);
            else
                fprintf(f, ", vizited true, %d %d\n", viz_mx, viz_my);
            fflush(f);
        }
        if (min_x > viz_mx)
            min_x = viz_mx;
        if (max_y < viz_my)
            max_y = viz_my;
        
        return viz_mx != 0 || viz_my != height - 1;
    }

    if (x == 0 && y == height - 1)
    {
        if (f != nullptr)
        {
            fprintf(f, ", reached false\n");
            fflush(f);
        }
        min_x = 0;
        max_y = height - 1;
        viz_mx = min_x;
        viz_my = max_y;
        return false;
    }

    if (min_x > x)
        min_x = x;
    if (max_y < y)
        max_y = y;

    if (viz_mx == -1 || viz_mx > min_x)
        viz_mx = min_x;
    if (viz_my == -1 || viz_my < max_y)
        viz_my = max_y;

    if (level > 6000)
    {
        if (f != nullptr)
        {
            fprintf(f, ", level limit %d, %d %d\n", level, min_x, max_y);
            fflush(f);
        }
        return true;
    }

    if (f != nullptr)
    {
        fprintf(f, ", deeper %d %d\n", min_x, max_y);
        fflush(f);
    }

    const bool res =
                     (dir == 4 || analyze_neighbours_p2(x - 1, y, src, level + 1, min_x, max_y, 0, f)) &&
                     (dir == 3 || analyze_neighbours_p2(x - 1, y + 1, src, level + 1, min_x, max_y, 7, f)) &&
                     (dir == 2 || analyze_neighbours_p2(x, y + 1, src, level + 1, min_x, max_y, 6, f)) &&
                     (dir == 1 || analyze_neighbours_p2(x + 1, y + 1, src, level + 1, min_x, max_y, 5, f)) &&
                     (dir == 0 || analyze_neighbours_p2(x + 1, y, src, level + 1, min_x, max_y, 4, f)) &&
                     (dir == 7 || analyze_neighbours_p2(x + 1, y - 1, src, level + 1, min_x, max_y, 3, f)) &&
                     (dir == 6 || analyze_neighbours_p2(x, y - 1, src, level + 1, min_x, max_y, 2, f)) &&
                     (dir == 5 || analyze_neighbours_p2(x - 1, y - 1, src, level + 1, min_x, max_y, 1, f));

    if (viz_mx > min_x)
        viz_mx = min_x;
    else
        min_x = viz_mx;

    if (viz_my < max_y)
        viz_my = max_y;
    else
        max_y = viz_my;
/*
    minmax_to_neighbours(x - 1, y, src, min_x, max_y);
    minmax_to_neighbours(x - 1, y - 1, src, min_x, max_y);
    minmax_to_neighbours(x, y - 1, src, min_x, max_y);
    minmax_to_neighbours(x + 1, y - 1, src, min_x, max_y);
    minmax_to_neighbours(x + 1, y, src, min_x, max_y);
    minmax_to_neighbours(x + 1, y + 1, src, min_x, max_y);
    minmax_to_neighbours(x, y + 1, src, min_x, max_y);
    minmax_to_neighbours(x - 1, y + 1, src, min_x, max_y);
*/
    if (f != nullptr)
    {
        fprintf(f, "%d, %d, %d, result %d %d\n", level, x, y, min_x, max_y);
        fflush(f);
    }

    return res;
}

PVideoFrame __stdcall MaskUpdate::GetFrame(int n, IScriptEnvironment* env)
{
    PVideoFrame src = child->GetFrame(n, env);
    PVideoFrame dst = env->NewVideoFrame(vi);

    const unsigned char* const srcp_org = src->GetReadPtr();
    const unsigned char* srcp = srcp_org;
    // Request a Read pointer from the source frame.
    // This will return the position of the upperleft pixel in YUY2 images,
    // and return the lower-left pixel in RGB.
    // RGB images are stored upside-down in memory. 
    // You should still process images from line 0 to height.

    unsigned char* const dstp_org = dst->GetWritePtr();
    unsigned char* dstp = dstp_org;
    // Request a Write pointer from the newly created destination image.
    // You can request a writepointer to images that have just been
    // created by NewVideoFrame. If you recieve a frame from PClip->GetFrame(...)
    // you must call env->MakeWritable(&frame) be recieve a valid write pointer.

    const int dst_pitch = dst->GetPitch();
    // Requests pitch (length of a line) of the destination image.
    // For more information on pitch see: http://www.avisynth.org/index.php?page=WorkingWithImages
    // (short version - pitch is always equal to or greater than width to allow for seriously fast assembly code)

    const int dst_width = dst->GetRowSize();
    // Requests rowsize (number of used bytes in a line.
    // See the link above for more information.

    const int dst_height = dst->GetHeight();
    // Requests the height of the destination image.

    const int src_pitch = src->GetPitch();
    const int src_width = src->GetRowSize();
    const int src_height = src->GetHeight();

    if (vi.IsRGB32())
    {
        memcpy(dstp, srcp, src_height*src_pitch);
        if (dst_width/4 < x_limit_ || dst_height < y_limit_)
            return dst;

        srcp += (src_height - y_limit_)*src_pitch;
        dstp += (dst_height - y_limit_)*dst_pitch;

        for (size_t i = 0; i < vizited_.size(); ++i)
            vizited_[i] = -1;

        unsigned char* const dstp_saved = dstp;
        for (int y = dst_height - y_limit_; y < dst_height; ++y)
        {
            for (int x = 0; x < x_limit_; ++x)
            {
                const bool black = dstp[x*4 + 0] == 0 && dstp[x*4 + 1] == 0 && dstp[x*4 + 2] == 0;
                if (!black)
                {
                    dstp[x*4 + 0] = 255;
                    dstp[x*4 + 1] = 255;
                    dstp[x*4 + 2] = 255;
                    dstp[x*4 + 3] = 255;
                }

                const int block_y = y - (dst_height - y_limit_);
                vizited_w_[x + block_y*x_limit_] = !black;
            }
            dstp += dst_pitch; // Add the pitch to the destination pointer.
        }

        dstp = dstp_saved;

        for (int y = dst_height - y_limit_; y < dst_height; ++y)
        {
            for (int x = 0; x < x_limit_; ++x)
            {
#if 0
                if (analyze_neighbours(x, y, src, 0))
                {
                    dstp[x*4 + 0] = 255;
                    dstp[x*4 + 1] = 255;
                    dstp[x*4 + 2] = 255;
                    dstp[x*4 + 3] = 255;
                }
#else
                const int block_y = y - (dst_height - y_limit_);
                if (!vizited_w_[x + block_y*x_limit_])
                {
/*
FILE* f = fopen("C:\\Users\\ilx\\wrk\\video\\Super8\\log.txt", "a");
if (f != nullptr)
{
    fprintf(f, "************ %d, %d (%d) - \n", x, block_y, y);
    fflush(f); fclose(f);
}
*/
                    WaveEntry e = {x, y};
                    std::vector<WaveEntry> w;
                    w.push_back(e);
                    const bool res = analyze_neighbours_wave(dst, w, false);
/*
f = fopen("C:\\Users\\ilx\\wrk\\video\\Super8\\log.txt", "a");
if (f != nullptr)
{
    fprintf(f, "%s\n", (res ? "white" : "black"));
    fflush(f); fclose(f);
}
*/
                }
#endif
            }
            srcp += src_pitch; // Add the pitch (note use of pitch and not width) of one line (in bytes) to the source pointer
            dstp += dst_pitch; // Add the pitch to the destination pointer.
        }

#if 0
        srcp = srcp_org + (src_height - 1)*src_pitch;
        dstp = dstp_org + (dst_height - 1)*dst_pitch;

        for (size_t i = 0; i < vizited_p2_.size(); ++i)
        {
            vizited_p2_[i].first = -1;
            vizited_p2_[i].second = -1;
        }

        FILE* f = nullptr; //fopen("D:\\Users\\ilx\\wrk\\video\\Super8\\1983\\1983_720_133\\log.txt", "a");
        for (int y = dst_height - 1; y >= dst_height - y_limit_; --y)
        {
            for (int x = 0; x < x_limit_; ++x)
            {
                if (f != nullptr)
                {
                    fprintf(f, "************ %d, %d\n", x, y);
                    fflush(f);
                }
                int min_x = INT_MAX;
                int max_y = INT_MIN;
                analyze_neighbours_p2(x, y, dst, 0, min_x, max_y, -1, f);

                if (f != nullptr)
                {
                    fprintf(f, "************ res %d, %d\n", min_x, max_y);
                    fflush(f);
                }
                if (min_x != 0 || max_y != dst_height - 1)
                {
                    dstp[x*4 + 0] = 0;
                    dstp[x*4 + 1] = 0;
                    dstp[x*4 + 2] = 0;
                    dstp[x*4 + 3] = 255;
                }
            }
            srcp -= src_pitch; // Add the pitch (note use of pitch and not width) of one line (in bytes) to the source pointer
            dstp -= dst_pitch; // Add the pitch to the destination pointer.
        }
        if (f != nullptr)
            fclose(f);
#else
        dstp = dstp_saved;

        for (int y = dst_height - y_limit_; y < dst_height; ++y)
        {
            for (int x = 0; x < x_limit_; ++x)
            {
                const int block_y = y - (dst_height - y_limit_);
                vizited_p2_w_[x + block_y*x_limit_] =
                    dstp[x*4 + 0] == 0 && dstp[x*4 + 1] == 0 && dstp[x*4 + 2] == 0 ? 1 : 0;
            }
            dstp += dst_pitch; // Add the pitch to the destination pointer.
        }

        dstp = dstp_saved;

        size_t max_res = 0;

        for (int y = dst_height - y_limit_; y < dst_height; ++y)
        {
            for (int x = 0; x < x_limit_; ++x)
            {
                const int block_y = y - (dst_height - y_limit_);
                if (vizited_p2_w_[x + block_y*x_limit_] == 0)
                {
                    WaveEntry e = {x, y};
                    std::vector<WaveEntry> w;
                    w.push_back(e);
                    const size_t res = analyze_neighbours_p2_wave(dst, w, 1);
                    if (max_res < res)
                        max_res = res;
                }
            }
            dstp += dst_pitch; // Add the pitch to the destination pointer.
        }

        dstp = dstp_saved;

        for (int y = dst_height - y_limit_; y < dst_height; ++y)
        {
            for (int x = 0; x < x_limit_; ++x)
            {
                const int block_y = y - (dst_height - y_limit_);
                const size_t val = vizited_p2_w_[x + block_y*x_limit_];
                if (val != 1 && val != max_res + 1)
                {
                    dstp[x*4 + 0] = 0;
                    dstp[x*4 + 1] = 0;
                    dstp[x*4 + 2] = 0;
                    dstp[x*4 + 3] = 255;
                }
            }
            dstp += dst_pitch; // Add the pitch to the destination pointer.
        }
#endif
    }

    return dst;
}

AVSValue __cdecl Create_MaskUpdate(AVSValue args, void* user_data, IScriptEnvironment* env)
{
    return new MaskUpdate(
                args[0].AsClip(), // the 0th parameter is the source clip
                args[1].AsInt(128), // mask W
                args[2].AsInt(128), // mask H
                env);  
}
