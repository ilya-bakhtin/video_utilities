#pragma once

#include <string>

#include "tchar.h"
#include <windows.h>

#ifdef UNICODE
#define tstring std::wstring
#else
#define tstring std::string
#endif

namespace string_utils
{

enum
{
	invalid_character_in_decimal_number = 1,
	invalid_character_in_hex_number,
	number_can_not_be_parsed,
};


std::string string_trim(const std::string& str);
int parse_int(const std::string& i_str, int& parsed);
std::string string_tolower(const std::string& i_str);
std::string string_toupper(const std::string& i_str);
tstring to_tstring(const char* in_str, UINT codepage = CP_ACP);
std::string to_ansi_string(LPCTSTR tstr, UINT codepage = CP_ACP);
std::string string_trim_remark(const std::string& str);
tstring prepend_app_path(LPCTSTR path, LPCTSTR ext);
tstring pr_time();

int parse_number(std::string& expr, int& res);
std::string get_word(const std::string& str, const char* extra_break_symbols = NULL);

// TODO we need some header for these kind things
class fclose_guard
{
public:
	fclose_guard(FILE* f): f_(f) {}
	~fclose_guard() {fclose(f_);}
private:
	FILE* f_;
};

}