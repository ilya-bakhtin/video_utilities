/*
 * frame.cc -- utilities for processing digital video frames
 * Copyright (C) 2000 Arne Schirmacher <arne@schirmacher.de>
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

/** Code for handling raw DV frame data
 
    These methods are for handling the raw DV frame data. It contains methods for 
    getting info and retrieving the audio data.
*/

// C++ includes

#include <string>
#include <iostream>
//#include <strstream>
//#include <iomanip>
//#include <deque>

//using std::strstream;
//using std::setw;
//using std::setfill;
//using std::deque;
using std::cout;
using std::endl;

// local includes
#include "frame.h"
#include "avisynth.h"

#ifndef HAVE_LIBDV
bool Frame::maps_initialized = false;
int Frame::palmap_ch1[2000];
int Frame::palmap_ch2[2000];
int Frame::palmap_2ch1[2000];
int Frame::palmap_2ch2[2000];

int Frame::ntscmap_ch1[2000];
int Frame::ntscmap_ch2[2000];
int Frame::ntscmap_2ch1[2000];
int Frame::ntscmap_2ch2[2000];

short Frame::compmap[4096];
#endif


/** constructor
 
    All Frame objects share a set of lookup maps,
    which are initalized once (we are using a variant of the Singleton pattern). 

*/

Frame::Frame() : bytesInFrame(0)
{
    memset(data, 0, 144000);
	int n;
	int y;

    if (maps_initialized == false) {

        for (n = 0; n < 1944; ++n) {
            int sequence1 = ((n / 3) + 2 * (n % 3)) % 6;
            int sequence2 = sequence1 + 6;
            int block = 3 * (n % 3) + ((n % 54) / 18);

            block = 6 + block * 16;
            {
                register int byte = 8 + 2 * (n / 54);
                palmap_ch1[n] = sequence1 * 150 * 80 + block * 80 + byte;
                palmap_ch2[n] = sequence2 * 150 * 80 + block * 80 + byte;
                byte += (n / 54);
                palmap_2ch1[n] = sequence1 * 150 * 80 + block * 80 + byte;
                palmap_2ch2[n] = sequence2 * 150 * 80 + block * 80 + byte;
            }
        }
        for (n = 0; n < 1620; ++n) {
            int sequence1 = ((n / 3) + 2 * (n % 3)) % 5;
            int sequence2 = sequence1 + 5;
            int block = 3 * (n % 3) + ((n % 45) / 15);

            block = 6 + block * 16;
            {
                register int byte = 8 + 2 * (n / 45);
                ntscmap_ch1[n] = sequence1 * 150 * 80 + block * 80 + byte;
                ntscmap_ch2[n] = sequence2 * 150 * 80 + block * 80 + byte;
                byte += (n / 45);
                ntscmap_2ch1[n] = sequence1 * 150 * 80 + block * 80 + byte;
                ntscmap_2ch2[n] = sequence2 * 150 * 80 + block * 80 + byte;
            }
        }
        for ( y = 0x700; y <= 0x7ff; ++y)
            compmap[y] = (y - 0x600) << 6;
        for ( y = 0x600; y <= 0x6ff; ++y)
            compmap[y] = (y - 0x500) << 5;
        for ( y = 0x500; y <= 0x5ff; ++y)
            compmap[y] = (y - 0x400) << 4;
        for ( y = 0x400; y <= 0x4ff; ++y)
            compmap[y] = (y - 0x300) << 3;
        for ( y = 0x300; y <= 0x3ff; ++y)
            compmap[y] = (y - 0x200) << 2;
        for ( y = 0x200; y <= 0x2ff; ++y)
            compmap[y] = (y - 0x100) << 1;
        for ( y = 0x000; y <= 0x1ff; ++y)
            compmap[y] = y;
        for ( y = 0x800; y <= 0xfff; ++y)
            compmap[y] = -1 - compmap[0xfff - y];
        maps_initialized = true;
    }
	for (  n = 0; n < 4; n++ )
		audio_buffers[n] = (__int16 *) malloc(2 * DV_AUDIO_MAX_SAMPLES * sizeof(__int16));
}


Frame::~Frame()
{
	for (int n = 0; n < 4; n++) {
		free(audio_buffers[n]);
	}
}


/** gets a subcode data packet
 
    This function returns a SSYB packet from the subcode data section.
 
    \param packNum the SSYB package id to return
    \param pack a reference to the variable where the result is stored
    \return true for success, false if no pack could be found */

