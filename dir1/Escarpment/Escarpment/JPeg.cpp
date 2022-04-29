/***********************************************************************************
JPeg.cpp - Code to merge the public-domain jpeg code into silicon page so its
easy to use.

begun 12/7/99 by Mike Rozak
Modified again 3/23/2000
Copyright 1999 Mike Rozak. All rights reserved
*/

#include "mymalloc.h"
#include <windows.h>
#define  XMD_H
#define  CANCELFARPOINTERS
#define JPEG_CJPEG_DJPEG	/* define proper options in jconfig.h */
#define JPEG_INTERNAL_OPTIONS	/* cjpeg.c,djpeg.c need to see xxx_SUPPORTED */
extern "C"
{
   #include "z:\jpeg\jpeg-6b\jconfig.h"		/* for version message */
   #include "z:\jpeg\jpeg-6b\jinclude.h"		/* for version message */
   #include "z:\jpeg\jpeg-6b\jpeglib.h"		/* for version message */
   #include "z:\jpeg\jpeg-6b\jerror.h"		/* for version message */
   #include "z:\jpeg\jpeg-6b\cderror.h"		/* for version message */
   #include "z:\jpeg\jpeg-6b\jversion.h"		/* for version message */
}
#include <ctype.h>		/* to declare isprint() */
#include "jpeg.h"

#include "escarpment.h"
#include "resleak.h"


typedef int			INT32;

CListVariable     glistJPEG;     // list of JPEG resource to file
CListVariable     glistBMP;      // list of bitmap resource to file


/*
 * cdjpeg.h
 *
 * Copyright (C) 1994-1997, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains common declarations for the sample applications
 * cjpeg and djpeg.  It is NOT used by the core JPEG library.
 */


/*
 * Object interface for cjpeg's source file decoding modules
 */

typedef struct cjpeg_source_struct * cjpeg_source_ptr;

struct cjpeg_source_struct {
  JMETHOD(void, start_input, (j_compress_ptr cinfo,
			      cjpeg_source_ptr sinfo));
  JMETHOD(JDIMENSION, get_pixel_rows, (j_compress_ptr cinfo,
				       cjpeg_source_ptr sinfo));
  JMETHOD(void, finish_input, (j_compress_ptr cinfo,
			       cjpeg_source_ptr sinfo));

  FILE *input_file;

  JSAMPARRAY buffer;
  JDIMENSION buffer_height;
};


/*
 * Object interface for djpeg's output file encoding modules
 */

typedef struct djpeg_dest_struct * djpeg_dest_ptr;

struct djpeg_dest_struct {
  /* start_output is called after jpeg_start_decompress finishes.
   * The color map will be ready at this time, if one is needed.
   */
  JMETHOD(void, start_output, (j_decompress_ptr cinfo,
			       djpeg_dest_ptr dinfo));
  /* Emit the specified number of pixel rows from the buffer. */
  JMETHOD(void, put_pixel_rows, (j_decompress_ptr cinfo,
				 djpeg_dest_ptr dinfo,
				 JDIMENSION rows_supplied));
  /* Finish up at the end of the image. */
  JMETHOD(void, finish_output, (j_decompress_ptr cinfo,
				djpeg_dest_ptr dinfo));

  /* Target file spec; filled in by djpeg.c after object is created. */
  FILE * output_file;

  /* Output pixel-row buffer.  Created by module init or start_output.
   * Width is cinfo->output_width * cinfo->output_components;
   * height is buffer_height.
   */
  JSAMPARRAY buffer;
  JDIMENSION buffer_height;
};


/*
 * cjpeg/djpeg may need to perform extra passes to convert to or from
 * the source/destination file format.  The JPEG library does not know
 * about these passes, but we'd like them to be counted by the progress
 * monitor.  We use an expanded progress monitor object to hold the
 * additional pass count.
 */

struct cdjpeg_progress_mgr {
  struct jpeg_progress_mgr pub;	/* fields known to JPEG library */
  int completed_extra_passes;	/* extra passes completed */
  int total_extra_passes;	/* total extra */
  /* last printed percentage stored here to avoid multiple printouts */
  int percent_done;
};

typedef struct cdjpeg_progress_mgr * cd_progress_ptr;


/* Short forms of external names for systems with brain-damaged linkers. */

#ifdef NEED_SHORT_EXTERNAL_NAMES
#define jinit_read_bmp		jIRdBMP
#define jinit_write_bmp		jIWrBMP
#define jinit_read_gif		jIRdGIF
#define jinit_write_gif		jIWrGIF
#define jinit_read_ppm		jIRdPPM
#define jinit_write_ppm		jIWrPPM
#define jinit_read_rle		jIRdRLE
#define jinit_write_rle		jIWrRLE
#define jinit_read_targa	jIRdTarga
#define jinit_write_targa	jIWrTarga
#define read_quant_tables	RdQTables
#define read_scan_script	RdScnScript
#define set_quant_slots		SetQSlots
#define set_sample_factors	SetSFacts
#define read_color_map		RdCMap
#define enable_signal_catcher	EnSigCatcher
#define start_progress_monitor	StProgMon
#define end_progress_monitor	EnProgMon
#define read_stdin		RdStdin
#define write_stdout		WrStdout
#endif /* NEED_SHORT_EXTERNAL_NAMES */

/* Module selection routines for I/O modules. */

EXTERN(cjpeg_source_ptr) jinit_read_bmp JPP((j_compress_ptr cinfo));
EXTERN(djpeg_dest_ptr) jinit_write_bmp JPP((j_decompress_ptr cinfo,
					    boolean is_os2));
EXTERN(cjpeg_source_ptr) jinit_read_gif JPP((j_compress_ptr cinfo));
EXTERN(djpeg_dest_ptr) jinit_write_gif JPP((j_decompress_ptr cinfo));
EXTERN(cjpeg_source_ptr) jinit_read_ppm JPP((j_compress_ptr cinfo));
EXTERN(djpeg_dest_ptr) jinit_write_ppm JPP((j_decompress_ptr cinfo));
EXTERN(cjpeg_source_ptr) jinit_read_rle JPP((j_compress_ptr cinfo));
EXTERN(djpeg_dest_ptr) jinit_write_rle JPP((j_decompress_ptr cinfo));
EXTERN(cjpeg_source_ptr) jinit_read_targa JPP((j_compress_ptr cinfo));
EXTERN(djpeg_dest_ptr) jinit_write_targa JPP((j_decompress_ptr cinfo));

/* cjpeg support routines (in rdswitch.c) */

EXTERN(boolean) read_quant_tables JPP((j_compress_ptr cinfo, char * filename,
				    int scale_factor, boolean force_baseline));
EXTERN(boolean) read_scan_script JPP((j_compress_ptr cinfo, char * filename));
EXTERN(boolean) set_quant_slots JPP((j_compress_ptr cinfo, char *arg));
EXTERN(boolean) set_sample_factors JPP((j_compress_ptr cinfo, char *arg));

/* djpeg support routines (in rdcolmap.c) */

EXTERN(void) read_color_map JPP((j_decompress_ptr cinfo, FILE * infile));

/* common support routines (in cdjpeg.c) */

EXTERN(void) enable_signal_catcher JPP((j_common_ptr cinfo));
EXTERN(void) start_progress_monitor JPP((j_common_ptr cinfo,
					 cd_progress_ptr progress));
EXTERN(void) end_progress_monitor JPP((j_common_ptr cinfo));
EXTERN(boolean) keymatch JPP((char * arg, const char * keyword, int minchars));
EXTERN(FILE *) read_stdin JPP((void));
EXTERN(FILE *) write_stdout JPP((void));

/* miscellaneous useful macros */

#ifdef DONT_USE_B_MODE		/* define mode parameters for fopen() */
#define READ_BINARY	"r"
#define WRITE_BINARY	"w"
#else
#ifdef VMS			/* VMS is very nonstandard */
#define READ_BINARY	"rb", "ctx=stm"
#define WRITE_BINARY	"wb", "ctx=stm"
#else				/* standard ANSI-compliant case */
#define READ_BINARY	"rb"
#define WRITE_BINARY	"wb"
#endif
#endif

#ifndef EXIT_FAILURE		/* define exit() codes if not provided */
#define EXIT_FAILURE  1
#endif
#ifndef EXIT_SUCCESS
#ifdef VMS
#define EXIT_SUCCESS  1		/* VMS is very nonstandard */
#else
#define EXIT_SUCCESS  0
#endif
#endif
#ifndef EXIT_WARNING
#ifdef VMS
#define EXIT_WARNING  1		/* VMS is very nonstandard */
#else
#define EXIT_WARNING  2
#endif
#endif

/* Create the add-on message string table. */

#define JMESSAGE(code,string)	string ,

static const char * const cdjpeg_message_table[] = {
#include "z:\jpeg\jpeg-6b\cderror.h"
  NULL
};

/*
 * wrbmp.c
 *
 * Copyright (C) 1994-1996, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains routines to write output images in Microsoft "BMP"
 * format (MS Windows 3.x and OS/2 1.x flavors).
 * Either 8-bit colormapped or 24-bit full-color format can be written.
 * No compression is supported.
 *
 * These routines may need modification for non-Unix environments or
 * specialized applications.  As they stand, they assume output to
 * an ordinary stdio stream.
 *
 * This code contributed by James Arthur Boucher.
 */



/*
 * To support 12-bit JPEG data, we'd have to scale output down to 8 bits.
 * This is not yet implemented.
 */

#if BITS_IN_JSAMPLE != 8
  Sorry, this code only copes with 8-bit JSAMPLEs. /* deliberate syntax err */
#endif

/*
 * Since BMP stores scanlines bottom-to-top, we have to invert the image
 * from JPEG's top-to-bottom order.  To do this, we save the outgoing data
 * in a virtual array during put_pixel_row calls, then actually emit the
 * BMP file during finish_output.  The virtual array contains one JSAMPLE per
 * pixel if the output is grayscale or colormapped, three if it is full color.
 */

/* Private version of data destination object */

typedef struct {
  struct djpeg_dest_struct pub;	/* public fields */

  boolean is_os2;		/* saves the OS2 format request flag */

  jvirt_sarray_ptr whole_image;	/* needed to reverse row order */
  JDIMENSION data_width;	/* JSAMPLEs per row */
  JDIMENSION row_width;		/* physical width of one row in the BMP file */
  int pad_bytes;		/* number of padding bytes needed per row */
  JDIMENSION cur_output_row;	/* next row# to write to virtual array */
} bmp_dest_struct;

