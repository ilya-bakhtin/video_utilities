// prepare_avs.cpp : Defines the entry point for the console application.
//

#include "dgindex.h"
#include "s8-avs-creator.h"
#include "segment.h"
#include "split_avs.h"

#include "algorithm"
#include "fstream"
#include "functional"
#include "iostream"
#include <iterator>
#include "memory"
#include "string"
#include "vector"

#include "tchar.h"
#include "windows.h"

#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>

namespace prepare_avs {

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
    ScanDir& operator=(const ScanDir& other);
    ScanDir& operator=(const ScanDir&& other);

    std::vector<std::unique_ptr<Segment> > files_;
    std::string dir_;
    std::vector<std::string> template_;
    std::vector<std::string> scn_template_;
    std::vector<std::string> file_template_;
    std::vector<std::string> avs_template_;

    int timeshift_;

    void add_avi_scenes();
    void load_scenes_template(bool files);
    void load_avs_template();
    void replace_word(std::string& str, const std::string& word, const std::string& word_new);
};

std::string ScanDir::to_utf8(const _TCHAR* wname)
{
    int len = WideCharToMultiByte(CP_UTF8, 0, wname, -1, nullptr, 0, nullptr, nullptr) + 1;
    std::vector<char> utf8(len);
    WideCharToMultiByte(CP_UTF8, 0, wname, -1, &utf8[0], len, nullptr, nullptr);
    return &utf8[0]; 
}

ScanDir::ScanDir(const _TCHAR* dir_name, const _TCHAR* ext) :
    timeshift_(0)
{
    WIN32_FIND_DATA FindFileData;
    HANDLE hFind;

    files_.clear();
    dir_ = to_utf8(dir_name);

    hFind = FindFirstFile((tstring(dir_name) + _T("/") + ext).c_str(), &FindFileData);
    std::unique_ptr<HANDLE, std::function<void (HANDLE*)> > close_guard(&hFind, [](HANDLE* h){if (*h != INVALID_HANDLE_VALUE) FindClose(*h);});

    if (hFind == INVALID_HANDLE_VALUE)
        printf ("FindFirstFile failed (%d)\n", GetLastError());
    else
    {
        _tprintf(_T("The first file found is %s\n"), FindFileData.cFileName);
        std::unique_ptr<Segment> seg(new SingleFileSegment(to_utf8(FindFileData.cFileName)));
        files_.push_back(std::move(seg));
        for (; FindNextFile(hFind, &FindFileData);)
        {
            _tprintf(_T("The next file found is %s\n"), FindFileData.cFileName);
            std::unique_ptr<Segment> seg(new SingleFileSegment(to_utf8(FindFileData.cFileName)));
            files_.push_back(std::move(seg));
        }
    }

    load_scenes_template(false);
    load_scenes_template(true);

    add_avi_scenes();

    std::sort(files_.begin(), files_.end(), [](const std::unique_ptr<Segment>& a, const std::unique_ptr<Segment>& b) -> bool {
        return *a < *b;
    });

    for (std::vector<std::unique_ptr<Segment> >::const_iterator i = files_.begin(); i < files_.end(); ++i)
        printf("%s %s\n", (*i)->get_time_str().c_str(), (*i)->get_name().c_str());
}

ScanDir::~ScanDir()
{
}

void ScanDir::add_avi_scenes()
{
    const std::string scenes_name = dir_ + "\\" + "scenes.avs";
    struct stat sb;
    const bool scenes = stat(scenes_name.c_str(), &sb) == 0;

    if (scenes)
    {
        std::ifstream sc_file(string_utils::to_tstring(scenes_name.c_str(), CP_UTF8));
        if (!sc_file.is_open())
        {
            std::cout << "Unable to open file " << scenes_name << std::endl;
            return;
        }

        std::string line;
        while (sc_file.good())
        {
            std::getline(sc_file, line);
            if (!line.empty())
            {
                std::unique_ptr<Segment> seg(new AviFileSegment(line, timeshift_));
                files_.push_back(std::move(seg));
            }
        }
        sc_file.close();
    }
}

