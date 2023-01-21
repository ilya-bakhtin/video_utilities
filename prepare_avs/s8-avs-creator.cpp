#include <iostream>
#include <string>

#include "s8-avs-creator.h"

S8AvsCreator::S8AvsCreator(const tstring& dir, std::istream& entries_file):
    dir_(string_utils::to_ansi_string(dir.c_str(), CP_UTF8)),
    entries_file_(entries_file)
{
}

bool S8AvsCreator::process_entry(const std::string& name, const std::string& render_template, const std::string& render_file, const std::string& ns)
{
//    template_name_(string_utils::to_ansi_string(template_name.c_str(), CP_UTF8)),
    std::ifstream tmpl(string_utils::to_tstring(name.c_str(), CP_UTF8));
//    std::string name = string_utils::to_ansi_string(tname.c_str(), CP_UTF8);

//    std::ifstream tmpl(tname);
    if (!tmpl.is_open())
    {
        if (ns.empty())
            std::cout << "Unable to open file " << name.c_str() << std::endl;
        return false;
    }

    std::vector<std::string> templ;

    std::string line;
    while (tmpl.good())
    {
        std::getline(tmpl, line);
        templ.push_back(line);
    }
    tmpl.close();

    const size_t p = name.rfind(".");
    std::string ext;

    if (p != std::string::npos)
        ext = name.substr(p);

    const bool is_vdscript = ext == ".vdscript";

    std::string dir = dir_ + "\\";

    std::string double_backslash_dir;
    for (std::string::const_iterator i = dir.begin(); i != dir.end(); ++i)
    {
        if (*i == '\\')
            double_backslash_dir += "\\\\";
        else
            double_backslash_dir += *i;
    }
//    dir = double_backslash_dir;

    std::ofstream render;
    if (!render_file.empty())
    {
        render.open(string_utils::to_tstring(render_file.c_str(), CP_UTF8));
        if (!render.is_open())
        {
            std::cout << "Unable to open file " << render_file << std::endl;
            return false;
        }
    }

    std::vector<std::string> ren_template;
    const bool have_ren_template = load_template(render_template.c_str(), ren_template);

    for (std::vector<std::string>::const_iterator i = entries_.begin(); i != entries_.end(); ++i)
    {
        line = *i;

        std::string clip;
        std::string sad;
        std::string sadc;

        size_t pos = line.find(" ");
        if (pos != std::string::npos)
        {
            clip = line.substr(0, pos);
            line = line.substr(pos + 1);
        }
        else
        {
            clip = line;
            line = "";
        }

        pos = line.find(" ");
        if (pos != std::string::npos)
        {
            sad = line.substr(0, pos);
            line = line.substr(pos + 1);
        }
        else
        {
            sad = "1100";
            line = "";
        }

        if (!line.empty())
        {
            sadc = line;
            line = "";
        }
        else
            sadc = "1100";

        const std::string of_name = clip + ns + ext;
        std::ofstream of(string_utils::to_tstring(of_name.c_str(), CP_UTF8));
        if (!of.is_open())
        {
            std::cout << "Unable to open file " << of_name << std::endl;
            return false;
        }

        for (std::vector<std::string>::const_iterator i = templ.begin(); i != templ.end(); ++i)
        {
            std::string s = *i;
            if (is_vdscript)
                replace_word(s, "$$$dir$$$", double_backslash_dir);
            else
                replace_word(s, "$$$dir$$$", dir);

            replace_word(s, "$$$dir_ds$$$", double_backslash_dir);
            replace_word(s, "$$$clip$$$", clip);
            replace_word(s, "$$$sad$$$", sad);
            replace_word(s, "$$$sadc$$$", sadc);

            of << s << std::endl;
        }

        of.close();

        if (render.is_open())
        {
            if (have_ren_template)
            {
                const std::string full_filename_c(dir_ + "\\" + clip + ns);
                const std::string full_filename_d(dir_ + "\\" + clip + ns);
                const std::string filename_c(full_filename_c + ".avs");
                const std::string filename_d(full_filename_d + ".avs");

                for (std::vector<std::string>::const_iterator i = ren_template.begin(); i != ren_template.end(); ++i)
                {
                    std::string s = *i;
                    replace_word(s, "$$$dir$$$", dir);
                    replace_word(s, "$$$dir_c$$$", dir);
                    replace_word(s, "$$$dir_d$$$", dir);
                    replace_word(s, "$$$dir_ds$$$", double_backslash_dir);
                    replace_word(s, "$$$clip$$$", full_filename_d);
                    replace_word(s, "$$$clip_c$$$", full_filename_c);
                    replace_word(s, "$$$clip_d$$$", full_filename_d);
                    replace_word(s, "$$$clip_name$$$", of_name);

                    render << s << std::endl;
                }
                render << std::endl;
            }
            else
            {
                if (is_vdscript)
                    render << "\"D:\\Program Files\\VirtualDub\\vdub64.exe\" /s " << dir << of_name << std::endl;
                else
                    render << "call render-step.bat %1 " << clip << std::endl;
            }
        }

    }

    if (render.is_open())
        render.close();

    return true;
}

void S8AvsCreator::process_entries(const tstring& template_name)
{
    while (entries_file_.good())
    {
        std::string line;
        std::getline(entries_file_, line);
        if (line.empty())
            continue;
        entries_.push_back(line);
    }

    if (!template_name.empty())
    {
        process_entry(string_utils::to_ansi_string(template_name.c_str(), CP_UTF8), "", "", "");
        return;
    }

    for (unsigned n = 0; ; ++n)
    {
        char ns[64] = "";
        if (n != 0)
            _snprintf_s(ns, sizeof(ns), "-%u", n);

        const std::string avs_template_name = dir_ + "\\" + "template" + ns + ".avs";
        const std::string vd_template_name = dir_ + "\\" + "template" + ns + ".vdscript";
        const std::string ren_template_name = dir_ + "\\" + "template" + ns + ".render";
        const std::string render_bat_name = dir_ + "\\" + "render" + ns + ".bat";

        const bool avs = process_entry(avs_template_name, ren_template_name, render_bat_name, ns);
        const bool vd = process_entry(vd_template_name, ren_template_name, render_bat_name, ns);

        if (!avs && !vd)
            break;
    }
}

void S8AvsCreator::replace_word(std::string& str, const std::string& word, const std::string& word_new)
{
    for (;;)
    {
        size_t pos = str.find(word);
        if (pos == std::string::npos)
            break;
        str = str.substr(0, pos) + word_new + str.substr(pos + word.size());
    }
}

bool S8AvsCreator::load_template(const char* name, std::vector<std::string>& templ)
{
    tstring tname = string_utils::to_tstring(name, CP_UTF8);

    struct _stat sb;
    if (_tstat(tname.c_str(), &sb) != 0)
    {
//        std::cout << "Couldn't stat \"" << name << "\"" << std::endl;
        return false;
    }

    std::ifstream tmpl(tname);
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
