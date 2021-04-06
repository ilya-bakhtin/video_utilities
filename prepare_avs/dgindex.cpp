#include "dgindex.h"

#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>

#include "windows.h"

DgindexScanDir::DgindexScanDir(const _TCHAR* dir):
    dir_(dir)
{
}

DgindexScanDir::~DgindexScanDir()
{
}

std::string DgindexScanDir::to_utf8(const _TCHAR* wname)
{
    int len = WideCharToMultiByte(CP_UTF8, 0, wname, -1, nullptr, 0, nullptr, nullptr) + 1;
    std::vector<char> utf8(len);
    WideCharToMultiByte(CP_UTF8, 0, wname, -1, &utf8[0], len, nullptr, nullptr);
    return &utf8[0]; 
}

void DgindexScanDir::scan(TCHAR* ext, bool audio)
{
    WIN32_FIND_DATA FindFileData;
    HANDLE hFind;

    std::vector<std::string>& files = audio ? aud_files_ : files_;

    files.clear();

    hFind = FindFirstFile((dir_ + _T("/") + ext).c_str(), &FindFileData);
    std::unique_ptr<HANDLE, std::function<void (HANDLE*)> > close_guard(&hFind, [](HANDLE* h){if (*h != INVALID_HANDLE_VALUE) FindClose(*h);});

    if (hFind == INVALID_HANDLE_VALUE)
        printf ("FindFirstFile failed (%d)\n", GetLastError());
    else
    {
        _tprintf(_T("The first file found is %s\n"), FindFileData.cFileName);
        files.push_back(to_utf8(FindFileData.cFileName));
        for (; FindNextFile(hFind, &FindFileData);)
        {
            _tprintf(_T("The next file found is %s\n"), FindFileData.cFileName);
            files.push_back(to_utf8(FindFileData.cFileName));
        }
    }

    std::sort(files.begin(), files.end());

    for (std::vector<std::string>::const_iterator i = files.begin(); i < files.end(); ++i)
        printf("%s\n", i->c_str());
}

void DgindexScanDir::save_dgi_render(int type)
{
    if (files_.size() == 0)
    {
        std::cout << "Nothing to save" << std::endl;
        return;
    }

    std::tstring filename(type == 0 || type == 1 ? _T("render.bat") : type == 2 ? _T("dgi.bat") : _T("mux.bat"));

    std::ofstream out(filename);
    if (!out.is_open())
    {
        std::cout << "Unable to open file " << filename.c_str() << std::endl;
        return;
    }

    std::string dir = to_utf8(dir_.c_str());

    size_t n = 0;
    for (std::vector<std::string>::const_iterator i = files_.begin(); i < files_.end(); ++i, ++n)
    {
        std::string name(*i);
        const size_t p = name.rfind(".");
        if (p != std::string::npos)
            name = name.substr(0, p);

        if (type == 0 || type == 1)
        {
            out << "\"D:\\Program Files\\MeGUI\\tools\\x264\\x264.exe\" --crf " + std::string(type == 0 ? "14.0" : "19.0") + " --sar 1:1 --output \"" <<
                   dir << "\\" << name << ".264\" \"" << dir << "\\" << name << ".avs\"" << std::endl;
            out << "\"D:\\Program Files\\MeGUI\\tools\\mp4box\\mp4box.exe\" -add \"" << dir << "\\" <<
                   name << ".264#trackID=1:fps=" << (type == 0 ? "25" : "30") << ":par=1:1:name=\" -tmp \"" << dir << "\" -new \"" << dir << "\\" << name << ".mp4\"" << std::endl;
            out << "\"D:\\Program Files\\MeGUI\\tools\\ffmpeg\\ffmpeg.exe\" -i \"" << dir << "\\" << name << "_a.avs\" " <<
                   "-y -acodec ac3 -ab 384k \"" << dir << "\\" << name << "_a.ac3\"" << std::endl;
            out << std::endl;
        }
        else if (type == 2)
        {
            out << "\"D:\\Program Files\\MeGUI\\tools\\dgindex\\dgindex.exe\"" <<
                " -AIF=\"[" + dir +"\\" + *i + "]\" -OF=\"[" + dir + "\\" + name +"]\" -FO=0 -exit -hide -OM=2" << std::endl;
        }
        else if (aud_files_.size() > n)
        {
            out << "\"D:\\Program Files\\MeGUI\\tools\\mp4box\\mp4box.exe\" -add \"" <<
                   dir + "\\" + name + ".mp4#trackID=1:" << (type == 3 ? "25" : "30") << ":par=1:1:name=\" -add \"" <<
                   dir + "\\" + aud_files_[n] + "#trackID=1:name=:delay=-240\" -tmp \"" + dir + "\" -new \"" + dir + "\\" + name + "-muxed.mp4\"" << std::endl;
        }
    }
    out.close();
}

void DgindexScanDir::save_avs(int type)
{
    std::string dir = to_utf8(dir_.c_str());

    for (std::vector<std::string>::const_iterator i = files_.begin(); i < files_.end(); ++i)
    {
        const std::string fullname(*i);
        const size_t p = fullname.rfind(".");
        std::string name;
        if (p != std::string::npos)
            name = fullname.substr(0, p);

        std::string filename;
        if (type != 2)
            filename = name + ".avs";
        else
            filename = name + "_a.avs";

        std::ofstream out(filename);
        if (!out.is_open())
        {
            std::cout << "Unable to open file " << filename.c_str() << std::endl;
            return;
        }

        if (type == 0)
        {
            out << "LoadPlugin(\"D:\\Program Files\\MeGUI\\tools\\dgindex\\DGDecode.dll\")" << std::endl;
            out << "DGDecode_mpeg2source(\"" + dir + "\\" + name + ".d2v\", info=3)" << std::endl;
            out << "Yadif" << std::endl;
            out << "SincResize(1920,1080)" << std::endl;
        }
        else if (type == 1)
        {
            out << "LoadPlugin(\"D:\\Program Files\\MeGUI\\tools\\lsmash\\LSMASHSource.dll\")" << std::endl;
            out << "LSMASHVideoSource(\"" + dir + "\\" + fullname + "\").ConvertToYV24" << std::endl;
            out << "last = FrameRate > 45 ? SelectEven : last" << std::endl;
            out << "ConvertToYV24.TurnRight.Spline64Resize(1080, 1920)" << std::endl;
            out << "last = FrameRate < 28 ? FrameRateConverter(Output=\"Flow\", Preset=\"slowest\", NewNum=30, NewDen=1, blksize=12) : last" << std::endl;
            out << "AssumeFps(30)" << std::endl;
            out << "Prefetch(6)" << std::endl;
        }
        else if (type == 2)
        {
            out << "LoadPlugin(\"D:\\Program Files\\MeGUI\\tools\\lsmash\\LSMASHSource.dll\")" << std::endl;
            out << "a = LSMASHAudioSource(\"" + dir + "\\" + fullname + "\")" << std::endl;
            out << "v = LSMASHVideoSource(\"" + dir + "\\" + fullname + "\")" << std::endl;
            out << "v = v.FrameRate < 28 ? v : (v.FrameRate > 45 ? v.AssumeFPS(60,1) : v.AssumeFPS(30,1))" << std::endl;
            out << "new_rate = Float(a.AudioLength)/v.FrameCount*v.FrameRateNumerator/v.FrameRateDenominator" << std::endl;
            out << "a = a.AssumeSampleRate(Round(new_rate)).ResampleAudio(48000)" << std::endl;
            out << "return AudioDub(v, a)" << std::endl;
        }
        out.close();
    }
}
