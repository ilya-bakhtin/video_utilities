#ifndef __SEGMENT_H__
#define __SEGMENT_H__

#include <string>

namespace prepare_avs {

class Segment
{
public:
    Segment();
    virtual ~Segment();

    virtual std::string get_name() const = 0;
    virtual bool is_single() const = 0;
    virtual std::string get_time_str() const = 0;
    bool operator<(const Segment& other) const;

private:
    Segment(const Segment& other);
    Segment(const Segment&& other);
    Segment& operator=(const Segment& other);
    Segment& operator=(const Segment&& other);
};

class SingleFileSegment: public Segment
{
public:
    SingleFileSegment(const std::string& name);

    virtual std::string get_name() const;
    virtual bool is_single() const;

private:
    std::string name_;
    virtual std::string get_time_str() const;
};

class AviFileSegment: public Segment
{
public:
    AviFileSegment(const std::string& descr, int timeshift);

    virtual std::string get_name() const;
    virtual bool is_single() const;

private:
    std::string descr_;
    int timeshift_;
    virtual std::string get_time_str() const;
};

} // namespace

#endif
