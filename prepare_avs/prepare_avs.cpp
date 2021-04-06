// prepare_avs.cpp : Defines the entry point for the console application.
//

#include "dgindex.h"
#include "s8-avs-creator.h"

#include "algorithm"
#include "fstream"
#include "functional"
#include "iostream"
#include "memory"
#include "string"
#include "vector"

#include "tchar.h"
#include "windows.h"

class ScanDir
{
public:
    ScanDir(const _TCHAR* dir, const _TCHAR* ext);
    virtual ~ScanDir();

    void save_partial_video_avs();
    void save_video_avs(const _TCHAR* filename);
    void save_audio_avs(const _TCHAR* filename);

protected:
    std::string to_utf8(const _TCHAR* wname);

private:
    static const bool is_x86 = false;

    ScanDir(const ScanDir& other);
    ScanDir(const ScanDir&& other);
    virtual ScanDir& operator=(const ScanDir& other);
    virtual ScanDir& operator=(const ScanDir&& other);

    std::vector<std::string> files_;
    std::string dir_;
};

std::string ScanDir::to_utf8(const _TCHAR* wname)
{
    int len = WideCharToMultiByte(CP_UTF8, 0, wname, -1, nullptr, 0, nullptr, nullptr) + 1;
    std::vector<char> utf8(len);
    WideCharToMultiByte(CP_UTF8, 0, wname, -1, &utf8[0], len, nullptr, nullptr);
    return &utf8[0]; 
}

ScanDir::ScanDir(const _TCHAR* dir_name, const _TCHAR* ext)
{
    WIN32_FIND_DATA FindFileData;
    HANDLE hFind;

    files_.clear();
    dir_ = to_utf8(dir_name);

    hFind = FindFirstFile((std::tstring(dir_name) + _T("/") + ext).c_str(), &FindFileData);
    std::unique_ptr<HANDLE, std::function<void (HANDLE*)> > close_guard(&hFind, [](HANDLE* h){if (*h != INVALID_HANDLE_VALUE) FindClose(*h);});

    if (hFind == INVALID_HANDLE_VALUE)
        printf ("FindFirstFile failed (%d)\n", GetLastError());
    else
    {
        _tprintf(_T("The first file found is %s\n"), FindFileData.cFileName);
        files_.push_back(to_utf8(FindFileData.cFileName));
        for (; FindNextFile(hFind, &FindFileData);)
        {
            _tprintf(_T("The next file found is %s\n"), FindFileData.cFileName);
            files_.push_back(to_utf8(FindFileData.cFileName));
        }
    }

    std::sort(files_.begin(), files_.end());

    for (std::vector<std::string>::const_iterator i = files_.begin(); i < files_.end(); ++i)
        printf("%s\n", i->c_str());
}

ScanDir::~ScanDir()
{
}

ScanDir& ScanDir::operator=(const ScanDir&& other)
{
    return *this;
}

ScanDir& ScanDir::operator=(const ScanDir& other)
{
    return *this;
}

void ScanDir::save_partial_video_avs()
{
    if (files_.size() == 0)
    {
        std::cout << "Nothing to save" << std::endl;
        return;
    }

    for (std::vector<std::string>::const_iterator i = files_.begin(); i < files_.end(); ++i)
    {
        const std::string in_filename(dir_ + "\\" + *i);
        const std::string filename(in_filename + ".avs");
        std::ofstream out(filename);
        if (!out.is_open())
        {
            std::cout << "Unable to open file " << filename << std::endl;
            continue;
        }

        if (is_x86)
            out << "LoadPlugin(\"D:\\Program Files (x86)\\MeGUI\\tools\\lsmash\\LSMASHSource.dll\")\n";
        else
            out << "LoadPlugin(\"D:\\Program Files\\MeGUI\\tools\\lsmash\\LSMASHSource.dll\")\n";
        out << "LSMASHVideoSource(\"" << in_filename << "\")\n";
        out << "#ColorYUV(autogain=true)\n";
        
        out.close();
    }
}

void ScanDir::save_video_avs(const _TCHAR* filename)
{
    if (files_.size() == 0)
    {
        std::cout << "Nothing to save" << std::endl;
        return;
    }

    std::ofstream out(filename);
    if (!out.is_open())
    {
        std::cout << "Unable to open file " << filename << std::endl;
        return;
    }

    if (is_x86)
        out << "LoadPlugin(\"D:\\Program Files (x86)\\MeGUI\\tools\\lsmash\\LSMASHSource.dll\")\n";
    else
        out << "LoadPlugin(\"D:\\Program Files\\MeGUI\\tools\\lsmash\\LSMASHSource.dll\")\n";

    std::string cat("last = v1");

    for (std::vector<std::string>::const_iterator i = files_.begin(); i < files_.end(); ++i)
    {
        const size_t n = i - files_.begin() + 1;

        out << "v" << n << " = LWLibavVideoSource(\"" << dir_ << "\\" << *i << ".avi" << "\").AssumeFPS(30,1)\n";
        if (n > 1)
        {
            char buf[64];
            _snprintf_s(buf, sizeof(buf), "%lu", n);
            cat += std::string(" + v") + buf;
        }
    }

    out << cat << "\n";
    out << "ConvertToYV12()\n";
    out.close();
}

