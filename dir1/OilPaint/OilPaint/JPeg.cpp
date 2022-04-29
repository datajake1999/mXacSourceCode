/***********************************************************************************
JPeg.cpp - Code to merge the public-domain jpeg code into silicon page so its
easy to use.

begun 12/7/99 by Mike Rozak
Copyright 1999 Mike Rozak. All rights reserved
*/

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
// #include "siliconpage.h"
extern HINSTANCE ghInstance;
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
   FILE  *f = NULL;
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
   dwSize = ftell (f);
   fseek (f, 0, 0);

   // read it in
   pMem = (PBYTE) malloc (dwSize);
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
      free (pMem);

   return hBit;
}

/************************************************************************************
JPegToBitamp - Converts from a JPEG file to a bitmap.

inputs
   PVOID    pData - jpeg
   DWORD    dwSize - size in bytes
returns
   HBITMAP  - hbitmap

IMPORTANT - This is using temporary files because the public domain stuff works from
   file and it's easier to do this

*/
HBITMAP JPegToBitmap (char *szJPeg)
{
   char  szBmp[256];
   FILE  *input_file = NULL;
   FILE  *output_file = NULL;
   HBITMAP  hBit = NULL;
   szBmp[0] = 0;

   // make temp name
   strcpy (szBmp, "c:\\"); // BUGFIX: just in case get temp path fails
   GetTempPath (sizeof(szBmp), szBmp);
   strcat (szBmp, "OilPaint.bmp");

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

#ifdef TESTMSG
   char szTemp[256];
   sprintf (szTemp, "Test version - fopen(%s)", szJPeg);
   MessageBox (NULL, szTemp, NULL, MB_OK);
#endif
  input_file = fopen(szJPeg, READ_BINARY);
  if (!input_file)
     goto done;

#ifdef TESTMSG
   sprintf (szTemp, "Test version - fopen(%s)", szBmp);
   MessageBox (NULL, szTemp, NULL, MB_OK);
#endif
  output_file = fopen(szBmp, WRITE_BINARY);
  if (!output_file)
     goto done;
#ifdef TESTMSG
   MessageBox (NULL, "Test version - process jpeg", NULL, MB_OK);
#endif

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


#ifdef TESTMSG
   sprintf (szTemp, "Test version - loadbitmapfromfile(%s)", szBmp);
   MessageBox (NULL, szTemp, NULL, MB_OK);
#endif
  // load it in
   hBit = LoadBitmapFile (szBmp);

done:
   if (input_file)
      fclose (input_file);
   if (output_file)
      fclose (output_file);
   if (szBmp)
      DeleteFile (szBmp);
   return hBit;
}



HBITMAP JPegToBitmap (PVOID pData, DWORD dwSize)
{
   char  szJPeg[256];
   HBITMAP  hBit = NULL;
   szJPeg[0] = 0;

   // make temp name
   GetTempPath (sizeof(szJPeg), szJPeg);
   strcat (szJPeg, "OilPaint.jpg");

   // save the jpeg
   FILE  *f;
   f = fopen (szJPeg, "wb");
   if (!f)
      return NULL;
   fwrite (pData, 1, dwSize, f);
   fclose (f);

   hBit = JPegToBitmap (szJPeg);

   if (szJPeg[0])
      DeleteFile (szJPeg);

   return hBit;
}


/************************************************************************************
JPegToBitamp - Converts from a JPEG file to a bitmap.

inputs
   DWORD - resource ID of type 'jpg'
returns
   HBITMAP  - hbitmap

*/
HBITMAP JPegToBitmap (DWORD dwID)
{
   HRSRC    hr;

   hr = FindResource (ghInstance, MAKEINTRESOURCE (dwID), "jpg");

   HGLOBAL  hg;
   hg = LoadResource (ghInstance, hr);

   PVOID pMem;
   pMem = LockResource (hg);

   DWORD dwSize;
   dwSize = SizeofResource (ghInstance, hr);

   HBITMAP  hBit;
   hBit = JPegToBitmap (pMem, dwSize);

   return hBit;
}

