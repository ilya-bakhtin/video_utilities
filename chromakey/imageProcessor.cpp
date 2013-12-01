#include "StdAfx.h"

#include "iostream" // TODO

#include "imageProcessor.h"

class background
{
public:
//	virtual ~background() {} TODO not necessary now

	virtual void getRGB(int x, int y, double& dR, double& dG, double& dB) = 0;
	virtual void get_translatedRGB(int x, int y, double& tR, double& tG, double& tB) = 0;
	virtual double get_denom(int x, int y) const = 0;
/* TODO
	static std::auto_ptr<background> create(img_options::back_type type);
*/
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
	imageProcessor::translateRGB(iR, iG, iB, dR_, dG_, dB_, tR_, tG_, tB_);
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
	imageProcessor::translateRGB(cLT_.GetR(), cLT_.GetG(), cLT_.GetB(), dr_[0], dg_[0], db_[0], tr_[0], tg_[0], tb_[0]);
	imageProcessor::translateRGB(cRT_.GetR(), cRT_.GetG(), cRT_.GetB(), dr_[1], dg_[1], db_[1], tr_[1], tg_[1], tb_[1]);
	imageProcessor::translateRGB(cLB_.GetR(), cLB_.GetG(), cLB_.GetB(), dr_[2], dg_[2], db_[2], tr_[2], tg_[2], tb_[2]);
	imageProcessor::translateRGB(cRB_.GetR(), cRB_.GetG(), cRB_.GetB(), dr_[3], dg_[3], db_[3], tr_[3], tg_[3], tb_[3]);
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

imageProcessor::imageProcessor(const Gdiplus::BitmapData& data_in, Gdiplus::BitmapData& data_out,
							   const img_options& opt, std::vector<std::vector<BYTE> >& alpha_map):
	data_in_(data_in),
	data_out_(data_out),
	opt_(opt),
	area_x_(0),
	area_y_(0),
	area_w_(data_out_.Width),
	area_h_(data_out_.Height),
	alpha_map_(alpha_map)
{
}

imageProcessor::imageProcessor(const imageProcessor& other):
	data_in_(other.data_in_),
	data_out_(other.data_out_),
	opt_(other.opt_),
	area_x_(other.area_x_),
	area_y_(other.area_y_),
	area_w_(other.area_w_),
	area_h_(other.area_h_),
	back_(other.back_->clone()), // each processor must have own background object
	alpha_map_(other.alpha_map_)
{
}

imageProcessor::~imageProcessor()
{
}

imageProcessor& imageProcessor::operator=(const imageProcessor& other)
{
	if (this != &other)
	{
		this->~imageProcessor();
		new (this) imageProcessor(other);
	}

	return *this;
}

imageProcessor_master::imageProcessor_master(const Gdiplus::BitmapData& data_in, Gdiplus::BitmapData& data_out,
											 const img_options& opt):
	imageProcessor(data_in, data_out, opt, alpha_map_body_)
{
	alpha_map_body_.resize(data_out_.Height);

	for (UINT y = 0; y < data_out_.Height; ++y)
		alpha_map_body_[y].resize(data_out_.Width);
}

void imageProcessor::translateRGB(const double &dR, const double &dG, const double &dB,
								  double &tR, double &tG, double &tB,
								  double coeff)
{
	tR = dR - coeff * (dG + dB);
	tG = dG - coeff * (dR + dB);
	tB = dB - coeff * (dR + dG);
}

void imageProcessor::translateRGB(int iR, int iG, int iB,
								  double &dR, double &dG, double &dB,
								  double &tR, double &tG, double &tB,
								  double coeff)
{
	dR = iR / 255.;
	dG = iG / 255.;
	dB = iB / 255.;

	translateRGB(dR, dG, dB, tR, tG, tB, coeff);

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

static
LPCTSTR pixel_format_name(Gdiplus::PixelFormat pixel_format) // TODO should it be here?
{
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
Gdiplus::Color operator* (Gdiplus::Color in,  double coeff)
{
	Gdiplus::ARGB aa = 0xFF000000 | (round(in.GetR()*coeff) << 16) | (round(in.GetG()*coeff) << 8) | round(in.GetB()*coeff);
	Gdiplus::Color c;
	c.SetValue(aa);
	return c;
}

void imageProcessor::set_area(UINT area_x, UINT area_y, UINT area_w, UINT area_h)
{
	area_x_ = area_x;
	area_y_ = area_y;
	area_w_ = area_w;
	area_h_ = area_h;
}

int imageProcessor::prepare_background()
{
	const UINT bw = data_in_.Width;
	const UINT bh = data_in_.Height;

	_tprintf(_T("input image is %dx%d %s\n"), bw, bh, pixel_format_name(data_in_.PixelFormat)); // TODO

	unsigned int left;
	unsigned int top;
	unsigned int right;
	unsigned int bottom;
	opt_.get_crop(left, top, right, bottom);
	// these parameters should be already validated

	const unsigned int obw = bw-left-right;
	const unsigned int obh = bh-top-bottom;

	double mlt;
	double mrt;
	double mlb;
	double mrb;
	opt_.get_mult(mlt, mrt, mlb, mrb);

	Gdiplus::Color back_color;
	int_point b_point;
	std::vector<Gdiplus::Color> grad_colors;
	std::vector<int_point> grad_points;
	img_options::back_type bt = opt_.get_back(back_color, b_point, grad_colors, grad_points);
//std::cout << "back_type is " << bt << std::endl; // TODO

	if (bt == img_options::flat_point)
	{
		b_point.x += left;
		b_point.y += top;

//std::cout << "back point " << b_point.x << ", " << b_point.y << std::endl; TODO
		if (b_point.x < 0 || b_point.x >= (int)bw ||
			b_point.y < 0 || b_point.y >= (int)bh)
		{
			_ftprintf(stderr, _T("invalid back point x or/and y\n"));
			return 1;
		}

//		in_bmp_.GetPixel(b_point.x, b_point.y, &back_color);
		UINT stride = abs(data_in_.Stride);
		Gdiplus::ARGB aa = ((Gdiplus::ARGB*)((char*)data_in_.Scan0 + stride*b_point.y))[b_point.x];
		back_color.SetValue(aa);
	}

	if (bt == img_options::flat_color || bt == img_options::flat_point)
	{
		if (mlt == mrt && mlt == mlb && mlt == mrb)
			back_color = back_color * mlt;
		else
		{
			grad_colors[0] = grad_colors[1] = grad_colors[2] = grad_colors[3] = back_color;
			bt = img_options::gradient_color;
// std::cout << "new back_type is " << bt << std::endl; TODO
		}
	}
	else if (bt != img_options::gradient_color)
	{
		if (bt == img_options::gradient_def_points)
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

// std::cout << "gradient back point " << i << " (" << grad_points[i].x << ", " << grad_points[i].y << ")" << std::endl; TODO
				if (grad_points[i].x < 0 || grad_points[i].x >= (int)bw ||
					grad_points[i].y < 0 || grad_points[i].y >= (int)bh)
				{
					_ftprintf(stderr, _T("invalid gradient back point x or/and y\n"));
					return 1;
				}
			}
		}

		for (int i = 0; i < 4; ++i)
		{
//			in_bmp_.GetPixel(grad_points[i].x, grad_points[i].y, &grad_colors[i]);
			UINT stride = abs(data_in_.Stride);
			Gdiplus::ARGB aa = ((Gdiplus::ARGB*)((char*)data_in_.Scan0 + stride*grad_points[i].y))[grad_points[i].x];
			grad_colors[i].SetValue(aa);
		}
	}

	if (bt == img_options::flat_color || bt == img_options::flat_point)
		back_.reset(new background_flat(back_color.GetR(), back_color.GetG(), back_color.GetB()));
	else
		back_.reset(new background_gradient4(obw, obh, grad_colors[0]*mlt, grad_colors[1]*mrt, grad_colors[2]*mlb, grad_colors[3]*mrb));

	return 0;
}

void imageProcessor::prepare_alpha()
{
	double circle_x;
	double circle_y;
	double circle_r;
	bool circle = opt_.get_circle(circle_x, circle_y, circle_r);

	double circle_offset;
	double circle_bias;
	opt_.get_circle_params(circle_offset, circle_bias);

	Gdiplus::Color debug_color;
	bool circle_debug = opt_.get_circle_debug(debug_color);

	unsigned int left;
	unsigned int top;
	unsigned int right;
	unsigned int bottom;
	opt_.get_crop(left, top, right, bottom);
	// these parameters should be already validated

	for (UINT out_y = area_y_; out_y < area_y_+area_h_; ++out_y)
	{
		for (UINT out_x = area_x_; out_x < area_x_+area_w_; ++out_x)
		{
			Gdiplus::Color c;
//			in_bmp_.GetPixel(out_x+left, out_y+top, &c);
			UINT stride = abs(data_in_.Stride);
			Gdiplus::ARGB aa = ((Gdiplus::ARGB*)((char*)data_in_.Scan0 + stride*(out_y+top)))[out_x+left];
			c.SetValue(aa);

			int CR = c.GetR();
			int CG = c.GetG();
			int CB = c.GetB();
			double r, g, b;
			{
				double cR, cG, cB;
				imageProcessor::translateRGB(CR, CG, CB, cR, cG, cB, r, g, b);
			}
			double R, G, B;
			back_->get_translatedRGB(out_x, out_y, R, G, B);

			double a = (R*r + G*g + B*b) / back_->get_denom(out_x, out_y); // Normalized dot product

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
			{
			Gdiplus::ARGB aa = (A << 24) | (CR << 16) | (CG << 8) | CB;
			c.SetValue(aa);
//			out_bmp_.SetPixel(out_x, out_y, c);
			UINT stride = abs(data_out_.Stride);
			((Gdiplus::ARGB*)((char*)data_out_.Scan0 + stride*out_y))[out_x] = aa;
			alpha_map_[out_y][out_x] = static_cast<BYTE>(A);
			}
		}
	}
}

void imageProcessor::process_bitmap()
{
	const UINT obw = data_out_.Width;
	const UINT obh = data_out_.Height;

	for (UINT out_y = area_y_; out_y < area_y_+area_h_; ++out_y)
	{
		for (UINT out_x = area_x_; out_x < area_x_+area_w_; ++out_x)
		{
			Gdiplus::Color c;

//			out_bmp_.GetPixel(out_x, out_y, &c);
			UINT stride = abs(data_out_.Stride);
			Gdiplus::ARGB aa = ((Gdiplus::ARGB*)((char*)data_out_.Scan0 + stride*out_y))[out_x];
			c.SetValue(aa);

			int A = alpha_map_[out_y][out_x];;
			double a = A / 255.;
			double cR, cG, cB;
			{
				double r, g, b;
				imageProcessor::translateRGB(c.GetR(), c.GetG(), c.GetB(), cR, cG, cB, r, g, b);
			}

			const img_options::adjust_edge_arg& ea = opt_.get_edge_arg();

			if (ea.adjust_edge_ &&
				out_x >= ea.edge_x_gap_ && out_y >= ea.edge_y_gap_ &&
				out_x <= obw-1-ea.edge_x_gap_ && out_y <= obh-1-ea.edge_y_gap_)
			{
				adjust_border(ea, out_x, out_y, c, A);
				a = A / 255.;
				{
					double r, g, b;
					imageProcessor::translateRGB(c.GetR(), c.GetG(), c.GetB(), cR, cG, cB, r, g, b);
				}
			}

			if (A == 0)
			{
				c.SetValue(0);
				aa = 0;
			}
			else
			{
				double bR, bG, bB;
				back_->getRGB(out_x, out_y, bR, bG, bB);

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

				aa = (A << 24) | (tr << 16) | (tg << 8) | tb;
				c.SetValue(aa);
			}

//			out_bmp_.SetPixel(out_x, out_y, c);
			stride = abs(data_out_.Stride);
			((Gdiplus::ARGB*)((char*)data_out_.Scan0 + stride*out_y))[out_x] = aa;
		}
	}
}

void imageProcessor::adjust_border(const img_options::adjust_edge_arg& ea,
								   UINT out_x, UINT out_y,
								   Gdiplus::Color& c, int& A)
{
	static const UINT max_color = 255;
	static const UINT alpha_threshold = round(max_color*ea.edge_thresh_);

	const UINT x0 = out_x - ea.edge_x_gap_;
	const UINT y0 = out_y - ea.edge_y_gap_;
	const UINT x1 = out_x + ea.edge_x_gap_;
	const UINT y1 = out_y + ea.edge_y_gap_;

//	const UINT ap = alpha_map_[out_y][out_x];

	const int a0 = alpha_map_[y0][x0];
	const int a1 = alpha_map_[y0][x1];
	const int a2 = alpha_map_[y1][x1];
	const int a3 = alpha_map_[y1][x0];

	UINT ag[6] = {
		abs(a0-a1), abs(a1-a2), abs(a2-a3), abs(a3-a0),
		abs(a0-a2), abs(a1-a3)
	};

	UINT max_ag = 0;
	UINT min_ag = 255;

	for (int i = 0; i < 6; ++i)
	{
		if (max_ag < ag[i])
			max_ag = ag[i];
		if (min_ag > ag[i])
			min_ag = ag[i];
	}

	if (max_ag >= alpha_threshold)
	{
		if (ea.adjust_edge_dbg_)
		{
			A = max_color;
			c = ea.edge_debug_color_;
		}
		else
		{
			double coeff = (double)(max_ag-alpha_threshold)/(max_color-alpha_threshold)*(0.5-ea.edge_coeff_)+0.5;

			int CR = c.GetR();
			int CG = c.GetG();
			int CB = c.GetB();

			double r, g, b;
			{
				double cR, cG, cB;
				translateRGB(CR, CG, CB, cR, cG, cB, r, g, b, coeff);
			}
			double R, G, B;
			double tR, tG, tB;
			back_->getRGB(out_x, out_y, R, G, B);

			translateRGB(R, G, B, tR, tG, tB, coeff);
			double denom = tR*tR + tG*tG + tB*tB;

			double na = (tR*r + tG*g + tB*b) / denom; // Normalized dot product

			if (na < 0)
				na = 0;
			if (na > 1)
				na = 1;

			na = 1 - na;

			if (A > round(na*max_color))
				A = round(na*max_color);
		}
	}
}
