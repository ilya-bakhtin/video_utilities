// chromakey.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include "iostream"
#include "memory"
#include "string"
#include "vector"

namespace {

class guard
{
public:
	guard() {}
private:
	guard(const guard&);
	guard& operator=(const guard&);
};

class gdiplus_guard : public guard
{
public:
	gdiplus_guard(ULONG_PTR gdiplusToken):
	  gdiplusToken_(gdiplusToken)
	{
	}
	~gdiplus_guard()
	{
		// Shutdown Gdiplus 
		Gdiplus::GdiplusShutdown(gdiplusToken_); 
	}

private:
	ULONG_PTR gdiplusToken_;
};

class image_guard
{
public:
	image_guard(Gdiplus::Image* image):
	  image_(image)
	{
	}
	~image_guard()
	{
		delete image_; 
	}

private:
	Gdiplus::Image* image_;
};

static
LPCTSTR pixel_format_name(Gdiplus::Image& img)
{
	Gdiplus::PixelFormat pixel_format = img.GetPixelFormat();

	switch (pixel_format)
	{
		case PixelFormat1bppIndexed:
			return _T("PixelFormat1bppIndexed");

		case PixelFormat4bppIndexed:
			return _T("PixelFormat4bppIndexed");

		case PixelFormat8bppIndexed:
			return _T("PixelFormat8bppIndexed");

		case PixelFormat16bppARGB1555:
			return _T("PixelFormat16bppARGB1555");

		case PixelFormat16bppGrayScale:
			return _T("PixelFormat16bppGrayScale");

		case PixelFormat16bppRGB555:
			return _T("PixelFormat16bppRGB555");

		case PixelFormat16bppRGB565:
			return _T("PixelFormat16bppRGB565");

		case PixelFormat24bppRGB:
			return _T("PixelFormat24bppRGB");

		case PixelFormat32bppARGB:
			return _T("PixelFormat32bppARGB");

		case PixelFormat32bppPARGB:
			return _T("PixelFormat32bppPARGB");

		case PixelFormat32bppRGB:
			return _T("PixelFormat32bppRGB");

		case PixelFormat48bppRGB:
			return _T("PixelFormat48bppRGB");

		case PixelFormat64bppARGB:
			return _T("PixelFormat64bppARGB");

		case PixelFormat64bppPARGB:
			return _T("PixelFormat64bppPARGB");

		default:
			return _T("Unknown Pixel Format");
	}
}

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

void
translateRGB(int iR, int iG, int iB,
			 double &dR, double &dG, double &dB,
			 double &tR, double &tG, double &tB )
{
	dR = iR / 255.;
	dG = iG / 255.;
	dB = iB / 255.;

	tR = dR - 0.5 * (dG + dB);
	tG = dG - 0.5 * (dR + dB);
	tB = dB - 0.5 * (dR + dG);

/*
	double m = dR < dG ? dR : dG;
	m = m < dB ? m : dB;
	double M = dR > dG ? dR : dG;
	M = M > dB ? M : dB;
	double L = 0.5 * (M+m); 

	tR = dR - L;
	tG = dG - L;
	tB = dB - L;

	tR = dR - m;
	tG = dG - m;
	tB = dB - m;
*/
/*
	if (tR < 0)
		tR = 0;
	if (tG < 0)
		tG = 0;
	if (tB < 0)
		tB = 0;

	if (tR > 1)
		tR = 1;
	if (tG > 1)
		tG = 1;
	if (tB > 1)
		tB = 1;
*/
}

class background
{
public:
//	virtual ~background() {} TODO not necessary now

	virtual void getRGB(int x, int y, double& dR, double& dG, double& dB) = 0;
	virtual void get_translatedRGB(int x, int y, double& tR, double& tG, double& tB) = 0;
	virtual double get_denom(int x, int y) const = 0;

	virtual std::auto_ptr<background> clone() const = 0;
};

class background_flat: public background
{
public:
	background_flat(int iR, int iG, int iB);

	virtual void getRGB(int x, int y, double& dR, double& dG, double& dB);
	virtual void get_translatedRGB(int x, int y, double& tR, double& tG, double& tB);
	virtual double get_denom(int x, int y) const;

