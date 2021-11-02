#include "stdafx.h"
#include "assert.h"
#include <list>
#include "fcntl.h"
#include "io.h"

#include "crypt.h"
#include "avi.h"

static const char *signature  = "01530000";

static bool ignore_date = false;

struct TheNode
{
	time_t	_recDate;
	time_t	_tmCode;
	int		frame;
};

struct TheSegment
{
	TheNode b;
	TheNode e;
	int		err_cnt;
	int		file;
};

typedef std::list<TheSegment> SegList;

static bool operator< (const TheNode& l, const TheNode& r)
{
	return l._recDate < r._recDate || (l._recDate == r._recDate && l._tmCode < r._tmCode);
}

static bool operator> (const TheNode& l, const TheNode& r)
{
	return r < l;
}

static bool operator<= (const TheNode& l, const TheNode& r)
{
	return !(l>r);
}

static bool operator>= (const TheNode& l, const TheNode& r)
{
	return !(l<r);
}

static bool operator== (const TheNode& l, const TheNode& r)
{
	return !(l < r) && !(r < l);
}

static
void AddSegment(SegList& seg_list, const TheSegment& inp_s)
{
	TheSegment s = inp_s;

	for (SegList::iterator i = seg_list.begin(); i != seg_list.end(); ++i)
	{
		TheSegment* lb = &*i;

		if (lb->e < s.b) // inserted is to the right; nothing to do
			continue;
		if (lb->b > s.e) // inserted is to the left
		{
			i = seg_list.insert(i, s);
			return;
		}

		if (s.b < lb->b) // split inserted
		{
			TheSegment ins_s = s;
			time_t len = lb->b._tmCode - ins_s.b._tmCode;
			ins_s.e._tmCode = ins_s.b._tmCode + len - 1;
			ins_s.e.frame = ins_s.b.frame + (int)len - 1;
			s.b._tmCode += len;
			s.b.frame += (int)len;
			if (ins_s.b._recDate != ins_s.e._recDate) // TODO - most likely it's not possible.
			{
				ins_s.e._recDate = ins_s.b._recDate + (len-1)/25;
				s.b._recDate = s.b._recDate + len/25;
			}
			i = seg_list.insert(i, ins_s);
		}
		else if (s.b > lb->b) // split current
		{
			TheSegment ins_s = *lb;
			time_t len = s.b._tmCode - lb->b._tmCode;
			ins_s.e._tmCode = ins_s.b._tmCode + len - 1;
			ins_s.e.frame = ins_s.b.frame + (int)len - 1;
			lb->b._tmCode += len;
			lb->b.frame += (int)len;
			if (ins_s.b._recDate != ins_s.e._recDate) // TODO - most likely it's not possible.
			{
				ins_s.e._recDate = ins_s.b._recDate + (len-1)/25;
				lb->b._recDate = lb->b._recDate + len/25;
			}
			i = seg_list.insert(i, ins_s);
		}
		else // s.b == lb->b
		{
			TheSegment ins_s = *lb;
			time_t len = lb->e._tmCode - lb->b._tmCode + 1;

			if (s.err_cnt < lb->err_cnt) // inserted is better
			{
				if (s.e < lb->e) // split current
				{
					len = s.e._tmCode - s.b._tmCode + 1;

					ins_s.e._tmCode = ins_s.b._tmCode + len - 1;
					ins_s.e.frame = ins_s.b.frame + (int)len - 1;
					lb->b._tmCode += len;
					lb->b.frame += (int)len;
					if (ins_s.b._recDate != ins_s.e._recDate) // TODO - most likely it's not possible.
					{
						ins_s.e._recDate = ins_s.b._recDate + (len-1)/25;
						lb->b._recDate = lb->b._recDate + len/25;
					}
					i = seg_list.insert(i, ins_s);
					lb = &*i;
				}

				 // overwrite current
				lb->file = s.file;
				lb->err_cnt = s.err_cnt;
				lb->b = s.b;
				lb->e._tmCode = s.b._tmCode + len - 1;
				lb->e.frame = s.b.frame + (int)len - 1;
				if (s.b._recDate != s.e._recDate) // TODO - most likely it's not possible.
					lb->e._recDate = s.b._recDate + (len-1)/25;

			}
			if (s.e <= lb->e)
				return;

			s.b._tmCode += len;
			s.b.frame += (int)len;
			if (s.b._recDate != s.e._recDate) // TODO - most likely it's not possible.
				lb->e._recDate = s.b._recDate + len/25;
		}
	}

	assert(seg_list.size() == 0 || s.e >= seg_list.begin()->b);

	seg_list.push_back(s);
}

