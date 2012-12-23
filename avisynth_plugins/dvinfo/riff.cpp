/*
 * riff.cc library for RIFF file format i/o
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

// C++ includes

#include <string>
//#include <stdio.h>
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

#include <fcntl.h>
#include <crtdbg.h>
#include <io.h>
//#include <unistd.h>

// local includes

#include "error.h"
#include "riff.h"


/** make a 32 bit "string-id"
 
    \param s a pointer to 4 chars
    \return the 32 bit "string id"
    \bugs It is not checked whether we really have 4 characters
 
    Some compilers understand constants like int id = 'ABCD'; but I
    could not get it working on the gcc compiler so I had to use this
    workaround. We can now use id = make_fourcc("ABCD") instead. */

FOURCC make_fourcc(char *s)
{
    if (s[0] == 0)
        return 0;
    else
        return *((unsigned int*) s);
}


RIFFDirEntry::RIFFDirEntry()
{}


RIFFDirEntry::RIFFDirEntry (FOURCC t, FOURCC n, int l, int o, int p): type(t), name(n), length(l), offset(o), parent(p), written(0)
{}


/** Creates the object without an output file.
 
*/

RIFFFile::RIFFFile() : fd(-1)
{}


/* Copy constructor
 
   Duplicate the file descriptor
*/

RIFFFile::RIFFFile(const RIFFFile& riff) : fd(-1)
{
    if (riff.fd != -1) {
        fd = dup(riff.fd);
    }
    directory = riff.directory;
}


/** Destroys the object.
 
    If it has an associated opened file, close it. */

RIFFFile::~RIFFFile()
{
    Close();
}


RIFFFile& RIFFFile::operator=(const RIFFFile& riff)
{
    if (fd != riff.fd) {
        Close();
        if (riff.fd != -1) {
            fd = dup(riff.fd);
        }
        directory = riff.directory;
    }
    return *this;
}



/** Opens the file read only.
 
    \param s the filename
*/

bool RIFFFile::Open(const char *s)
{
    fd = _open(s, _O_BINARY | _O_RDONLY | _O_RANDOM);

    if (fd == -1)
        return false;
    else
        return true;
}


/** Destroys the object.
 
    If it has an associated opened file, close it. */

void RIFFFile::Close()
{
    if (fd != -1) {
        close(fd);
        fd = -1;
    }
}

/** Adds an entry to the list of containers.
 
    \param type the type of this entry
    \param name the name
    \param length the length of the data in the container
    \param list the container in which this object is contained. 
    \return the ID of the newly created entry
 
    The topmost object is not contained in any other container. Use
    the special ID RIFF_NO_PARENT to create the topmost object. */

int RIFFFile::AddDirectoryEntry(FOURCC type, FOURCC name, __int64 length, int list)
{
    /* Put all parameters in an RIFFDirEntry object. The offset is
       currently unknown. */

    RIFFDirEntry entry(type, name, length, 0 /* offset */, list);

    /* If the new chunk is in a list, then get the offset and size
       of that list. The offset of this chunk is the end of the list
       (parent_offset + parent_length) plus the size of the chunk
       header. */

    if (list != RIFF_NO_PARENT) {
        RIFFDirEntry parent = GetDirectoryEntry(list);
        entry.offset = parent.offset + parent.length + RIFF_HEADERSIZE;
    }

    /* The list which this new chunk is a member of has now increased in
       size. Get that directory entry and bump up its length by the size
       of the chunk. Since that list may also be contained in another
       list, walk up to the top of the tree. */

    while (list != RIFF_NO_PARENT) {
        RIFFDirEntry parent = GetDirectoryEntry(list);
        parent.length += RIFF_HEADERSIZE + length;
        SetDirectoryEntry(list, parent);
        list = parent.parent;
    }

    directory.insert(directory.end(), entry);

    return directory.size() - 1;
}


/** Modifies an entry.
 
    The entry.written flag is set to false because the contents has been modified
 
    \param i the ID of the entry which is to modify
    \param entry the new entry 
    \note Do not change length, offset, or the parent container.
    \note Do not change an empty name ("") to a name and vice versa */

