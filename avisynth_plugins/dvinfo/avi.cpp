/*
 * avi.cc library for AVI file format i/o
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
 */

//C++ includes

#include <string>
#include <iostream>
#include <strstream>
#include <iomanip>

using std::cout;
using std::hex;
using std::dec;
using std::setw;
using std::setfill;
using std::endl;

// C includes

#include <stdio.h>
#include <fcntl.h>
#include <assert.h>
#include <io.h>

// local includes
#include "riff.h"
#include "avi.h"



#define PADDING_SIZE (512)
#define PADDING_1GB (0x40000000)
#define IX00_INDEX_SIZE (4028)

#define AVIF_HASINDEX 0x00000010
#define AVIF_MUSTUSEINDEX 0x00000020
#define AVIF_TRUSTCKTYPE 0x00000800
#define AVIF_ISINTERLEAVED 0x00000100
#define AVIF_WASCAPTUREFILE 0x00010000
#define AVIF_COPYRIGHTED 0x00020000


static char g_zeroes[PADDING_SIZE];

/** The constructor
 
    \todo mainHdr not initialized
    \todo add checking for NULL pointers
 
*/

AVIFile::AVIFile() : RIFFFile(), 
        idx1( NULL), file_list( -1), riff_list( -1),
        hdrl_list( -1), avih_chunk( -1), movi_list( -1), junk_chunk( -1), idx1_chunk( -1),
        index_type( -1), current_ix00( -1), odml_list( -1), dmlh_chunk( -1), isUpdateIdx1(true)
{
    // cerr << "0x" << hex << (long)this << dec << " AVIFile::AVIFile() : RIFFFile(), ..." << endl;

    for (int i = 0; i < 2; ++i) {
	indx[i] = new AVISuperIndex;
        memset(indx[i], 0, sizeof(AVISuperIndex));
        ix[i] = new AVIStdIndex;
        memset(ix[i], 0, sizeof(AVIStdIndex));
        indx_chunk[i] = -1;
        ix_chunk[i] = -1;
        strl_list[i] = -1;
        strh_chunk[i] = -1;
        strf_chunk[i] = -1;
    }
    idx1 = new AVISimpleIndex;
    memset(idx1, 0, sizeof(AVISimpleIndex));
}


/** The copy constructor
 
    \todo add checking for NULL pointers
 
*/

AVIFile::AVIFile(const AVIFile& avi) : RIFFFile(avi)
{
    // cerr << "0x" << hex << (long)this << dec << " 0x" << hex << (long)&avi << dec << " AVIFile::AVIFile(const AVIFile& avi) : RIFFFile(avi)" << endl;

    mainHdr = avi.mainHdr;
    idx1 = new AVISimpleIndex;
    *idx1 = *avi.idx1;
    file_list = avi.file_list;
    riff_list = avi.riff_list;
    hdrl_list = avi.hdrl_list;
    avih_chunk = avi.avih_chunk;
    movi_list = avi.movi_list;
    junk_chunk = avi.junk_chunk;
    idx1_chunk = avi.idx1_chunk;

	int i;

    for ( i = 0; i < 2; ++i) {
        indx[i] = new AVISuperIndex;
        *indx[i] = *avi.indx[i];
        ix[i] = new AVIStdIndex;
        *ix[i] = *avi.ix[i];
        indx_chunk[i] = avi.indx_chunk[i];
        ix_chunk[i] = avi.ix_chunk[i];
        strl_list[i] = avi.strl_list[i];
        strh_chunk[i] = avi.strh_chunk[i];
        strf_chunk[i] = avi.strf_chunk[i];
    }

    index_type = avi.index_type;
    current_ix00 = avi.current_ix00;

    for ( i = 0; i < 62; ++i)
        dmlh[i] = avi.dmlh[i];

    isUpdateIdx1 = avi.isUpdateIdx1;

}


/** The assignment operator
 
*/