static
md5_hash calc_hash(int avi_handle, __int64 avi_size)
{
	char buf[1024];
	__int64 step = avi_size / sizeof(buf);

	if (step == 0)
		step = 1;

	memset(buf, 0, sizeof(buf));

	__int64 pos = 0;
	for (int i = 0; i < sizeof(buf) && pos < avi_size; ++i, pos += step)
	{
		_lseeki64(avi_handle, pos, SEEK_SET);
		_read(avi_handle, buf+i, 1);
	}

	return calculate_md5(buf, sizeof(buf));
}

static
bool test_md5(const char* buf, const md5_hash& hash)
{
	if (strlen(buf) < 32)
		return false;

	BYTE inp_hash[16];
	memset(inp_hash, 0, sizeof(inp_hash));

	for (int i = 0; i < 32; ++i)
	{
		int d;

		if (buf[i] >= '0' && buf[i] <= '9')
			d = buf[i] - '0';
		else if (buf[i] >= 'A' && buf[i] <= 'F')
			d = buf[i] - 'A' + 0xA;
		else if (buf[i] >= 'a' && buf[i] <= 'f')
			d = buf[i] - 'a' + 0xA;

		if ((i&1) == 0)
			d <<= 4;

		inp_hash[i/2] |= d;
	}

	return memcmp(inp_hash, hash.hash, 16) == 0;
}

static
bool get_str(char* buf, int sz_buf, FILE* f)
{
	if (fgets(buf, sz_buf, f) != buf)
		return false;

	size_t l = strlen(buf);
	if (l != 0 && buf[l-1] == 0xA)
		buf[l-1] = 0;

	return true;
}

static
bool calculate_file_hash(const TCHAR* filename, md5_hash& hash, __int64& real_avi_size)
{
	int af = _open(filename, _O_RDONLY);
	if (af == -1)
		return false;
	_lseeki64(af, 0, SEEK_END);
	real_avi_size = _telli64(af);
	hash = calc_hash(af, real_avi_size);
	_close(af);

	return true;
}

static
void make_digest_filename(const TCHAR* filename, TCHAR* digest_filename /*_MAX_PATH*/)
{
	TCHAR drv[_MAX_DRIVE];
	TCHAR dir[_MAX_DIR];
	TCHAR fname[_MAX_FNAME];
	TCHAR ext[_MAX_EXT];

	_splitpath(filename, drv, dir, fname, ext);

	_makepath(digest_filename, drv, dir, fname, _T("digest"));
}

class file_guard
{
public:
	file_guard(FILE* f) : f_(f)	{}

	~file_guard()
	{
		if (f_ != NULL)
			fclose(f_);
	}

private:
	file_guard(const file_guard&); // to prevent copy
	file_guard& operator=(const file_guard&); // to prevent assignment

	FILE* f_;
};

