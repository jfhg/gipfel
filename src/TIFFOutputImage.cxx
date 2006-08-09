// 
// Copyright 2006 by Johannes Hofmann
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Library General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
// USA.
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "TIFFOutputImage.H"

TIFFOutputImage::TIFFOutputImage(const char *f) {
	file = strdup(f);
	tiff = NULL;
	row = NULL;
}

TIFFOutputImage::~TIFFOutputImage() {
	if (row) {
		free(row);
	}
	if (file) {
		free(file);
	}
}

int
TIFFOutputImage::init_internal(int w1, int h1) {
	if (row) {
		free(row);
		row = NULL;
	}

	row = (unsigned char*) malloc(sizeof(char) * 4 * w1);
	if (!row) {
		perror("malloc");
		return 1;
	}
	memset(row, 0, sizeof(char) * 4 * w1);

	if (tiff) {
		TIFFClose(tiff);
	}

	if((tiff = TIFFOpen(file, "w")) == NULL){
		fprintf(stderr, "can't open %s\n", file);
		return 1;
	}

	TIFFSetField(tiff, TIFFTAG_IMAGEWIDTH, w1);
	TIFFSetField(tiff, TIFFTAG_IMAGELENGTH, h1);
	TIFFSetField(tiff, TIFFTAG_COMPRESSION, COMPRESSION_DEFLATE);
	TIFFSetField(tiff, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
	TIFFSetField(tiff, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
	TIFFSetField(tiff, TIFFTAG_ROWSPERSTRIP, 1);
	TIFFSetField(tiff, TIFFTAG_BITSPERSAMPLE, 8);
	TIFFSetField(tiff, TIFFTAG_SAMPLESPERPIXEL, 4);

	return 0;
}

int
TIFFOutputImage::set_pixel_internal(int x, char r, char g, char b) {
	row[x*4+0] = r;
	row[x*4+1] = g;
	row[x*4+2] = b;
	row[x*4+3] = 255;

	return 0;
}

int
TIFFOutputImage::next_line_internal() {
	int ret;

	TIFFWriteEncodedStrip(tiff, line -1 , row, W * 4);

	memset(row, 0, sizeof(char) * 4 * W);
	return 0;
}

int
TIFFOutputImage::done_internal() {
	if (tiff) {
		TIFFClose(tiff);
		tiff = NULL;
	}

	if (row) {
		free(row);
		row = NULL;
	}

	return 0;
}	