	virtual std::auto_ptr<background> clone() const;

private:
	 double dR_;
	 double dG_;
	 double dB_;
	 double tR_;
	 double tG_;
	 double tB_;
	 double denom_;
};

background_flat::background_flat(int iR, int iG, int iB)
{
	translateRGB(iR, iG, iB, dR_, dG_, dB_, tR_, tG_, tB_);
	denom_ = tR_*tR_ + tG_*tG_ + tB_*tB_;
}

void background_flat::getRGB(int /*x*/, int /*y*/, double& dR, double& dG, double& dB)
{
	dR = dR_;
	dG = dG_;
	dB = dB_;
}

void background_flat::get_translatedRGB(int /*x*/, int /*y*/, double& tR, double& tG, double& tB)
{
	tR = tR_;
	tG = tG_;
	tB = tB_;
}

double background_flat::get_denom(int /*x*/, int /*y*/) const
{
	return denom_;
}

std::auto_ptr<background> background_flat::clone() const
{
	return std::auto_ptr<background>(new background_flat(*this));
}

class background_gradient4: public background
{
public:
	background_gradient4(int w, int h,
						 Gdiplus::Color cTL, Gdiplus::Color cTR,
						 Gdiplus::Color cBL, Gdiplus::Color cBR);

	virtual void getRGB(int x, int y, double& dR, double& dG, double& dB);
	virtual void get_translatedRGB(int x, int y, double& tR, double& tG, double& tB);
	virtual double get_denom(int x, int y) const;

	virtual std::auto_ptr<background> clone() const;

private:
	int w_;
	int h_;
	Gdiplus::Color cLT_;
	Gdiplus::Color cRT_;
	Gdiplus::Color cLB_;
	Gdiplus::Color cRB_;

	int cur_y_;
	double dr_[6], dg_[6], db_[6];
	double tr_[6], tg_[6], tb_[6];

	mutable double denom_; //TODO

	void prepare_y(int y);
};

background_gradient4::background_gradient4(int w, int h,
											 Gdiplus::Color cLT, Gdiplus::Color cRT,
											 Gdiplus::Color cLB, Gdiplus::Color cRB):
	w_(w),
	h_(h),
	cur_y_(-1),
	cLT_(cLT),
	cRT_(cRT),
	cLB_(cLB),
	cRB_(cRB),
	denom_(1)
{
	translateRGB(cLT_.GetR(), cLT_.GetG(), cLT_.GetB(), dr_[0], dg_[0], db_[0], tr_[0], tg_[0], tb_[0]);
	translateRGB(cRT_.GetR(), cRT_.GetG(), cRT_.GetB(), dr_[1], dg_[1], db_[1], tr_[1], tg_[1], tb_[1]);
	translateRGB(cLB_.GetR(), cLB_.GetG(), cLB_.GetB(), dr_[2], dg_[2], db_[2], tr_[2], tg_[2], tb_[2]);
	translateRGB(cRB_.GetR(), cRB_.GetG(), cRB_.GetB(), dr_[3], dg_[3], db_[3], tr_[3], tg_[3], tb_[3]);
}

void background_gradient4::getRGB(int x, int y, double& dR, double& dG, double& dB)
{
	if (y != cur_y_)
		prepare_y(y);

	double alpha = double(w_ - x - 1) / double(w_ - 1);

	dR = dr_[4]*alpha + dr_[5]*(1-alpha);
	dG = dg_[4]*alpha + dg_[5]*(1-alpha);
	dB = db_[4]*alpha + db_[5]*(1-alpha);
}

void background_gradient4::get_translatedRGB(int x, int y, double& tR, double& tG, double& tB)
{
	if (y != cur_y_)
		prepare_y(y);

	double alpha = double(w_ - x - 1) / double(w_ - 1);

	tR = tr_[4]*alpha + tr_[5]*(1-alpha);
	tG = tg_[4]*alpha + tg_[5]*(1-alpha);
	tB = tb_[4]*alpha + tb_[5]*(1-alpha);

	denom_ = tR*tR + tG*tG + tB*tB;
}

double background_gradient4::get_denom(int /*x*/, int /*y*/) const
{
	return denom_;
}

void background_gradient4::prepare_y(int y)
{
	double alpha = double(h_ - y - 1) / double(h_ - 1);

	dr_[4] = dr_[0]*alpha + dr_[2]*(1-alpha);
	dg_[4] = dg_[0]*alpha + dg_[2]*(1-alpha);
	db_[4] = db_[0]*alpha + db_[2]*(1-alpha);
	dr_[5] = dr_[1]*alpha + dr_[3]*(1-alpha);
	dg_[5] = dg_[1]*alpha + dg_[3]*(1-alpha);
	db_[5] = db_[1]*alpha + db_[3]*(1-alpha);

	tr_[4] = tr_[0]*alpha + tr_[2]*(1-alpha);
	tg_[4] = tg_[0]*alpha + tg_[2]*(1-alpha);
	tb_[4] = tb_[0]*alpha + tb_[2]*(1-alpha);
	tr_[5] = tr_[1]*alpha + tr_[3]*(1-alpha);
	tg_[5] = tg_[1]*alpha + tg_[3]*(1-alpha);
	tb_[5] = tb_[1]*alpha + tb_[3]*(1-alpha);

	cur_y_ = y;
}

std::auto_ptr<background> background_gradient4::clone() const
{
	return std::auto_ptr<background>(new background_gradient4(*this));
}

Gdiplus::Color operator* (Gdiplus::Color in,  double coeff)
{
	Gdiplus::ARGB aa = 0xFF000000 | (round(in.GetR()*coeff) << 16) | (round(in.GetG()*coeff) << 8) | round(in.GetB()*coeff);
	Gdiplus::Color c;
	c.SetValue(aa);
	return c;
}

class int_point
{
public:
	int_point():
	  x(0),
	  y(0)
	{
	}
	int_point(int ix, int iy):
	  x(ix),
	  y(iy)
	{
	}

