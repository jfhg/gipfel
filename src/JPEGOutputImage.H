//
// Copyright 2006-2009 Johannes Hofmann <Johannes.Hofmann@gmx.de>
//
// This software may be used and distributed according to the terms
// of the GNU General Public License, incorporated herein by reference.

#ifndef JPEGOUTPUTIMAGE_H
#define JPEGOUTPUTIMAGE_H

#include <stdio.h>
extern "C" {
#include <jpeglib.h>
#undef HAVE_STDLIB_H
}

#include "OutputImage.H"

class JPEGOutputImage : public OutputImage {
	private:
		unsigned char *row;
		char *file;
		struct jpeg_compress_struct cinfo;
		struct jpeg_error_mgr jerr;
		FILE *fp;
		int quality;

	protected:
		int init_internal();
		int set_pixel_internal(int x, int r, int g, int b);	
		int next_line_internal();
		int done_internal();

	public:
		JPEGOutputImage(const char *file, int quality = 90);
		~JPEGOutputImage();
};

#endif