bool Frame::GetSSYBPack(int packNum, Pack &pack) const
{
	/* number of DIF sequences is different for PAL and NTSC */

    int seqCount = IsPAL() ? 12 : 10;

    /* process all DIF sequences */

    for (int i = 0; i < seqCount; ++i) {

        /* there are two DIF blocks in the subcode section */

        for (int j = 0; j < 2; ++j) {

            /* each block has 6 packets */

            for (int k = 0; k < 6; ++k) {

                /* calculate address: 150 DIF blocks per sequence, 80 bytes
                   per DIF block, subcode blocks start at block 1, block and
                   packet have 3 bytes header, packet is 8 bytes long
                   (including header) */

                const unsigned char *s = &data[i * 150 * 80 + 1 * 80 + j * 80 + 3 + k * 8 + 3];
                // printf("ssyb %d: %2.2x %2.2x %2.2x %2.2x %2.2x\n",
                // j * 6 + k, s[0], s[1], s[2], s[3], s[4]);
                if (s[0] == packNum) {

//					sprintf(buf, "GetSSYBPack[%x]: sequence %d, block %d, packet %d\n", packNum,i,j,k);
//					OutputDebugString(buf);

                    pack.data[0] = s[0];
                    pack.data[1] = s[1];
                    pack.data[2] = s[2];
                    pack.data[3] = s[3];
                    pack.data[4] = s[4];
                    return true;
                }
            }
        }
    }
    return false;

}


/** gets a video auxiliary data packet
 
    Every DIF block in the video auxiliary data section contains 15
    video auxiliary data packets, for a total of 45 VAUX packets. As
    the position of a VAUX packet is fixed, we could directly look it
    up, but I choose to walk through all data as with the other
    GetXXXX routines.
 
    \param packNum the VAUX package id to return
    \param pack a reference to the variable where the result is stored
    \return true for success, false if no pack could be found */

bool Frame::GetVAUXPack(int packNum, Pack &pack) const
{
	/* number of DIF sequences is different for PAL and NTSC */

    int seqCount = IsPAL() ? 12 : 10;

    /* process all DIF sequences */

    for (int i = 0; i < seqCount; ++i) {

        /* there are three DIF blocks in the VAUX section */

        for (int j = 0; j < 3; ++j) {

            /* each block has 15 packets */

            for (int k = 0; k < 15; ++k) {

                /* calculate address: 150 DIF blocks per sequence, 80 bytes
                   per DIF block, vaux blocks start at block 3, block has 3
                   bytes header, packets have no header and are 5 bytes
                   long. */

                const unsigned char *s = &data[i * 150 * 80 + 3 * 80 + j * 80 + 3 + k * 5];
                // printf("vaux %d: %2.2x %2.2x %2.2x %2.2x %2.2x\n",
                // j * 15 + k, s[0],  s[1],  s[2],  s[3],  s[4]);
                if (s[0] == packNum) {
                    pack.data[0] = s[0];
                    pack.data[1] = s[1];
                    pack.data[2] = s[2];
                    pack.data[3] = s[3];
                    pack.data[4] = s[4];
                    return true;
                }
            }
        }
    }
    return false;
}


/** gets an audio auxiliary data packet
 
    Every DIF block in the audio section contains 5 bytes audio
    auxiliary data and 72 bytes of audio data.  The function searches
    through all DIF blocks although AAUX packets are only allowed in
    certain defined DIF blocks.
 
    \param packNum the AAUX package id to return
    \param pack a reference to the variable where the result is stored
    \return true for success, false if no pack could be found */

bool Frame::GetAAUXPack(int packNum, Pack &pack) const
{
    /* number of DIF sequences is different for PAL and NTSC */

    int seqCount = IsPAL() ? 12 : 10;

    /* process all DIF sequences */

    for (int i = 0; i < seqCount; ++i) {

        /* there are nine audio DIF blocks */

        for (int j = 0; j < 9; ++j) {

            /* calculate address: 150 DIF blocks per sequence, 80 bytes
               per DIF block, audio blocks start at every 16th beginning
               with block 6, block has 3 bytes header, followed by one
               packet. */

            const unsigned char *s = &data[i * 150 * 80 + 6 * 80 + j * 16 * 80 + 3];
            if (s[0] == packNum) {
                // printf("aaux %d: %2.2x %2.2x %2.2x %2.2x %2.2x\n",
                // j, s[0], s[1], s[2], s[3], s[4]);
                pack.data[0] = s[0];
                pack.data[1] = s[1];
                pack.data[2] = s[2];
                pack.data[3] = s[3];
                pack.data[4] = s[4];
                return true;
            }
        }
    }
    return false;
}


/** gets the date and time of recording of this frame
 
    Returns a struct tm with date and time of recording of this frame.
 
    This code courtesy of Andy (http://www.videox.net/) 
 
    \param recDate the time and date of recording of this frame
    \return true for success, false if no date or time information could be found */

