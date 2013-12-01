#pragma once

#include "memory"
#include "vector"

// surely it's possible to use CPoint or Gdiplus::Point
// however i've got a plan to get rid of Windows types
// and make a portable solution
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

// img_options class provides interface to image processing parameters
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

	struct adjust_edge_arg
	{
		adjust_edge_arg(bool adjust_edge,
						bool adjust_edge_dbg,
						unsigned int edge_x_gap,
						unsigned int edge_y_gap,
						double edge_thresh,
						double edge_coeff,
						Gdiplus::Color edge_debug_color);

		bool adjust_edge_;
		bool adjust_edge_dbg_;
		unsigned int edge_x_gap_;
		unsigned int edge_y_gap_;
		double edge_thresh_;
		double edge_coeff_;
		Gdiplus::Color edge_debug_color_;
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
	virtual const adjust_edge_arg& get_edge_arg() const = 0;
};

class background;

// imageProcessor class holds private copies of objects that
// are necessary for each tread and references to common ones
class imageProcessor
{
public:
	imageProcessor(const imageProcessor&);
	imageProcessor& operator=(const imageProcessor&);
	~imageProcessor();

	static
	void translateRGB(const double &dR, const double &dG, const double &dB,
					  double &tR, double &tG, double &tB,
					  double coeff = 0.5);
	static
	void translateRGB(int iR, int iG, int iB,
					  double &dR, double &dG, double &dB,
					  double &tR, double &tG, double &tB,
					  double coeff = 0.5);

	void set_area(UINT area_x, UINT area_y, UINT area_w, UINT area_h);

	// prepares backround object. must be called firstly from single thread
	// before all others processing
	int prepare_background();

	// prepares alpha map. can work at different parts of image
	// from concurrent threads
	void prepare_alpha();


	// calculates final bitmap based on incaming colours and calculated alpha map.
	// can work at different parts of image from concurrent threads
	void process_bitmap();

protected:
	// imageProcessor class may only be constructed by the imageProcessor_master
	imageProcessor(const Gdiplus::BitmapData& data_in_, Gdiplus::BitmapData& data_out_,
				   const img_options& opt, std::vector<std::vector<BYTE> >& alpha_map_);

	// internally used by process_bitmap() to do some additional
	// calculations at image edges
	void adjust_border(const img_options::adjust_edge_arg& ea,
					   UINT out_x, UINT out_y, Gdiplus::Color& c, int& A);

	const Gdiplus::BitmapData& data_in_;
	Gdiplus::BitmapData& data_out_;
private:
	const img_options& opt_;

	UINT area_x_;
	UINT area_y_;
	UINT area_w_;
	UINT area_h_;

	std::auto_ptr<background> back_;
	std::vector<std::vector<BYTE> >& alpha_map_;
};

// imageProcessor class holds objects that can be used simultaneously
// from concurrent threads
class imageProcessor_master: public imageProcessor
{
public:
	imageProcessor_master(const Gdiplus::BitmapData& data_in_, Gdiplus::BitmapData& data_out_,
						  const img_options& opt);
private:
// copy CTOR and assignment are disabled
	imageProcessor_master(const imageProcessor_master&);
	imageProcessor_master& operator=(const imageProcessor_master&);

	std::vector<std::vector<BYTE> > alpha_map_body_;
};
