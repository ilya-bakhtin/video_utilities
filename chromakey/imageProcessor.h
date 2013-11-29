#pragma once

#include "memory"
#include "vector"

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

class img_options
{
public:
	enum back_type
	{
		flat_color,
		flat_point,
		gradient_color,
		gradient_def_points,
		gradient_points,
	};

	virtual void get_crop(unsigned int& left, unsigned int& top,
						  unsigned int& right, unsigned int& bottom) const = 0;
	virtual back_type get_back(Gdiplus::Color& flat_color, int_point& point,
							   std::vector<Gdiplus::Color>& gradient_colors,
							   std::vector<int_point>& gradient_points) const = 0;
	virtual void get_mult(double& lt, double& rt, double& lb, double& rb) const = 0;
	virtual bool get_circle(double& cx, double& cy, double& cr) const = 0;
	virtual void get_circle_params(double& offset, double& bias) const = 0;
	virtual bool get_circle_debug(Gdiplus::Color& debug_color) const = 0;
};

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

class imageProcessor
{
public:
	imageProcessor(Gdiplus::Bitmap& in_bmp, Gdiplus::Bitmap& out_bmp,
				   const img_options& opt);
	~imageProcessor(void);

static
void translateRGB(int iR, int iG, int iB,
				  double &dR, double &dG, double &dB,
				  double &tR, double &tG, double &tB);

int prepare_background();
void prepare_alpha();
void process_bitmap();

private:
	imageProcessor(const imageProcessor&);
	imageProcessor& operator=(const imageProcessor&);

	Gdiplus::Bitmap& in_bmp_;
	Gdiplus::Bitmap& out_bmp_;
	const img_options& opt_;

	std::auto_ptr<background> back_;
};