void ScanDir::load_scenes_template(bool files)
{
    const std::string template_name = dir_ + "\\" + (files ? "template.files" : "template.scenes");

    struct stat sb;
    if (stat(template_name.c_str(), &sb) != 0)
        return;

    std::ifstream tmpl_file(string_utils::to_tstring(template_name.c_str(), CP_UTF8));
    if (!tmpl_file.is_open())
    {
        std::cout << "Unable to open file " << template_name << std::endl;
        return;
    }

    std::vector<std::string>& scn_template = files ? file_template_ : scn_template_;

    std::string line;
    while (tmpl_file.good())
    {
        std::getline(tmpl_file, line);
        scn_template.push_back(line);

        if (!files)
        {
            static const char timeshift[] = "#timeshift";

            size_t ts = line.find(timeshift);
            if (ts != std::string::npos)
                sscanf(line.substr(ts + sizeof(timeshift)).c_str(), "%d", &timeshift_);
        }
    }
    tmpl_file.close();

    while (scn_template.back().empty())
        scn_template.pop_back();
}

void ScanDir::load_avs_template()
{
    const std::string template_name = dir_ + "\\" + "template.avs";

    struct stat sb;
    if (stat(template_name.c_str(), &sb) != 0)
        return;

    std::ifstream tmpl_file(string_utils::to_tstring(template_name.c_str(), CP_UTF8));
    if (!tmpl_file.is_open())
    {
        std::cout << "Unable to open file " << template_name << std::endl;
        return;
    }

    std::string line;
    while (tmpl_file.good())
    {
        std::getline(tmpl_file, line);
        avs_template_.push_back(line);
    }

    tmpl_file.close();

    while (avs_template_.back().empty())
        avs_template_.pop_back();
}

void ScanDir::replace_word(std::string& str, const std::string& word, const std::string& word_new)
{
    for (;;)
    {
        size_t pos = str.find(word);
        if (pos == std::string::npos)
            break;
        str = str.substr(0, pos) + word_new + str.substr(pos + word.size());
    }
}

