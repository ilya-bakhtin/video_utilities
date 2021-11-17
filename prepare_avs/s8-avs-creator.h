#ifndef __S8_AVS_CREATOR_H__
#define __S8_AVS_CREATOR_H__

#include <fstream>
#include <string>
#include <vector>

#include <windows.h>

#include "string_utils.h"

class S8AvsCreator
{
public:
    S8AvsCreator(const tstring& dir, const tstring& template_name, std::istream& entries_file);
    void process_entries();

private:
    std::string dir_;
    std::string template_name_;
    std::istream& entries_file_;
    std::vector<std::string> template_;

    void replace_word(std::string& str, const std::string& word, const std::string& word_new);
};

#endif