typedef bmp_dest_struct * bmp_dest_ptr;


/* Forward declarations */
LOCAL(void) write_colormap
	JPP((j_decompress_ptr cinfo, bmp_dest_ptr dest,
	     int map_colors, int map_entry_size));


/*
 * Write some pixel data.
 * In this module rows_supplied will always be 1.
 */

METHODDEF(void)
put_pixel_rows (j_decompress_ptr cinfo, djpeg_dest_ptr dinfo,
		JDIMENSION rows_supplied)
/* This version is for writing 24-bit pixels */
{
  bmp_dest_ptr dest = (bmp_dest_ptr) dinfo;
  JSAMPARRAY image_ptr;
  register JSAMPROW inptr, outptr;
  register JDIMENSION col;
  int pad;

  /* Access next row in virtual array */
  image_ptr = (*cinfo->mem->access_virt_sarray)
    ((j_common_ptr) cinfo, dest->whole_image,
     dest->cur_output_row, (JDIMENSION) 1, TRUE);
  dest->cur_output_row++;

  /* Transfer data.  Note destination values must be in BGR order
   * (even though Microsoft's own documents say the opposite).
   */
  inptr = dest->pub.buffer[0];
  outptr = image_ptr[0];
  for (col = cinfo->output_width; col > 0; col--) {
    outptr[2] = *inptr++;	/* can omit GETJSAMPLE() safely */
    outptr[1] = *inptr++;
    outptr[0] = *inptr++;
    outptr += 3;
  }

  /* Zero out the pad bytes. */
  pad = dest->pad_bytes;
  while (--pad >= 0)
    *outptr++ = 0;
}

METHODDEF(void)
put_gray_rows (j_decompress_ptr cinfo, djpeg_dest_ptr dinfo,
	       JDIMENSION rows_supplied)
/* This version is for grayscale OR quantized color output */
{
  bmp_dest_ptr dest = (bmp_dest_ptr) dinfo;
  JSAMPARRAY image_ptr;
  register JSAMPROW inptr, outptr;
  register JDIMENSION col;
  int pad;

  /* Access next row in virtual array */
  image_ptr = (*cinfo->mem->access_virt_sarray)
    ((j_common_ptr) cinfo, dest->whole_image,
     dest->cur_output_row, (JDIMENSION) 1, TRUE);
  dest->cur_output_row++;

  /* Transfer data. */
  inptr = dest->pub.buffer[0];
  outptr = image_ptr[0];
  for (col = cinfo->output_width; col > 0; col--) {
    *outptr++ = *inptr++;	/* can omit GETJSAMPLE() safely */
  }

  /* Zero out the pad bytes. */
  pad = dest->pad_bytes;
  while (--pad >= 0)
    *outptr++ = 0;
}


/*
 * Startup: normally writes the file header.
 * In this module we may as well postpone everything until finish_output.
 */

METHODDEF(void)
start_output_bmp (j_decompress_ptr cinfo, djpeg_dest_ptr dinfo)
{
  /* no work here */
}


/*
 * Finish up at the end of the file.
 *
 * Here is where we really output the BMP file.
 *
 * First, routines to write the Windows and OS/2 variants of the file header.
 */

LOCAL(void)
write_bmp_header (j_decompress_ptr cinfo, bmp_dest_ptr dest)
/* Write a Windows-style BMP file header, including colormap if needed */
{
  char bmpfileheader[14];
  char bmpinfoheader[40];
#define PUT_2B(array,offset,value)  \
	(array[offset] = (char) ((value) & 0xFF), \
	 array[offset+1] = (char) (((value) >> 8) & 0xFF))
#define PUT_4B(array,offset,value)  \
	(array[offset] = (char) ((value) & 0xFF), \
	 array[offset+1] = (char) (((value) >> 8) & 0xFF), \
	 array[offset+2] = (char) (((value) >> 16) & 0xFF), \
	 array[offset+3] = (char) (((value) >> 24) & 0xFF))
  INT32 headersize, bfSize;
  int bits_per_pixel, cmap_entries;

  /* Compute colormap size and total file size */
  if (cinfo->out_color_space == JCS_RGB) {
    if (cinfo->quantize_colors) {
      /* Colormapped RGB */
      bits_per_pixel = 8;
      cmap_entries = 256;
    } else {
      /* Unquantized, full color RGB */
      bits_per_pixel = 24;
      cmap_entries = 0;
    }
  } else {
    /* Grayscale output.  We need to fake a 256-entry colormap. */
    bits_per_pixel = 8;
    cmap_entries = 256;
  }
  /* File size */
  headersize = 14 + 40 + cmap_entries * 4; /* Header and colormap */
  bfSize = headersize + (INT32) dest->row_width * (INT32) cinfo->output_height;
  
  /* Set unused fields of header to 0 */
  MEMZERO(bmpfileheader, SIZEOF(bmpfileheader));
  MEMZERO(bmpinfoheader, SIZEOF(bmpinfoheader));

  /* Fill the file header */
  bmpfileheader[0] = 0x42;	/* first 2 bytes are ASCII 'B', 'M' */
  bmpfileheader[1] = 0x4D;
  PUT_4B(bmpfileheader, 2, bfSize); /* bfSize */
  /* we leave bfReserved1 & bfReserved2 = 0 */
  PUT_4B(bmpfileheader, 10, headersize); /* bfOffBits */

  /* Fill the info header (Microsoft calls this a BITMAPINFOHEADER) */
  PUT_2B(bmpinfoheader, 0, 40);	/* biSize */
  PUT_4B(bmpinfoheader, 4, cinfo->output_width); /* biWidth */
  PUT_4B(bmpinfoheader, 8, cinfo->output_height); /* biHeight */
  PUT_2B(bmpinfoheader, 12, 1);	/* biPlanes - must be 1 */
  PUT_2B(bmpinfoheader, 14, bits_per_pixel); /* biBitCount */
  /* we leave biCompression = 0, for none */
  /* we leave biSizeImage = 0; this is correct for uncompressed data */
  if (cinfo->density_unit == 2) { /* if have density in dots/cm, then */
    PUT_4B(bmpinfoheader, 24, (INT32) (cinfo->X_density*100)); /* XPels/M */
    PUT_4B(bmpinfoheader, 28, (INT32) (cinfo->Y_density*100)); /* XPels/M */
  }
  PUT_2B(bmpinfoheader, 32, cmap_entries); /* biClrUsed */
  /* we leave biClrImportant = 0 */

  if (JFWRITE(dest->pub.output_file, bmpfileheader, 14) != (size_t) 14)
    ERREXIT(cinfo, JERR_FILE_WRITE);
  if (JFWRITE(dest->pub.output_file, bmpinfoheader, 40) != (size_t) 40)
    ERREXIT(cinfo, JERR_FILE_WRITE);

  if (cmap_entries > 0)
    write_colormap(cinfo, dest, cmap_entries, 4);
}


LOCAL(void)
write_os2_header (j_decompress_ptr cinfo, bmp_dest_ptr dest)
/* Write an OS2-style BMP file header, including colormap if needed */
{
  char bmpfileheader[14];
  char bmpcoreheader[12];
  INT32 headersize, bfSize;
  int bits_per_pixel, cmap_entries;

  /* Compute colormap size and total file size */
  if (cinfo->out_color_space == JCS_RGB) {
    if (cinfo->quantize_colors) {
      /* Colormapped RGB */
      bits_per_pixel = 8;
      cmap_entries = 256;
    } else {
      /* Unquantized, full color RGB */
      bits_per_pixel = 24;
      cmap_entries = 0;
    }
  } else {
    /* Grayscale output.  We need to fake a 256-entry colormap. */
    bits_per_pixel = 8;
    cmap_entries = 256;
  }
  /* File size */
  headersize = 14 + 12 + cmap_entries * 3; /* Header and colormap */
  bfSize = headersize + (INT32) dest->row_width * (INT32) cinfo->output_height;
  
  /* Set unused fields of header to 0 */
  MEMZERO(bmpfileheader, SIZEOF(bmpfileheader));
  MEMZERO(bmpcoreheader, SIZEOF(bmpcoreheader));

  /* Fill the file header */
  bmpfileheader[0] = 0x42;	/* first 2 bytes are ASCII 'B', 'M' */
  bmpfileheader[1] = 0x4D;
  PUT_4B(bmpfileheader, 2, bfSize); /* bfSize */
  /* we leave bfReserved1 & bfReserved2 = 0 */
  PUT_4B(bmpfileheader, 10, headersize); /* bfOffBits */

  /* Fill the info header (Microsoft calls this a BITMAPCOREHEADER) */
  PUT_2B(bmpcoreheader, 0, 12);	/* bcSize */
  PUT_2B(bmpcoreheader, 4, cinfo->output_width); /* bcWidth */
  PUT_2B(bmpcoreheader, 6, cinfo->output_height); /* bcHeight */
  PUT_2B(bmpcoreheader, 8, 1);	/* bcPlanes - must be 1 */
  PUT_2B(bmpcoreheader, 10, bits_per_pixel); /* bcBitCount */

  if (JFWRITE(dest->pub.output_file, bmpfileheader, 14) != (size_t) 14)
    ERREXIT(cinfo, JERR_FILE_WRITE);
  if (JFWRITE(dest->pub.output_file, bmpcoreheader, 12) != (size_t) 12)
    ERREXIT(cinfo, JERR_FILE_WRITE);

  if (cmap_entries > 0)
    write_colormap(cinfo, dest, cmap_entries, 3);
}


/*
 * Write the colormap.
 * Windows uses BGR0 map entries; OS/2 uses BGR entries.
 */