	int x;
	int y;
};

class options
{
public:
	options(int argc, const TCHAR* const* argv);

	enum back_type
	{
		flat_color,
		flat_point,
		gradient_color,
		gradient_def_points,
		gradient_points,
	};

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
};

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
	debug_color_(0, 0, 0)
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
		"\t\tsingle RGB has to be hex, (R,G,B) must be dec" <<
		std::endl;
}

static
int prepare_background(Gdiplus::Bitmap& in_bmp,
					   const options& opt, std::auto_ptr<background>& back)
{
	const UINT bw = in_bmp.GetWidth();
	const UINT bh = in_bmp.GetHeight();

	_tprintf(_T("input image is %dx%d %s\n"), bw, bh, pixel_format_name(in_bmp));

	unsigned int left;
	unsigned int top;
	unsigned int right;
	unsigned int bottom;
	opt.get_crop(left, top, right, bottom);
	// these parameters should be already validated

	const unsigned int obw = bw-left-right;
	const unsigned int obh = bh-top-bottom;

	double mlt;
	double mrt;
	double mlb;
	double mrb;
	opt.get_mult(mlt, mrt, mlb, mrb);

	Gdiplus::Color back_color;
	int_point b_point;
	std::vector<Gdiplus::Color> grad_colors;
	std::vector<int_point> grad_points;
	options::back_type bt = opt.get_back(back_color, b_point, grad_colors, grad_points);
std::cout << "back_type is " << bt << std::endl;

	if (bt == options::flat_point)
	{
		b_point.x += left;
		b_point.y += top;

std::cout << "back point " << b_point.x << ", " << b_point.y << std::endl;
		if (b_point.x < 0 || b_point.x >= (int)bw ||
			b_point.y < 0 || b_point.y >= (int)bh)
		{
			_ftprintf(stderr, _T("invalid back point x or/and y\n"));
			return 1;
		}

		in_bmp.GetPixel(b_point.x, b_point.y, &back_color);
	}

	if (bt == options::flat_color || bt == options::flat_point)
	{
		if (mlt == mrt && mlt == mlb && mlt == mrb)
			back_color = back_color * mlt;
		else
		{
			grad_colors[0] = grad_colors[1] = grad_colors[2] = grad_colors[3] = back_color;
			bt = options::gradient_color;
std::cout << "new back_type is " << bt << std::endl;
		}
	}
	else if (bt != options::gradient_color)
	{
		if (bt == options::gradient_def_points)
		{
			grad_points[0] = int_point(left, top);
			grad_points[1] = int_point(bw-right-1, top);
			grad_points[2] = int_point(left, bh-bottom-1);
			grad_points[3] = int_point(bw-right-1, bh-bottom-1);
		}
		else
		{
			for (int i = 0; i < 4; ++i)
			{
				grad_points[i].x += left;
				grad_points[i].y += top;

std::cout << "gradient back point " << i << " (" << grad_points[i].x << ", " << grad_points[i].y << ")" << std::endl;
				if (grad_points[i].x < 0 || grad_points[i].x >= (int)bw ||
					grad_points[i].y < 0 || grad_points[i].y >= (int)bh)
				{
					_ftprintf(stderr, _T("invalid gradient back point x or/and y\n"));
					return 1;
				}
			}
		}

		for (int i = 0; i < 4; ++i)
			in_bmp.GetPixel(grad_points[i].x, grad_points[i].y, &grad_colors[i]);
	}

	if (bt == options::flat_color || bt == options::flat_point)
		back.reset(new background_flat(back_color.GetR(), back_color.GetG(), back_color.GetB()));
	else
		back.reset(new background_gradient4(obw, obh, grad_colors[0]*mlt, grad_colors[1]*mrt, grad_colors[2]*mlb, grad_colors[3]*mrb));

	return 0;
}

