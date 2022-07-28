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
    S8AvsCreator(const tstring& dir, std::istream& entries_file);
    void process_entries(const tstring& template_name);

private:
    std::string dir_;
    std::istream& entries_file_;
    std::vector<std::string> entries_;

    bool process_entry(const std::string& name, const std::string& render_file, const std::string& ns);
    void replace_word(std::string& str, const std::string& word, const std::string& word_new);
};

#endif
