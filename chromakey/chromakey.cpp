// chromakey.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include "imageprocessor.h"

#include "iostream"
#include "memory"
#include "string"
#include "vector"

#include "process.h" // TODO
#include "time.h" // TODO

namespace {

class guard
{
public:
	guard():
		dismissed_(false)
	{
	}

	virtual ~guard()
	{
		dismiss();
	}

	void dismiss()
	{
		if (!dismissed_)
		{
			action();
			dismissed_ = true;
		}
	}

private:
	bool dismissed_;

	guard(const guard&);
	guard& operator=(const guard&);

	virtual void action() {}
};

class gdiplus_guard: public guard
{
public:
	gdiplus_guard(ULONG_PTR gdiplusToken):
	  gdiplusToken_(gdiplusToken)
	{
	}

private:
	ULONG_PTR gdiplusToken_;

	virtual void action()
	{
		// Shutdown Gdiplus 
		Gdiplus::GdiplusShutdown(gdiplusToken_); 
	}
};

class image_guard: public guard
{
public:
	image_guard(Gdiplus::Image* image):
	  image_(image)
	{
	}

private:
	Gdiplus::Image* image_;

	virtual void action()
	{
		delete image_;
	}
};

static
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
	UINT  num = 0;          // number of image encoders
	UINT  size = 0;         // size of the image encoder array in bytes

	Gdiplus::ImageCodecInfo* pImageCodecInfo = NULL;

	Gdiplus::GetImageEncodersSize(&num, &size);
	if (size == 0)
		return -1;  // Failure

	pImageCodecInfo = (Gdiplus::ImageCodecInfo*)(malloc(size));
	if (pImageCodecInfo == NULL)
		return -1;  // Failure

	GetImageEncoders(num, size, pImageCodecInfo);

	for (UINT j = 0; j < num; ++j)
	{
		if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0)
		{
			*pClsid = pImageCodecInfo[j].Clsid;
			free(pImageCodecInfo);
			return j;  // Success
		}    
	}

	free(pImageCodecInfo);
	return -1;  // Failure
}

class options: public img_options
{
public:
	options(int argc, const TCHAR* const* argv);

	bool parse_commandline();

	const tstring& err_msg() const;

	bool is_help() const;

	const std::wstring& in_file() const;
	const std::wstring& out_file() const;
	const tstring& t_in_file() const;
	const tstring& t_out_file() const;

	void get_crop(unsigned int& left, unsigned int& top,
					unsigned int& right, unsigned int& bottom) const;
	back_type get_back(Gdiplus::Color& flat_color, int_point& point,
						std::vector<Gdiplus::Color>& gradient_colors,
						std::vector<int_point>& gradient_points) const;
	void get_mult(double& lt, double& rt, double& lb, double& rb) const;
	bool get_circle(double& cx, double& cy, double& cr) const;
	void get_circle_params(double& offset, double& bias) const;
	bool get_circle_debug(Gdiplus::Color& debug_color) const;
	int get_threads() const;
	const adjust_edge_arg& get_edge_arg() const;

private:
	int argc_;
	const TCHAR* const* argv_;
	int arg_pos_;

	tstring err_msg_;

	bool help_;

	std::wstring in_filename_;
	std::wstring out_filename_;
#if !defined(UNICODE)
	tstring t_in_filename_;
	tstring t_out_filename_;
#endif

	unsigned int crop_left_;
	unsigned int crop_top_;
	unsigned int crop_right_;
	unsigned int crop_bottom_;

	back_type back_type_;
	Gdiplus::Color back_color_;
	int_point back_point_;
	std::vector<Gdiplus::Color> gradient_colors_;
	std::vector<int_point> gradient_points_;

	double back_mul_LT_;
	double back_mul_RT_;
	double back_mul_LB_;
	double back_mul_RB_;
	bool circle_;
	double circle_x_;
	double circle_y_;
	double circle_r_;
	double circle_offset_;
	double circle_bias_;
	bool circle_debug_;
	Gdiplus::Color debug_color_;
	unsigned int threads_;
	adjust_edge_arg edge_arg_;

	bool parse_prologue(const tstring& arg, bool& parsed, tstring* values,
						const tstring& prm_name, const tstring& short_flag, const tstring& long_flag);