static
bool read_digest(const TCHAR* digest_filename,
				 int fileno,
				 const md5_hash& hash,
				 const __int64& real_avi_size,
				 int& frames,
				 SegList& seg_list)
{
	FILE *f = fopen(digest_filename, _T("r"));

	if (f == NULL)
		return false;

	file_guard fg(f);

	bool res = false;

	char buf[1024];
	__int64 avi_size;

	if (get_str(buf, sizeof(buf), f) && stricmp(buf, signature) == 0 &&
		get_str(buf, sizeof(buf), f) && /*stricmp(buf, filename) == 0 &&*/ // lets trust the digest
		get_str(buf, sizeof(buf), f) && sscanf(buf, "%I64d", &avi_size) == 1 && real_avi_size == avi_size &&
		get_str(buf, sizeof(buf), f) && test_md5(buf, hash) &&
		get_str(buf, sizeof(buf), f) && sscanf(buf, "%d", &frames) == 1)
	{
		SegList t_seg_list;
		bool	bad_rec = false;

		while (get_str(buf, sizeof(buf), f))
		{
			TheSegment s;
			int cnt;
			if (sizeof(s.b._recDate) == 4)
				cnt = sscanf(buf, "%d %d %d %d %d %d %d\n", &s.b.frame, &s.b._recDate, &s.b._tmCode, &s.e.frame, &s.e._recDate, &s.e._tmCode, &s.err_cnt);
			else
				cnt = sscanf(buf, "%d %lld %lld %d %lld %lld %d\n", &s.b.frame, &s.b._recDate, &s.b._tmCode, &s.e.frame, &s.e._recDate, &s.e._tmCode, &s.err_cnt);

			if (cnt != 7)
				bad_rec = true;
			else
			{
				s.file = fileno;
				AddSegment(t_seg_list, s);
			}
		}
		
		if (!bad_rec)
		{
			for (SegList::iterator i = t_seg_list.begin(); i != t_seg_list.end(); ++i)
				AddSegment(seg_list, *i);

			printf("digest correct\n");
			res = true;
		}
	}

	return res;
}

static
FILE* create_digest_file(const TCHAR* digest_filename,
						 const TCHAR* filename,
						 const md5_hash& hash,
						 const __int64& real_avi_size)
{
	FILE* f = fopen(digest_filename, _T("w"));
	if (f == NULL)
		printf("warning: can not create digest file %s\n", digest_filename);
	else
	{
		fprintf(f, "%s\n", signature);
		fprintf(f, "%s\n", filename);
		fprintf(f, "%I64d\n", real_avi_size);
		for (int i = 0; i < sizeof(hash.hash); ++i)
			fprintf(f, "%02x", hash.hash[i]);
		fprintf(f, "\n");
	}

	return f;
}

