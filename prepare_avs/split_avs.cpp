#include "split_avs.h"

#include "string_utils.h"

#include <iostream>
#include <string>

SplitAvs::SplitAvs():
    started_(false),
    param_done_(false),
    body_started_(false),
    block_started_(false),
    first_num_(0)
{
}

void SplitAvs::process_file(std::istream& file)
{
    while (file.good())
    {
        std::string line;
        std::getline(file, line);

        if (line == "#******")
        {
            if (started_)
                break;
            started_ = true;
            continue;
        }

        if (!started_)
            continue;

        if (!param_done_)
        {
            if (line.empty())
                continue;

            if (line[0] == '#')
            {
                line = line.substr(1);
                if (sscanf(line.c_str(), "%d", &first_num_) != 1)
                {
                    std::cerr << "parameter string is not found" << std::endl;
                    break;
                }
                param_done_ = true;
                continue;
            }
        }

        if (!body_started_)
        {
            if (line.empty())
                continue;
            body_started_ = true;
        }

//        std::ostream& out = std::cout;

        if (line.empty())
        {
            block_started_ = false;
            if (!block_.empty())
            {
                std::string last = string_utils::string_tolower(block_[block_.size() - 1]);
//std::cout << "**** " << last << std::endl;
                if (last.find("adjustframerate") != std::string::npos || last.find("assumefps(24)") != std::string::npos)
                {
//                    out << "v" << first_num_ << "r.avs" << std::endl;
                    char buf[128];
                    sprintf(buf, "v%dr.avs", first_num_);
                    std::ofstream out(buf);
                    if (!out.is_open())
                    {
                        std::cout << "Unable to open file " << buf << std::endl;
                        return;
                    }
                    out << "import(\"11449 Bakhtin 02 N8.720p_04.mov.avs\")" << std::endl;

                    for (std::vector<std::string>::const_iterator i = block_.begin(); i != block_.end(); ++i)
                        out << *i << std::endl;

                    out.close();

                    ++first_num_;
                }
                block_.clear();
            }
        }
        else
        {
            if (!block_started_)
                block_started_= true;
//            out << line << std::endl;
            block_.push_back(line);
        }
    }
}