AVIFile& AVIFile::operator=(const AVIFile& avi)
{
    // cerr << "0x" << hex << (long)this << dec << " 0x" << hex << (long)&avi << dec << " AVIFile& AVIFile::operator=(const AVIFile& avi)" << endl;

	int i;

    if (this != &avi) {
        RIFFFile::operator=(avi);
        mainHdr = avi.mainHdr;
        *idx1 = *avi.idx1;
        file_list = avi.file_list;
        riff_list = avi.riff_list;
        hdrl_list = avi.hdrl_list;
        avih_chunk = avi.avih_chunk;
        movi_list = avi.movi_list;
        junk_chunk = avi.junk_chunk;
        idx1_chunk = avi.idx1_chunk;

        for ( i = 0; i < 2; ++i) {
            *indx[i] = *avi.indx[i];
            *ix[i] = *avi.ix[i];
            indx_chunk[i] = avi.indx_chunk[i];
            ix_chunk[i] = avi.ix_chunk[i];
            strl_list[i] = avi.strl_list[i];
            strh_chunk[i] = avi.strh_chunk[i];
            strf_chunk[i] = avi.strf_chunk[i];
        } 

        index_type = avi.index_type;
        current_ix00 = avi.current_ix00;

        for ( i = 0; i < 62; ++i)
            dmlh[i] = avi.dmlh[i];

        isUpdateIdx1 = avi.isUpdateIdx1;
    }
    return *this;
}


/** The destructor
 
*/

AVIFile::~AVIFile()
{
    // cerr << "0x" << hex << (long)this << dec << " AVIFile::~AVIFile()" << endl;

    for (int i = 0; i < 2; ++i) {
        delete ix[i];
        delete indx[i];
    }
    delete idx1;
}

/** Find position and size of a given frame in the file
 
    Depending on which index is available, search one of them to
    find position and frame size
 
    \todo the size parameter is redundant. All frames have the same size, which is also in the mainHdr.
    \todo all index related operations should be isolated 
    \param offset the file offset to the start of the frame
    \param size the size of the frame
    \param the number of the frame we wish to find
    \return 0 if the frame could be found, -1 otherwise
*/

int AVIFile::GetDVFrameInfo(__int64 &offset, int &size, int frameNum)
{
    int i;

	switch (index_type) {
    case AVI_LARGE_INDEX:

        /* find relevant index in indx0 */


        for (i = 0; frameNum >= indx[0]->aIndex[i].dwDuration; frameNum -= indx[0]->aIndex[i].dwDuration, ++i)
            ;

        if (i != current_ix00) {
            _lseeki64(fd, indx[0]->aIndex[i].qwOffset + RIFF_HEADERSIZE, SEEK_SET) ;
//ix[0]: AVIStdIndex = header(24) + 4028*2*DWORD = 32224 + 24 = 32248
//                                  17895         143160       143184
                        if (indx[0]->aIndex[i].dwSize > /*32248*/143184 + RIFF_HEADERSIZE) {
				return -1;	//illegal frame requested
			}
            read(fd, ix[0], indx[0]->aIndex[i].dwSize - RIFF_HEADERSIZE) ;
            current_ix00 = i;
        }

        if (frameNum < ix[0]->nEntriesInUse) {
            offset = ix[0]->qwBaseOffset + ix[0]->aIndex[frameNum].dwOffset;
            size = ix[0]->aIndex[frameNum].dwSize;
            return 0;
        } else
            return -1;
        break;

    case AVI_SMALL_INDEX:
        int index = -1;
        int frameNumIndex = 0;
        for ( i = 0; i < idx1->nEntriesInUse; ++i) {
            FOURCC chunkID1 = make_fourcc("00dc");
            FOURCC chunkID2 = make_fourcc("00db");
            if (idx1->aIndex[i].dwChunkId ==  chunkID1 ||
                    idx1->aIndex[i].dwChunkId == chunkID2 ) {
                if (frameNumIndex == frameNum) {
                    index = i;
                    break;
                }
                ++frameNumIndex;
            }
        }
        if (index != -1) {
            // compatibility check for broken dvgrab dv2 format
            if (idx1->aIndex[0].dwOffset > GetDirectoryEntry(movi_list).offset) {
                offset = idx1->aIndex[index].dwOffset + RIFF_HEADERSIZE;
            } else {
                // new, correct dv2 format
                offset = idx1->aIndex[index].dwOffset + RIFF_HEADERSIZE + GetDirectoryEntry(movi_list).offset;
            }
            size = idx1->aIndex[index].dwSize;
            return 0;
        } else
            return -1;
        break;
    }
    return -1;
}


