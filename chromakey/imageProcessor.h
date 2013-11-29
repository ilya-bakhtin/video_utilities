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

class background;

class imageProcessor
{
public:
	imageProcessor(Gdiplus::BitmapData& data_in_, Gdiplus::BitmapData& data_out_,
				   const img_options& opt);
	imageProcessor(const imageProcessor&);
	~imageProcessor(void);
	imageProcessor& operator=(const imageProcessor&);

static
void translateRGB(int iR, int iG, int iB,
				  double &dR, double &dG, double &dB,
				  double &tR, double &tG, double &tB);

void set_area(UINT area_x, UINT area_y, UINT area_w, UINT area_h);

int prepare_background();
void prepare_alpha();
void process_bitmap();

private:
	Gdiplus::BitmapData& data_in_;
	Gdiplus::BitmapData& data_out_;
	const img_options& opt_;

	UINT area_x_;
	UINT area_y_;
	UINT area_w_;
	UINT area_h_;

	std::auto_ptr<background> back_;
};
