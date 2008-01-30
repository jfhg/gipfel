//
// Copyright 2006 Johannes Hofmann <Johannes.Hofmann@gmx.de>
//
// This software may be used and distributed according to the terms
// of the GNU General Public License, incorporated herein by reference.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
extern "C" {
#include <jpeglib.h>
}

#include "JPEGOutputImage.H"

JPEGOutputImage::JPEGOutputImage(const char *f, int q) {
	file = strdup(f);
	fp = NULL;
	row = NULL;
	quality = q;
}

JPEGOutputImage::~JPEGOutputImage() {
	if (row) {
		free(row);
	}
	if (file) {
		free(file);
	}
}

int
JPEGOutputImage::init_internal(int w1, int h1) {
	if (row) {
		free(row);
		row = NULL;
	}

	row = (unsigned char*) malloc(sizeof(char) * 3 * W);
	if (!row) {
		perror("malloc");
		return 1;
	}
	memset(row, 0, sizeof(char) * 3 * W);

	if (fp) {
		fclose(fp);
	}

	if ((fp = fopen(file, "wb")) == NULL) {
		fprintf(stderr, "can't open %s\n", file);
		return 1;
	}

	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);
	jpeg_stdio_dest(&cinfo, fp);
	cinfo.image_width = W;
	cinfo.image_height = H;
	cinfo.input_components = 3;          /* # of color components per pixel */
	cinfo.in_color_space = JCS_RGB;

	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, quality, TRUE);

	jpeg_start_compress(&cinfo, TRUE);

	return 0;
}

int
JPEGOutputImage::set_pixel_internal(int x, int r, int g, int b) {
	row[x*3+0] = (unsigned char) (r / 255);
	row[x*3+1] = (unsigned char) (g / 255);
	row[x*3+2] = (unsigned char) (b / 255);

	return 0;
}

int
JPEGOutputImage::next_line_internal() {
	JSAMPROW row_pointer[1];

	row_pointer[0] = row;
	jpeg_write_scanlines(&cinfo, row_pointer, 1);
	memset(row, 0, sizeof(char) * 3 * W);
	return 0;
}

int
JPEGOutputImage::done_internal() {
	jpeg_finish_compress(&cinfo);
	jpeg_destroy_compress(&cinfo);

	if (fp) {
		fclose(fp);
		fp = NULL;
	}
	if (row) {
		free(row);
	}

	if (fp) {
		fclose(fp);
		fp = NULL;
	}
	return 0;
}	

