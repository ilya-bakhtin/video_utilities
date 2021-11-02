#include "segment.h"

#include "string_utils.h"

#include <time.h>

namespace prepare_avs {

Segment::Segment()
{
}

Segment::~Segment()
{
}

bool Segment::operator<(const Segment& other) const
{
    return get_time_str() < other.get_time_str();
}

SingleFileSegment::SingleFileSegment(const std::string& name) :
    name_(name)
{
}

std::string SingleFileSegment::get_name() const
{
    return name_;
}

bool SingleFileSegment::is_single() const
{
    return true;
}

std::string SingleFileSegment::get_time_str() const
{
    std::string res;

    for (std::string::const_iterator i = name_.begin(); i != name_.end() && *i != '.'; ++i)
    {
        if (*i >= '0' && *i <= '9')
            res += *i;
    }
    return res;
}

AviFileSegment::AviFileSegment(const std::string& descr, int timeshift) :
    descr_(descr),
    timeshift_(timeshift)
{
}

std::string AviFileSegment::get_name() const
{
    size_t pos = descr_.find("]");
    if (pos == std::string::npos || pos + 1 >= descr_.length())
        return "";
    return string_utils::string_trim(descr_.substr(pos + 1));
}

std::string AviFileSegment::get_time_str() const
{
    std::string res;

    for (std::string::const_iterator i = descr_.begin(); i != descr_.end() && *i != ']'; ++i)
    {
        if (*i >= '0' && *i <= '9')
            res += *i;
    }
    if (timeshift_ != 0)
    {
        std::string s_year = res.substr(0, 4);
        std::string s_mon = res.substr(4, 2);
        std::string s_mday = res.substr(6, 2);
        std::string s_hour = res.substr(8, 2);
        std::string s_min = res.substr(10, 2);
        std::string s_sec = res.substr(12, 2);

        struct tm seg_time = {0,};

        if (!s_year.empty() && !s_mon.empty() && !s_mday.empty() && !s_hour.empty() && !s_min.empty() && !s_sec.empty() &&
            sscanf(s_year.c_str(), "%d", &seg_time.tm_year) == 1 &&
            sscanf(s_mon.c_str(), "%d", &seg_time.tm_mon) == 1 &&
            sscanf(s_mday.c_str(), "%d", &seg_time.tm_mday) == 1 &&
            sscanf(s_hour.c_str(), "%d", &seg_time.tm_hour) == 1 &&
            sscanf(s_min.c_str(), "%d", &seg_time.tm_min) == 1 &&
            sscanf(s_sec.c_str(), "%d", &seg_time.tm_sec) == 1)
        {
            seg_time.tm_year -= 1900;
            seg_time.tm_mon -= 1;
            time_t stime = _mkgmtime(&seg_time);
            if (stime != -1)
            {
                stime += timeshift_;
                struct tm* gmt = gmtime(&stime);
                if (gmt != NULL)
                {
                    seg_time = *gmt;
                    char buf[32];
                    strftime(buf, sizeof(buf), "%Y%m%d%H%M%S", &seg_time);
                    res = buf;
                }
            }
        }
    }
    return res;
}

bool AviFileSegment::is_single() const
{
    return false;
}

} // namespace