LOCAL(void)
write_colormap (j_decompress_ptr cinfo, bmp_dest_ptr dest,
		int map_colors, int map_entry_size)
{
  JSAMPARRAY colormap = cinfo->colormap;
  int num_colors = cinfo->actual_number_of_colors;
  FILE * outfile = dest->pub.output_file;
  int i;

  if (colormap != NULL) {
    if (cinfo->out_color_components == 3) {
      /* Normal case with RGB colormap */
      for (i = 0; i < num_colors; i++) {
	putc(GETJSAMPLE(colormap[2][i]), outfile);
	putc(GETJSAMPLE(colormap[1][i]), outfile);
	putc(GETJSAMPLE(colormap[0][i]), outfile);
	if (map_entry_size == 4)
	  putc(0, outfile);
      }
    } else {
      /* Grayscale colormap (only happens with grayscale quantization) */
      for (i = 0; i < num_colors; i++) {
	putc(GETJSAMPLE(colormap[0][i]), outfile);
	putc(GETJSAMPLE(colormap[0][i]), outfile);
	putc(GETJSAMPLE(colormap[0][i]), outfile);
	if (map_entry_size == 4)
	  putc(0, outfile);
      }
    }
  } else {
    /* If no colormap, must be grayscale data.  Generate a linear "map". */
    for (i = 0; i < 256; i++) {
      putc(i, outfile);
      putc(i, outfile);
      putc(i, outfile);
      if (map_entry_size == 4)
	putc(0, outfile);
    }
  }
  /* Pad colormap with zeros to ensure specified number of colormap entries */ 
  if (i > map_colors)
    ERREXIT1(cinfo, JERR_TOO_MANY_COLORS, i);
  for (; i < map_colors; i++) {
    putc(0, outfile);
    putc(0, outfile);
    putc(0, outfile);
    if (map_entry_size == 4)
      putc(0, outfile);
  }
}


METHODDEF(void)
finish_output_bmp (j_decompress_ptr cinfo, djpeg_dest_ptr dinfo)
{
  bmp_dest_ptr dest = (bmp_dest_ptr) dinfo;
  register FILE * outfile = dest->pub.output_file;
  JSAMPARRAY image_ptr;
  register JSAMPROW data_ptr;
  JDIMENSION row;
  register JDIMENSION col;
  cd_progress_ptr progress = (cd_progress_ptr) cinfo->progress;

  /* Write the header and colormap */
  if (dest->is_os2)
    write_os2_header(cinfo, dest);
  else
    write_bmp_header(cinfo, dest);

  /* Write the file body from our virtual array */
  for (row = cinfo->output_height; row > 0; row--) {
    if (progress != NULL) {
      progress->pub.pass_counter = (long) (cinfo->output_height - row);
      progress->pub.pass_limit = (long) cinfo->output_height;
      (*progress->pub.progress_monitor) ((j_common_ptr) cinfo);
    }
    image_ptr = (*cinfo->mem->access_virt_sarray)
      ((j_common_ptr) cinfo, dest->whole_image, row-1, (JDIMENSION) 1, FALSE);
    data_ptr = image_ptr[0];
    for (col = dest->row_width; col > 0; col--) {
      putc(GETJSAMPLE(*data_ptr), outfile);
      data_ptr++;
    }
  }
  if (progress != NULL)
    progress->completed_extra_passes++;

  /* Make sure we wrote the output file OK */
  fflush(outfile);
  if (ferror(outfile))
    ERREXIT(cinfo, JERR_FILE_WRITE);
}


/*
 * The module selection routine for BMP format output.
 */

GLOBAL(djpeg_dest_ptr)
jinit_write_bmp (j_decompress_ptr cinfo, boolean is_os2)
{
  bmp_dest_ptr dest;
  JDIMENSION row_width;

  /* Create module interface object, fill in method pointers */
  dest = (bmp_dest_ptr)
      (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
				  SIZEOF(bmp_dest_struct));
  dest->pub.start_output = start_output_bmp;
  dest->pub.finish_output = finish_output_bmp;
  dest->is_os2 = is_os2;

  if (cinfo->out_color_space == JCS_GRAYSCALE) {
    dest->pub.put_pixel_rows = put_gray_rows;
  } else if (cinfo->out_color_space == JCS_RGB) {
    if (cinfo->quantize_colors)
      dest->pub.put_pixel_rows = put_gray_rows;
    else
      dest->pub.put_pixel_rows = put_pixel_rows;
  } else {
    ERREXIT(cinfo, JERR_BMP_COLORSPACE);
  }

  /* Calculate output image dimensions so we can allocate space */
  jpeg_calc_output_dimensions(cinfo);

  /* Determine width of rows in the BMP file (padded to 4-byte boundary). */
  row_width = cinfo->output_width * cinfo->output_components;
  dest->data_width = row_width;
  while ((row_width & 3) != 0) row_width++;
  dest->row_width = row_width;
  dest->pad_bytes = (int) (row_width - dest->data_width);

  /* Allocate space for inversion array, prepare for write pass */
  dest->whole_image = (*cinfo->mem->request_virt_sarray)
    ((j_common_ptr) cinfo, JPOOL_IMAGE, FALSE,
     row_width, cinfo->output_height, (JDIMENSION) 1);
  dest->cur_output_row = 0;
  if (cinfo->progress != NULL) {
    cd_progress_ptr progress = (cd_progress_ptr) cinfo->progress;
    progress->total_extra_passes++; /* count file input as separate pass */
  }

  /* Create decompressor output buffer. */
  dest->pub.buffer = (*cinfo->mem->alloc_sarray)
    ((j_common_ptr) cinfo, JPOOL_IMAGE, row_width, (JDIMENSION) 1);
  dest->pub.buffer_height = 1;

  return (djpeg_dest_ptr) dest;
}

/************************************************************************************
LoadBitmapFile - Loads in a bitmap file
*/
HBITMAP LoadBitmapFile (char *psz)
{
   // try opening the file
   PMEGAFILE f;
   PBYTE pMem = NULL;
   HDC   hDC = NULL;
   HBITMAP hBit = NULL;


   f = MegaFileOpen (psz);
   if (!f) {
      goto done;
   }

   // how big is it?
   MegaFileSeek (f, 0, SEEK_END);
   DWORD dwSize;
   dwSize = (DWORD) MegaFileTell (f);
   MegaFileSeek (f, 0, 0);

   // read it in
   pMem = (PBYTE) ESCMALLOC (dwSize);
   if (!pMem) {
      goto done;
   }

   MegaFileRead (pMem, 1, dwSize, f);

   // figure out where everything is
   PBYTE pMax;
   pMax = pMem + dwSize;

   BITMAPFILEHEADER  *pHeader;
   pHeader = (BITMAPFILEHEADER*) pMem;
   if ( ((PBYTE) (pHeader+1) > pMax) || pHeader->bfType != 0x4d42) {
      goto done;
   }

   // other info
   BITMAPINFOHEADER *pbi;
   pbi = (BITMAPINFOHEADER*) (pHeader+1);
   if ((PBYTE)(pbi+1) > pMax) {
      goto done;
   }

   if (pbi->biSize < sizeof (BITMAPINFOHEADER)) {
      goto done;
   }

   // data bits
   PBYTE pImage;
   pImage = pMem + pHeader->bfOffBits;


   // create a bitmap out of this
   hDC = GetDC (NULL);
   if (!hDC) {
      goto done;
   }

   hBit = CreateDIBitmap (hDC, pbi, CBM_INIT, pImage, (BITMAPINFO*) pbi, DIB_RGB_COLORS);
   if (!hBit) {
      goto done;
   }


done:

   if (hDC)
      ReleaseDC (NULL, hDC);
   if (f)
      MegaFileClose (f);

   if (pMem)
      ESCFREE (pMem);

   return hBit;
}


/************************************************************************************
LoadBitmapFileNoMegaFile - Loads in a bitmap file
*/
HBITMAP LoadBitmapFileNoMegaFile (char *psz)
{
   // try opening the file
   FILE * f;
   PBYTE pMem = NULL;
   HDC   hDC = NULL;
   HBITMAP hBit = NULL;


   f = fopen (psz, "rb");
   if (!f) {
      goto done;
   }

   // how big is it?
   fseek (f, 0, SEEK_END);
   DWORD dwSize;
   dwSize = (DWORD) ftell (f);
   fseek (f, 0, 0);

   // read it in
   pMem = (PBYTE) ESCMALLOC (dwSize);
   if (!pMem) {
      goto done;
   }

   fread (pMem, 1, dwSize, f);

   // figure out where everything is
   PBYTE pMax;
   pMax = pMem + dwSize;

   BITMAPFILEHEADER  *pHeader;
   pHeader = (BITMAPFILEHEADER*) pMem;
   if ( ((PBYTE) (pHeader+1) > pMax) || pHeader->bfType != 0x4d42) {
      goto done;
   }

   // other info
   BITMAPINFOHEADER *pbi;
   pbi = (BITMAPINFOHEADER*) (pHeader+1);
   if ((PBYTE)(pbi+1) > pMax) {
      goto done;
   }

   if (pbi->biSize < sizeof (BITMAPINFOHEADER)) {
      goto done;
   }

   // data bits
   PBYTE pImage;
   pImage = pMem + pHeader->bfOffBits;


   // create a bitmap out of this
   hDC = GetDC (NULL);
   if (!hDC) {
      goto done;
   }

   hBit = CreateDIBitmap (hDC, pbi, CBM_INIT, pImage, (BITMAPINFO*) pbi, DIB_RGB_COLORS);
   if (!hBit) {
      goto done;
   }


done:

   if (hDC)
      ReleaseDC (NULL, hDC);
   if (f)
      fclose (f);

   if (pMem)
      ESCFREE (pMem);

   return hBit;
}