/** Read in a frame
 
    \todo we actually don't need the frame here, we could use just a void pointer
    \param frame a reference to the frame object that will receive the frame data
    \param frameNum the frame number to read
    \return 0 if the frame could be read, -1 otherwise
*/

int AVIFile::GetDVFrame(Frame &frame, int frameNum)
{
    __int64	offset;
    int	size;

    if (GetDVFrameInfo(offset, size, frameNum) != 0)
        return -1;

	_lseeki64(fd, offset, SEEK_SET) ;
    read(fd, frame.data, size);

    return 0;
}


int AVIFile::GetTotalFrames() const
{
    return mainHdr.dwTotalFrames;
}



/** If this is not a movi list, read its contents
 
*/

void AVIFile::ParseList(int parent)
{
    FOURCC      type;
    FOURCC      name;
    int         length;
    int         list;
    __int64     pos;
    __int64     listEnd;

    /* Read in the chunk header (type and length). */
    read(fd, &type, sizeof(type));
    read(fd, &length, sizeof(length));
    if (length & 1)
        length++;

    /* The contents of the list starts here. Obtain its offset. The list
       name (4 bytes) is already part of the contents). */

    pos = _lseeki64(fd, 0, SEEK_CUR);
    read(fd, &name, sizeof(name));

    /* if we encounter a movi list, do not read it. It takes too much time
       and we don't need it anyway. */

    if (name != make_fourcc("movi")) {
//    if (1) {

        /* Add an entry for this list. */
        list = AddDirectoryEntry(type, name, sizeof(name), parent);

        /* Read in any chunks contained in this list. This list is the
           parent for all chunks it contains. */

        listEnd = pos + length;
        while (pos < listEnd) {
            ParseChunk(list);
            pos = _lseeki64(fd, 0, SEEK_CUR);
        }
    } else {
        /* Add an entry for this list. */

        movi_list = AddDirectoryEntry(type, name, length, parent);

        pos = _lseeki64(fd, length - 4, SEEK_CUR);
    }
}


void AVIFile::ParseRIFF()
{
    RIFFFile::ParseRIFF();

    avih_chunk = FindDirectoryEntry(make_fourcc("avih"));
    if (avih_chunk != -1)
        ReadChunk(avih_chunk, (void*)&mainHdr);
}


void AVIFile::ReadIndex()
{
    indx_chunk[0] = FindDirectoryEntry(make_fourcc("indx"));
    if (indx_chunk[0] != -1) {
        ReadChunk(indx_chunk[0], (void*)indx[0]);
        index_type = AVI_LARGE_INDEX;
		
        /* recalc number of frames from each index */
        mainHdr.dwTotalFrames = 0;
        for (int i = 0; 
            i < indx[0]->nEntriesInUse; 
            mainHdr.dwTotalFrames += indx[0]->aIndex[i++].dwDuration);
        return;
    }
    idx1_chunk = FindDirectoryEntry(make_fourcc("idx1"));
    if (idx1_chunk != -1) {
        ReadChunk(idx1_chunk, (void*)idx1);
        idx1->nEntriesInUse = GetDirectoryEntry(idx1_chunk).length / 16;
        index_type = AVI_SMALL_INDEX;
		
		/* recalc number of frames from the simple index */
        int frameNumIndex = 0;
        FOURCC chunkID1 = make_fourcc("00dc");
        FOURCC chunkID2 = make_fourcc("00db");
        for (int i = 0; i < idx1->nEntriesInUse; ++i) {
            if (idx1->aIndex[i].dwChunkId == chunkID1 || 
                    idx1->aIndex[i].dwChunkId == chunkID2 ) {
                ++frameNumIndex;
            }
        }
        mainHdr.dwTotalFrames = frameNumIndex;
        return;
    }
}



bool AVIFile::isOpenDML( void )
{
	int i, j = 0;
	FOURCC dmlh = make_fourcc("dmlh");
	
	while ( (i = FindDirectoryEntry( dmlh, j++ )) != -1 )
	{
		return true;
	}
    return false;
}
