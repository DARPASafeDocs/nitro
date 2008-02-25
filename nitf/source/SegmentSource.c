/* =========================================================================
 * This file is part of NITRO
 * =========================================================================
 * 
 * (C) Copyright 2004 - 2008, General Dynamics - Advanced Information Systems
 *
 * NITRO is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public 
 * License along with this program; if not, If not, 
 * see <http://www.gnu.org/licenses/>.
 *
 */

#include "nitf/SegmentSource.h"

/*
 *  Private implementation struct
 */
typedef struct _MemorySourceImpl
{
    char *data;
    size_t size;
    off_t mark;
    int byteSkip;
    off_t start;
}
MemorySourceImpl;


NITFPRIV(MemorySourceImpl *) toMemorySource(NITF_DATA * data,
        nitf_Error * error)
{
    MemorySourceImpl *memorySource = (MemorySourceImpl *) data;
    if (memorySource == NULL)
    {
        nitf_Error_init(error, "Null pointer reference",
                        NITF_CTXT, NITF_ERR_INVALID_OBJECT);
        return NULL;
    }
    return memorySource;
}


NITFPRIV(NITF_BOOL) MemorySource_contigRead(MemorySourceImpl *
        memorySource, char *buf,
        size_t size,
        nitf_Error * error)
{
    memcpy(buf, memorySource->data + memorySource->mark, size);
    memorySource->mark += size;
    return NITF_SUCCESS;
}


NITFPRIV(NITF_BOOL) MemorySource_offsetRead(MemorySourceImpl *
        memorySource, char *buf,
        size_t size,
        nitf_Error * error)
{
    int i = 0;
    int j = 0;

    while (i < size)
    {
        buf[i++] = *(memorySource->data + memorySource->mark++);
        memorySource->mark += (memorySource->byteSkip);
    }
    return NITF_SUCCESS;
}


/*
 *  Private read implementation for memory source.
 */
NITFPRIV(NITF_BOOL) MemorySource_read(NITF_DATA * data,
                                      char *buf,
                                      size_t size, nitf_Error * error)
{
    MemorySourceImpl *memorySource = toMemorySource(data, error);
    if (!memorySource)
        return NITF_FAILURE;

    /*  We like the contiguous read case, its fast  */
    /*  We want to make sure we reward this case    */
    if (memorySource->byteSkip == 0)
        return MemorySource_contigRead(memorySource, buf, size, error);

    return MemorySource_offsetRead(memorySource, buf, size, error);
}


NITFPRIV(void) MemorySource_destruct(NITF_DATA * data)
{
    MemorySourceImpl *memorySource = (MemorySourceImpl *) data;
    assert(memorySource);
    NITF_FREE(memorySource);
}

NITFPRIV(size_t) MemorySource_getSize(NITF_DATA * data)
{
    MemorySourceImpl *memorySource = (MemorySourceImpl *) data;
    assert(memorySource);
    return (size_t)(memorySource->size / (memorySource->byteSkip + 1));
}


NITFAPI(nitf_SegmentSource *) nitf_SegmentMemorySource_construct
(
    char *data,
    size_t size,
    off_t start,
    int byteSkip,
    nitf_Error * error
)
{
    static nitf_IDataSource iMemorySource =
        {
            &MemorySource_read,
            &MemorySource_destruct,
            &MemorySource_getSize
        };
    MemorySourceImpl *impl = NULL;
    nitf_SegmentSource *segmentSource = NULL;

    impl = (MemorySourceImpl *) NITF_MALLOC(sizeof(MemorySourceImpl));
    if (!impl)
    {
        nitf_Error_init(error, NITF_STRERROR(NITF_ERRNO), NITF_CTXT,
                        NITF_ERR_MEMORY);
        return NULL;
    }

    impl->data = data;
    impl->size = size;
    impl->mark = impl->start = (start >= 0 ? start : 0);
    impl->byteSkip = byteSkip >= 0 ? byteSkip : 0;

    segmentSource = (nitf_SegmentSource *) NITF_MALLOC(sizeof(nitf_SegmentSource));
    if (!segmentSource)
    {
        nitf_Error_init(error, NITF_STRERROR(NITF_ERRNO), NITF_CTXT,
                        NITF_ERR_MEMORY);
        return NULL;
    }
    segmentSource->data = impl;
    segmentSource->iface = &iMemorySource;
    return segmentSource;
}


/*
 *  Private implementation struct
 */
typedef struct _FileSourceImpl
{
    nitf_IOHandle handle;
    off_t start;
    off_t size;
    int byteSkip;
    off_t mark;
}
FileSourceImpl;

NITFPRIV(void) FileSource_destruct(NITF_DATA * data)
{
    NITF_FREE(data);
}

NITFPRIV(size_t) FileSource_getSize(NITF_DATA * data)
{
    FileSourceImpl *fileSource = (FileSourceImpl *) data;
    assert(fileSource);
    return (size_t)((fileSource->size - fileSource->start) / (fileSource->byteSkip + 1));
}


