/*
 * avi.h library for AVI file format i/o
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
 */

/** Common AVI declarations
 
    Some of this comes from the public domain AVI specification, which
    explains the microsoft-style definitions.
 
    \file avi.h
*/

#ifndef _AVI_H
#define _AVI_H 1

#include "riff.h"
#include "frame.h"


#define AVI_SMALL_INDEX (0x01)
#define AVI_LARGE_INDEX (0x02)
#define AVI_INDEX_OF_INDEXES (0x00)
#define AVI_INDEX_OF_CHUNKS (0x01)
#define AVI_INDEX_2FIELD (0x01)

enum { AVI_PAL, AVI_NTSC, AVI_AUDIO_48KHZ, AVI_AUDIO_44KHZ, AVI_AUDIO_32KHZ };

/** Declarations of the main AVI file header
 
    The contents of this struct goes into the 'avih' chunk.  */

#pragma pack(1)	//default pack is 8byte !
//WAVEFORMATEX, RECT and BITMAPINFOHEADER are dup defined. so renamed

typedef struct
{
    /// frame display rate (or 0L)
    DWORD dwMicroSecPerFrame;

    /// max. transfer rate
    DWORD dwMaxBytesPerSec;

    /// pad to multiples of this size, normally 2K
    DWORD dwPaddingGranularity;

    /// the ever-present flags
    DWORD dwFlags;

    /// # frames in file
    DWORD dwTotalFrames;
    DWORD dwInitialFrames;
    DWORD dwStreams;
    DWORD dwSuggestedBufferSize;

    DWORD dwWidth;
    DWORD dwHeight;

    DWORD dwReserved[4];
}
MainAVIHeader;



typedef struct
{
    DWORD dwDVAAuxSrc;
    DWORD dwDVAAuxCtl;
    DWORD dwDVAAuxSrc1;
    DWORD dwDVAAuxCtl1;
    DWORD dwDVVAuxSrc;
    DWORD dwDVVAuxCtl;
    DWORD dwDVReserved[2];
}
DVINFO;


typedef struct
{
    WORD wLongsPerEntry;
    BYTE bIndexSubType;
    BYTE bIndexType;
    DWORD nEntriesInUse;
    DWORD dwChunkId;
    DWORD dwReserved[3];
    struct avisuperindex_entry
    {
        QUADWORD qwOffset;
        DWORD dwSize;
        DWORD dwDuration;
    }
    aIndex[2014];
}
AVISuperIndex;

typedef struct
{
    WORD wLongsPerEntry;
    BYTE bIndexSubType;
    BYTE bIndexType;
    DWORD nEntriesInUse;
    DWORD dwChunkId;
    QUADWORD qwBaseOffset;	// __int64 ???
    DWORD dwReserved;
    struct avifieldindex_entry
    {
        DWORD dwOffset;
        DWORD dwSize;
    }
//    aIndex[4029];	//4028 orig. Why ?
	aIndex[17895];
}
AVIStdIndex;

typedef struct
{
    struct avisimpleindex_entry
    {
        FOURCC	dwChunkId;
        DWORD	dwFlags;
        DWORD	dwOffset;
        DWORD	dwSize;
    }
    aIndex[20000];
    DWORD	nEntriesInUse;
}
AVISimpleIndex;


#pragma pack()

/** base class for all AVI type files
 
    It contains methods and members which are the same in all AVI type files regardless of the particular compression, number
    of streams etc. 
 
    The AVIFile class also contains methods for handling several indexes to the video frame content. */

class AVIFile : public RIFFFile
{
public:
    AVIFile();
    AVIFile(const AVIFile&);
    virtual ~AVIFile();
    virtual AVIFile& operator=(const AVIFile&);

    virtual int GetDVFrameInfo(__int64 &offset, int &size, unsigned int frameNum);
    virtual int GetDVFrame(Frame &frame, int frameNum);
    virtual int GetTotalFrames() const;
    virtual void ParseList(int parent);
    virtual void ParseRIFF(void);
    virtual void ReadIndex(void);
	virtual bool isOpenDML(void);
	
protected:
    MainAVIHeader       mainHdr;
    AVISimpleIndex      *idx1;
    int                 file_list;
    int                 riff_list;
    int                 hdrl_list;
    size_t              avih_chunk;
    size_t              movi_list;
    int                 junk_chunk;
    size_t              idx1_chunk;

//    AVIStreamHeader     streamHdr[2];
    AVISuperIndex       *indx[2];
    AVIStdIndex         *ix[2];
    size_t              indx_chunk[2];
    int                 ix_chunk[2];
    int                 strl_list[2];
    int                 strh_chunk[2];
    int                 strf_chunk[2];

    int                 index_type;
    int                 current_ix00;

    int                 dmlh[62];
    int                 odml_list;
    int                 dmlh_chunk;
	bool                isUpdateIdx1;

};

#endif