/************************************************************************************
JPegToBitmapNoMegaFile - Converts from a JPEG file to a bitmap.

inputs
   PVOID    pData - jpeg
   DWORD    dwSize - size in bytes
returns
   HBITMAP  - hbitmap

IMPORTANT - This is using temporary files because the public domain stuff works from
   file and it's easier to do this

*/
DLLEXPORT BOOL JPegToBitmapNoMegaFile (char *szJPeg, char *szBmp)
{
   FILE  *input_file = NULL;
   FILE  *output_file = NULL;
   HBITMAP  hBit = NULL;

  struct jpeg_decompress_struct cinfo;
  struct jpeg_error_mgr jerr;
  djpeg_dest_ptr dest_mgr;
  dest_mgr = NULL;
  JDIMENSION num_scanlines;

  /* On Mac, fetch a command line. */

  /* Initialize the JPEG decompression object with default error handling. */
  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_decompress(&cinfo);
  /* Add some application-specific error messages (from cderror.h) */
  jerr.addon_message_table = cdjpeg_message_table;
  jerr.first_addon_message = JMSG_FIRSTADDONCODE;
  jerr.last_addon_message = JMSG_LASTADDONCODE;

  /* Insert custom marker processor for COM and APP12.
   * APP12 is used by some digital camera makers for textual info,
   * so we provide the ability to display it as text.
   * If you like, additional APPn marker types can be selected for display,
   * but don't try to override APP0 or APP14 this way (see libjpeg.doc).
   */
//  jpeg_set_marker_processor(&cinfo, JPEG_COM, print_text_marker);
  // jpeg_set_marker_processor(&cinfo, JPEG_APP0+12, print_text_marker);

  /* Now safe to enable signal catcher. */

  OUTPUTDEBUGFILE (szJPeg);
  input_file = fopen(szJPeg, READ_BINARY);
  if (!input_file)
     goto done;

  // BUGFIX - if starts out with bimap header then just copy it over because
  // sometimes windows saves jpeg images as bitmaps
  WORD wHdr;
  fread (&wHdr, sizeof (wHdr), 1, input_file);
  fseek (input_file, 0, 0);   // go back
  if (wHdr == MAKEWORD ('B', 'M')) {
     fclose (input_file);
     CopyFile (szJPeg, szBmp, FALSE);
     return TRUE;
  }

  OUTPUTDEBUGFILE (szBmp);
  output_file = fopen(szBmp, WRITE_BINARY);
  if (!output_file)
     goto done;

  /* Specify data source for decompression */
  jpeg_stdio_src(&cinfo, input_file);

  /* Read file header, set default decompression parameters */
  (void) jpeg_read_header(&cinfo, TRUE);

  /* Initialize the output module now to let it override any crucial
   * option settings (for instance, GIF wants to force color quantization).
   */
    dest_mgr = jinit_write_bmp(&cinfo, FALSE);
  dest_mgr->output_file = output_file;

  /* Start decompressor */
  (void) jpeg_start_decompress(&cinfo);

  /* Write output file header */
  (*dest_mgr->start_output) (&cinfo, dest_mgr);

  /* Process data */
  while (cinfo.output_scanline < cinfo.output_height) {
    num_scanlines = jpeg_read_scanlines(&cinfo, dest_mgr->buffer,
					dest_mgr->buffer_height);
    (*dest_mgr->put_pixel_rows) (&cinfo, dest_mgr, num_scanlines);
  }

#ifdef PROGRESS_REPORT
  /* Hack: count final pass as done in case finish_output does an extra pass.
   * The library won't have updated completed_passes.
   */
  progress.pub.completed_passes = progress.pub.total_passes;
#endif

  /* Finish decompression and release memory.
   * I must do it in this order because output module has allocated memory
   * of lifespan JPOOL_IMAGE; it needs to finish before releasing memory.
   */
  (*dest_mgr->finish_output) (&cinfo, dest_mgr);
  (void) jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);

  /* Close files, if we opened them */
  fclose(input_file);
  input_file = NULL;
  fclose(output_file);
  output_file = NULL;


done:
   if (input_file)
      fclose (input_file);
   if (output_file)
      fclose (output_file);
   return TRUE;
}




DLLEXPORT HBITMAP JPegToBitmapNoMegaFile (char *szJPeg)
{
   char  szBmp[256], szPath[256];;
   HBITMAP  hBit = NULL;
   szBmp[0] = 0;

   // make temp name
   GetTempPath (sizeof(szPath), szPath);
   GetTempFileName (szPath, "bmp", 0, szBmp);

   JPegToBitmapNoMegaFile (szJPeg, szBmp);

  // load it in
   hBit = LoadBitmapFileNoMegaFile (szBmp);

   if (szBmp[0])
      DeleteFile (szBmp);
   return hBit;
}


DLLEXPORT HBITMAP JPegToBitmap (PVOID pData, DWORD dwSize)
{
   char  szJPeg[256];
   HBITMAP  hBit = NULL;
   szJPeg[0] = 0;

   // make temp name
   char  szPath[256];;
   GetTempPath (sizeof(szPath), szPath);
   GetTempFileName (szPath, "bmp", 0, szJPeg);

   // save the jpeg
   FILE  *f;
   OUTPUTDEBUGFILE (szJPeg);
   f = fopen (szJPeg, "wb");
   if (!f)
      return NULL;
   fwrite (pData, 1, dwSize, f);
   fclose (f);

   hBit = JPegToBitmapNoMegaFile (szJPeg);

   if (szJPeg[0])
      DeleteFile (szJPeg);

   return hBit;
}



/****************************************************************************
BitmapToJPeg - Conert an HBITMAP to a JPEG, filling in memory

inputs
   HBITMAP        hBit - Bitmap
   PCMem          pMem - Memory to fill in. m_dwCurPosn will be filled in with the size
*/
BOOL BitmapToJPeg (HBITMAP hBit, PCMem pMem)
{
   char  szBmp[256], szJPeg[256];
   BOOL fRet = FALSE;
   szJPeg[0] = 0;
   szBmp[0] = 0;

   // make temp name
   char  szPath[256];
   GetTempPath (sizeof(szPath), szPath);
   GetTempFileName (szPath, "bmp", 0, szBmp);
   GetTempFileName (szPath, "jpg", 0, szJPeg);

   // save the bitmap
   if (!BitmapSave (hBit, szBmp))
      goto done;

   // convert to jpeg
   if (!BitmapToJPegNoMegaFile (szBmp, szJPeg))
      goto done;

   // load in the jpeg
   FILE  *f;
   f = fopen (szJPeg, "rb");
   if (!f)
      goto done;
   // how big is it?
   fseek (f, 0, SEEK_END);
   DWORD dwSize;
   dwSize = (DWORD) ftell (f);
   fseek (f, 0, 0);
   if (!pMem->Required (dwSize)) {
      fclose (f);
      goto done;
   }
   fread (pMem->p, 1, dwSize, f);
   pMem->m_dwCurPosn = dwSize;
   fclose (f);

   fRet = TRUE;
done:
   if (szJPeg[0])
      DeleteFile (szJPeg);
   if (szBmp[0])
      DeleteFile (szBmp);

   return fRet;
}


/************************************************************************************
JPegToBitampWithMegaFile - Converts JPEG image (file) to a bitmap image (file).
This uses the megafile if there is one.

inputs
   char           *pszJPG - Jpeg image file
   char           *pszBMP - Bitmap image file
   BOOL           fJPGIgnoreMega - Set to TRUE if the JPEG image is not in the megafile
   BOOL           fBMPIgnoreMega - Set to TRUE if the BMP image is not in the megafile
returns
   BOOL - TRUE if success
*/
BOOL JPegToBitmapWithMegaFile (char *pszJPG, char *pszBMP, BOOL fJPGIgnoreMega, BOOL fBMPIgnoreMega)
{
   // if both ignoring megafile then go to other func
   PCMegaFile pmf = MegaFileGet();
   if ((fJPGIgnoreMega && fBMPIgnoreMega) || !pmf)
      return JPegToBitmapNoMegaFile (pszJPG, pszBMP);

   // else, will need to do some temporary files
   char  szPath[256];
   GetTempPath (sizeof(szPath), szPath);

   // else, at the very least, need a JPG in temporary file
   char  szJPeg[256], szBmp[256];
   szJPeg[0] = szBmp[0] = 0;
   if (!fJPGIgnoreMega) {
      GetTempFileName (szPath, "jpg", 0, szJPeg);

      // write this to disk...
      PMEGAFILE f = MegaFileOpen (pszJPG, TRUE);
      if (!f) {
         DeleteFile (szJPeg); // BUGFIX
         return FALSE; // not going to happen
      }

      // write out to temporary file
      OUTPUTDEBUGFILE (szJPeg);
      FILE *fTemp = fopen (szJPeg, "wb");
      if (!fTemp) {
         MegaFileClose (f);
         DeleteFile (szJPeg); // BUGFIX
         return FALSE;
      }
      fwrite (f->pbMem, 1, (size_t) f->iMemSize, fTemp);
      fclose (fTemp);

      MegaFileClose(f);
   }

   // temporary file for bitmap?
   if (!fBMPIgnoreMega)
      GetTempFileName (szPath, "bmp", 0, szBmp);

   // convert...
   BOOL fRet = JPegToBitmapNoMegaFile (szJPeg[0] ? szJPeg : pszJPG,
      szBmp[0] ? szBmp : pszBMP);

   // write the bitmap out to the megafule
   if (fRet && szBmp[0]) {
      WCHAR szw[256];
      MultiByteToWideChar (CP_ACP, 0, szBmp, -1, szw, sizeof(szw)/sizeof(WCHAR));
      if (!pmf->Save (szw)) {
         fRet = FALSE;
         goto done;
      }
   }

done:
   // delete temprorary files
   if (szJPeg[0])
      DeleteFile (szJPeg);
   if (szBmp[0])
      DeleteFile (szBmp);

   return fRet;
}

/************************************************************************************
JPegToBitamp - Converts from a JPEG file to a bitmap.

inputs
   DWORD - resource ID of type 'jpg'
   HINSTANCE   hInstance
returns
   HBITMAP  - hbitmap

*/
DLLEXPORT HBITMAP JPegToBitmap (size_t dwID, HINSTANCE hInstance)
{
   // if this ID matches one of the maps the load it from a file
   DWORD i;
   for (i = 0; i < glistJPEG.Num(); i++) {
      DWORD *pdw = (DWORD*) glistJPEG.Get(i);
      if (*pdw == dwID) {
         char  szTemp[256];
         WideCharToMultiByte (CP_ACP, 0, (WCHAR*) (pdw+1), -1, szTemp, sizeof(szTemp), 0, 0);
         return JPegOrBitmapLoad (szTemp, TRUE);
      }
   }

   HRSRC    hr;

   hr = FindResource (hInstance, MAKEINTRESOURCE (dwID), "jpg");
   if (!hr)
      return NULL;

   HGLOBAL  hg;
   hg = LoadResource (hInstance, hr);

   PVOID pMem;
   pMem = LockResource (hg);

   DWORD dwSize;
   dwSize = SizeofResource (hInstance, hr);

   HBITMAP  hBit;
   hBit = JPegToBitmap (pMem, dwSize);

   return hBit;
}


