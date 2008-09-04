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

#include "nitf/ImageWriter.h"
#include "nitf/ImageIO.h"


/*
 *  Private implementation struct
 */
typedef struct _ImageWriterImpl
{
    nitf_Uint32 numBitsPerPixel;
    nitf_Uint32 numImageBands;
    nitf_Uint32 numMultispectralImageBands;
    nitf_Uint32 numRows;
    nitf_Uint32 numCols;
    nitf_ImageSource *imageSource;
    nitf_ImageIO *imageBlocker;

} ImageWriterImpl;



NITFPRIV(void) ImageWriter_destruct(NITF_DATA * data)
{
    ImageWriterImpl *impl = (ImageWriterImpl *) data;

    if (impl)
    {
        if (impl->imageBlocker)
            nitf_ImageIO_destruct(&impl->imageBlocker);
        if (impl->imageSource)
            nitf_ImageSource_destruct(&impl->imageSource);
        NITF_FREE(impl);
    }
}


NITFPRIV(NITF_BOOL) ImageWriter_write(NITF_DATA * data,
        nitf_IOHandle io, nitf_Error * error)
{
    nitf_Uint8 **user = NULL;
    int row, band;
    size_t rowSize;
    nitf_Uint32 numImageBands = 0;
    off_t offset;
    nitf_BandSource *bandSrc = NULL;
    ImageWriterImpl *impl = (ImageWriterImpl *) data;

    numImageBands = impl->numImageBands + impl->numMultispectralImageBands;
    rowSize = impl->numCols * NITF_NBPP_TO_BYTES(impl->numBitsPerPixel);

    user = (nitf_Uint8 **) NITF_MALLOC(sizeof(nitf_Uint8*) * numImageBands);
    assert(user);
    for (band = 0; band < numImageBands; band++)
    {
        user[band] = (nitf_Uint8 *) NITF_MALLOC(rowSize);
        assert(user[band]);
    }

    offset = nitf_IOHandle_tell(io, error);
    if (!NITF_IO_SUCCESS(offset))
        goto CATCH_ERROR;

    if (!nitf_ImageIO_setFileOffset(impl->imageBlocker, offset, error))
        goto CATCH_ERROR;

    if (!nitf_ImageIO_writeSequential(impl->imageBlocker, io, error))
        goto CATCH_ERROR;

    for (row = 0; row < impl->numRows; ++row)
    {
        for (band = 0; band < numImageBands; ++band)
        {
            bandSrc = nitf_ImageSource_getBand(impl->imageSource,
                                               band, error);
            if (bandSrc == NULL)
                return NITF_FAILURE;

            if (!(*(bandSrc->iface->read)) (bandSrc->data, (char *) user[band],
                                            (size_t) rowSize, error))
            {
                goto CATCH_ERROR;
            }
        }

        if (!nitf_ImageIO_writeRows(impl->imageBlocker, io, 1, user, error))
            goto CATCH_ERROR;
    }

    if (!nitf_ImageIO_writeDone(impl->imageBlocker, io, error))
        goto CATCH_ERROR;

    for (band = 0; band < numImageBands; band++)
    {
        NITF_FREE(user[band]);
    }

    NITF_FREE(user);

    return NITF_SUCCESS;

CATCH_ERROR:
    return NITF_FAILURE;
}



NITFAPI(nitf_ImageWriter *) nitf_ImageWriter_construct(
        nitf_ImageSubheader *subheader, nitf_Error * error)
{
    static nitf_IWriteHandler iWriteHandler =
    {
        &ImageWriter_write,
        &ImageWriter_destruct
    };

    ImageWriterImpl *impl = NULL;
    nitf_ImageWriter *imageWriter = NULL;

    impl = (ImageWriterImpl *) NITF_MALLOC(sizeof(ImageWriterImpl));
    if (!impl)
    {
        nitf_Error_init(error, NITF_STRERROR(NITF_ERRNO), NITF_CTXT,
                        NITF_ERR_MEMORY);
        goto CATCH_ERROR;
    }

    NITF_TRY_GET_UINT32(subheader->numBitsPerPixel, &impl->numBitsPerPixel, error);
    NITF_TRY_GET_UINT32(subheader->numImageBands, &impl->numImageBands, error);
    NITF_TRY_GET_UINT32(subheader->numMultispectralImageBands, &impl->numMultispectralImageBands, error);
    NITF_TRY_GET_UINT32(subheader->numRows, &impl->numRows, error);
    NITF_TRY_GET_UINT32(subheader->numCols, &impl->numCols, error);

    impl->imageSource = NULL;

    /* TODO: currently the compIface is always NULL.. */
    impl->imageBlocker = nitf_ImageIO_construct(subheader, 0, 0, NULL, NULL, error);
    if (!impl->imageBlocker)
        goto CATCH_ERROR;

    imageWriter = (nitf_ImageWriter *) NITF_MALLOC(sizeof(nitf_ImageWriter));
    if (!imageWriter)
    {
        nitf_Error_init(error, NITF_STRERROR(NITF_ERRNO), NITF_CTXT,
                        NITF_ERR_MEMORY);
        goto CATCH_ERROR;
    }
    imageWriter->data = impl;
    imageWriter->iface = &iWriteHandler;
    return imageWriter;

  CATCH_ERROR:
    if (impl && impl->imageBlocker)
        nitf_ImageIO_destruct(&impl->imageBlocker);
    if (impl)
        NITF_FREE(impl);
    return NULL;
}


NITFAPI(NITF_BOOL) nitf_ImageWriter_attachSource(nitf_ImageWriter * imageWriter,
        nitf_ImageSource *imageSource, nitf_Error * error)
{
    ImageWriterImpl *impl = (ImageWriterImpl*)imageWriter->data;
    
    if (impl->imageSource != NULL)
    {
        nitf_Error_init(error, "Image source already attached",
                        NITF_CTXT, NITF_ERR_INVALID_PARAMETER);
        return NITF_FAILURE;
    }
    
    impl->imageSource = imageSource;
    return NITF_SUCCESS;
}


NITFPROT(int) nitf_ImageWriter_setWriteCaching(nitf_ImageWriter *imageWriter,
        int enable)
{
    ImageWriterImpl *impl = (ImageWriterImpl*)imageWriter->data;
    return(nitf_ImageIO_setWriteCaching(impl->imageBlocker, enable));
}
