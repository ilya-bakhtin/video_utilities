#include "s8-avs-creator.h"

#include "string_utils.h"

#include <iostream>
#include <string>

S8AvsCreator::S8AvsCreator(const TCHAR* template_name, std::istream& entries_file):
    template_name_(template_name),
    entries_file_(entries_file)
{

}

void S8AvsCreator::process_entries()
{
    std::ifstream tmpl(template_name_);
    if (!tmpl.is_open())
    {
        std::cout << "Unable to open file " << template_name_ << std::endl;
        return;
    }

    std::string line;
    while (tmpl.good())
    {
        std::getline(tmpl, line);
        template_.push_back(line);
    }
    tmpl.close();

    tstring tname(template_name_);
    const size_t p = tname.rfind(_T("."));
    tstring ext;
    if (p != std::string::npos)
        ext = tname.substr(p).c_str();

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

        const tstring of_name = string_utils::to_tstring(clip.c_str()) + ext;
        std::ofstream of(of_name);
        if (!of.is_open())
        {
            std::wcout << "Unable to open file " << of_name << std::endl;
            return;
        }

        for (std::vector<std::string>::const_iterator i = template_.begin(); i != template_.end(); ++i)
        {
            std::string s = *i;
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