bool Frame::GetRecordingDate(struct tm &recDate) const
{
    Pack pack62;
    Pack pack63;

    if (GetSSYBPack(0x62, pack62) == false)
        return false;

    int day = pack62.data[2];
    int month = pack62.data[3];
    int year = pack62.data[4];

    if (GetSSYBPack(0x63, pack63) == false)
        return false;

    int sec = pack63.data[2];
    int min = pack63.data[3];
    int hour = pack63.data[4];

    sec = (sec & 0xf) + 10 * ((sec >> 4) & 0x7);
    min = (min & 0xf) + 10 * ((min >> 4) & 0x7);
    hour = (hour & 0xf) + 10 * ((hour >> 4) & 0x3);
    year = (year & 0xf) + 10 * ((year >> 4) & 0xf);
    month = (month & 0xf) + 10 * ((month >> 4) & 0x1);
    day = (day & 0xf) + 10 * ((day >> 4) & 0x3);

    if (year < 25)
        year += 2000;
    else
        year += 1900;

    recDate.tm_sec = sec;
    recDate.tm_min = min;
    recDate.tm_hour = hour;
    recDate.tm_mday = day;
    recDate.tm_mon = month - 1;
    recDate.tm_year = year - 1900;
    recDate.tm_wday = -1;
    recDate.tm_yday = -1;
    recDate.tm_isdst = -1;

    /* sanity check of the results */

    if (mktime(&recDate) == -1)
        return false;
    return true;
}


/*
string Frame::GetRecordingDate(void) const
{
    string recDate;
    struct tm date;
    strstream sb;

    if (GetRecordingDate(date) == true) {
        sb << setfill('0')
        << setw(4) << date.tm_year + 1900 << '.'
        << setw(2) << date.tm_mon + 1 << '.'
        << setw(2) << date.tm_mday << '_'
        << setw(2) << date.tm_hour << '-'
        << setw(2) << date.tm_min << '-'
        << setw(2) << date.tm_sec;
        sb >> recDate;
    } else {
        recDate = "0000.00.00_00-00-00";
    }
    return recDate;
}
*/

/** gets the timecode information of this frame
 
    Returns a string with the timecode of this frame. The timecode is
    the relative location of this frame on the tape, and is defined
    by hour, minute, second and frame (within the last second).
     
    \param timeCode the TimeCode struct
    \return true for success, false if no timecode information could be found */

bool Frame::GetTimeCode(tm &timeCode) const
{
    Pack tc;

    if (GetSSYBPack(0x13, tc) == false)
        return false;

    int frame = tc.data[1];
    int sec = tc.data[2];
    int min = tc.data[3];
    int hour = tc.data[4];

    timeCode.tm_mday = (frame & 0xf) + 10 * ((frame >> 4) & 0x3);
    timeCode.tm_sec = (sec & 0xf) + 10 * ((sec >> 4) & 0x7);
    timeCode.tm_min = (min & 0xf) + 10 * ((min >> 4) & 0x7);
    timeCode.tm_hour = (hour & 0xf) + 10 * ((hour >> 4) & 0x3);
	//dirty hack to store frame in tm struct !
	timeCode.tm_isdst = 0;
	timeCode.tm_mon = 0;
	timeCode.tm_wday = 0;
	timeCode.tm_yday = timeCode.tm_mday;
	timeCode.tm_year = 100;
    return true;
}


/** gets the audio properties of this frame
 
    get the sampling frequency and the number of samples in this particular DV frame (which can vary)
 
    \param info the AudioInfo record
    \return true, if audio properties could be determined */

bool Frame::GetAudioInfo(AudioInfo &info) const
{
    int af_size;
    int smp;
    int flag;
    Pack pack50;
	

	info.channels = 2;


    /* Check whether this frame has a valid AAUX source packet
       (header == 0x50). If so, get the audio samples count. If not,
       skip this audio data. */

    if (GetAAUXPack(0x50, pack50) == true) {

        /* get size, sampling type and the 50/60 flag. The number of
           audio samples is dependend on all of these. */

        af_size = pack50.data[1] & 0x3f;
        smp = (pack50.data[4] >> 3) & 0x07;
        flag = pack50.data[3] & 0x20;

        if (flag == 0) {
            info.frames = 60;
            switch (smp) {
            case 0:
                info.frequency = 48000;
                info.samples = 1580 + af_size;
                break;
            case 1:
                info.frequency = 44100;
                info.samples = 1452 + af_size;
                break;
            case 2:
                info.frequency = 32000;
                info.samples = 1053 + af_size;

                break;
            }
        } else { // 50 frames (PAL)
            info.frames = 50;
            switch (smp) {
            case 0:
                info.frequency = 48000;
                info.samples = 1896 + af_size;
                break;
            case 1:
                info.frequency = 44100;
                info.samples = 0; // I don't know
                break;
            case 2:
                info.frequency = 32000;
                info.samples = 1264 + af_size;

                break;
            }
        }
        return true;
    } else {
        return false;
    }
}