static
void prepare_alpha(Gdiplus::Bitmap& in_bmp, Gdiplus::Bitmap& out_bmp,
				   const options& opt, const background& in_back)
{
	std::auto_ptr<background> back(in_back.clone());

	double circle_x;
	double circle_y;
	double circle_r;
	bool circle = opt.get_circle(circle_x, circle_y, circle_r);

	double circle_offset;
	double circle_bias;
	opt.get_circle_params(circle_offset, circle_bias);

	Gdiplus::Color debug_color;
	bool circle_debug = opt.get_circle_debug(debug_color);

	unsigned int left;
	unsigned int top;
	unsigned int right;
	unsigned int bottom;
	opt.get_crop(left, top, right, bottom);
	// these parameters should be already validated

	const unsigned int obw = out_bmp.GetWidth();
	const unsigned int obh = out_bmp.GetHeight();

	for (UINT out_y = 0; out_y < obh; ++out_y)
	{
		for (UINT out_x = 0; out_x < obw; ++out_x)
		{
			Gdiplus::Color c;
			in_bmp.GetPixel(out_x+left, out_y+top, &c);

			int CR = c.GetR();
			int CG = c.GetG();
			int CB = c.GetB();
			double r, g, b;
			{
				double cR, cG, cB;
				translateRGB(CR, CG, CB, cR, cG, cB, r, g, b);
			}
			double R, G, B;
			back->get_translatedRGB(out_x, out_y, R, G, B);

			double a = (R*r + G*g + B*b) / back->get_denom(out_x, out_y); // Normalized dot product

			if (a < 0)
				a = 0;
			if (a > 1)
				a = 1;

			a = 1 - a;

			if (circle && (circle_debug || (a != 0 && a != 1)) &&
				out_y <= circle_y+circle_offset)
			{
				double dy = (double)out_y-circle_y;
				double dx = (double)out_x-circle_x;

				double dist = sqrt(dx*dx+dy*dy);

				if (dist > circle_r)
				{
					if (circle_debug)
					{
						a = 1;
						CR = debug_color.GetR();
						CG = debug_color.GetG();
						CB = debug_color.GetB();
					}
					else
					{
						double coeff = (dist - circle_r) / circle_bias;
						a -= coeff;
						if (a < 0)
							a = 0;
					}
				}
			}
			int A = round(a*255);

			Gdiplus::ARGB aa = (A << 24) | (CR << 16) | (CG << 8) | CB;
			c.SetValue(aa);
			out_bmp.SetPixel(out_x, out_y, c);
		}
	}
}

static
void process_bitmap(Gdiplus::Bitmap& out_bmp, const background& in_back)
{
	std::auto_ptr<background> back(in_back.clone());

	const unsigned int obw = out_bmp.GetWidth();
	const unsigned int obh = out_bmp.GetHeight();

	for (UINT out_y = 0; out_y < obh; ++out_y)
	{
		for (UINT out_x = 0; out_x < obw; ++out_x)
		{
			Gdiplus::Color c;
			out_bmp.GetPixel(out_x, out_y, &c);
			int A = c.GetA();
			double a = A / 255.;
			double cR, cG, cB;
			{
				double r, g, b;
				translateRGB(c.GetR(), c.GetG(), c.GetB(), cR, cG, cB, r, g, b);
			}
			if (A == 0)
				c.SetValue(0);
			else
			{
				double bR, bG, bB;
				back->getRGB(out_x, out_y, bR, bG, bB);

				double xR = (cR - (1-a)*bR) / a;
				double xG = (cG - (1-a)*bG) / a;
				double xB = (cB - (1-a)*bB) / a;

				if (xR < 0)
					xR = 0;
				if (xG < 0)
					xG = 0;
				if (xB < 0)
					xB = 0;

				if (xR > 1)
					xR = 1;
				if (xG > 1)
					xG = 1;
				if (xB > 1)
					xB = 1;

				int tr = round(xR * 255);
				int tg = round(xG * 255);
				int tb = round(xB * 255);

				Gdiplus::ARGB aa = (A << 24) | (tr << 16) | (tg << 8) | tb;
				c.SetValue(aa);
			}

			out_bmp.SetPixel(out_x, out_y, c);
		}
	}
}

} // namespace

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

	std::auto_ptr<background> back;

	if (prepare_background(*in_bmp, opt, back) != 0)
	{
		return 1;
	}

	const unsigned int obw = bw-left-right;
	const unsigned int obh = bh-top-bottom;

	Gdiplus::Bitmap out_bmp(obw, obh);

	CLSID pngClsid;
	const WCHAR* png_desc = L"image/png";
	if (GetEncoderClsid(png_desc, &pngClsid) == -1)
	{
		_ftprintf(stderr, _T("Can not find encoder for %s\n"), png_desc);
		return 1;
	}

	prepare_alpha(*in_bmp, out_bmp, opt, *back);
	process_bitmap(out_bmp, *back);

	out_bmp.Save(opt.out_file().c_str(), &pngClsid, NULL);

	return 0;
}