	bool parse_help(const tstring& arg, bool& parsed);
	bool parse_color(const tstring& color_str, Gdiplus::Color& color, const tstring& param_name);
	bool parse_crop(const tstring& arg, bool& parsed);
	bool parse_mult(const tstring& arg, bool& parsed);
	bool parse_circle(const tstring& arg, bool& parsed);
	bool parse_circle_params(const tstring& arg, bool& parsed);
	bool parse_circle_debug(const tstring& arg, bool& parsed);
	bool parse_single_back_literal(const tstring& arg, bool& parsed);
	bool parse_single_back_point(const tstring& arg, bool& parsed);
	bool parse_gradient_back_literal(const tstring& arg, bool& parsed);
	bool parse_gradient_back_points(const tstring& arg, bool& parsed);
	bool parse_threads(const tstring& arg, bool& parsed);
	bool parse_adjust_params(const tstring& arg, bool& parsed);
	bool parse_adjust_debug(const tstring& arg, bool& parsed);
};

}

img_options::adjust_edge_arg::adjust_edge_arg(
					bool adjust_edge,
					bool adjust_edge_dbg,
					unsigned int edge_x_gap,
					unsigned int edge_y_gap,
					double edge_thresh,
					double edge_coeff,
					Gdiplus::Color edge_debug_color):
	adjust_edge_(adjust_edge),
	adjust_edge_dbg_(adjust_edge_dbg),
	edge_x_gap_(edge_x_gap),
	edge_y_gap_(edge_y_gap),
	edge_thresh_(edge_thresh),
	edge_coeff_(edge_coeff),
	edge_debug_color_(edge_debug_color)
{
}