/************************************************************************************
BMPToBitmap - Reads in a BMP resource and convert it to HBITAMP. Really this is
just LoadBitmap(), except that if the debug overrides are set this may load from a file.

Also, at some point it will use the bitmap cache

inputs
   DWORD - resource ID of bitmap
   HINSTANCE   hInstance
returns
   HBITMAP  - hbitmap

*/
HBITMAP BMPToBitmap (DWORD dwID, HINSTANCE hInstance)
{
   // if this ID matches one of the maps the load it from a file
   DWORD i;
   for (i = 0; i < glistBMP.Num(); i++) {
      DWORD *pdw = (DWORD*) glistBMP.Get(i);
      if (*pdw == dwID) {
         char  szTemp[256];
         WideCharToMultiByte (CP_ACP, 0, (WCHAR*) (pdw+1), -1, szTemp, sizeof(szTemp), 0, 0);
         return JPegOrBitmapLoad (szTemp, TRUE);
      }
   }

   return (HBITMAP) LoadImage (hInstance, MAKEINTRESOURCE (dwID), IMAGE_BITMAP,
      0, 0, LR_DEFAULTSIZE);
}


/************************************************************************************
JPegOrBitmapLoad - Loads a jpeg or bitmap file

inputs
   char           *pszFile - File
   BOOL           fIgnoreMegaFile - If TRUE then don't pass to the megafile
*/
DLLEXPORT HBITMAP JPegOrBitmapLoad (char *szFile, BOOL fIgnoreMegaFile)
{
   // try opening the file
   HRESULT  hRes = NOERROR;
   PMEGAFILE f = NULL;
   PBYTE pMem = NULL;
   HDC   hDC = NULL;
   HBITMAP hBit = NULL;

   char  szBmp[256];
   szBmp[0] = 0;

   // if it's jpeg then special
   int   iLen;
   iLen = (int) strlen(szFile);
   // BUGFIX - Allow it to be .jpg or .jpeg
   if ( ((iLen > 4) && !_strnicmp(szFile + (iLen-4), ".jpg", 4)) ||
      ((iLen > 5) && !_strnicmp(szFile + (iLen-5), ".jpeg", 5)) ){
      // it's a jpeg file
      // make temp name
      char  szPath[256];

      // make temp name
      GetTempPath (sizeof(szPath), szPath);
      GetTempFileName (szPath, "bmp", 0, szBmp);

      // cheap, actually write out bitmap file
      JPegToBitmapWithMegaFile (szFile, szBmp, fIgnoreMegaFile, TRUE);

      fIgnoreMegaFile = TRUE; // set this so when open bitmap will go direct from disk
   }

   f = MegaFileOpen (szBmp[0] ? szBmp : szFile, TRUE, fIgnoreMegaFile ? MFO_IGNOREMEGAFILE : 0);
   if (!f) {
      hRes = E_FAIL;
      goto done;
   }

   // how big is it?
   MegaFileSeek (f, 0, SEEK_END);
   DWORD dwSize;
   dwSize = (DWORD) MegaFileTell (f);
   MegaFileSeek (f, 0, 0);

   // read it in
   pMem = (PBYTE) ESCMALLOC (dwSize);
   if (!pMem) {
      hRes = E_OUTOFMEMORY;
      goto done;
   }

   MegaFileRead (pMem, 1, dwSize, f);

   // figure out where everything is
   PBYTE pMax;
   pMax = pMem + dwSize;

   BITMAPFILEHEADER  *pHeader;
   pHeader = (BITMAPFILEHEADER*) pMem;
   if ( ((PBYTE) (pHeader+1) > pMax) || pHeader->bfType != 0x4d42) {
      hRes = E_FAIL;
      goto done;
   }

   // other info
   BITMAPINFOHEADER *pbi;
   pbi = (BITMAPINFOHEADER*) (pHeader+1);
   if ((PBYTE)(pbi+1) > pMax) {
      hRes = E_FAIL;
      goto done;
   }

   if (pbi->biSize < sizeof (BITMAPINFOHEADER)) {
      hRes = E_FAIL;
      goto done;
   }

   // data bits
   PBYTE pImage;
   pImage = pMem + pHeader->bfOffBits;


   // create a bitmap out of this
   hDC = GetDC (NULL);
   if (!hDC) {
      hRes = E_FAIL;
      goto done;
   }

   hBit = CreateDIBitmap (hDC, pbi, CBM_INIT, pImage, (BITMAPINFO*) pbi, DIB_RGB_COLORS);


done:

   if (hDC)
      ReleaseDC (NULL, hDC);
   if (f)
      MegaFileClose (f);

   if (pMem)
      ESCFREE (pMem);

   if (szBmp[0])
      DeleteFile (szBmp);

   return hBit;
}


/*****************************************************************************
BMPSize - Returns the size of the bitmap (in pixels)

inputs
   HBMP     hBmp - bitmap
   int      *piWidth, int *piHeight - filled in with the size
returns
   BOOL - TRUE if succed
*/
BOOL BMPSize (HBITMAP hBmp, int *piWidth, int *piHeight)
{
   *piWidth = *piHeight = 0;

   // size of the bitmap
   BITMAP   bm;
   if (!GetObject (hBmp, sizeof(bm), &bm))
      return FALSE;

   *piWidth = bm.bmWidth;
   *piHeight = bm.bmHeight;

   return TRUE;
}

/******************************************************************************
TransparentBitmap - Takes a bitmap and makes it transparent. For one, it
creates a mask, and two, it colors in the transparent areas.

input
   HBITMAP  hBit - modified to color
   COLORREF    cMatch - Color to match against. If -1 then use pixel 0,0
   DWORD       dwColorDist - Longest acceptable color distance. recommend 0 for bmp, 10 for jpg

returns
   HBITMAP - returns transparent bitmap
*/
DLLEXPORT HBITMAP TransparentBitmap (HBITMAP hBit, COLORREF cMatch, DWORD dwColorDist)
{
   HDC hDCDesk, hDC, hDCMask;
   HBITMAP  hbmpMask = NULL;

   hDCDesk = GetDC (NULL);
   hDC = CreateCompatibleDC (hDCDesk);
   hDCMask = CreateCompatibleDC (hDCDesk);

   // load bitmap in & figure out size
   SelectObject (hDC, hBit);

   BITMAP   bm;
   GetObject (hBit, sizeof(bm), &bm);

   // create mask and blank it to black
   hbmpMask = CreateCompatibleBitmap (hDC, bm.bmWidth, bm.bmHeight);
   SelectObject (hDCMask, hbmpMask);
   if (!hbmpMask) 
      goto done;
   HBRUSH   hbr;
   RECT  r;
   hbr = CreateSolidBrush (RGB(0x00,0x00,0x00));
   r.left = r.top = 0;
   r.right = bm.bmWidth;
   r.bottom = bm.bmHeight;
   FillRect (hDCMask, &r, hbr);
   DeleteObject (hbr);

   // get first pixel
   COLORREF cr;
   if (cMatch == (DWORD)-1)
      cr = GetPixel (hDC, 0, 0);
   else
      cr = cMatch;

#define  COLORDIST(x,y)    (max(x,y)-min(x,y))

   int   x, y;
   for (y = 0; y < bm.bmHeight; y++) for (x = 0; x < bm.bmWidth; x++) {
      COLORREF cNew;

      cNew = GetPixel (hDC, x, y);

      // color approximately the same
      if ((DWORD) COLORDIST(GetRValue(cNew), GetRValue(cr)) > dwColorDist)
         continue;
      if ((DWORD) COLORDIST(GetGValue(cNew), GetGValue(cr)) > dwColorDist)
         continue;
      if ((DWORD) COLORDIST(GetBValue(cNew), GetBValue(cr)) > dwColorDist)
         continue;

      // set the bitmap pixel to black
      SetPixel (hDC, x, y, 0);

      // set the mask pixel to white
      SetPixel (hDCMask, x, y, RGB(0xff,0xff,0xff));
   }

done:
   DeleteDC (hDC);
   DeleteDC (hDCMask);
   ReleaseDC (NULL, hDCDesk);

   return hbmpMask;
}


/******************************************************************************
JPegTransparentBlt - Transparent bitblt or bitstretch.

inputs
   HBITMAP     hbmpImage - image
   HBITMAP     hbmpMask - mask. same size as image. Can be NULL
   HDC         hdcInto; // draw into
   RECT        *prInto - Where it blt into.
   RECT        *prFrom - Portion of the image to blt from. If size != prIntro's size
                  then does bltstretch
   RECT        *prClip - clipping rectangle. Can be NULL. Doesn't really set clipping region. More cheezy.
returns
   none
*/
void BMPTransparentBlt (HBITMAP hbmpImage, HBITMAP hbmpMask, HDC hDCInto,
                     RECT *prInto, RECT *prFrom, RECT *prClip)
{
   // size difference
   BOOL fDiff = ((prInto->right - prInto->left) != (prFrom->right - prFrom->left)) ||
      ((prInto->bottom - prInto->top) != (prFrom->bottom - prFrom->top));
   int   iOldMode;
   if (fDiff) {
      iOldMode = SetStretchBltMode (hDCInto, COLORONCOLOR);
   }

   HDC   hMem;
   // SetBkColor(hdcDest, RGB(255, 255, 255));    // 1s --> 0xFFFFFF
   // SetTextColor(hdcDest, RGB(0, 0, 0));        // 0s --> 0x000000
   // The above should be defaults

   // clip region
   RECT  rClipInto;
   RECT  rClipFrom;
   if (prClip) {
      if (!IntersectRect (&rClipInto, prInto, prClip))
         return;  // nothing let

      // figure out how far move in & out
      int   iWidthFrom, iHeightFrom, iWidthInto, iHeightInto;
      iWidthFrom = prFrom->right - prFrom->left;
      iHeightFrom = prFrom->bottom - prFrom->top;
      iWidthInto = prInto->right - prInto->left;
      iHeightInto = prInto->bottom - prInto->top;

      // translate the percent clipped into from
      rClipFrom = *prFrom;
      if (rClipInto.top != prInto->top)
         rClipFrom.top = (int) (prFrom->top + (rClipInto.top - prInto->top) /
            (double) iWidthInto * (double) iWidthFrom);
      if (rClipInto.bottom != prInto->bottom)
         rClipFrom.bottom = (int) (prFrom->top + (rClipInto.bottom - prInto->top) /
            (double) iWidthInto * (double) iWidthFrom);
      if (rClipInto.left != prInto->left)
         rClipFrom.left = (int) (prFrom->left + (rClipInto.left - prInto->left) /
            (double) iHeightInto * (double) iHeightFrom);
      if (rClipInto.right != prInto->right)
         rClipFrom.right = (int) (prFrom->left + (rClipInto.right - prInto->left) /
            (double) iHeightInto * (double) iHeightFrom);

      prInto = &rClipInto;
      prFrom = &rClipFrom;
   }

   if (hbmpMask) {
      hMem = CreateCompatibleDC (hDCInto);
      SelectObject (hMem, hbmpMask);
      if (!fDiff)
         EscBitBlt(
            hDCInto,
            prInto->left, prInto->top,
            prInto->right - prInto->left, prInto->bottom - prInto->top,
            hMem,
            prFrom->left, prFrom->top,
            SRCAND, hbmpMask);
      else
         EscStretchBlt(
            hDCInto,
            prInto->left, prInto->top,
            prInto->right - prInto->left, prInto->bottom - prInto->top,
            hMem,
            prFrom->left, prFrom->top,
            prFrom->right - prFrom->left, prFrom->bottom - prFrom->top,
            SRCAND, hbmpMask);
      DeleteDC (hMem);
   }

   // main bitmap
   hMem = CreateCompatibleDC (hDCInto);
   SelectObject (hMem, hbmpImage);
   if (!fDiff)
      EscBitBlt(
         hDCInto,
         prInto->left, prInto->top,
         prInto->right - prInto->left, prInto->bottom - prInto->top,
         hMem,
         prFrom->left, prFrom->top,
         hbmpMask ? SRCPAINT : SRCCOPY, hbmpImage);
   else
      EscStretchBlt(
         hDCInto,
         prInto->left, prInto->top,
         prInto->right - prInto->left, prInto->bottom - prInto->top,
         hMem,
         prFrom->left, prFrom->top,
         prFrom->right - prFrom->left, prFrom->bottom - prFrom->top,
         hbmpMask ? SRCPAINT : SRCCOPY, hbmpImage);
   DeleteDC (hMem);

   // done
   if (fDiff) {
      SetStretchBltMode (hDCInto, iOldMode);
   }
}