/** gets the size of the frame
 
    Depending on the type (PAL or NTSC) of the frame, the length of the frame is returned 
 
    \return the length of the frame in Bytes */

int Frame::GetFrameSize(void) const
{
    return IsPAL() ? 144000 : 120000;
}


/** get the video frame rate

    \return frames per second
*/
float Frame::GetFrameRate() const
{
	return IsPAL() ? 25.0 : 30000.0/1001.0;
}


/** checks whether the frame is in PAL or NTSC format
 
    \todo function can't handle "empty" frame
    \return true for PAL frame, false for a NTSC frame
*/

bool Frame::IsPAL(void) const
{
    unsigned char dsf = data[3] & 0x80;
    bool pal = (dsf == 0) ? false : true;
    return pal;
}


/** checks whether this frame is the first in a new recording
 
    To determine this, the function looks at the recStartPoint bit in
    AAUX pack 51.
 
    \return true if this frame is the start of a new recording */

bool Frame::IsNewRecording() const
{
    Pack aauxSourceControl;

    /* if we can't find the packet, we return "no new recording" */

    if (GetAAUXPack(0x51, aauxSourceControl) == false)
        return false;

    unsigned char recStartPoint = aauxSourceControl.data[2] & 0x80;

    return recStartPoint == 0 ? true : false;
}


/** check whether we have received as many bytes as expected for this frame
 
    \return true if this frames is completed, false otherwise */

bool Frame::IsComplete(void) const
{
    return bytesInFrame == GetFrameSize();
}


/** retrieves the audio data from the frame
 
    The DV frame contains audio data mixed in the video data blocks, 
    which can be retrieved easily using this function.
 
    The audio data consists of 16 bit, two channel audio samples (a 16 bit word for channel 1, followed by a 16 bit word
    for channel 2 etc.)
 
    \param sound a pointer to a buffer that holds the audio data
    \return the number of bytes put into the buffer, or 0 if no audio data could be retrieved */

int Frame::ExtractAudio(void *sound) const
{
    AudioInfo info;
	
    if (GetAudioInfo(info) == true) {
        /* Collect the audio samples */
        char* s = (char *) sound;

        switch (info.frequency) {
        case 32000:

            /* This is 4 channel audio */

            if (IsPAL()) {
                short* p = (short*)sound;
                for (int n = 0; n < info.samples; ++n) {
                    register int r = ((unsigned char*)data)[palmap_2ch1[n] + 1]; // LSB
                    *p++ = compmap[(((unsigned char*)data)[palmap_2ch1[n]] << 4) + (r >> 4)];   // MSB
                    *p++ = compmap[(((unsigned char*)data)[palmap_2ch1[n] + 1] << 4) + (r & 0x0f)];
                }


            } else {
                short* p = (short*)sound;
                for (int n = 0; n < info.samples; ++n) {
                    register int r = ((unsigned char*)data)[ntscmap_2ch1[n] + 1]; // LSB
                    *p++ = compmap[(((unsigned char*)data)[ntscmap_2ch1[n]] << 4) + (r >> 4)];   // MSB
                    *p++ = compmap[(((unsigned char*)data)[ntscmap_2ch1[n] + 1] << 4) + (r & 0x0f)];
                }
            }
            break;

        case 44100:
        case 48000:

            /* this can be optimized significantly */

            if (IsPAL()) {
                for (int n = 0; n < info.samples; ++n) {
                    *s++ = ((char*)data)[palmap_ch1[n] + 1]; /* LSB */
                    *s++ = ((char*)data)[palmap_ch1[n]];     /* MSB */
                    *s++ = ((char*)data)[palmap_ch2[n] + 1]; /* LSB */
                    *s++ = ((char*)data)[palmap_ch2[n]];     /* MSB */
                }
            } else {
                for (int n = 0; n < info.samples; ++n) {
                    *s++ = ((char*)data)[ntscmap_ch1[n] + 1]; /* LSB */
                    *s++ = ((char*)data)[ntscmap_ch1[n]];     /* MSB */
                    *s++ = ((char*)data)[ntscmap_ch2[n] + 1]; /* LSB */
                    *s++ = ((char*)data)[ntscmap_ch2[n]];     /* MSB */
                }
            }
            break;

            /* we can't handle any other format in the moment */

        default:
            info.samples = 0;
        }
		
	} else
		info.samples = 0;
	
	return info.samples * info.channels * 2;
}
