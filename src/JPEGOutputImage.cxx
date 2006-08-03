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

}

int
JPEGOutputImage::set_pixel_internal(int x, char r, char g, char b) {
	row[x*3+0] = r;
	row[x*3+1] = g;
	row[x*3+2] = b;

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
	next_line_internal();
	jpeg_finish_compress(&cinfo);
	fclose(fp);
	fp = NULL;
	jpeg_destroy_compress(&cinfo);
	if (row) {
		free(row);
	}

	if (fp) {
		fclose(fp);
		fp = NULL;
	}
	return 0;
}	