void RIFFFile::SetDirectoryEntry(int i, RIFFDirEntry &entry)
{
    _ASSERT(i >= 0 && i < (int)directory.size());

    entry.written = false;
    directory[i] = entry;
}


/** Retrieves an entry.
 
    Gets the whole RIFFDirEntry object.
 
    \param i the ID of the entry to retrieve
    \return the entry */

RIFFDirEntry RIFFFile::GetDirectoryEntry(int i) const
{
    _ASSERT (i >= 0 && i < (int)directory.size());

    return directory[i];
}


/** finds the index
 
    finds the index of a given directory entry type 
 
    \todo inefficient if the directory has lots of items
    \param type the type of the entry to find
    \param n    the zero-based instance of type to locate
    \return the index of the found object in the directory, or -1 if not found */

int RIFFFile::FindDirectoryEntry (FOURCC type, int n) const
{
    int i, j = 0;
    int count = directory.size();

    for (i = 0; i < count; ++i)
        if (directory[i].type == type)
		{
			if (j == n)
				return i;
			j++;
		}

    return -1;
}


/** Reads all items that are contained in one list
 
    Read in one chunk and add it to the directory. If the chunk
    happens to be of type LIST, then call ParseList recursively for
    it.
 
    \param parent The id of the item to process
*/

void RIFFFile::ParseChunk(int parent)
{
    FOURCC      type;
    int         length;
    int         typesize;

    /* Check whether it is a LIST. If so, let ParseList deal with it */

    read(fd, &type, sizeof(type));
    if (type == make_fourcc("LIST")) {
        typesize = -sizeof(type);
        _lseeki64(fd, typesize, SEEK_CUR);
        ParseList(parent);
    }

    /* it is a normal chunk, create a new directory entry for it */

    else {
        read(fd, &length, sizeof(length));
        if (length & 1)
            length++;
        AddDirectoryEntry(type, 0, length, parent);
        _lseeki64(fd, length, SEEK_CUR);
    }
}


/** Reads all items that are contained in one list
 
    \param parent The id of the list to process
 
*/

void RIFFFile::ParseList(int parent)
{
    FOURCC      type;
    FOURCC      name;
    int         list;
    int         length;
    __int64     pos;
    __int64	listEnd;

    /* Read in the chunk header (type and length). */
    read(fd, &type, sizeof(type));
    read(fd, &length, sizeof(length));

    if (length & 1)
        length++;

    /* The contents of the list starts here. Obtain its offset. The list
       name (4 bytes) is already part of the contents). */

    pos = lseek(fd, 0, SEEK_CUR);
    read(fd, &name, sizeof(name));

    /* Add an entry for this list. */

    list = AddDirectoryEntry(type, name, sizeof(name), parent);

    /* Read in any chunks contained in this list. This list is the
       parent for all chunks it contains. */

    listEnd = pos + length;
    while (pos < listEnd) {
        ParseChunk(list);
        pos = _lseeki64(fd, 0, SEEK_CUR);
    }
}


/** Reads the directory structure of the whole RIFF file
 
*/

void RIFFFile::ParseRIFF(void)
{
    FOURCC      type;
    int         length;
    __int64     pos;
    int         container = AddDirectoryEntry(make_fourcc("FILE"), make_fourcc("FILE"), 0, RIFF_NO_PARENT);

    pos = _lseeki64(fd, 0, SEEK_SET);

    /* calculate file size from RIFF header instead from physical file. */

    while ((read(fd, &type, sizeof(type)) > 0) && 
           (read(fd, &length, sizeof(length)) > 0) &&
           (type == make_fourcc("RIFF"))) {

        _lseeki64(fd, pos, SEEK_SET);
        ParseList(container);
        pos = _lseeki64(fd, 0, SEEK_CUR);
    }
}


/** Reads one item including its contents from the RIFF file
 
    \param chunk_index The index of the item to write
    \param data A pointer to the data
 
*/

void RIFFFile::ReadChunk(int chunk_index, void *data)
{
    RIFFDirEntry entry;

    entry = GetDirectoryEntry(chunk_index);
    _lseeki64(fd, entry.offset, SEEK_SET);
    read(fd, data, entry.length);
}