namespace {

options::options(int argc, const TCHAR* const* argv):
	argc_(argc),
	argv_(argv),
	arg_pos_(0),
	help_(false),
	crop_left_(0),
	crop_top_(0),
	crop_right_(0),
	crop_bottom_(0),
	back_type_(flat_point),
	back_color_(0, 255, 0),
	back_mul_LT_(1),
	back_mul_RT_(1),
	back_mul_LB_(1),
	back_mul_RB_(1),
	circle_(false),
	circle_x_(0),
	circle_y_(0),
	circle_r_(0),
	circle_offset_(0),
	circle_bias_(1000),
	circle_debug_(false),
	debug_color_(0, 0, 0),
	threads_(0),
	edge_arg_(false, false, 5, 5, 0.85, 0.3, Gdiplus::Color(0, 0, 0))
{
	gradient_colors_.reserve(4);
	gradient_points_.resize(4);
	for (int i = 0; i < 4; ++i)
		gradient_colors_.push_back(Gdiplus::Color(0, 255, 0));

};

static
bool to_wstring(std::wstring& dst, LPCTSTR src)
{
#ifdef UNICODE
	dst = src;
#else
	size_t size = mbstowcs(NULL, src, 0) + 1;
	if (size == static_cast<size_t>(-1))
	{
		// can not convert
		return false;
	}

	std::vector<wchar_t> dest;
	dest.resize(size, 0);

	size = mbstowcs(&dest[0], src, size);
	if (size == static_cast<size_t>(-1))
	{
		// can not convert
		return false;
	}

	dst = &dest[0];
#endif
	return true;
}

bool options::parse_commandline()
{
	for (arg_pos_ = 1; arg_pos_ < argc_; ++arg_pos_)
	{
		tstring arg(argv_[arg_pos_]);

		if (arg[0] != _T('-'))
		{
			// filename
			if (in_filename_.empty())
			{
				if (!to_wstring(in_filename_, arg.c_str()))
				{
					err_msg_ = _T("can not use input file name ") + arg;
					return false;
				}
			}
			else
			{
				if (!to_wstring(out_filename_, arg.c_str()))
				{
					err_msg_ = _T("can not use output file name ") + arg;
					return false;
				}
			}
			continue;
		}

		bool parsed;

		if (!parse_help(arg, parsed))
			return false;

		if (!parsed && !parse_crop(arg, parsed))
			return false;

		if (!parsed && !parse_mult(arg, parsed))
			return false;

		if (!parsed && !parse_circle(arg, parsed))
			return false;

		if (!parsed && !parse_circle_params(arg, parsed))
			return false;

		if (!parsed && !parse_circle_debug(arg, parsed))
			return false;
	
		if (!parsed && !parse_single_back_literal(arg, parsed))
			return false;

		if (!parsed && !parse_single_back_point(arg, parsed))
			return false;

		if (!parsed && !parse_gradient_back_literal(arg, parsed))
			return false;

		if (!parsed && !parse_gradient_back_points(arg, parsed))
			return false;

		if (!parsed && !parse_threads(arg, parsed))
			return false;

		if (!parsed && !parse_adjust_params(arg, parsed))
			return false;

		if (!parsed && !parse_adjust_debug(arg, parsed))
			return false;

		if (!parsed)
		{
			err_msg_ = _T("unknown flag ") + arg;
			return false;
		}
	}

	return true;
}

bool options::parse_prologue(const tstring& arg, bool& parsed, tstring* values,
							 const tstring& prm_name, const tstring& short_flag, const tstring& long_flag)
{
	parsed = false;

	bool sflag = arg == short_flag;
	size_t fpos = sflag ? tstring::npos : arg.find(long_flag);

	if (!sflag && fpos == tstring::npos)
		return true;

	if (values != NULL)
	{
		if (sflag)
		{
			if (arg_pos_ + 1 >= argc_)
			{
				err_msg_ = prm_name + _T(" parameters missing");
				return false;
			}

			++arg_pos_;
			*values = argv_[arg_pos_];
		}
		else
		{
			*values = arg.substr(long_flag.size());
		}
	}

	parsed = true;

	return true;
}

bool options::parse_help(const tstring& arg, bool& parsed)
{
	if (!parse_prologue(arg, parsed, NULL, _T("help"), _T("-h"), _T("--help")))
		return false;

	if (parsed)
		help_ = true;

	return true;
}

bool options::parse_crop(const tstring& arg, bool& parsed)
{
	tstring crop;

	if (!parse_prologue(arg, parsed, &crop, _T("crop"), _T("-c"), _T("--crop=")))
		return false;

	if (!parsed)
		return true;

	parsed = false;

// TODO trim?
	crop = crop.substr(1, crop.size()-2);

	if (_stscanf(crop.c_str(), _T("%d, %d, %d, %d"),
					&crop_left_, &crop_top_, &crop_right_, &crop_bottom_) != 4)
	{
		err_msg_ = _T("can not parse crop parameters");
		return false;
	}

	parsed = true;

	return true;
}

bool options::parse_mult(const tstring& arg, bool& parsed)
{
	tstring mult;

	if (!parse_prologue(arg, parsed, &mult, _T("mult"), _T("-m"), _T("--mult=")))
		return false;

	if (!parsed)
		return true;

	parsed = false;

// TODO trim?
	if (mult[0] == _T('('))
	{
		mult = mult.substr(1, mult.size()-2);

		if (_stscanf(mult.c_str(), _T("%lg, %lg, %lg, %lg"),
						&back_mul_LT_, &back_mul_RT_, &back_mul_LB_, &back_mul_RB_) != 4)
		{
			err_msg_ = _T("can not parse mult parameters");
			return false;
		}
	}
	else
	{
		if (_stscanf(mult.c_str(), _T("%lg"), &back_mul_LT_) != 1)
		{
			err_msg_ = _T("can not parse mult parameters");
			return false;
		}
		back_mul_RT_ = back_mul_LB_ = back_mul_RB_ = back_mul_LT_;
	}

	parsed = true;

	return true;
}

bool options::parse_circle(const tstring& arg, bool& parsed)
{
	tstring circle;

	if (!parse_prologue(arg, parsed, &circle, _T("circle"), _T("-r"), _T("--circle=")))
		return false;

	if (!parsed)
		return true;

	parsed = false;

// TODO trim?
	circle = circle.substr(1, circle.size()-2);

	if (_stscanf(circle.c_str(), _T("%lg, %lg, %lg"),
					&circle_x_, &circle_y_, &circle_r_) != 3)
	{
		err_msg_ = _T("can not parse circle parameters");
		return false;
	}

	parsed = true;
	circle_ = true;

	return true;
}

bool options::parse_circle_params(const tstring& arg, bool& parsed)
{
	tstring circle_prm;

	if (!parse_prologue(arg, parsed, &circle_prm, _T("circle offset, bias:"), _T("-R"), _T("--circle_parameters=")))
		return false;

	if (!parsed)
		return true;

	parsed = false;

// TODO trim?
	circle_prm = circle_prm.substr(1, circle_prm.size()-2);

	if (_stscanf(circle_prm.c_str(), _T("%lg, %lg"),
					&circle_offset_, &circle_bias_) != 2)
	{
		err_msg_ = _T("can not parse circle offset, bias");
		return false;
	}

	parsed = true;

	return true;
}

bool options::parse_circle_debug(const tstring& arg, bool& parsed)
{
	tstring circle_dbg;

	if (!parse_prologue(arg, parsed, &circle_dbg, _T("circle debug"), _T("-d"), _T("--circle_debug=")))
		return false;

	if (!parsed)
		return true;

	parsed = false;

// TODO trim?
	if (!parse_color(circle_dbg, debug_color_, _T("circle debug")))
		return false;

	parsed = true;
	circle_debug_ = true;

	return true;
}

bool options::parse_single_back_literal(const tstring& arg, bool& parsed)
{
	tstring back;

	if (!parse_prologue(arg, parsed, &back, _T("single color back"), _T("-b"), _T("--back=")))
		return false;

	if (!parsed)
		return true;

	parsed = false;

// TODO trim?
	if (!parse_color(back, back_color_, _T("single color back")))
		return false;

	parsed = true;
	back_type_ = flat_color;

	return true;
}

bool options::parse_gradient_back_literal(const tstring& arg, bool& parsed)
{
	tstring back;

	if (!parse_prologue(arg, parsed, &back, _T("gradient colors back"), _T("-B"), _T("--gradient_back=")))
		return false;

	if (!parsed)
		return true;

	parsed = false;

// TODO trim?
	back = back.substr(1, back.size()-2);

	size_t offs = 0;
	for (int i = 0; i < 4; ++i)
	{
		tstring color;

		if (i == 3)
			color = back.substr(offs, back.size()-offs);
		else
		{
			size_t cp;
			if (back[offs] != '(')
				cp = back.find(',', offs);
			else
			{
				cp = back.find(_T("),"), offs);
				if (cp != std::string::npos)
					++cp;
			}

			if (cp == std::string::npos)
			{
				err_msg_ = _T("can not parse gradient colors back parameters ") + back;
				return false;
			}
			else
			{
				color = back.substr(offs, cp-offs);
				offs = cp + 1;
			}
		}
		if (!parse_color(color, gradient_colors_[i], _T("gradient colors back")))
			return false;
	}

	parsed = true;
	back_type_ = gradient_color;

	return true;
}

bool options::parse_single_back_point(const tstring& arg, bool& parsed)
{
	tstring back;

	if (!parse_prologue(arg, parsed, &back, _T("single color point"), _T("-p"), _T("--point")))
		return false;

	if (!parsed)
		return true;

	parsed = false;

// TODO trim?
	if (back.empty() || back == _T("-"))
	{
		back_point_.x = 0;
		back_point_.y = 0;
	}
	else
	{
		size_t f = 1;
		if (back[0] == _T('='))
			++f;

		back = back.substr(f, back.size()-1-f);

		if (_stscanf(back.c_str(), _T("%d, %d"), &back_point_.x, &back_point_.y) != 2)
		{
			err_msg_ = _T("can not parse single color point parameters ") + back;
			return false;
		}
	}

	parsed = true;
	back_type_ = flat_point;

	return true;
}

bool options::parse_gradient_back_points(const tstring& arg, bool& parsed)
{
//		"\t-P - | ((ltX,ltY),(rtX,rtY),(lbX,lbY),(lbX,lbY)), --gradient_points<=((ltX,ltY),(rtX,rtY),(lbX,lbY),(lbX,lbY))>\n" <<
	tstring back;

	if (!parse_prologue(arg, parsed, &back, _T("gradient points"), _T("-P"), _T("--gradient_points")))
		return false;

	if (!parsed)
		return true;

	parsed = false;

// TODO trim?
	if (back.empty() || back == _T("-"))
	{
		back_type_ = gradient_def_points;
	}
	else
	{
		size_t f = 1;
		if (back[0] == _T('='))
			++f;

		back = back.substr(f, back.size()-1-f);

		size_t offs = 0;
		for (int i = 0; i < 4; ++i)
		{
			tstring coord;

			if (i == 3)
				coord = back.substr(offs, back.size()-offs);
			else
			{
				size_t cp;
				if (back[offs] != '(')
					cp = back.find(',', offs);
				else
				{
					cp = back.find(_T("),"), offs);
					if (cp != std::string::npos)
						++cp;
				}

				if (cp == std::string::npos)
				{
					err_msg_ = _T("can not parse gradient points parameters ") + back;
					return false;
				}
				else
				{
					coord = back.substr(offs, cp-offs);
					offs = cp + 1;
				}
			}
			coord = coord.substr(1, coord.size()-2);
			if (_stscanf(coord.c_str(), _T("%d, %d"), &gradient_points_[i].x, &gradient_points_[i].y) != 2)
			{
				err_msg_ = _T("can not parse gradient points parameters ") + back;
				return false;
			}
		}
		back_type_ = gradient_points;
	}

	parsed = true;

	return true;
}

bool options::parse_color(const tstring& in_color_str, Gdiplus::Color& color, const tstring& param_name)
{
	Gdiplus::ARGB RGB;
	tstring color_str(in_color_str);

	if (in_color_str[0] == _T('('))
	{
		color_str = color_str.substr(1, color_str.size()-2);

		int R;
		int G;
		int B;
		if (_stscanf(color_str.c_str(), _T("%d, %d, %d"), &R, &G, &B) != 3)
		{
			err_msg_ = _T("can not parse ") + param_name + _T(" parameters ") + color_str;
			return false;
		}
		R &= 0xFF;
		G &= 0xFF;
		B &= 0xFF;

		RGB = (R << 16) | (G << 8) | B;
	}
	else
	{
		if (_stscanf(color_str.c_str(), _T("%x"), &RGB) != 1)
		{
			err_msg_ = _T("can not parse ") + param_name + _T(" parameters ") + color_str;
			return false;
		}
	}

	RGB |= 0xFF000000;

	color.SetValue(RGB);

	return true;
}

bool options::parse_threads(const tstring& arg, bool& parsed)
{
	tstring threads;

	if (!parse_prologue(arg, parsed, &threads, _T("threads"), _T("-t"), _T("--threads=")))
		return false;

	if (!parsed)
		return true;

	parsed = false;

// TODO trim?
	if (_stscanf(threads.c_str(), _T("%d"), &threads_) != 1)
	{
		err_msg_ = _T("can not parse threads parameters");
		return false;
	}

	if (threads_ < 0)
		threads_ = 0;

	parsed = true;

	return true;
}

bool options::parse_adjust_params(const tstring& arg, bool& parsed)
{
	tstring aep;

	if (!parse_prologue(arg, parsed, &aep, _T("adjust edge params"), _T("-e"), _T("--adjust_edges")))
		return false;

	if (!parsed)
		return true;

	parsed = false;

// TODO trim?
	if (aep.empty() || aep == _T("-"))
	{
		edge_arg_.edge_x_gap_ = 5;
		edge_arg_.edge_y_gap_ = 5;
		edge_arg_.edge_thresh_ = 0.85;
		edge_arg_.edge_coeff_ = 0.3;
	}
	else
	{
		size_t f = 1;
		if (aep[0] == _T('='))
			++f;

		aep = aep.substr(f, aep.size()-1-f);
		if (_stscanf(aep.c_str(), _T("%d, %d, %lg, %lg"),
					 &edge_arg_.edge_x_gap_, &edge_arg_.edge_y_gap_,
					 &edge_arg_.edge_thresh_, &edge_arg_.edge_coeff_) != 4)
		{
			err_msg_ = _T("can not parse adjust edge parameters ") + aep;
			return false;
		}
	}

	parsed = true;
	edge_arg_.adjust_edge_ = true;

	return true;
}

bool options::parse_adjust_debug(const tstring& arg, bool& parsed)
{
	tstring edge_dbg;

	if (!parse_prologue(arg, parsed, &edge_dbg, _T("edge debug"), _T("-g"), _T("--edge_debug=")))
		return false;

	if (!parsed)
		return true;

	parsed = false;

// TODO trim?
	if (!parse_color(edge_dbg, edge_arg_.edge_debug_color_, _T("edge debug")))
		return false;

	parsed = true;
	edge_arg_.adjust_edge_dbg_ = true;

	return true;
}

const tstring& options::err_msg() const
{
	return err_msg_;
}

bool options::is_help() const
{
	return help_;
}

const std::wstring& options::in_file() const
{
	return in_filename_;
}

const std::wstring& options::out_file() const
{
	return out_filename_;
}

const tstring& options::t_in_file() const
{
#ifdef UNICODE
	return in_file();
#else
	return t_in_filename_;
#endif
}

const tstring& options::t_out_file() const
{
#ifdef UNICODE
	return out_file();
#else
	return t_out_filename_;
#endif
}

void options::get_crop(unsigned int& left, unsigned int& top,
						unsigned int& right, unsigned int& bottom) const
{
	left = crop_left_;
	top = crop_top_;
	right = crop_right_;
	bottom = crop_bottom_;
}

options::back_type options::get_back(Gdiplus::Color& flat_color, int_point& point,
									 std::vector<Gdiplus::Color>& gradient_colors,
									 std::vector<int_point>& gradient_points) const
{
	flat_color = back_color_;
	point = back_point_;
	gradient_colors = gradient_colors_;
	gradient_points = gradient_points_;
	return back_type_;
}

void options::get_mult(double& lt, double& rt, double& lb, double& rb) const
{
	lt = back_mul_LT_;
	rt = back_mul_RT_;
	lb = back_mul_LB_;
	rb = back_mul_RB_;
}

bool options::get_circle(double& cx, double& cy, double& cr) const
{
	cx = circle_x_;
	cy = circle_y_;
	cr = circle_r_;

	return circle_;
}

void options::get_circle_params(double& offset, double& bias) const
{
	offset = circle_offset_;
	bias = circle_bias_;
}

bool options::get_circle_debug(Gdiplus::Color& debug_color) const
{
	debug_color = debug_color_;
	return circle_debug_;
}

int options::get_threads() const
{
	return threads_;
}

const img_options::adjust_edge_arg& options::get_edge_arg() const
{
	return edge_arg_;
}

static
void usage(bool brief)
{
	if (brief)
	{
		std::cout <<
			"Usage: chromakey [options] file_in [file_out]\n" <<
			"Example: chromakey -c (10,20,30,40) -m (1.1,1.3,1.,1.) in.png out.png\n" <<
			"Try `chromakey --help' for more information\n" << std::endl;
		return;
	}

	std::cout <<
		"chromakey options:\n" <<
		"\t-h, --help print this help message\n" <<
		"\t-c (Left,Top,Right,Bottom), --crop=(Left,Top,Right,Bottom)\n" <<
		"\t-b RGB | (R,G,B), --back=RGB | (R,G,B)\n" <<
		"\t-p - | (px,py), --point<=(px,py)>\n" <<
		"\t\tleft top point will be used if \"-p -\" or \"-point\" is submitted" <<
		"\t-B (ltRGB,rtRGB,lbRGB,rbRGB) | ((ltR,ltG,ltB),(rtR,rtG,rtB),(lbR,lbG,lbB),(rbR,rbG,rbB)), --gradient_back=(ltRGB,rtRGB,lbRGB,rbRGB) | ((ltR,ltG,ltB),(rtR,rtG,rtB),(lbR,lbG,lbB),(rbR,rbG,rbB))\n" <<
		"\t-P - | ((ltX,ltY),(rtX,rtY),(lbX,lbY),(lbX,lbY)), --gradient_points<=((ltX,ltY),(rtX,rtY),(lbX,lbY),(lbX,lbY))>\n" <<
		"\t\tLT RT LB RB points will be used if \"-P -\" or \"--gradient_points\" is submitted" <<
		"\t-m coeff | (cLT,cRT,cLB,cRB), --mult=coeff | (cLT,cRT,cLB,cRB)\n" <<
		"\t\tfloating point values for multiplication coefficients are welcome\n"
		"\t-r (x,y,r), --circle=(x,y,r)\n" <<
		"\t-R (offset,bias), --circle_parameters=(offset,bias)\n" <<
		"\t\tdefault values for (offset,bias) are (0,1000)\n" <<
		"\t-d RGB | (R,G,B) --circle_debug=RGB | (R,G,B)\n" <<
		"\t\tsingle RGB has to be hex, (R,G,B) must be dec\n" <<
		"\t-t threads --threads=threads\n" <<
		"\t-e - | (wx,wy,threshhold,coeff), --adjust_edges<=(wx,wy,threshhold,coeff)>\n" <<
		"\t\tdefault walues are (5,5,0.85,0.3)" <<
		"\t-g RGB | (R,G,B) --edge_debug=RGB | (R,G,B)\n" <<
		std::endl;
}

} // namespace

