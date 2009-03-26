//
// Copyright 2006 Johannes Hofmann <Johannes.Hofmann@gmx.de>
//
// This software may be used and distributed according to the terms
// of the GNU General Public License, incorporated herein by reference.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "TIFFOutputImage.H"

TIFFOutputImage::TIFFOutputImage(const char *f, int b) : OutputImage () {
	bitspersample = (b==16)?16:8;
	file = strdup(f);
	tiff = NULL;
	row = NULL;
}

TIFFOutputImage::~TIFFOutputImage() {
	if (row)
		free(row);

	if (file)
		free(file);
}

int
TIFFOutputImage::init_internal() {
	if (row)
		free(row);
	row = NULL;

	row = (unsigned char*) calloc((bitspersample / 8) * 4 * W, sizeof(char));
	if (!row) {
		perror("calloc");
		return 1;
	}

	if (tiff)
		TIFFClose(tiff);

	if ((tiff = TIFFOpen(file, "w")) == NULL){
		fprintf(stderr, "can't open %s\n", file);
		return 1;
	}

	TIFFSetField(tiff, TIFFTAG_IMAGEWIDTH, W);
	TIFFSetField(tiff, TIFFTAG_IMAGELENGTH, H);
	TIFFSetField(tiff, TIFFTAG_COMPRESSION, COMPRESSION_DEFLATE);
	TIFFSetField(tiff, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
	TIFFSetField(tiff, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
	TIFFSetField(tiff, TIFFTAG_ROWSPERSTRIP, 1);
	TIFFSetField(tiff, TIFFTAG_BITSPERSAMPLE, bitspersample);
	TIFFSetField(tiff, TIFFTAG_SAMPLESPERPIXEL, 4);

	return 0;
}

int
TIFFOutputImage::set_pixel_internal(int x, int r, int g, int b) {
	if (bitspersample == 8) {	
		row[x*4+0] = (unsigned char) (r / 255);
		row[x*4+1] = (unsigned char) (g / 255);
		row[x*4+2] = (unsigned char) (b / 255);
		row[x*4+3] = 255;
	} else if (bitspersample == 16) {
		unsigned short *row16 = (unsigned short*) row;
		row16[x*4+0] = (unsigned short) r;
		row16[x*4+1] = (unsigned short) g;
		row16[x*4+2] = (unsigned short) b;
		row16[x*4+3] = 65025;
	}

	return 0;
}

int
TIFFOutputImage::next_line_internal() {
	TIFFWriteEncodedStrip(tiff, line - 1 , row, W * (bitspersample / 8) * 4);

	memset(row, 0, (bitspersample / 8) * 4 * W * sizeof(char));
	return 0;
}

int
TIFFOutputImage::done_internal() {
	if (tiff)
		TIFFClose(tiff);
	tiff = NULL;

	if (row)
		free(row);
	row = NULL;

	return 0;
}	
