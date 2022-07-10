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
    ScanDir(const _TCHAR* dir, const _TCHAR* cdir, const _TCHAR* ext);
    virtual ~ScanDir();

    bool save_partial_video_avs(unsigned n);
    void save_video_avs(const _TCHAR* filename);
    void save_audio_avs(const _TCHAR* filename);

    static std::string escape_backslash(const char* in_str);

private:
    static const bool is_x86 = false;

    ScanDir(const ScanDir& other);
    ScanDir(const ScanDir&& other);
    ScanDir& operator=(const ScanDir& other);
    ScanDir& operator=(const ScanDir&& other);

    std::vector<std::unique_ptr<Segment> > files_;
    std::string dir_;
    std::string cdir_;
    std::vector<std::string> scn_template_;
    std::vector<std::string> file_template_;

    int timeshift_;

    void add_avi_scenes();
    bool load_template(const char* name, std::vector<std::string>& templ);
    void load_scenes_template(bool files);
    void replace_word(std::string& str, const std::string& word, const std::string& word_new);
};

std::string ScanDir::escape_backslash(const char* in_str)
{
    std::string double_backslash_str;
    for (const char* p = in_str; *p != 0; ++p)
    {
        if (*p == '\\')
            double_backslash_str += "\\\\";
        else
            double_backslash_str += *p;
    }
    return double_backslash_str;
}

