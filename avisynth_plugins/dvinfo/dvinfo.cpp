/* dvinfo
** ** (c) 2003 - Ernst Peché
** 
*/

#include "avisynth.h"
#include <time.h>
#include "avi.h"

class DVInfo : public GenericVideoFilter {

	char filename[255];
	char output[255];
	AVSValue x;
	AVSValue y;
	AVSValue font;
	AVSValue size;
	AVSValue text_color;
	AVSValue halo_color;
	char rec_format[255];
	char tc_format[255];
	char fix_format[255];
	char fix_format_old[255];
	char fix_format_new[255];
	bool show_error;

    AVIFile avi;
	Frame f;
	struct tm recDate;
	struct tm timeCode;
	struct tm lastvalidrecDate;
	int showcounter;
	int autoframes;
	float threshold;

public:
    DVInfo( PClip _child1, 
		const char _filename[], const char _output[], 
		const AVSValue _x, const AVSValue _y, const AVSValue _font, const AVSValue _size, const AVSValue _text_color, const AVSValue _halo_color,
		const char _rec_format[], const char _tc_format[], const bool _show_error, const float _threshold, const int _autoframes,
		const char _fix_format[],
		IScriptEnvironment* env): GenericVideoFilter(_child1)
	{
		strcpy(filename, _filename);
		strcpy(output, _output);
		x = _x;
		y = _y;
		font = _font;
		size = _size;
		text_color = _text_color;
		halo_color = _halo_color;
		strcpy(rec_format, _rec_format);
		strcpy(tc_format, _tc_format);
		strcpy(fix_format, _fix_format);
		show_error = _show_error;
		threshold = _threshold;
		autoframes = _autoframes;

		if (!avi.Open(filename)) env->ThrowError("DVInfo: Can't open AVI File");
		avi.ParseRIFF();
		avi.ReadIndex();
	}

		
	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);

	~DVInfo() {
		avi.Close();
	}

};