/******************************************************************************
EscRemapJPEG - Given a resource ID specified in a MML file, this causes any
access to the resouce to be remapped to a file on disk. Useful for debugging in
the test applicaton.

inputs
   DWORD    dwID - resource ID
   PWSTR    pszFile - file name to remap to. If NULL, then deletes previous occurance
returns
   none
*/
void EscRemapJPEG (DWORD dwID, PWSTR pszFile)
{
   // if it exists already then dlete it
   DWORD i;
   for (i = 0; i < glistJPEG.Num(); i++) {
      DWORD *pdw = (DWORD*) glistJPEG.Get(i);
      if (*pdw == dwID) {
         glistJPEG.Remove(i);
         break;
      }
   }

   // BUGFIX - clear out the cache
   PCBitmapCache pb;
   pb = EscBitmapCache ();
   if (pb)
      pb->CacheCleanUp (TRUE);

   // if null then don't add
   if (!pszFile)
      return;

   // else add
   BYTE     abTemp[512];
   memcpy (abTemp, &dwID, sizeof(dwID));
   wcscpy ((WCHAR*) (abTemp + sizeof(DWORD)), pszFile);
   glistJPEG.Add (abTemp, sizeof(DWORD) + (wcslen(pszFile)+1)*2);
}


/******************************************************************************
EscRemapBMP - Given a resource ID specified in a MML file, this causes any
access to the resouce to be remapped to a file on disk. Useful for debugging in
the test applicaton.

inputs
   DWORD    dwID - resource ID
   PWSTR    pszFile - file name to remap to. If NULL, then deletes previous occurance
returns
   none
*/
void EscRemapBMP (DWORD dwID, PWSTR pszFile)
{
   // if it exists already then dlete it
   DWORD i;
   for (i = 0; i < glistBMP.Num(); i++) {
      DWORD *pdw = (DWORD*) glistBMP.Get(i);
      if (*pdw == dwID) {
         glistBMP.Remove(i);
         break;
      }
   }

   // BUGFIX - clear out the cache
   PCBitmapCache pb;
   pb = EscBitmapCache ();
   if (pb)
      pb->CacheCleanUp (TRUE);

   // if null then don't add
   if (!pszFile)
      return;

   // else add
   BYTE     abTemp[512];
   memcpy (abTemp, &dwID, sizeof(dwID));
   wcscpy ((WCHAR*) (abTemp + sizeof(DWORD)), pszFile);
   glistBMP.Add (abTemp, sizeof(DWORD) + (wcslen(pszFile)+1)*2);
}


/*******************************************************************************
BitmapSave - Saves a bitmap to a file
*/
DLLEXPORT BOOL BitmapSave (HBITMAP hBit, char *pszBmp)
{
   HDIB hDib;
   hDib = BitmapToDIB(hBit);
   if (!hDib)
      return FALSE;

   if (SaveDIB (hDib, pszBmp)) {
      DestroyDIB (hDib);
      return FALSE;
   }
   DestroyDIB (hDib);
   return TRUE;
}

/*
 * rdbmp.c
 *
 * Copyright (C) 1994-1996, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains routines to read input images in Microsoft "BMP"
 * format (MS Windows 3.x, OS/2 1.x, and OS/2 2.x flavors).
 * Currently, only 8-bit and 24-bit images are supported, not 1-bit or
 * 4-bit (feeding such low-depth images into JPEG would be silly anyway).
 * Also, we don't support RLE-compressed files.
 *
 * These routines may need modification for non-Unix environments or
 * specialized applications.  As they stand, they assume input from
 * an ordinary stdio stream.  They further assume that reading begins
 * at the start of the file; start_input may need work if the
 * user interface has already read some data (e.g., to determine that
 * the file is indeed BMP format).
 *
 * This code contributed by James Arthur Boucher.
 */

//#include "cdjpeg.h"		/* Common decls for cjpeg/djpeg applications */
#define BMP_SUPPORTED
#ifdef BMP_SUPPORTED


/* Macros to deal with unsigned chars as efficiently as compiler allows */

#ifdef HAVE_UNSIGNED_CHAR
typedef unsigned char U_CHAR;
#define UCH(x)	((int) (x))
#else /* !HAVE_UNSIGNED_CHAR */
#ifdef CHAR_IS_UNSIGNED
typedef char U_CHAR;
#define UCH(x)	((int) (x))
#else
typedef char U_CHAR;
#define UCH(x)	((int) (x) & 0xFF)
#endif
#endif /* HAVE_UNSIGNED_CHAR */


#define	ReadOK(file,buffer,len)	(JFREAD(file,buffer,len) == ((size_t) (len)))


/* Private version of data source object */

typedef struct _bmp_source_struct * bmp_source_ptr;

typedef struct _bmp_source_struct {
  struct cjpeg_source_struct pub; /* public fields */

  j_compress_ptr cinfo;		/* back link saves passing separate parm */

  JSAMPARRAY colormap;		/* BMP colormap (converted to my format) */

  jvirt_sarray_ptr whole_image;	/* Needed to reverse row order */
  JDIMENSION source_row;	/* Current source row number */
  JDIMENSION row_width;		/* Physical width of scanlines in file */

  int bits_per_pixel;		/* remembers 8- or 24-bit format */
} bmp_source_struct;


LOCAL(int)
read_byte (bmp_source_ptr sinfo)
/* Read next byte from BMP file */
{
  register FILE *infile = sinfo->pub.input_file;
  register int c;

  if ((c = getc(infile)) == EOF)
    ERREXIT(sinfo->cinfo, JERR_INPUT_EOF);
  return c;
}


LOCAL(void)
read_colormap (bmp_source_ptr sinfo, int cmaplen, int mapentrysize)
/* Read the colormap from a BMP file */
{
  int i;

  switch (mapentrysize) {
  case 3:
    /* BGR format (occurs in OS/2 files) */
    for (i = 0; i < cmaplen; i++) {
      sinfo->colormap[2][i] = (JSAMPLE) read_byte(sinfo);
      sinfo->colormap[1][i] = (JSAMPLE) read_byte(sinfo);
      sinfo->colormap[0][i] = (JSAMPLE) read_byte(sinfo);
    }
    break;
  case 4:
    /* BGR0 format (occurs in MS Windows files) */
    for (i = 0; i < cmaplen; i++) {
      sinfo->colormap[2][i] = (JSAMPLE) read_byte(sinfo);
      sinfo->colormap[1][i] = (JSAMPLE) read_byte(sinfo);
      sinfo->colormap[0][i] = (JSAMPLE) read_byte(sinfo);
      (void) read_byte(sinfo);
    }
    break;
  default:
    ERREXIT(sinfo->cinfo, JERR_BMP_BADCMAP);
    break;
  }
}


/*
 * Read one row of pixels.
 * The image has been read into the whole_image array, but is otherwise
 * unprocessed.  We must read it out in top-to-bottom row order, and if
 * it is an 8-bit image, we must expand colormapped pixels to 24bit format.
 */

METHODDEF(JDIMENSION)
get_8bit_row (j_compress_ptr cinfo, cjpeg_source_ptr sinfo)
/* This version is for reading 8-bit colormap indexes */
{
  bmp_source_ptr source = (bmp_source_ptr) sinfo;
  register JSAMPARRAY colormap = source->colormap;
  JSAMPARRAY image_ptr;
  register int t;
  register JSAMPROW inptr, outptr;
  register JDIMENSION col;

  /* Fetch next row from virtual array */
  source->source_row--;
  image_ptr = (*cinfo->mem->access_virt_sarray)
    ((j_common_ptr) cinfo, source->whole_image,
     source->source_row, (JDIMENSION) 1, FALSE);

  /* Expand the colormap indexes to real data */
  inptr = image_ptr[0];
  outptr = source->pub.buffer[0];
  for (col = cinfo->image_width; col > 0; col--) {
    t = GETJSAMPLE(*inptr++);
    *outptr++ = colormap[0][t];	/* can omit GETJSAMPLE() safely */
    *outptr++ = colormap[1][t];
    *outptr++ = colormap[2][t];
  }

  return 1;
}


