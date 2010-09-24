// kino30to24.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"
#include "float.h"
#include "math.h"
#include "stdio.h"
#include "avisynth.h"
//#include "TitleExpand.h"

/****************************
 * The following is the header definitions.
 * For larger projects, move this into a .h file
 * that can be included.
 ****************************/


class kino30to24 : public GenericVideoFilter {
public:
  // This defines that these functions are present in your class.
  // These functions must be that same as those actually implemented.
  // Since the functions are "public" they are accessible to other classes.
  // Otherwise they can only be called from functions within the class itself.

	kino30to24(PClip _child,  int frame, IScriptEnvironment* env);
  // This is the constructor. It does not return any value, and is always used, 
  //  when an instance of the class is created.
  // Since there is no code in this, this is the definition.

  ~kino30to24();
  // The is the destructor definition. This is called when the filter is destroyed.


	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
  // This is the function that AviSynth calls to get a given frame.
  // So when this functions gets called, the filter is supposed to return frame n.

	__int64 d0;
	__int64 d1;
	__int64 d2;
	__int64 d3;
	__int64 d4;
};

/***************************
 * The following is the implementation 
 * of the defined functions.
 ***************************/

#define period 12

//Here is the acutal constructor code used
kino30to24::kino30to24(PClip _child, int frame, IScriptEnvironment* env) :
	GenericVideoFilter(_child)
{
	d0 = d1 = d2 = d3 = d4 = 0;
}

// This is where any actual destructor code used goes
kino30to24::~kino30to24() {
  // This is where you can deallocate any memory you might have used.
}

static
int compareYV12(PVideoFrame first, PVideoFrame second, int &max_diff)
{
	const unsigned char *fp;
	const unsigned char *sp;
	const int width = first->GetRowSize();
	const int height = first->GetHeight();
	const int pitch = first->GetPitch();

	const static int oy = 2;
	const static int ox = 2;

	fp = first->GetReadPtr()+oy*pitch;
	sp = second->GetReadPtr()+oy*pitch;

	max_diff = 0;

	for (int y = oy; y < height-oy; ++y)
	{
		for (int x = ox; x < width-ox; ++x)
		{
			int v = abs(sp[x] - fp[x]);
			if (max_diff < v)
				max_diff = v;
		}
		fp += pitch;
		sp += pitch;
	}

	int md = max_diff / 2;

	int pixels = 0;

	fp = first->GetReadPtr()+oy*pitch;
	sp = second->GetReadPtr()+oy*pitch;
	for (int y = oy; y < height-oy; ++y)
	{
		for (int x = ox; x < width-ox; ++x)
		{
			int v = abs(sp[x] - fp[x]);
			if (v > md)
				++pixels;
		}
		fp += pitch;
		sp += pitch;
	}

	return pixels;
}