static
int process_file(const TCHAR* filename, int fileno, SegList& seg_list, int& frames)
{
    AVIFile avi;

	if (!avi.Open(filename))
	{
		printf("DVInfo: Can't open AVI File %s\n", filename);
		return 1;
	}

	md5_hash hash;
	__int64 real_avi_size;
	if (!calculate_file_hash(filename, hash, real_avi_size))
	{
		printf("calculate_file_hash: Can't open AVI File %s\n", filename);
		return 1;
	}

	TCHAR digest_filename[_MAX_PATH];
	make_digest_filename(filename, digest_filename);

	if (read_digest(digest_filename, fileno, hash, real_avi_size, frames, seg_list))
		return 0;

	FILE *digest = create_digest_file(digest_filename, filename, hash, real_avi_size);

	file_guard fg(digest);

	avi.ParseRIFF();
	avi.ReadIndex();

	int n = avi.GetTotalFrames();
	frames = n;

	if (digest != NULL)
		fprintf(digest, "%d\n", n);

	int prev_frame = -1;
	int prev_errors = -1;
	time_t prevFrm_tmCode = 0;
	time_t prevFrm_recDate = 0;
	time_t prev_tmCode = 0;
	time_t prev_recDate = 0;

	int recDate_sec_frames = 0;

	for (int i = 0; i < n; ++i)
	{
		Frame	f;
		bool	is_drop;

		if (avi.GetDVFrame(f, i, is_drop) != 0)
			printf("error reading DV frame %d\n", i);
		else if (is_drop)
			printf("drop frame %d\n", i);
		else
		{
			struct	tm tmCode;
			time_t	_tmCode = 0;

			struct	tm recDate;
			time_t	_recDate = 0;
			bool	tm_code_valid = false;
			bool	rec_date_valid = false;

			if (f.GetTimeCode(tmCode))
			{
				tm_code_valid = true;
				_tmCode = ((tmCode.tm_hour*60 + tmCode.tm_min) * 60 + tmCode.tm_sec)*25 + tmCode.tm_mday;
 			}

			if (f.GetRecordingDate(recDate))
			{
				rec_date_valid = true;
				_recDate = mktime(&recDate);
			}

			if (ignore_date)
				rec_date_valid = true;

			if (!tm_code_valid)
				printf("no time code at %d\n", i);
			if (!rec_date_valid)
				printf("no recording date at %d\n", i);

			int blocks;
			int errors = f.GetErrorsCount(blocks);

			if (errors != 0)
				printf("frame %d has %d erroneous blocks of %d\n", i, errors, blocks);

			if (prev_frame != -1 && _tmCode - prevFrm_tmCode != 1)
			{
				if (sizeof(_tmCode) == 4)
					printf("frame %d jumps in time code from %d to %d\n", i, prevFrm_tmCode, _tmCode);
				else
					printf("frame %d jumps in time code from %lld to %lld\n", i, prevFrm_tmCode, _tmCode);
			}
			
			if (prev_frame != -1 && 
				(!tm_code_valid || !rec_date_valid || 
					_tmCode - prevFrm_tmCode != 1 || (_recDate - prevFrm_recDate != 0 /*&& (recDate_sec_frames%25) != 0*/) || errors != prev_errors))
			{
//				printf("the cut: %d - %d  %d - %d  %d - %d\n", prev_frame, i-1, prev_tmCode, prevFrm_tmCode, prev_recDate, prevFrm_recDate);
//				printf("the cut: %d - %d\n", prev_frame, i-1);
				
				TheSegment s;
				s.file = fileno;
				s.err_cnt = prev_errors;
				s.b.frame = prev_frame;
				s.b._recDate = prev_recDate;
				s.b._tmCode = prev_tmCode;
				s.e.frame = i-1;
				s.e._recDate = prevFrm_recDate;
				s.e._tmCode = prevFrm_tmCode;
				AddSegment(seg_list, s);

				if (digest != NULL)
				{
					if (sizeof(s.b._recDate) == 4)
						fprintf(digest, "%d %d %d %d %d %d %d\n", s.b.frame, s.b._recDate, s.b._tmCode, s.e.frame, s.e._recDate, s.e._tmCode, s.err_cnt);
					else
						fprintf(digest, "%d %lld %lld %d %lld %lld %d\n", s.b.frame, s.b._recDate, s.b._tmCode, s.e.frame, s.e._recDate, s.e._tmCode, s.err_cnt);
				}

				prev_frame = -1;
			}

			if (!ignore_date)
			{
				if (!tm_code_valid || !rec_date_valid)
				{
					_tmCode = prevFrm_tmCode+1;
					_recDate = prevFrm_recDate;
				}
			}
			else
			{
				if (!tm_code_valid)
					_tmCode = prevFrm_tmCode+1;
				if (!rec_date_valid)
					_recDate = prevFrm_recDate;
			}

			if (prev_frame == -1)
			{
				prev_frame = i;
				prev_errors = errors;
				prev_tmCode = _tmCode;
				prev_recDate = _recDate;
				recDate_sec_frames = 1;
			}
			else
				++recDate_sec_frames;

			prevFrm_tmCode = _tmCode;
			prevFrm_recDate = _recDate;
		}
	}

	if (prev_frame != -1)
	{
//		printf("the cut: %d - %d\n", prev_frame, n-1);

		TheSegment s;
		s.file = fileno;
		s.err_cnt = prev_errors;
		s.b.frame = prev_frame;
		s.b._recDate = prev_recDate;
		s.b._tmCode = prev_tmCode;
		s.e.frame = n-1;
		s.e._recDate = prevFrm_recDate;
		s.e._tmCode = prevFrm_tmCode;
		AddSegment(seg_list, s);
		if (digest != NULL)
		{
			if (sizeof(s.b._recDate) == 4)
				fprintf(digest, "%d %d %d %d %d %d %d\n", s.b.frame, s.b._recDate, s.b._tmCode, s.e.frame, s.e._recDate, s.e._tmCode, s.err_cnt);
			else
				fprintf(digest, "%d %lld %lld %d %lld %lld %d\n", s.b.frame, s.b._recDate, s.b._tmCode, s.e.frame, s.e._recDate, s.e._tmCode, s.err_cnt);
		}
	}

	return 0;
}