PVideoFrame __stdcall DVInfo::GetFrame(int n, IScriptEnvironment* env) {

	PClip resultClip;
	AVSValue args[8];

	char rec_time[255];
	char tc_time[255];
	char current_frame[255];
	char dvinfotext[255];

	const char* arg_names[11] = { 0, 0, "x", "y", "font", "size", "text_color", "halo_color"};

    recDate.tm_sec=0;
    recDate.tm_min=0;
    recDate.tm_hour=0;
    recDate.tm_mday=0;
    recDate.tm_mon=0;
    recDate.tm_year=0;
    recDate.tm_wday=0;
    recDate.tm_yday=0;
    recDate.tm_isdst=0;

	timeCode.tm_sec=0;
    timeCode.tm_min=0;
    timeCode.tm_hour=0;
    timeCode.tm_mday=0;
    timeCode.tm_mon=0;
    timeCode.tm_year=0;
    timeCode.tm_wday=0;
    timeCode.tm_yday=0;
    timeCode.tm_isdst=0;

	setlocale( LC_TIME, ".OCP" );

//get frame and read date and timecode
	if (n >= avi.GetTotalFrames()) {
		strcpy(current_frame, show_error ? "frame# too high" : "");	//frame requested too high
		strcpy(rec_time, show_error ? "frame# too high" : "");
		strcpy(tc_time, show_error ? "frame# too high" : "");
	} else {
		bool is_drop;
		sprintf(current_frame, "%u", n);
		if (avi.GetDVFrame(f, n, is_drop)) {
			strcpy(rec_time, show_error ? "error reading DV frame" : "");
			strcpy(tc_time, show_error ? "error reading DV frame" : "");
		}
		else if (is_drop)
		{
			strcpy(rec_time, "drop frame");
			strcpy(tc_time, "drop frame");
		} else {
			if (!f.GetRecordingDate(recDate)) {
				strcpy(rec_time, show_error ? "no recording date" : "");
				lastvalidrecDate.tm_sec=0;
				lastvalidrecDate.tm_min=0;
				lastvalidrecDate.tm_hour=0;
				lastvalidrecDate.tm_mday=0;
				lastvalidrecDate.tm_mon=0;
				lastvalidrecDate.tm_year=0;
				lastvalidrecDate.tm_wday=0;
				lastvalidrecDate.tm_yday=0;
				lastvalidrecDate.tm_isdst=0;
			} else {
				if (threshold==0) {	//show always (or only on change)
					strftime(rec_time, 255, rec_format, &recDate);	//print recDate into rec_time using formatstring rec_format
				} else {
					strftime(fix_format_new, 255, fix_format, &recDate);
					strftime(fix_format_old, 255, fix_format, &lastvalidrecDate);
					if ( (abs(difftime(mktime(&recDate), mktime(&lastvalidrecDate) ))>threshold) 	//date difference above threshold in seconds
						|| (strcmp(fix_format_new, fix_format_old)) ) { //or fix_format format changes
						showcounter = autoframes;
					}
					if (showcounter>0) {	//reset counter
						showcounter--;
						strftime(rec_time, 255, rec_format, &recDate);	//print recDate into rec_time using formatstring rec_format
					} else {	//blank string
						strcpy(rec_time,"");
					}
					lastvalidrecDate = recDate;	//remember last valid date

				};
			}
			if (!f.GetTimeCode(timeCode)) {
				strcpy(tc_time, show_error ? "no timecode" : "");
			} else {
				++timeCode.tm_mday;
				strftime(tc_time, 255, tc_format, &timeCode);
			}
		}
	}

	setlocale( LC_TIME, "" );
			
	env->SetVar("current_frame",current_frame);	//SetVar passes the value byref !!
	env->SetVar("rec_time",rec_time);
	env->SetVar("tc_time", tc_time);

//evaluate formula	
	args[0] = output;
	args[1] = env->Invoke("Eval",AVSValue(args,1));
	if (args[1].IsString()) {
		strcpy(dvinfotext, args[1].AsString());
	} else {
		env->ThrowError("DVInfo: Output must be a string");
	};

//call Subtitle
	args[0] = child;
	args[1] = dvinfotext;
	args[2] = x;
	args[3] = y;
	args[4] = font;
	args[5] = size;
	args[6] = text_color;
	args[7] = halo_color;

	resultClip = (env->Invoke("Subtitle",AVSValue(args,8), arg_names )).AsClip();
	PVideoFrame src1 = resultClip->GetFrame(n, env);
//	PVideoFrame src1 = env->NewVideoFrame(vi);
	
	return src1;
}


AVSValue __cdecl Create_DVInfo(AVSValue args, void* user_data, IScriptEnvironment* env) {
	return new DVInfo(args[0].AsClip(), args[1].AsString(""), args[2].AsString("rec_time"),
						args[3], args[4], args[5], args[6], args[7], args[8],
						args[9].AsString("%Y-%m-%d %H:%M:%S"), args[10].AsString("%H:%M:%S-%d"),
						args[11].AsBool(true), args[12].AsFloat(0.0), args[13].AsInt(25),
						args[14].AsString(""),
						env);
}

extern "C" __declspec(dllexport) const char* __stdcall AvisynthPluginInit2(IScriptEnvironment* env)
{
  env->AddFunction("DVInfo","c[filename]s[output]s"
								"[x]i[y]i[font]s[size]i[text_color]i[halo_color]i"
								"[rec_format]s[tc_format]s[show_error]b"
								"[threshold]f[autoframes]i[fix_format]s",
								Create_DVInfo,0);
  return 0;
}

// 0:Clip
// 1,2:string "Filename", string "output", 
// 3,4,5,6,7,8:int x, int y, string "font", int "size", int "text_color", int "halo_color"
// 9,10,11:string "rec_format", string "tc_format", bool "show error"
// 12,13: int threshold, int autoframes
// 14: string "fix_format"
// Variables: current_frame, tc_time, rec_time