METHODDEF(JDIMENSION)
get_24bit_row (j_compress_ptr cinfo, cjpeg_source_ptr sinfo)
/* This version is for reading 24-bit pixels */
{
  bmp_source_ptr source = (bmp_source_ptr) sinfo;
  JSAMPARRAY image_ptr;
  register JSAMPROW inptr, outptr;
  register JDIMENSION col;

  /* Fetch next row from virtual array */
  source->source_row--;
  image_ptr = (*cinfo->mem->access_virt_sarray)
    ((j_common_ptr) cinfo, source->whole_image,
     source->source_row, (JDIMENSION) 1, FALSE);

  /* Transfer data.  Note source values are in BGR order
   * (even though Microsoft's own documents say the opposite).
   */
  inptr = image_ptr[0];
  outptr = source->pub.buffer[0];
  for (col = cinfo->image_width; col > 0; col--) {
    outptr[2] = *inptr++;	/* can omit GETJSAMPLE() safely */
    outptr[1] = *inptr++;
    outptr[0] = *inptr++;
    outptr += 3;
  }

  return 1;
}


/*
 * This method loads the image into whole_image during the first call on
 * get_pixel_rows.  The get_pixel_rows pointer is then adjusted to call
 * get_8bit_row or get_24bit_row on subsequent calls.
 */

METHODDEF(JDIMENSION)
preload_image (j_compress_ptr cinfo, cjpeg_source_ptr sinfo)
{
  bmp_source_ptr source = (bmp_source_ptr) sinfo;
  register FILE *infile = source->pub.input_file;
  register int c;
  register JSAMPROW out_ptr;
  JSAMPARRAY image_ptr;
  JDIMENSION row, col;
  cd_progress_ptr progress = (cd_progress_ptr) cinfo->progress;

  /* Read the data into a virtual array in input-file row order. */
  for (row = 0; row < cinfo->image_height; row++) {
    if (progress != NULL) {
      progress->pub.pass_counter = (long) row;
      progress->pub.pass_limit = (long) cinfo->image_height;
      (*progress->pub.progress_monitor) ((j_common_ptr) cinfo);
    }
    image_ptr = (*cinfo->mem->access_virt_sarray)
      ((j_common_ptr) cinfo, source->whole_image,
       row, (JDIMENSION) 1, TRUE);
    out_ptr = image_ptr[0];
    for (col = source->row_width; col > 0; col--) {
      /* inline copy of read_byte() for speed */
      if ((c = getc(infile)) == EOF)
	ERREXIT(cinfo, JERR_INPUT_EOF);
      *out_ptr++ = (JSAMPLE) c;
    }
  }
  if (progress != NULL)
    progress->completed_extra_passes++;

  /* Set up to read from the virtual array in top-to-bottom order */
  switch (source->bits_per_pixel) {
  case 8:
    source->pub.get_pixel_rows = get_8bit_row;
    break;
  case 24:
    source->pub.get_pixel_rows = get_24bit_row;
    break;
  default:
    ERREXIT(cinfo, JERR_BMP_BADDEPTH);
  }
  source->source_row = cinfo->image_height;

  /* And read the first row */
  return (*source->pub.get_pixel_rows) (cinfo, sinfo);
}


/*
 * Read the file header; return image size and component count.
 */

METHODDEF(void)
start_input_bmp (j_compress_ptr cinfo, cjpeg_source_ptr sinfo)
{
  bmp_source_ptr source = (bmp_source_ptr) sinfo;
  U_CHAR bmpfileheader[14];
  U_CHAR bmpinfoheader[64];
#define GET_2B(array,offset)  ((unsigned int) UCH(array[offset]) + \
			       (((unsigned int) UCH(array[offset+1])) << 8))
#define GET_4B(array,offset)  ((INT32) UCH(array[offset]) + \
			       (((INT32) UCH(array[offset+1])) << 8) + \
			       (((INT32) UCH(array[offset+2])) << 16) + \
			       (((INT32) UCH(array[offset+3])) << 24))
  INT32 bfOffBits;
  INT32 headerSize;
  INT32 biWidth = 0;		/* initialize to avoid compiler warning */
  INT32 biHeight = 0;
  unsigned int biPlanes;
  INT32 biCompression;
  INT32 biXPelsPerMeter,biYPelsPerMeter;
  INT32 biClrUsed = 0;
  int mapentrysize = 0;		/* 0 indicates no colormap */
  INT32 bPad;
  JDIMENSION row_width;

  /* Read and verify the bitmap file header */
  if (! ReadOK(source->pub.input_file, bmpfileheader, 14))
    ERREXIT(cinfo, JERR_INPUT_EOF);
  if (GET_2B(bmpfileheader,0) != 0x4D42) /* 'BM' */
    ERREXIT(cinfo, JERR_BMP_NOT);
  bfOffBits = (INT32) GET_4B(bmpfileheader,10);
  /* We ignore the remaining fileheader fields */

  /* The infoheader might be 12 bytes (OS/2 1.x), 40 bytes (Windows),
   * or 64 bytes (OS/2 2.x).  Check the first 4 bytes to find out which.
   */
  if (! ReadOK(source->pub.input_file, bmpinfoheader, 4))
    ERREXIT(cinfo, JERR_INPUT_EOF);
  headerSize = (INT32) GET_4B(bmpinfoheader,0);
  if (headerSize < 12 || headerSize > 64)
    ERREXIT(cinfo, JERR_BMP_BADHEADER);
  if (! ReadOK(source->pub.input_file, bmpinfoheader+4, headerSize-4))
    ERREXIT(cinfo, JERR_INPUT_EOF);

  switch ((int) headerSize) {
  case 12:
    /* Decode OS/2 1.x header (Microsoft calls this a BITMAPCOREHEADER) */
    biWidth = (INT32) GET_2B(bmpinfoheader,4);
    biHeight = (INT32) GET_2B(bmpinfoheader,6);
    biPlanes = GET_2B(bmpinfoheader,8);
    source->bits_per_pixel = (int) GET_2B(bmpinfoheader,10);

    switch (source->bits_per_pixel) {
    case 8:			/* colormapped image */
      mapentrysize = 3;		/* OS/2 uses RGBTRIPLE colormap */
      TRACEMS2(cinfo, 1, JTRC_BMP_OS2_MAPPED, (int) biWidth, (int) biHeight);
      break;
    case 24:			/* RGB image */
      TRACEMS2(cinfo, 1, JTRC_BMP_OS2, (int) biWidth, (int) biHeight);
      break;
    default:
      ERREXIT(cinfo, JERR_BMP_BADDEPTH);
      break;
    }
    if (biPlanes != 1)
      ERREXIT(cinfo, JERR_BMP_BADPLANES);
    break;
  case 40:
  case 64:
    /* Decode Windows 3.x header (Microsoft calls this a BITMAPINFOHEADER) */
    /* or OS/2 2.x header, which has additional fields that we ignore */
    biWidth = GET_4B(bmpinfoheader,4);
    biHeight = GET_4B(bmpinfoheader,8);
    biPlanes = GET_2B(bmpinfoheader,12);
    source->bits_per_pixel = (int) GET_2B(bmpinfoheader,14);
    biCompression = GET_4B(bmpinfoheader,16);
    biXPelsPerMeter = GET_4B(bmpinfoheader,24);
    biYPelsPerMeter = GET_4B(bmpinfoheader,28);
    biClrUsed = GET_4B(bmpinfoheader,32);
    /* biSizeImage, biClrImportant fields are ignored */

    switch (source->bits_per_pixel) {
    case 8:			/* colormapped image */
      mapentrysize = 4;		/* Windows uses RGBQUAD colormap */
      TRACEMS2(cinfo, 1, JTRC_BMP_MAPPED, (int) biWidth, (int) biHeight);
      break;
    case 24:			/* RGB image */
      TRACEMS2(cinfo, 1, JTRC_BMP, (int) biWidth, (int) biHeight);
      break;
    default:
      ERREXIT(cinfo, JERR_BMP_BADDEPTH);
      break;
    }
    if (biPlanes != 1)
      ERREXIT(cinfo, JERR_BMP_BADPLANES);
    if (biCompression != 0)
      ERREXIT(cinfo, JERR_BMP_COMPRESSED);

    if (biXPelsPerMeter > 0 && biYPelsPerMeter > 0) {
      /* Set JFIF density parameters from the BMP data */
      cinfo->X_density = (UINT16) (biXPelsPerMeter/100); /* 100 cm per meter */
      cinfo->Y_density = (UINT16) (biYPelsPerMeter/100);
      cinfo->density_unit = 2;	/* dots/cm */
    }
    break;
  default:
    ERREXIT(cinfo, JERR_BMP_BADHEADER);
    break;
  }

  /* Compute distance to bitmap data --- will adjust for colormap below */
  bPad = bfOffBits - (headerSize + 14);

  /* Read the colormap, if any */
  if (mapentrysize > 0) {
    if (biClrUsed <= 0)
      biClrUsed = 256;		/* assume it's 256 */
    else if (biClrUsed > 256)
      ERREXIT(cinfo, JERR_BMP_BADCMAP);
    /* Allocate space to store the colormap */
    source->colormap = (*cinfo->mem->alloc_sarray)
      ((j_common_ptr) cinfo, JPOOL_IMAGE,
       (JDIMENSION) biClrUsed, (JDIMENSION) 3);
    /* and read it from the file */
    read_colormap(source, (int) biClrUsed, mapentrysize);
    /* account for size of colormap */
    bPad -= biClrUsed * mapentrysize;
  }

  /* Skip any remaining pad bytes */
  if (bPad < 0)			/* incorrect bfOffBits value? */
    ERREXIT(cinfo, JERR_BMP_BADHEADER);
  while (--bPad >= 0) {
    (void) read_byte(source);
  }

  /* Compute row width in file, including padding to 4-byte boundary */
  if (source->bits_per_pixel == 24)
    row_width = (JDIMENSION) (biWidth * 3);
  else
    row_width = (JDIMENSION) biWidth;
  while ((row_width & 3) != 0) row_width++;
  source->row_width = row_width;

  /* Allocate space for inversion array, prepare for preload pass */
  source->whole_image = (*cinfo->mem->request_virt_sarray)
    ((j_common_ptr) cinfo, JPOOL_IMAGE, FALSE,
     row_width, (JDIMENSION) biHeight, (JDIMENSION) 1);
  source->pub.get_pixel_rows = preload_image;
  if (cinfo->progress != NULL) {
    cd_progress_ptr progress = (cd_progress_ptr) cinfo->progress;
    progress->total_extra_passes++; /* count file input as separate pass */
  }

  /* Allocate one-row buffer for returned data */
  source->pub.buffer = (*cinfo->mem->alloc_sarray)
    ((j_common_ptr) cinfo, JPOOL_IMAGE,
     (JDIMENSION) (biWidth * 3), (JDIMENSION) 1);
  source->pub.buffer_height = 1;

  cinfo->in_color_space = JCS_RGB;
  cinfo->input_components = 3;
  cinfo->data_precision = 8;
  cinfo->image_width = (JDIMENSION) biWidth;
  cinfo->image_height = (JDIMENSION) biHeight;
}


