#include "string_utils.h"

#include <memory>

#include <time.h>

namespace string_utils
{

std::string string_trim(const std::string& str)
{
	std::string::size_type len = str.size();
	if (len == 0)
		return "";

	std::string::size_type first = len;
	std::string::size_type last = len;

	for (std::string::size_type i = 0; i < len; ++i)
	{
		if (!isspace(static_cast<unsigned char>(str[i])))
		{
			first = i;
			break;
		}
	}

	for (size_t i = len-1; i >= first; --i)
	{
		if (!isspace(static_cast<unsigned char>(str[i])))
		{
			last = i;
			break;
		}
	}

	return str.substr(first, last-first+1);
}

int parse_int(const std::string& i_str, int& parsed)
{
	std::string str = string_trim(i_str);

	int idx = 0;
	if (str.find("0x") == 0)
		idx = 2;
	else if (str.find("$") == 0)
		idx = 1;
	else
	{
		for (size_t i = 0; i < str.size(); ++i)
		{
			if (str[i] != '-' && (str[i] < '0' || str[i] > '9'))
			{
				return invalid_character_in_decimal_number;
			}
		}
	}

	int res;

	if (idx == 0)
		res = sscanf(str.c_str(), "%d", &parsed);
	else
	{
		str = str.substr(idx);
		for (size_t i = 0; i < str.size(); ++i)
		{
			if	(str[i] != '-' &&
				(str[i] < '0' || str[i] > '9') &&
				(idx == 0 || ((str[i] < 'a' || str[i] > 'z') && (str[i] < 'A' || str[i] > 'Z'))))
			{
				return invalid_character_in_hex_number;
			}
		}
		res = sscanf(str.c_str(), "%x", &parsed);
	}

	return res == 1 ? 0 : number_can_not_be_parsed;
}

std::string string_tolower(const std::string& i_str)
{
	std::string str = i_str;

	for (std::string::size_type i = 0; i < str.size(); ++i)
		str[i] = tolower(str[i]);

	return str;
}

std::string string_toupper(const std::string& i_str)
{
	std::string str = i_str;

	for (std::string::size_type i = 0; i < str.size(); ++i)
		str[i] = toupper(str[i]);

	return str;
}

tstring to_tstring(const char* in_str, UINT codepage)
{
#ifdef UNICODE
	WCHAR wbuf[2048];

	int length = MultiByteToWideChar(codepage, 0, in_str, -1, NULL, 0);
	if (length == 0 || length >= sizeof(wbuf)/sizeof(wbuf[0]))
		return _T("");
	else
	{
		MultiByteToWideChar(codepage, 0, in_str, -1, wbuf, length);
		return wbuf;
	}
#else
	return in_str;
#endif
}

std::string to_ansi_string(LPCTSTR tstr, UINT codepage)
{
#ifdef UNICODE
	int str_length = WideCharToMultiByte(codepage, 0/*WC_NO_BEST_FIT_CHARS*/, tstr, -1, NULL, 0, NULL, NULL);
	if (str_length == 0)
		return "";
	else
	{
		std::auto_ptr<char> ansi_str(new char[str_length]);
		WideCharToMultiByte(codepage, 0/*WC_NO_BEST_FIT_CHARS*/, tstr, -1, ansi_str.get(), str_length, NULL, NULL);
		return ansi_str.get();
	}
#else
	return tstr;
#endif
}

std::string string_trim_remark(const std::string& str)
{
	for (std::string::size_type i = 0; i < str.size(); ++i)
	{
		if (str[i] == ';' || str[i] == '#')
		{
			return str.substr(0, i);
		}
	}
	return str;
}

tstring prepend_app_path(LPCTSTR path, LPCTSTR ext)
{
	TCHAR m_path[MAX_PATH];
	GetModuleFileName(NULL, m_path, sizeof(m_path)/sizeof(m_path[0]));

	TCHAR	drv[_MAX_DRIVE];
	TCHAR	dir[_MAX_DIR];
	_tsplitpath(m_path, drv, dir, NULL, NULL);
	_tmakepath(m_path, drv, dir, path, ext);

	return m_path;
}

static
int parse_hex(std::string& str, int& res)
{
	int parsed = 0;
	int value = 0;

	for (std::string::size_type i = 0;
			i < str.size() && 
			((str[i] >= '0' && str[i] <= '9') || (str[i] >= 'a' && str[i] <= 'f'));
			++i, ++parsed)
	{
		if (str[i] >= '0' && str[i] <= '9')
			value = (value << 4) + (str[i] - '0');
		else
			value = (value << 4) + (str[i] - 'a');
	}

	if (parsed == 0)
		return 1; // TODO

	str = str.substr(parsed);
	res = value;

	return 0;
}

static
int parse_dec(std::string& str, int& res)
{
	int parsed = 0;
	int value = 0;

	for (std::string::size_type i = 0;
			i < str.size() && str[i] >= '0' && str[i] <= '9';
			++i, ++parsed)
	{
		value = (value * 10) + (str[i] - '0');
	}

	if (parsed == 0)
		return 1; // TODO

	str = str.substr(parsed);
	res = value;

	return 0;
}

int parse_number(std::string& expr, int& res)
{
	int ret;

	if (expr[0] == '$')
	{
		expr = expr.substr(1);
		ret = parse_hex(expr, res);
	}
	else if (expr.find("0x") == 0)
	{
		expr = expr.substr(2);
		ret = parse_hex(expr, res);
	}
	else
	{
		ret = parse_dec(expr, res);
	}

	return ret;
}

std::string get_word(const std::string& str, const char* extra_break_symbols)
{
	std::string::size_type i;
	std::string ebs;
	if (extra_break_symbols != NULL)
		ebs = extra_break_symbols;

	for (i = 0; i < str.size() &&
		        !isspace(static_cast<unsigned char>(str[i])) &&
				ebs.find(str[i]) == std::string::npos; ++i) {}
	return str.substr(0,i);
}
/*
tstring pr_time()
{
	__time64_t tm;
	tm = _time64(&tm);

	struct tm *gmt;
	gmt = _gmtime64(&tm);

	CString s;
	s.Format(_T("%04d_%02d_%02d_%02d_%02d_%02d"), 1900+gmt->tm_year, gmt->tm_mon+1, gmt->tm_mday, gmt->tm_hour, gmt->tm_min, gmt->tm_sec);

	return (LPCTSTR)s;
}
*/
}