PVideoFrame __stdcall kino30to24::GetFrame(int n, IScriptEnvironment* env)
{
#if 0
static const int s = 1;
//	if ((n+s)%4 == 3)
//	{
//	}
//	else
		return child->GetFrame(n + (n+s)/4, env);
//		return child->GetFrame((n+s)/4*5+((n+s)%4)-s, env);

#endif

static int prev_n = 0;

	PVideoFrame src = child->GetFrame(n, env);
PVideoFrame dst = env->NewVideoFrame(vi);
//memcpy(dst->GetWritePtr(PLANAR_U), src->GetReadPtr(PLANAR_U), src->GetRowSize(PLANAR_U)*src->GetHeight(PLANAR_U));
//memcpy(dst->GetWritePtr(PLANAR_V), src->GetReadPtr(PLANAR_V), src->GetRowSize(PLANAR_V)*src->GetHeight(PLANAR_V));
memset(dst->GetWritePtr(PLANAR_U), 128, src->GetRowSize(PLANAR_U)*src->GetHeight(PLANAR_U));
memset(dst->GetWritePtr(PLANAR_V), 128, src->GetRowSize(PLANAR_V)*src->GetHeight(PLANAR_V));

	if (n == prev_n)
	{
		FILE *f = fopen("c:\\d\\kino30.log", "w");
		fclose(f);
	}
	else
	{
		PVideoFrame prev = child->GetFrame(n-1, env);

		const int width = src->GetRowSize();
		const int height = src->GetHeight();
		const int pitch = src->GetPitch();

		const unsigned char* srcp = src->GetReadPtr();
		unsigned char* dstp = dst->GetWritePtr();
		const unsigned char* prevp = prev->GetReadPtr();
		if (vi.IsYV12())
		{
#if 0
			int h;
			int w;

			double min_val = 0;

			// Y Plane
			for (h = 0; h < height; ++h)
			{
				for (w = 0; w < width; ++w)
				{
					double v = 0;
					const int cell_h = 4;
					const int cell_w = 4;
					for (int i = 0; i < cell_h; ++i)
					{
						for (int j = 0; j < cell_w; ++j)
						{
							int y=h-i;
							int x=w-j;
							if (y < 0)
								y = 0;
							else
								y=-i;
							if (x < 0)
								x = 0;
							v += (srcp+pitch*y)[x] - (prevp+pitch*y)[x];
						}
					}
//					double v = srcp[w] - prevp[w];
					v /= cell_h*cell_w;
					min_val += abs(v);
				}

				srcp += pitch;
				prevp += pitch;
			}
#else
			int h;
			int w;

			// Y Plane
			const unsigned char* srcp = (unsigned char*)src->GetReadPtr();
			unsigned char* dstp = (unsigned char*)dst->GetReadPtr();
			const unsigned char* prevp = prev->GetReadPtr();
			double val = 0;
			const static int oy = 2;
			const static int ox = 2;
			for (h = oy, srcp+=oy*pitch, dstp+=oy*pitch, prevp+=oy*pitch; h < height-oy; ++h)
			{
				for (w = ox; w < width-ox; ++w)
				{
					double v = (srcp[w] - prevp[w]);
					if (abs(v) > 64)
						val += abs(v);
					dstp[w] = (unsigned char)abs(v);
				}
				srcp += pitch;
				dstp += pitch;
				prevp += pitch;
			}
			// end Y Plane
#endif
			FILE *f = fopen("c:\\d\\kino30.log", "a");
			int max_diff;
			int pels = compareYV12(prev, src, max_diff);

			__int64 cn = 0; 

			if ((n+0)%5 == 0)
				cn = d0 += pels;
			if ((n+1)%5 == 0)
				cn = d1 += pels;
			if ((n+2)%5 == 0)
				cn = d2 += pels;
			if ((n+3)%5 == 0)
				cn = d3 += pels;
			if ((n+4)%5 == 0)
				cn = d4 += pels;
			
			fprintf(f, "%6d %8d %8d %I64d\n", n, pels, max_diff, cn);
			fclose(f);
			prev_n = n;
		}
	}

	return dst;

#if 0
	PVideoFrame dst = env->NewVideoFrame(vi);
	
	const unsigned char* srcp = src->GetReadPtr();
  // Request a Read pointer from the source frame.
  // This will return the position of the upperleft pixel in YUY2 images,
  // and return the lower-left pixel in RGB.
  // RGB images are stored upside-down in memory. 
  // You should still process images from line 0 to height.

	unsigned char* dstp = dst->GetWritePtr();
	// Request a Write pointer from the newly created destination image.
  // You can request a writepointer to images that have just been
  // created by NewVideoFrame. If you recieve a frame from PClip->GetFrame(...)
  // you must call env->MakeWritable(&frame) be recieve a valid write pointer.
	
	const int dst_pitch = dst->GetPitch();
  // Requests pitch (length of a line) of the destination image.
  // For more information on pitch see: http://www.avisynth.org/index.php?page=WorkingWithImages
	// (short version - pitch is always equal to or greater than width to allow for seriously fast assembly code)

	const int dst_width = dst->GetRowSize();
  // Requests rowsize (number of used bytes in a line.
  // See the link above for more information.

	const int dst_height = dst->GetHeight();
  // Requests the height of the destination image.

	const int src_pitch = src->GetPitch();

	if (vi.IsRGB32())
	{
		for (int h = 0; h < src_height; ++h)
		{
			int dstw = 0;
			for (int w = 0; w < src_width/4; ++w)
			{
				if ((w%period) == dframe && w != src_width/4-1)
				{
					for (int i = 0; i < 4; ++i)
						*(dstp + dstw*4 + i) = ((int)*(srcp + w*4 + i)+(int)*(srcp + (w+1)*4 + i))/2;
					++dstw;
				}
				else if ((w%period) != dframe+1)
				{
					for (int i = 0; i < 4; ++i)
						*(dstp + dstw*4 + i) = *(srcp + w*4 + i);
					++dstw;
				}
			}															
			
			srcp = srcp + src_pitch; // Add the pitch (note use of pitch and not width) of one line (in bytes) to the source pointer
			dstp = dstp + dst_pitch; // Add the pitch to the destination pointer.
		}
	}

	if (vi.IsYV12())
	{
		int h;
		int w;

		// Y Plane
		for (h = 0; h < src_height; ++h)
		{
			int dstw = 0;

			for (w = 0; w < src_width; ++w)
			{
				if ((w%period) == dframe && w != src_width-1)
				{
					dstp[dstw] = ((int)srcp[w] + (int)srcp[w+1])/2;
					++dstw;
				}
				else if ((w%period) != dframe+1)
				{
					dstp[dstw] = srcp[w];
					++dstw;
				}
			}

			srcp = srcp + src_pitch;
			dstp = dstp + dst_pitch;
		}
		// end Y Plane

		const int dst_pitchUV = dst->GetPitch(PLANAR_U);	// The pitch,height and width information
		const int dst_widthUV = dst->GetRowSize(PLANAR_U);	// is guaranted to be the same for both
		const int dst_heightUV = dst->GetHeight(PLANAR_U);	// the U and V planes so we only the U
		const int src_pitchUV = src->GetPitch(PLANAR_U);	// plane values and use them for V as
		const int src_widthUV = src->GetRowSize(PLANAR_U);	// well
		const int src_heightUV = src->GetHeight(PLANAR_U);	//
		
		// UV planes
		srcp = src->GetReadPtr(PLANAR_U);
		dstp = dst->GetWritePtr(PLANAR_U);
		
		for (int uv = 0; uv < 2; ++uv)
		{
			for (h = 0; h < src_heightUV; ++h)
			{
				int dstw = 0;

				for (w = 0; w < src_widthUV*2; ++w)
				{
					if ((w%period) == dframe && w < src_widthUV*2-1)
					{
						dstp[dstw/2] = ((int)srcp[w/2] + (int)srcp[(w+1)/2])/2;
						++dstw;
					}
					else if ((w%period) != dframe+1)
					{
						dstp[dstw/2] = srcp[w/2];
						++dstw;
					}
				}

				srcp = srcp + src_pitchUV;
				dstp = dstp + dst_pitchUV;
			}

			srcp = src->GetReadPtr(PLANAR_V);
			dstp = dst->GetWritePtr(PLANAR_V);
		}
		// end UV planes
	}

	if (vi.IsYUY2())
	{
		int h;
		int w;

		// Y Plane
		for (h = 0; h < src_height; ++h)
		{
			int dstw = 0;

			for (w = 0; w < src_width/2; ++w) // walk through Y
			{
				if ((w%period) == dframe && w != src_width/2-1)
				{
					dstp[dstw*2] = ((int)srcp[w*2] + (int)srcp[(w+1)*2])/2;
					++dstw;
				}
				else if ((w%period) != dframe+1)
				{
					dstp[dstw*2] = srcp[w*2];
					++dstw;
				}
//dstp[dstw*2+1] = 128;	// force b&w
			}

			srcp = srcp + src_pitch;
			dstp = dstp + dst_pitch;
		}
		// end Y Plane

		// UV planes
		for (int uv = 0; uv < 2; ++uv)
		{
			srcp = src->GetReadPtr();
			dstp = dst->GetWritePtr();
			for (h = 0; h < src_height; ++h)
			{
				int dstw = 0;

				for (w = 0; w < src_width/2; ++w)
				{
					if ((w%period) == dframe && w != src_width/2-1)
					{
						dstp[(dstw/2)*4+1+uv*2] = ((int)srcp[(w/2)*4+1+uv*2] + (int)srcp[((w+1)/2)*4+1+uv*2])/2;
						++dstw;
					}
					else if ((w%period) != dframe+1)
					{
						dstp[(dstw/2)*4+1+uv*2] = srcp[(w/2)*4+1+uv*2];
						++dstw;
					}
				}

				srcp = srcp + src_pitch;
				dstp = dstp + dst_pitch;
			}
		}
		// end UV planes
	}

	return dst;
#endif
}


// This is the function that created the filter, when the filter has been called.
// This can be used for simple parameter checking, so it is possible to create different filters,
// based on the arguments recieved.

AVSValue __cdecl Create_kino30to24(AVSValue args, void* user_data, IScriptEnvironment* env)
{
    return new kino30to24(
				args[0].AsClip(), // the 0th parameter is the source clip
				args[1].AsInt(0),
				env);  
    // Calls the constructor with the arguments provied.
}


// The following function is the function that actually registers the filter in AviSynth
// It is called automatically, when the plugin is loaded to see which functions this filter contains.

extern "C" __declspec(dllexport) const char* __stdcall AvisynthPluginInit2(IScriptEnvironment* env)
{
    env->AddFunction("kino30to24", "ci", Create_kino30to24, 0);
//    env->AddFunction("TitleExpand", "c", Create_TitleExpand, 0);
    return "`kino30to24' kino30to24 plugin";
}

BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 )
{
    return TRUE;
}