/*
 * Finish up at the end of the file.
 */

METHODDEF(void)
finish_input_bmp (j_compress_ptr cinfo, cjpeg_source_ptr sinfo)
{
  /* no work */
}


/*
 * The module selection routine for BMP format input.
 */

GLOBAL(cjpeg_source_ptr)
jinit_read_bmp (j_compress_ptr cinfo)
{
  bmp_source_ptr source;

  /* Create module interface object */
  source = (bmp_source_ptr)
      (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
				  SIZEOF(bmp_source_struct));
  source->cinfo = cinfo;	/* make back link for subroutines */
  /* Fill in method ptrs, except get_pixel_rows which start_input sets */
  source->pub.start_input = start_input_bmp;
  source->pub.finish_input = finish_input_bmp;

  return (cjpeg_source_ptr) source;
}

#endif /* BMP_SUPPORTED */

/*
 * jdatadst.c
 *
 * Copyright (C) 1994-1996, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains compression data destination routines for the case of
 * emitting JPEG data to a file (or any stdio stream).  While these routines
 * are sufficient for most applications, some will want to use a different
 * destination manager.
 * IMPORTANT: we assume that fwrite() will correctly transcribe an array of
 * JOCTETs into 8-bit-wide elements on external storage.  If char is wider
 * than 8 bits on your machine, you may need to do some tweaking.
 */

/* this is not a core library module, so it doesn't define JPEG_INTERNALS */
//#include "jinclude.h"
//#include "jpeglib.h"
//#include "jerror.h"


/* Expanded data destination object for stdio output */

typedef struct {
  struct jpeg_destination_mgr pub; /* public fields */

  FILE * outfile;		/* target stream */
  JOCTET * buffer;		/* start of buffer */
} my_destination_mgr;

typedef my_destination_mgr * my_dest_ptr;

#define OUTPUT_BUF_SIZE  4096	/* choose an efficiently fwrite'able size */


/*
 * Initialize destination --- called by jpeg_start_compress
 * before any data is actually written.
 */

METHODDEF(void)
init_destination (j_compress_ptr cinfo)
{
  my_dest_ptr dest = (my_dest_ptr) cinfo->dest;

  /* Allocate the output buffer --- it will be released when done with image */
  dest->buffer = (JOCTET *)
      (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
				  OUTPUT_BUF_SIZE * SIZEOF(JOCTET));

  dest->pub.next_output_byte = dest->buffer;
  dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;
}


/*
 * Empty the output buffer --- called whenever buffer fills up.
 *
 * In typical applications, this should write the entire output buffer
 * (ignoring the current state of next_output_byte & free_in_buffer),
 * reset the pointer & count to the start of the buffer, and return TRUE
 * indicating that the buffer has been dumped.
 *
 * In applications that need to be able to suspend compression due to output
 * overrun, a FALSE return indicates that the buffer cannot be emptied now.
 * In this situation, the compressor will return to its caller (possibly with
 * an indication that it has not accepted all the supplied scanlines).  The
 * application should resume compression after it has made more room in the
 * output buffer.  Note that there are substantial restrictions on the use of
 * suspension --- see the documentation.
 *
 * When suspending, the compressor will back up to a convenient restart point
 * (typically the start of the current MCU). next_output_byte & free_in_buffer
 * indicate where the restart point will be if the current call returns FALSE.
 * Data beyond this point will be regenerated after resumption, so do not
 * write it out when emptying the buffer externally.
 */

METHODDEF(boolean)
empty_output_buffer (j_compress_ptr cinfo)
{
  my_dest_ptr dest = (my_dest_ptr) cinfo->dest;

  if (JFWRITE(dest->outfile, dest->buffer, OUTPUT_BUF_SIZE) !=
      (size_t) OUTPUT_BUF_SIZE)
    ERREXIT(cinfo, JERR_FILE_WRITE);

  dest->pub.next_output_byte = dest->buffer;
  dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;

  return TRUE;
}


/*
 * Terminate destination --- called by jpeg_finish_compress
 * after all data has been written.  Usually needs to flush buffer.
 *
 * NB: *not* called by jpeg_abort or jpeg_destroy; surrounding
 * application must deal with any cleanup that should happen even
 * for error exit.
 */

METHODDEF(void)
term_destination (j_compress_ptr cinfo)
{
  my_dest_ptr dest = (my_dest_ptr) cinfo->dest;
  size_t datacount = OUTPUT_BUF_SIZE - dest->pub.free_in_buffer;

  /* Write any data remaining in the buffer */
  if (datacount > 0) {
    if (JFWRITE(dest->outfile, dest->buffer, datacount) != datacount)
      ERREXIT(cinfo, JERR_FILE_WRITE);
  }
  fflush(dest->outfile);
  /* Make sure we wrote the output file OK */
  if (ferror(dest->outfile))
    ERREXIT(cinfo, JERR_FILE_WRITE);
}


/*
 * Prepare for output to a stdio stream.
 * The caller must have already opened the stream, and is responsible
 * for closing it after finishing compression.
 */

GLOBAL(void)
jpeg_stdio_dest (j_compress_ptr cinfo, FILE * outfile)
{
  my_dest_ptr dest;

  /* The destination object is made permanent so that multiple JPEG images
   * can be written to the same file without re-executing jpeg_stdio_dest.
   * This makes it dangerous to use this manager and a different destination
   * manager serially with the same JPEG object, because their private object
   * sizes may be different.  Caveat programmer.
   */
  if (cinfo->dest == NULL) {	/* first time for this JPEG object? */
    cinfo->dest = (struct jpeg_destination_mgr *)
      (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
				  SIZEOF(my_destination_mgr));
  }

  dest = (my_dest_ptr) cinfo->dest;
  dest->pub.init_destination = init_destination;
  dest->pub.empty_output_buffer = empty_output_buffer;
  dest->pub.term_destination = term_destination;
  dest->outfile = outfile;
}

/************************************************************************************
BitmapToJPeg - Converts from a bitmap file to a JPEG file.

inputs
   char        *pszBitmap - bitmap
   char        *pszJPEG - jpeg fle
returns
   BOOL - TRUE if success
*/
DLLEXPORT BOOL BitmapToJPegNoMegaFile (char *szBmp, char *szJPeg)
{
  struct jpeg_compress_struct cinfo;
  struct jpeg_error_mgr jerr;
  cjpeg_source_ptr src_mgr;
  FILE * input_file;
  FILE * output_file;
  JDIMENSION num_scanlines;

  /* Initialize the JPEG compression object with default error handling. */
  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_compress(&cinfo);
  /* Add some application-specific error messages (from cderror.h) */
  jerr.addon_message_table = cdjpeg_message_table;
  jerr.first_addon_message = JMSG_FIRSTADDONCODE;
  jerr.last_addon_message = JMSG_LASTADDONCODE;

  /* Initialize JPEG parameters.
   * Much of this may be overridden later.
   * In particular, we don't yet know the input file's color space,
   * but we need to provide some value for jpeg_set_defaults() to work.
   */

  cinfo.in_color_space = JCS_RGB; /* arbitrary guess */
  jpeg_set_defaults(&cinfo);

  /* Scan command line to find file names.
   * It is convenient to use just one switch-parsing routine, but the switch
   * values read here are ignored; we will rescan the switches after opening
   * the input file.
   */

  /* Open the input file. */
  OUTPUTDEBUGFILE (szBmp);
   if ((input_file = fopen(szBmp, READ_BINARY)) == NULL) {
      return FALSE;
    }

  /* Open the output file. */
   OUTPUTDEBUGFILE (szJPeg);
    if ((output_file = fopen(szJPeg, WRITE_BINARY)) == NULL) {
      fclose (input_file);
      return FALSE;
    }
  
  /* Figure out the input file format, and set up to read it. */
  src_mgr = jinit_read_bmp(&cinfo);
  src_mgr->input_file = input_file;

  /* Read the input file header to obtain file size & colorspace. */
  (*src_mgr->start_input) (&cinfo, src_mgr);

  /* Now that we know input colorspace, fix colorspace-dependent defaults */
  jpeg_default_colorspace(&cinfo);

  /* Specify data destination for compression */
  jpeg_stdio_dest(&cinfo, output_file);

  /* Start compressor */
  jpeg_start_compress(&cinfo, TRUE);

  /* Process data */
  while (cinfo.next_scanline < cinfo.image_height) {
    num_scanlines = (*src_mgr->get_pixel_rows) (&cinfo, src_mgr);
    (void) jpeg_write_scanlines(&cinfo, src_mgr->buffer, num_scanlines);
  }

  /* Finish compression and release memory */
  (*src_mgr->finish_input) (&cinfo, src_mgr);
  jpeg_finish_compress(&cinfo);
  jpeg_destroy_compress(&cinfo);

  /* Close files, if we opened them */
  fclose(input_file);
  fclose(output_file);

  return TRUE;
}


/***********************************************************************************
BitmapToJPegNoMegaFile - Takes a HBITMAP and saves it as a JPEG. Returns TRUE if sueccess
*/
DLLEXPORT BOOL BitmapToJPegNoMegaFile (HBITMAP hBit, char *szJPeg)
{
   char  szBmp[256], szPath[256];;
   szBmp[0] = 0;

   // make temp name
   GetTempPath (sizeof(szPath), szPath);
   GetTempFileName (szPath, "bmp", 0, szBmp);

   // save
   if (!BitmapSave (hBit, szBmp))
      return FALSE;

   // convert
   BOOL fRet;
   fRet = BitmapToJPegNoMegaFile (szBmp, szJPeg);
   if (szBmp[0])
      DeleteFile (szBmp);
   return fRet;
}

// BUGBUG - MOdify so doesn't write to temporary file when converting from
// JPEG to bitmap, and vice-versa

// BUGBUG - get rid of exit() calls since they're exploits. Look for
// other potential problems too
