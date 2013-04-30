/*
 * riff.h library for RIFF file format i/o
 * Copyright (C) 2000 - 2002 Arne Schirmacher <arne@schirmacher.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * 
 *
 */

#ifndef _RIFF_H
#define _RIFF_H 1

#include <vector>
using std::vector;

#define QUADWORD unsigned __int64	//changed to unsigned
#define DWORD unsigned long			//changed to unsigned
#define LONG unsigned long
#define WORD unsigned short			//changed to unsigned
#define BYTE unsigned char
#define FOURCC unsigned long

#define RIFF_NO_PARENT (-1)
#define RIFF_LISTSIZE (4)
#define RIFF_HEADERSIZE (8)

#ifdef __cplusplus
extern "C"
{
    FOURCC make_fourcc(char *s);
}
#endif

class RIFFDirEntry
{
public:
    FOURCC type;
    FOURCC name;
    __int64 length;
    __int64 offset;
    size_t parent;
    int written;

    RIFFDirEntry();
    RIFFDirEntry(FOURCC t, FOURCC n, __int64 l, int o, size_t p);
};


class RIFFFile
{
public:
    RIFFFile();
    RIFFFile(const RIFFFile&);
    virtual ~RIFFFile();
    RIFFFile& operator=(const RIFFFile&);

    virtual bool Open(const char *s);
    virtual void Close();
	virtual size_t AddDirectoryEntry(FOURCC type, FOURCC name, __int64 length, size_t list);
	virtual void SetDirectoryEntry(size_t i, RIFFDirEntry &entry);
    virtual RIFFDirEntry GetDirectoryEntry(size_t i) const;
    virtual size_t FindDirectoryEntry(FOURCC type, size_t n = 0) const;
    virtual void ParseChunk(size_t parent);
    virtual void ParseList(size_t parent);
    virtual void ParseRIFF(void);
    virtual void ReadChunk(size_t chunk_index, void *data);

protected:
    int fd;

private:
    vector<RIFFDirEntry> directory;
};

#endif