class handles_guard: public guard
{
public:
	explicit handles_guard(const std::vector<HANDLE>& handles):
		handles_(&handles),
		handle_(NULL)
	{
	}
	explicit handles_guard(HANDLE handle):
		handles_(NULL),
		handle_(handle)
	{
	}

private:
	const std::vector<HANDLE>* handles_;
	HANDLE handle_;

	virtual void action()
	{
		if (handle_ != NULL)
			::CloseHandle(handle_);

		if (handles_ != NULL)
		{
			for (size_t i = 0; i < (*handles_).size(); ++i)
			{
				if ((*handles_)[i] != NULL)
					::CloseHandle((*handles_)[i]);
			}
		}
	}
};

class bits_guard: public guard
{
public:
	bits_guard(Gdiplus::Bitmap& bmp, Gdiplus::BitmapData& data):
		bmp_(bmp),
		data_(data)
	{
	}

private:
	Gdiplus::Bitmap& bmp_;
	Gdiplus::BitmapData& data_;

	virtual void action()
	{
		bmp_.UnlockBits(&data_);
	}
};

struct worker_arg
{
	worker_arg(imageProcessor* proc, volatile LONG* threads, HANDLE second_step_enable):
		remainder_(threads),
		second_step_enable_(second_step_enable),
		proc_(proc)
	{
	}

