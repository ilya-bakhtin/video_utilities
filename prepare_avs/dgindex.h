#ifndef __DGINDEX_H__
#define __DGINDEX_H__

#include <string>
#include <vector>

#include "tchar.h"

#ifdef UNICODE
#define tstring wstring
#else
#define tstring string
#endif

class DgindexScanDir
{
public:
    DgindexScanDir(const _TCHAR* dir);
    virtual ~DgindexScanDir();

    void scan(TCHAR* ext, bool audio);
    void save_dgi_render(int type);
    void save_avs(int type);

    void save_dgindex_avs();
    void save_video_avs(const _TCHAR* filename);
    void save_audio_avs(const _TCHAR* filename);

protected:
    std::string to_utf8(const _TCHAR* wname);

private:
    std::vector<std::string> files_;
    std::vector<std::string> aud_files_;
    std::tstring dir_;
};

#endif