ScanDir::ScanDir(const _TCHAR* dir_name, const _TCHAR* cdir_name, const _TCHAR* ext) :
    timeshift_(0)
{
    WIN32_FIND_DATA FindFileData;
    HANDLE hFind;

    files_.clear();
    dir_ = string_utils::to_ansi_string(dir_name, CP_UTF8);
    cdir_ = string_utils::to_ansi_string(cdir_name, CP_UTF8);

    hFind = FindFirstFile((tstring(dir_name) + _T("/") + ext).c_str(), &FindFileData);
    std::unique_ptr<HANDLE, std::function<void (HANDLE*)> > close_guard(&hFind, [](HANDLE* h){if (*h != INVALID_HANDLE_VALUE) FindClose(*h);});

    if (hFind == INVALID_HANDLE_VALUE)
        printf ("FindFirstFile failed (%d)\n", GetLastError());
    else
    {
        _tprintf(_T("The first file found is %s\n"), FindFileData.cFileName);
        std::unique_ptr<Segment> seg(new SingleFileSegment(string_utils::to_ansi_string(FindFileData.cFileName, CP_UTF8)));
        files_.push_back(std::move(seg));
        for (; FindNextFile(hFind, &FindFileData);)
        {
            _tprintf(_T("The next file found is %s\n"), FindFileData.cFileName);
            std::unique_ptr<Segment> seg(new SingleFileSegment(string_utils::to_ansi_string(FindFileData.cFileName, CP_UTF8)));
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
    const std::string scenes_name = cdir_ + "\\" + "scenes.avs";
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
    const std::string template_name = cdir_ + "\\" + (files ? "template.files" : "template.scenes");

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

bool ScanDir::load_template(const char* name, std::vector<std::string>& templ)
{
    struct stat sb;
    if (stat(name, &sb) != 0)
        return false;

    std::ifstream tmpl(string_utils::to_tstring(name, CP_UTF8));
    if (!tmpl.is_open())
    {
        std::cout << "Unable to open file " << name << std::endl;
        return false;
    }

    std::string line;
    while (tmpl.good())
    {
        std::getline(tmpl, line);
        templ.push_back(line);
    }
    tmpl.close();

    while (templ.back().empty())
        templ.pop_back();

    return true;
}

bool ScanDir::save_partial_video_avs(unsigned n)
{
    if (files_.size() == 0)
    {
        std::cout << "Nothing to save" << std::endl;
        return false;
    }

    char ns[64];
    _snprintf_s(ns, sizeof(ns), "-%u", n);

    const std::string avs_template_name = cdir_ + "\\" + "template" + (n == 0 ? "" : ns) + ".avs";
    const std::string vd_template_name = cdir_ + "\\" + "template" + (n == 0 ? "" : ns) + ".vdscript";
    const std::string ren_template_name = cdir_ + "\\" + "template" + (n == 0 ? "" : ns) + ".render";
    const std::string render_bat_name = cdir_ + "\\" + "render" + (n == 0 ? "" : ns) + ".bat";

    std::vector<std::string> avs_template;
    const bool have_avs_template = load_template(avs_template_name.c_str(), avs_template);

    std::vector<std::string> vd_template;
    const bool have_vd_template = load_template(vd_template_name.c_str(), vd_template);

    std::vector<std::string> ren_template;
    const bool have_ren_template = load_template(ren_template_name.c_str(), ren_template);

    if (!have_avs_template && !have_vd_template && !have_ren_template)
        return false;

    std::ofstream render;
    if (have_vd_template || have_ren_template)
    {
        render.open(string_utils::to_tstring(render_bat_name.c_str(), CP_UTF8));
        if (!render.is_open())
        {
            std::cout << "Unable to open file " << render_bat_name << std::endl;
            return false;
        }
    }

    const std::string common_filename(cdir_ + "\\" + "common.avs");
    if (n == 0)
    {
        std::ofstream common(string_utils::to_tstring(common_filename.c_str(), CP_UTF8));
        if (!common.is_open())
            std::cout << "Unable to open file " << common_filename << std::endl;
        else
        {
            common << "global source_dir = \"" << cdir_ << "\\\"" << std::endl;
            common << "global destination_dir = \"" << dir_ << "\\\"" << std::endl;
            common.close();
        }
    }

    for (std::vector<std::unique_ptr<Segment> >::const_iterator i = files_.begin(); i < files_.end(); ++i)
    {
        if (!(*i)->is_single())
            continue;

        const std::string in_filename((*i)->get_name());
        const std::string full_filename_c(cdir_ + "\\" + in_filename);
        const std::string full_filename_d(dir_ + "\\" + in_filename);
        const std::string filename_c(full_filename_c + (n == 0 ? "" : ns) + ".avs");
        const std::string filename_d(full_filename_d + (n == 0 ? "" : ns) + ".avs");

        std::ofstream out(string_utils::to_tstring(filename_c.c_str(), CP_UTF8));
        if (!out.is_open())
        {
            std::cout << "Unable to open file " << filename_c << std::endl;
            continue;
        }

        if (!have_avs_template)
        {
            if (is_x86)
                out << "LoadPlugin(\"D:\\Program Files (x86)\\MeGUI\\tools\\lsmash\\LSMASHSource.dll\")\n";
            else
                out << "LoadPlugin(\"D:\\Program Files\\MeGUI\\tools\\lsmash\\LSMASHSource.dll\")\n";
            out << "LSMASHVideoSource(\"" << full_filename_d << "\")\n";
            out << "#ColorYUV(autogain=true)\n";
        }
        else
        {
            out << "import(\"" << common_filename << "\")" << std::endl << std::endl;

            for (std::vector<std::string>::const_iterator i = avs_template.begin(); i != avs_template.end(); ++i)
            {
                std::string s = *i;
                replace_word(s, "$$$clip$$$", "destination_dir + \"" + in_filename + "\"");
                replace_word(s, "$$$clip_c$$$", "source_dir + \"" + in_filename + "\"");
                replace_word(s, "$$$clip_d$$$", "destination_dir + \"" + in_filename + "\"");
                replace_word(s, "$$$clip_name$$$", "\"" + in_filename + "\"");
                replace_word(s, "$$$dir$$$", "destination_dir");
                replace_word(s, "$$$dir_c$$$", "source_dir");
                replace_word(s, "$$$dir_d$$$", "destination_dir");

                out << s << std::endl;
            }
        }
        
        out.close();

        const std::string double_backslash_name = escape_backslash(in_filename.c_str());

        const std::string dir = dir_ + "\\";
        const std::string cdir = cdir_ + "\\";
        const std::string double_backslash_dir = escape_backslash(dir.c_str());

        if (have_vd_template)
        {
            const std::string vd_name = full_filename_c + (n == 0 ? "" : ns) + ".vdscript";
            std::ofstream of(string_utils::to_tstring(vd_name.c_str(), CP_UTF8));
            if (!of.is_open())
            {
                std::cout << "Unable to open file " << vd_name << std::endl;
                return false;
            }

            for (std::vector<std::string>::const_iterator i = vd_template.begin(); i != vd_template.end(); ++i)
            {
                std::string s = *i;
                replace_word(s, "$$$dir$$$", double_backslash_dir);
                replace_word(s, "$$$clip$$$", double_backslash_name);

                of << s << std::endl;
            }

            of.close();
        }

        if (have_vd_template || have_ren_template)
        {
            if (!have_ren_template)
                render << "\"D:\\Program Files\\VirtualDub\\vdub64.exe\" /s " + full_filename_c + ".vdscript" << std::endl;
            else
            {
                for (std::vector<std::string>::const_iterator i = ren_template.begin(); i != ren_template.end(); ++i)
                {
                    std::string s = *i;
                    replace_word(s, "$$$dir$$$", dir);
                    replace_word(s, "$$$dir_c$$$", cdir);
                    replace_word(s, "$$$dir_d$$$", dir);
                    replace_word(s, "$$$dir_ds$$$", double_backslash_dir);
                    replace_word(s, "$$$clip$$$", full_filename_d);
                    replace_word(s, "$$$clip_c$$$", full_filename_c);
                    replace_word(s, "$$$clip_d$$$", full_filename_d);
                    replace_word(s, "$$$clip_name$$$", in_filename);

                    render << s << std::endl;
                }
                render << std::endl;
            }
        }
    }

    if (have_vd_template || have_ren_template)
        render.close();

    return true;
}

void ScanDir::save_video_avs(const _TCHAR* filename)
{
    if (files_.size() == 0)
    {
        std::cout << "Nothing to save" << std::endl;
        return;
    }

    std::wcout << filename << std::endl;

    std::ofstream out(filename);
    if (!out.is_open())
    {
        std::wcout << "Unable to open file " << filename << std::endl;
        return;
    }

    const std::string common_filename(cdir_ + "\\" + "common.avs");
    out << "import(\"" << common_filename << "\")" << std::endl << std::endl;

    const bool has_scene_start_template = !scn_template_.empty() && scn_template_.back().find("$$$start$$$") != std::string::npos;
    const bool has_scene_end_template = !scn_template_.empty() && scn_template_.back().find("$$$end$$$") != std::string::npos;
    const bool has_file_template = !file_template_.empty();

    if (has_file_template)
    {
        for (std::vector<std::string>::const_iterator i = file_template_.begin(); i + 1 != file_template_.end(); ++i)
        {
            std::string str(*i);

            replace_word(str, "$$$dir$$$", "destination_dir");
            replace_word(str, "$$$dir_d$$$", "destination_dir");
            replace_word(str, "$$$dir_c$$$", "source_dir");

            out << str << "\n";
        }
    }
    else
    {
        if (is_x86)
            out << "LoadPlugin(\"D:\\Program Files (x86)\\MeGUI\\tools\\lsmash\\LSMASHSource.dll\")\n";
        else
            out << "LoadPlugin(\"D:\\Program Files\\MeGUI\\tools\\lsmash\\LSMASHSource.dll\")\n";
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
            if (!has_file_template)
                out << "v" << n << " = LWLibavVideoSource(\"" << dir_ << "\\" << (*i)->get_name() << ".avi" << "\").AssumeFPS(30,1)\n";
            else
            {
                const std::string in_filename((*i)->get_name());
                std::string str(file_template_.back()); 
                replace_word(str, "$$$video$$$", "source_dir + \"" + in_filename + ".avi\"");
                replace_word(str, "$$$audio$$$", "source_dir + \"" + in_filename + "\"");

                replace_word(str, "$$$clip$$$", "destination_dir + \"" + in_filename + "\"");
                replace_word(str, "$$$clip_c$$$", "source_dir + \"" + in_filename + "\"");
                replace_word(str, "$$$clip_d$$$", "destination_dir + \"" + in_filename + "\"");
                replace_word(str, "$$$clip_name$$$", "\"" + in_filename + "\"");
                replace_word(str, "$$$dir$$$", "destination_dir");
                replace_word(str, "$$$dir_d$$$", "destination_dir");
                replace_word(str, "$$$dir_c$$$", "source_dir");
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
    {std::cout << "Nothing to save" << std::endl;
        return;
    }

    std::ofstream out(filename);
    if (!out.is_open())
    {
        std::wcout << "Unable to open file " << filename << std::endl;
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

    DWORD dir_len = GetCurrentDirectory(0, nullptr);
    std::vector<_TCHAR> vdir(dir_len + 1);
    if (GetCurrentDirectory(dir_len + 1, &vdir[0]) == 0)
    {
        printf("GetCurrentDirectory error %u", GetLastError());
        return 1;
    }
    tstring cdir = &vdir[0];

    tstring dir;
    if (mode == mp4_phone && argc > 2)
        dir = argv[2];
    else
        dir = cdir;

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

    ScanDir sd(dir.c_str(), cdir.c_str(), ext.c_str());

    for (unsigned n = 0; sd.save_partial_video_avs(n); ++n) {}

    sd.save_video_avs((cdir + _T("\\video.avs")).c_str());
    sd.save_audio_avs((cdir + _T("\\audio.avs")).c_str());
    return 0;
}