	volatile LONG* remainder_;
	HANDLE second_step_enable_;
	imageProcessor* proc_;
};

static
unsigned __stdcall worker(void* arg)
{
	worker_arg* a = reinterpret_cast<worker_arg*>(arg);
	a->proc_->prepare_alpha();

	if (::InterlockedDecrement(a->remainder_) == 0)
		::SetEvent(a->second_step_enable_);
	::WaitForSingleObject(a->second_step_enable_, INFINITE);

	a->proc_->process_bitmap();
	return 0;
}

int _tmain(int argc, _TCHAR* argv[])
{
	options opt(argc, argv);

	if (!opt.parse_commandline())
	{
		tcerr << _T("chromakey: ") << opt.err_msg() << std::endl;
		if (opt.in_file().empty())
			tcerr << _T("chromakey: Input file is not specified") << std::endl;
		if (opt.out_file().empty())
			tcerr << _T("chromakey: Output file is not specified") << std::endl;

		usage(true);
		return 1;
	}

	if (opt.is_help())
	{
		usage(false);
		return 0;
	}

	// Start Gdiplus 
	Gdiplus::GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken; 
	Gdiplus::Status res = Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
	if (res != Gdiplus::Ok)
	{
		_ftprintf(stderr, _T("chromakey: GdiplusStartup error %d\n"), res);
		return 1;
	}
	gdiplus_guard gg(gdiplusToken);

	// Load the image 
	Gdiplus::Bitmap* in_bmp = Gdiplus::Bitmap::FromFile(opt.in_file().c_str());
	if (in_bmp == NULL)
	{
		_ftprintf(stderr, _T("chromakey: Can not load image file %s\n"), opt.t_in_file().c_str());
		return 1;
	}
	image_guard ig(in_bmp);

	const UINT bw = in_bmp->GetWidth();
	const UINT bh = in_bmp->GetHeight();

	unsigned int left;
	unsigned int top;
	unsigned int right;
	unsigned int bottom;
	opt.get_crop(left, top, right, bottom);
	if (left+right >= bw || top+bottom >= bh)
	{
		_ftprintf(stderr, _T("chromakey: invalid crop parameters\n"));
		return 1;
	}

	CLSID pngClsid;
	const WCHAR* png_desc = L"image/png";
	if (GetEncoderClsid(png_desc, &pngClsid) == -1)
	{
		_ftprintf(stderr, _T("Can not find encoder for %s\n"), png_desc);
		return 1;
	}

	const unsigned int obw = bw-left-right;
	const unsigned int obh = bh-top-bottom;

	Gdiplus::Bitmap out_bmp(obw, obh);

	Gdiplus::BitmapData data_in;
	Gdiplus::BitmapData data_out;
	Gdiplus::Rect rect_in(0, 0, in_bmp->GetWidth(), in_bmp->GetHeight());
	Gdiplus::Rect rect_out(0, 0, out_bmp.GetWidth(), out_bmp.GetHeight());
	res = out_bmp.LockBits(&rect_out, Gdiplus::ImageLockModeRead|Gdiplus::ImageLockModeWrite, PixelFormat32bppARGB, &data_out);
	if (res != Gdiplus::Ok)
	{
		std::cerr << "Unable to lock out bmp bits" << std::endl;
		return 1;
	}
	bits_guard out_data_guard(out_bmp, data_out);

	res = in_bmp->LockBits(&rect_in, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &data_in);
	if (res != Gdiplus::Ok)
	{
		std::cerr << "Unable to lock out bmp bits" << std::endl;
		return 1;
	}
	bits_guard in_data_guard(*in_bmp, data_in);

	imageProcessor_master proc(data_in, data_out, opt);
	if (proc.prepare_background() != 0)
	{
		return 1;
	}

	clock_t ct = clock();

	int thrd_num = opt.get_threads();
	if (thrd_num > MAXIMUM_WAIT_OBJECTS)
		thrd_num = MAXIMUM_WAIT_OBJECTS; // i hope it will be quite enough

	if (thrd_num > 1)
	{
		int n = (int)floor(sqrt((double)thrd_num));
		thrd_num = n*n;

		std::vector<imageProcessor> procs;
		procs.resize(thrd_num, proc);

		int dx = obw/n;
		int dy = obh/n;
//std::cout << n << " " << dx << " " << dy << " " << MAXIMUM_WAIT_OBJECTS << std::endl;
		for (int ny = 0; ny < n; ++ny)
		{
			int h = ny < n-1 ? dy : (obh - ny*dy);

			for (int nx = 0; nx < n; ++nx)
			{
				int w = nx < n-1 ? dx : (obw - nx*dx);
//std::cout << ny*n+nx << " " << w << " " << h << std::endl;
				procs[ny*n+nx].set_area(dx*nx, dy*ny, w, h);
			}
		}
//		procs[0].set_area(0, 0, obw/2, obh/2);
//		procs[1].set_area(obw/2, 0, obw - obw/2, obh/2);
//		procs[2].set_area(0, obh/2, obw/2, obh - obh/2);
//		procs[3].set_area(obw/2, obh/2, obw - obw/2, obh - obh/2);

		std::vector<HANDLE> threads;
		threads.resize(thrd_num, NULL);

		handles_guard hguard(threads);

		HANDLE second_step_enable = ::CreateEvent(NULL, true, false, NULL);
		if (second_step_enable == NULL)
		{
			std::cerr << "Unable to create event" << std::endl;
			return 1;
		}
		handles_guard eguard(second_step_enable);

		volatile LONG threads_no = thrd_num;
		std::vector<worker_arg> args;
		args.resize(thrd_num, worker_arg(&procs[0], &threads_no, second_step_enable));

		for (int i = 0; i < thrd_num; ++i)
		{
			args[i].proc_ = &procs[i];
			threads[i] = reinterpret_cast<HANDLE>(
				_beginthreadex(NULL, 0, worker, &args[i], 0, NULL));
			if (threads[i] == NULL)
			{
				std::cerr << "Unable to start thread" << std::endl;
				return 1;
			}
		}

		DWORD wr = ::WaitForMultipleObjects(thrd_num, &threads[0], true, INFINITE);
		std::cout << "wait " << wr << std::endl;
	}
	else
	{
		proc.prepare_alpha();

		std::cout << "alpha time " << (double)(clock() - ct)/CLOCKS_PER_SEC << std::endl;

		proc.process_bitmap();
	}

	std::cout << "bitmap done " << (double)(clock() - ct)/CLOCKS_PER_SEC << std::endl;

	in_data_guard.dismiss();
	out_data_guard.dismiss();

	std::cout << "bits unlocked " << (double)(clock() - ct)/CLOCKS_PER_SEC << std::endl;
	out_bmp.Save(opt.out_file().c_str(), &pngClsid, NULL);

	std::cout << "total time " << (double)(clock() - ct)/CLOCKS_PER_SEC << std::endl;

	return 0;
}
