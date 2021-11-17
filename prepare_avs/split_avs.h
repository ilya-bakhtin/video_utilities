#ifndef __SPLIT_AVS_H__
#define __SPLIT_AVS_H__

#include "string_utils.h"

#include <fstream>
#include <vector>

class SplitAvs
{
public:
    SplitAvs();
    void process_file(const tstring& filename);

private:
    bool started_;
    bool param_done_;
    bool body_started_;
    bool block_started_;
    int first_num_;
    std::vector<std::string> block_;
};

#endif
