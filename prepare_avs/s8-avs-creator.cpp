#include <iostream>
#include <string>

#include "s8-avs-creator.h"

S8AvsCreator::S8AvsCreator(const tstring& dir, const tstring& template_name, std::istream& entries_file):
    dir_(string_utils::to_ansi_string(dir.c_str(), CP_UTF8)),
    template_name_(string_utils::to_ansi_string(template_name.c_str(), CP_UTF8)),
    entries_file_(entries_file)
{
}

void S8AvsCreator::process_entries()
{
    std::ifstream tmpl(string_utils::to_tstring(template_name_.c_str(), CP_UTF8));
    if (!tmpl.is_open())
    {
        std::cout << "Unable to open file " << template_name_.c_str() << std::endl;
        return;
    }

    std::string line;
    while (tmpl.good())
    {
        std::getline(tmpl, line);
        template_.push_back(line);
    }
    tmpl.close();

    const size_t p = template_name_.rfind(".");
    std::string ext;
    if (p != std::string::npos)
        ext = template_name_.substr(p).c_str();

    const bool is_vdscript = ext == ".vdscript";

    std::string dir = dir_ + "\\";
    if (is_vdscript)
    {
        std::string double_backslash_dir;
        for (std::string::const_iterator i = dir.begin(); i != dir.end(); ++i)
        {
            if (*i == '\\')
                double_backslash_dir += "\\\\";
            else
                double_backslash_dir += *i;
        }
        dir = double_backslash_dir;
    }

    while (entries_file_.good())
    {
        std::getline(entries_file_, line);
        if (line.empty())
            continue;

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

        const std::string of_name = clip + ext;
        std::ofstream of(string_utils::to_tstring(of_name.c_str(), CP_UTF8));
        if (!of.is_open())
        {
            std::cout << "Unable to open file " << of_name << std::endl;
            return;
        }

        for (std::vector<std::string>::const_iterator i = template_.begin(); i != template_.end(); ++i)
        {
            std::string s = *i;
            replace_word(s, "$$$dir$$$", dir);
            replace_word(s, "$$$clip$$$", clip);
            replace_word(s, "$$$sad$$$", sad);
            replace_word(s, "$$$sadc$$$", sadc);

            of << s << std::endl;
        }

        of.close();
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
