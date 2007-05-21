//
// Copyright 2006 Johannes Hofmann <Johannes.Hofmann@gmx.de>
//
// This software may be used and distributed according to the terms
// of the GNU General Public License, incorporated herein by reference.

#ifndef TIFFOUTPUTIMAGE_H
#define TIFFOUTPUTIMAGE_H

#include <stdio.h>
#include <tiffio.h>

#include "OutputImage.H"

class TIFFOutputImage : public OutputImage {
	private:
		int bitspersample;
		unsigned char *row;
		char *file;
		TIFF *tiff;

	public:
		TIFFOutputImage(const char *file, int b = 8);

		~TIFFOutputImage();

	protected:
		int init_internal(int w, int h);

		int set_pixel_internal(int x, int r, int g, int b);	

		int next_line_internal();

		int done_internal();
};

#endif