int _tmain(int argc, _TCHAR* argv[])
{
	ignore_date = argc >= 2 && std::string(argv[1]) == _T("-d");

	if (argc < 2 || (ignore_date && argc < 3))
	{
		printf("usage: dv_dump [-d] <filename>\n");
		return 1;
	}

	SegList seg_list;

	TCHAR mfn[_MAX_PATH];
	GetModuleFileName(NULL, mfn, sizeof(mfn));
	TCHAR drv[_MAX_DRIVE];
	TCHAR dir[_MAX_DIR];
	TCHAR fname[_MAX_FNAME];
	TCHAR ext[_MAX_EXT];
	_splitpath(mfn, drv, dir, fname, ext);
	TCHAR result_filename[_MAX_PATH];
	_makepath(result_filename, drv, dir, _T("result"), _T("vcf"));
	TCHAR scenes_filename[_MAX_PATH];
	_makepath(scenes_filename, drv, dir, _T("scenes"), _T("avs"));

	FILE* const vcf = fopen(result_filename, "w");
	if (vcf == NULL)
	{
		printf("Can't create result file %s\n", result_filename);
		return 1;
	}
	file_guard fg(vcf);

	FILE* const avs = fopen(scenes_filename, "w");
	if (avs == NULL)
	{
		printf("Can't create avs file %s\n", scenes_filename);
		return 1;
	}
	file_guard fga(avs);

	int first_arg = ignore_date?2:1;
	int offset = 0;
	for (int i = first_arg; i < argc; ++i)
	{
		int frames;
		if (process_file(argv[i], i-1, seg_list, frames) != 0)
			return 1;
		fprintf(vcf, "declare offset_%d;\n", i-1);
		fprintf(vcf, "offset_%d = %d;\n", i-1, offset);
		offset += frames;
	}

	fprintf(vcf, "VirtualDub.Open(U\"%s\");\n", argv[first_arg]);
	for (int i = first_arg+1; i < argc; ++i)
		fprintf(vcf, "VirtualDub.Append(U\"%s\");\n", argv[i]);

	fprintf(vcf, "VirtualDub.video.SetMode(0);\n");
	fprintf(vcf, "VirtualDub.subset.Clear();\n");

	bool		f = true;
	TheSegment	s;
	struct tm b_rd;
	struct tm e_rd;
	char	b_date[256];
	char	e_date[256];
	int		b_h, b_m, b_s, b_f;
	int		e_h, e_m, e_s, e_f;
	int		st_frame = 0;
	int		seg_len;
	time_t scene_start_time;
	int scene_start_frame;
	int scene_n = 0;

	for (SegList::iterator i = seg_list.begin(); i != seg_list.end(); ++i)
	{
		if (f)
		{
			s = *i;
			f = false;
			scene_start_time = s.b._recDate;
			scene_start_frame = s.b.frame;
		}
		else
		{
			SegList::const_iterator n = i;
			++n;
			const bool is_last = n == seg_list.end();
			if (i->b._recDate > s.e._recDate + 1 || is_last)
			{
				b_rd = *localtime(&scene_start_time);
				strftime(b_date, sizeof(b_date), "[%Y-%m-%d %H:%M:%S]", &b_rd);

//				fprintf(avs, "# %s\n", b_date);
				fprintf(avs, "%s (%d, %d)\n\n", b_date, scene_start_frame, (!is_last ? i->b.frame - 1 : i->e.frame));

				scene_start_time = i->b._recDate;
				scene_start_frame = i->b.frame;
				++scene_n;
			}

			if (s.file != i->file || i->b.frame != s.e.frame+1 || i->err_cnt != s.err_cnt)
			{
				b_rd = *localtime(&s.b._recDate); e_rd = *localtime(&s.e._recDate);
				strftime(b_date, sizeof(b_date), "[%d/%m/%Y %H:%M:%S]", &b_rd);
				strftime(e_date, sizeof(e_date), "[%d/%m/%Y %H:%M:%S]", &e_rd);
				b_s = (int)s.b._tmCode / 25;
				b_f = (int)s.b._tmCode % 25;
				b_m = b_s / 60;
				b_s = b_s % 60;
				b_h = b_m / 60;
				b_m = b_m % 60;
				e_s = (int)s.e._tmCode / 25;
				e_f = (int)s.e._tmCode % 25;
				e_m = e_s / 60;
				e_s = e_s % 60;
				e_h = e_m / 60;
				e_m = e_m % 60;

				seg_len = s.e.frame-s.b.frame+1;
				if (sizeof(s.b._tmCode) == 4)
				{
					printf("%d %5d-%5d %5d-%5d %s-%s %02d:%02d:%02d:%02d-%02d:%02d:%02d:%02d %5d-%5d %d\n",
						s.file, s.b._tmCode, s.e._tmCode, s.b.frame, s.e.frame, b_date, e_date, b_h, b_m, b_s, b_f, e_h, e_m, e_s, e_f, st_frame, st_frame+seg_len-1, s.err_cnt);
				}
				else
				{
					printf("%d %5lld-%5lld %5d-%5d %s-%s %02d:%02d:%02d:%02d-%02d:%02d:%02d:%02d %5d-%5d %d\n",
						s.file, s.b._tmCode, s.e._tmCode, s.b.frame, s.e.frame, b_date, e_date, b_h, b_m, b_s, b_f, e_h, e_m, e_s, e_f, st_frame, st_frame+seg_len-1, s.err_cnt);
				}
				fprintf(vcf, "VirtualDub.subset.AddRange(offset_%d + %d, %d);\n", s.file, s.b.frame, seg_len);
				st_frame += seg_len;

				s = *i;
			}
			else
				s.e = i->e;
		}
	}

	b_rd = *localtime(&s.b._recDate); e_rd = *localtime(&s.e._recDate);
	strftime(b_date, sizeof(b_date), "[%d/%m/%Y %H:%M:%S]", &b_rd);
	strftime(e_date, sizeof(e_date), "[%d/%m/%Y %H:%M:%S]", &e_rd);
	b_s = (int)s.b._tmCode / 25;
	b_f = (int)s.b._tmCode % 25;
	b_m = b_s / 60;
	b_s = b_s % 60;
	b_h = b_m / 60;
	b_m = b_m % 60;
	e_s = (int)s.e._tmCode / 25;
	e_f = (int)s.e._tmCode % 25;
	e_m = e_s / 60;
	e_s = e_s % 60;
	e_h = e_m / 60;
	e_m = e_m % 60;

	seg_len = s.e.frame-s.b.frame+1;
	if (sizeof(s.b._tmCode) == 4)
	{
		printf("%d %5d-%5d %5d-%5d %s-%s %02d:%02d:%02d:%02d-%02d:%02d:%02d:%02d %5d-%5d %d\n",
			s.file, s.b._tmCode, s.e._tmCode, s.b.frame, s.e.frame, b_date, e_date, b_h, b_m, b_s, b_f, e_h, e_m, e_s, e_f, st_frame, st_frame+seg_len-1, s.err_cnt);
	}
	else
	{
		printf("%d %5lld-%5lld %5d-%5d %s-%s %02d:%02d:%02d:%02d-%02d:%02d:%02d:%02d %5d-%5d %d\n",
			s.file, s.b._tmCode, s.e._tmCode, s.b.frame, s.e.frame, b_date, e_date, b_h, b_m, b_s, b_f, e_h, e_m, e_s, e_f, st_frame, st_frame+seg_len-1, s.err_cnt);
	}
	fprintf(vcf, "VirtualDub.subset.AddRange(offset_%d + %d, %d);\n", s.file, s.b.frame, seg_len);
	st_frame += seg_len;

	return 0;
}