void ScanDir::save_audio_avs(const _TCHAR* filename)
{
    if (files_.size() == 0)
    {
        std::cout << "Nothing to save" << std::endl;
        return;
    }

    std::ofstream out(filename);
    if (!out.is_open())
    {
        std::cout << "Unable to open file " << filename << std::endl;
        return;
    }

    if (is_x86)
        out << "LoadPlugin(\"D:\\Program Files (x86)\\MeGUI\\tools\\lsmash\\LSMASHSource.dll\")\n";
    else
        out << "LoadPlugin(\"D:\\Program Files\\MeGUI\\tools\\lsmash\\LSMASHSource.dll\")\n";

    std::string cat("last = v1");

    for (std::vector<std::string>::const_iterator i = files_.begin(); i < files_.end(); ++i)
    {
        const size_t n = i - files_.begin() + 1;

        out << "a" << n << " = LSMASHAudioSource(\"" << dir_ << "\\" << *i  << "\")\n";
        out << "v" << n << " = LSMASHVideoSource(\"" << dir_ << "\\" << *i  << "\").AssumeFPS(30,1)\n";

//        out << "new_rate = a" << n << ".AudioRate*a" << n << ".AudioDuration/(v" << n << ".FrameCount/v" << n << ".FrameRate)\n";
        out << "new_rate = Float(a" << n <<".AudioLength)/v" << n << ".FrameCount*v" << n << ".FrameRateNumerator/v" << n << ".FrameRateDenominator\n";

//        out << "v" << n << " = AudioDub(v" << n << ", a" << n << ").AssumeSampleRate(Round(new_rate)).AssumeFPS(30,1, true).ResampleAudio(48000)\n";
        out << "a" << n << " = a" << n << ".AssumeSampleRate(Round(new_rate)).ResampleAudio(48000)\n";
        out << "v" << n << " = AudioDub(v" << n << ", a" << n << ")\n";

        out << "\n";
        if (n > 1)
        {
            char buf[64];
            _snprintf_s(buf, sizeof(buf), "%lu", n);
            cat += std::string(" + v") + buf;
        }
    }

    out << cat << "\n";
    out << "return last\n";
    out.close();
}

enum Mode {
    mp4_phone = 0,
    dgindex,
    mux,
    ren_mov,
    mux_mov,
    s8_avs,
    split_avs,
};

int _tmain(int argc, _TCHAR* argv[])
{
    Mode mode = mp4_phone;
    std::tstring ext;
    std::tstring tmpl;
    if (argc > 1)
    {
        if (_tcscmp(_T("-dgindex"), argv[1]) == 0)
            mode = dgindex;
        else if (_tcscmp(_T("-mux"), argv[1]) == 0)
            mode = mux;
        else if (_tcscmp(_T("-ren-mov"), argv[1]) == 0)
            mode = ren_mov;
        else if (_tcscmp(_T("-mux-mov"), argv[1]) == 0)
            mode = mux_mov;
        else if (argc > 2 && _tcscmp(_T("-s8-avs"), argv[1]) == 0)
        {
            tmpl = argv[2];
            mode = s8_avs;
        }
        else if (_tcscmp(_T("-split-avs"), argv[1]) == 0)
            mode = split_avs;
        else
            ext = argv[1];
    }
    else
        ext = _T("*.mp4");

    std::tstring dir;
    if (mode == mp4_phone)
    {
        if (argc > 2)
            dir = argv[2];
        else
        {
            DWORD dir_len = GetCurrentDirectory(0, nullptr);
            std::vector<_TCHAR> vdir(dir_len + 1);
            if (GetCurrentDirectory(dir_len + 1, &vdir[0]) == 0)
            {
                printf("GetCurrentDirectory error %u", GetLastError());
                return 1;
            }
            dir = &vdir[0];
        }
        _tprintf(_T("%s\\%s will be used\n"), dir.c_str(), ext.c_str());
    }

    if (mode == dgindex)
    {
        DgindexScanDir sd(dir.c_str());
        sd.scan(_T("*.mpeg"), false);
        sd.save_dgi_render(0);
        sd.save_dgi_render(2);
        sd.save_avs(0);
        return 0;
    }
    else if (mode == ren_mov)
    {
        DgindexScanDir sd(dir.c_str());
        sd.scan(_T("*.mov"), false);
        sd.save_avs(1);
        sd.save_avs(2);
        sd.save_dgi_render(1);
        return 0;
    }
    else if (mode == mux)
    {
        DgindexScanDir sd(dir.c_str());
        sd.scan(_T("*.mpeg"), false);
        sd.scan(_T("*.mp2"), true);
        sd.save_dgi_render(3);
        return 0;
    }
    else if (mode == mux_mov)
    {
        DgindexScanDir sd(dir.c_str());
        sd.scan(_T("*.mov"), false);
        sd.scan(_T("*.ac3"), true);
        sd.save_dgi_render(4);
        return 0;
    }
    else if (mode == s8_avs)
    {
        S8AvsCreator c(tmpl.c_str(), std::cin);
        c.process_entries();
        return 0;
    }

    ScanDir sd(dir.c_str(), ext.c_str());
    sd.save_partial_video_avs();
    sd.save_video_avs(_T("video.avs"));
    sd.save_audio_avs(_T("audio.avs"));
    return 0;
}