void ScanDir::save_partial_video_avs()
{
    if (files_.size() == 0)
    {
        std::cout << "Nothing to save" << std::endl;
        return;
    }

    load_avs_template();

    const std::string vd_template_name = dir_ + "\\" + "template.vdscript";
    const std::string render_bat_name = dir_ + "\\" + "render.bat";
    struct stat sb;
    const bool vd_template = stat(vd_template_name.c_str(), &sb) == 0;

    std::ofstream render;
    if (vd_template)
    {
        std::ifstream tmpl(string_utils::to_tstring(vd_template_name.c_str(), CP_UTF8));
        if (!tmpl.is_open())
        {
            std::cout << "Unable to open file " << vd_template_name << std::endl;
            return;
        }

        std::string line;
        while (tmpl.good())
        {
            std::getline(tmpl, line);
            template_.push_back(line);
        }
        tmpl.close();

        render.open(string_utils::to_tstring(render_bat_name.c_str(), CP_UTF8));
        if (!render.is_open())
        {
            std::cout << "Unable to open file " << render_bat_name << std::endl;
            return;
        }
    }

    const std::string common_filename(dir_ + "\\" + "common.avs");

    if (!avs_template_.empty())
    {
        std::ofstream common(string_utils::to_tstring(common_filename.c_str(), CP_UTF8));
        if (!common.is_open())
            std::cout << "Unable to open file " << common_filename << std::endl;
        else
        {
            common << "global source_dir = \"" << dir_ << "\\\"" << std::endl;
            common.close();
        }
    }

    for (std::vector<std::unique_ptr<Segment> >::const_iterator i = files_.begin(); i < files_.end(); ++i)
    {
        if (!(*i)->is_single())
            continue;

        const std::string in_filename((*i)->get_name());
        const std::string full_filename(dir_ + "\\" + in_filename);
        const std::string filename(full_filename + ".avs");

        std::ofstream out(string_utils::to_tstring(filename.c_str(), CP_UTF8));
        if (!out.is_open())
        {
            std::cout << "Unable to open file " << filename << std::endl;
            continue;
        }

        if (avs_template_.empty())
        {
            if (is_x86)
                out << "LoadPlugin(\"D:\\Program Files (x86)\\MeGUI\\tools\\lsmash\\LSMASHSource.dll\")\n";
            else
                out << "LoadPlugin(\"D:\\Program Files\\MeGUI\\tools\\lsmash\\LSMASHSource.dll\")\n";
            out << "LSMASHVideoSource(\"" << full_filename << "\")\n";
            out << "#ColorYUV(autogain=true)\n";
        }
        else
        {
            out << "import(\"" << common_filename << "\")" << std::endl << std::endl;

            for (std::vector<std::string>::const_iterator i = avs_template_.begin(); i != avs_template_.end(); ++i)
            {
                std::string s = *i;
                replace_word(s, "$$$clip$$$", "source_dir + \"" + in_filename + "\"");
                replace_word(s, "$$$clip_name$$$", "\"" + in_filename + "\"");

                out << s << std::endl;
            }
        }
        
        out.close();

        if (vd_template)
        {
            render << "\"D:\\Program Files\\VirtualDub\\vdub64.exe\" /s " + full_filename + ".vdscript" << std::endl;

            std::string double_backslash_name;
            for (std::string::const_iterator i = in_filename.begin(); i != in_filename.end(); ++i)
            {
                if (*i == '\\')
                    double_backslash_name += "\\\\";
                else
                    double_backslash_name += *i;
            }

            std::string dir = dir_ + "\\";
            std::string double_backslash_dir;
            for (std::string::const_iterator i = dir.begin(); i != dir.end(); ++i)
            {
                if (*i == '\\')
                    double_backslash_dir += "\\\\";
                else
                    double_backslash_dir += *i;
            }

            const std::string vd_name = full_filename + ".vdscript";
            std::ofstream of(string_utils::to_tstring(vd_name.c_str(), CP_UTF8));
            if (!of.is_open())
            {
                std::cout << "Unable to open file " << vd_name << std::endl;
                return;
            }

            for (std::vector<std::string>::const_iterator i = template_.begin(); i != template_.end(); ++i)
            {
                std::string s = *i;
                replace_word(s, "$$$dir$$$", double_backslash_dir);
                replace_word(s, "$$$clip$$$", double_backslash_name);

                of << s << std::endl;
            }

            of.close();
        }
    }

    if (vd_template)
        render.close();
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

    const std::string common_filename(dir_ + "\\" + "common.avs");
    out << "import(\"" << common_filename << "\")" << std::endl << std::endl;

    const bool has_scene_start_template = !scn_template_.empty() && scn_template_.back().find("$$$start$$$") != std::string::npos;
    const bool has_scene_end_template = !scn_template_.empty() && scn_template_.back().find("$$$end$$$") != std::string::npos;
    const bool has_file_video_template = !file_template_.empty() && file_template_.back().find("$$$video$$$") != std::string::npos;
    const bool has_file_audio_template = !file_template_.empty() && file_template_.back().find("$$$audio$$$") != std::string::npos;

    if (file_template_.empty())
    {
        if (is_x86)
            out << "LoadPlugin(\"D:\\Program Files (x86)\\MeGUI\\tools\\lsmash\\LSMASHSource.dll\")\n";
        else
            out << "LoadPlugin(\"D:\\Program Files\\MeGUI\\tools\\lsmash\\LSMASHSource.dll\")\n";
    }
    else
    {
        for (std::vector<std::string>::const_iterator i = file_template_.begin(); i + 1 != file_template_.end(); ++i)
            out << *i << "\n";
    }

    int timeshift = 0;

    if (!scn_template_.empty())
    {
        for (std::vector<std::string>::const_iterator i = scn_template_.begin(); i + 1 != scn_template_.end(); ++i)
            out << *i << "\n";
    }

    std::string cat("last = v1");

    for (std::vector<std::unique_ptr<Segment> >::const_iterator i = files_.begin(); i < files_.end(); ++i)
    {
        const size_t n = i - files_.begin() + 1;

        if ((*i)->is_single())
        {
            if (!has_file_video_template || !has_file_audio_template)
                out << "v" << n << " = LWLibavVideoSource(\"" << dir_ << "\\" << (*i)->get_name() << ".avi" << "\").AssumeFPS(30,1)\n";
            else
            {
                std::string str(file_template_.back()); 
                replace_word(str, "$$$video$$$", "source_dir + \"" + (*i)->get_name() + ".avi\"");
                replace_word(str, "$$$audio$$$", "source_dir + \"" + (*i)->get_name() + "\"");
                out << "v" << n << " = " << str << "\n";
            }
        }
        else
        {
            if (!has_scene_start_template || !has_scene_end_template)
                out << "v" << n << " = " << (*i)->get_name() << "\n";
            else
            {
                std::string rng((*i)->get_name());
                size_t p0 = rng.find('(');
                size_t p1 = rng.find(')');
                if (p0 != std::string::npos && p1 != std::string::npos && p1 > p0)
                {
                    rng = rng.substr(p0 + 1, p1 - p0 - 1);
                    size_t comma = rng.find(',');
                    if (comma != std::string::npos)
                    {
                        std::string str(scn_template_.back()); 
                        replace_word(str, "$$$start$$$", rng.substr(0, comma));
                        replace_word(str, "$$$end$$$", rng.substr(comma + 1));
                        out << "v" << n << " = " << str << "\n";
                    }
                }
            }
        }

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

    for (std::vector<std::unique_ptr<Segment> >::const_iterator i = files_.begin(); i < files_.end(); ++i)
    {
        const size_t n = i - files_.begin() + 1;

        out << "a" << n << " = LSMASHAudioSource(\"" << dir_ << "\\" << (*i)->get_name()  << "\")\n";
        out << "v" << n << " = LSMASHVideoSource(\"" << dir_ << "\\" << (*i)->get_name()  << "\").AssumeFPS(30,1)\n";

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

} // namespace

using namespace prepare_avs;

int _tmain(int argc, _TCHAR* argv[])
{
    Mode mode = mp4_phone;
    tstring ext;
    tstring tmpl;

    SetConsoleOutputCP(CP_UTF8);

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
        else if (_tcscmp(_T("-s8-avs"), argv[1]) == 0)
        {
            if (argc > 2)
            {
                tmpl = argv[2];
                mode = s8_avs;
            }
            else
            {
                std::cerr << "please specify template name" << std::endl;
                exit(1);
            }
        }
        else if (_tcscmp(_T("-split-avs"), argv[1]) == 0)
        {
            if (argc > 2)
            {
                tmpl = argv[2];
                mode = split_avs;
            }
            else
            {
                std::cerr << "please specify main avs file name" << std::endl;
                exit(1);
            }
        }
        else
            ext = argv[1];
    }
    else
        ext = _T("*.mp4");

    tstring dir;
    if (mode == mp4_phone && argc > 2)
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

    std::cout << string_utils::to_ansi_string(dir.c_str(), CP_UTF8) << "\\" << string_utils::to_ansi_string(ext.c_str(), CP_UTF8) << " will be used" << std::endl;

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
        S8AvsCreator c(dir, tmpl, std::cin);
        c.process_entries();
        return 0;
    }
    else if (mode == split_avs)
    {
        SplitAvs s;
        s.process_file(tmpl);
        return 0;
    }

    ScanDir sd(dir.c_str(), ext.c_str());
    sd.save_partial_video_avs();
    sd.save_video_avs(_T("video.avs"));
    sd.save_audio_avs(_T("audio.avs"));
    return 0;
}
