// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#define _CRT_SECURE_NO_WARNINGS

#include "targetver.h"

#include <math.h>
#include <stdio.h>
#include <tchar.h>

#include "windows.h"
#pragma comment( lib, "gdiplus.lib" ) 
#include "gdiplus.h"

#define round(x)	(int)floor(0.5 + (x))

#ifdef UNICODE
#define tstring std::wstring
#define tcout std::wcout
#define tcerr std::wcerr
#else
#define tstring std::string
#define tcout std::cout
#define tcerr std::cerr
#endif