NITFPRIV(FileSourceImpl *) toFileSource(NITF_DATA * data,
                                        nitf_Error * error)
{
    FileSourceImpl *fileSource = (FileSourceImpl *) data;
    if (fileSource == NULL)
    {
        nitf_Error_init(error, "Null pointer reference",
                        NITF_CTXT, NITF_ERR_INVALID_OBJECT);
        return NULL;
    }
    return fileSource;
}


NITFPRIV(NITF_BOOL) FileSource_contigRead(FileSourceImpl * fileSource,
        char *buf,
        size_t size, nitf_Error * error)
{
    if (!NITF_IO_SUCCESS(nitf_IOHandle_read(fileSource->handle,
                                            buf, size, error)))
        return NITF_FAILURE;
    fileSource->mark += size;
    return NITF_SUCCESS;
}


/*
 *  The idea here is we will speed it up by creating a temporary buffer
 *  for reading from the io handle.  Even with the allocation, this should
 *  be much faster than seeking every time.
 *
 *  The basic idea is that we allocate the temporary buffer to the request
 *  size * the skip factor.  It should be noted that the tradeoff here is that,
 *  for very large read values, this may be really undesirable, especially for
 *  large skip factors.
 *
 *  If this proves to be a problem, I will revert it back to a seek/read paradigm
 *  -DP
 */
NITFPRIV(NITF_BOOL) FileSource_offsetRead(FileSourceImpl * fileSource,
        char *buf,
        size_t size, nitf_Error * error)
{

    size_t tsize = size * (fileSource->byteSkip + 1);

    char *tbuf;
    off_t lmark = 0;
    int i = 0;
    int j = 0;
    if (tsize + fileSource->mark > fileSource->size)
        tsize = fileSource->size - fileSource->mark;

    tbuf = (char *) NITF_MALLOC(tsize);
    if (!tbuf)
    {
        nitf_Error_init(error,
                        NITF_STRERROR(NITF_ERRNO),
                        NITF_CTXT, NITF_ERR_MEMORY);
        return NITF_FAILURE;
    }

    if (!nitf_IOHandle_read(fileSource->handle, tbuf, tsize, error))
    {
        NITF_FREE(tbuf);
        return NITF_FAILURE;
    }
    /*  Downsize for buf */
    while (i < size)
    {
        buf[i++] = *(tbuf + lmark++);
        lmark += (fileSource->byteSkip);
    }
    fileSource->mark += lmark;
    NITF_FREE(tbuf);
    return NITF_SUCCESS;
}


/*
 *  Private read implementation for file source.
 */
NITFPRIV(NITF_BOOL) FileSource_read(NITF_DATA * data,
                                    char *buf,
                                    size_t size, nitf_Error * error)
{
    FileSourceImpl *fileSource = toFileSource(data, error);
    if (!fileSource)
        return NITF_FAILURE;

    if (!NITF_IO_SUCCESS(nitf_IOHandle_seek(fileSource->handle,
                                            fileSource->mark,
                                            NITF_SEEK_SET, error)))
        return NITF_FAILURE;
    if (fileSource->byteSkip == 0)
        return FileSource_contigRead(fileSource, buf, size, error);
    return FileSource_offsetRead(fileSource, buf, size, error);
}


NITFAPI(nitf_SegmentSource *) nitf_SegmentFileSource_construct
(
    nitf_IOHandle handle,
    off_t start,
    int byteSkip,
    nitf_Error * error
)
{
    static nitf_IDataSource iFileSource =
        {
            &FileSource_read,
            &FileSource_destruct,
            &FileSource_getSize
        };
    FileSourceImpl *impl = NULL;
    nitf_SegmentSource *segmentSource = NULL;

    impl = (FileSourceImpl *) NITF_MALLOC(sizeof(FileSourceImpl));
    if (!impl)
    {
        nitf_Error_init(error, NITF_STRERROR(NITF_ERRNO), NITF_CTXT,
                        NITF_ERR_MEMORY);
        return NULL;
    }
    impl->handle = handle;
    impl->byteSkip = byteSkip >= 0 ? byteSkip : 0;
    impl->mark = impl->start = (start >= 0 ? start : 0);
    impl->size = nitf_IOHandle_getSize(handle, error);

    if (!NITF_IO_SUCCESS(impl->size))
    {
        NITF_FREE(impl);
        return NULL;
    }

    segmentSource = (nitf_SegmentSource *) NITF_MALLOC(sizeof(nitf_SegmentSource));
    if (!segmentSource)
    {
        nitf_Error_init(error, NITF_STRERROR(NITF_ERRNO), NITF_CTXT,
                        NITF_ERR_MEMORY);
        return NULL;
    }
    segmentSource->data = impl;
    segmentSource->iface = &iFileSource;
    return segmentSource;
}
