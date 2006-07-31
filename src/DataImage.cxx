// 
// DataImage routines.
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
#include <math.h>
extern "C" {
#include <jpeglib.h>
#include <tiffio.h>
}

#include <Fl/fl_draw.h>

#include "DataImage.H"

DataImage::DataImage(int X, int Y, int W, int H, int channels): Fl_Widget(X, Y, W, H) {
	d = channels;
	data = (uchar*) malloc(W * H * d);
	memset(data, 0, W * H * d);
}

DataImage::~DataImage() {

}


int
DataImage::set_pixel(int x, int y, char r, char g, char b) {
	if (x < 0 || x >= w() || y < 0 || y >= h()) {
		return 1;
	}

	long index = (y * w() * d + (x * d)); // X/Y -> buf index  
	*(data+index+0) = r;
	*(data+index+1) = g;
	*(data+index+2) = b;
	if (d == 4) {
		*(data+index+3) = 255;
	}

	return 0;
}


void
DataImage::draw() {
	fl_push_clip(x(), y(), w(), h());

	fl_draw_image(data, x(), y(), w(), h(), d);

	fl_pop_clip();
}

int
DataImage::get_pixel_bilinear(Fl_Image *img, double x, double y,
                     char *r, char *g, char *b) {


}

int
DataImage::get_pixel_nearest(Fl_Image *img, double x, double y,
                     char *r, char *g, char *b) {
	if (isnan(x) || isnan(y)) {
		return 1;
	} else {
		return get_pixel(img, (int) rint(x), (int) rint(y), r, g, b);
	}
}

int
DataImage::get_pixel(Fl_Image *img, int x, int y,
                     char *r, char *g, char *b) {
	if ( img->d() == 0 ) {
		return 1;
	}

	if (x < 0 || x >=img->w() || y < 0 || y >= img->h()) {
		return 1;
	}
	long index = (y * img->w() * img->d()) + (x * img->d()); // X/Y -> buf index  
	switch ( img->count() ) {
		case 1: {                                            // bitmap
				const char *buf = img->data()[0];
				switch ( img->d() ) {
					case 1: {                                    // 8bit
							*r = *g = *b = *(buf+index);
							break;
						}
					case 3:                                      // 24bit
						*r = *(buf+index+0);
						*g = *(buf+index+1);
						*b = *(buf+index+2);
						break;
					default:                                     // ??
						printf("Not supported: chans=%d\n", img->d());
						return 1;
                    }
				break;
			}
		default:                                             // ?? pixmap, bit vals
			printf("Not supported: count=%d\n", img->count());
			exit(1);
	}

	return 0;
}


int
DataImage::write_jpeg(const char *file, int quality) {
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	FILE *outfile; 
	JSAMPROW row_pointer[1];
	int row_stride; 

	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);

	if ((outfile = fopen(file, "wb")) == NULL) {
		fprintf(stderr, "can't open %s\n", file);
		return 1;
	}

	jpeg_stdio_dest(&cinfo, outfile);
	cinfo.image_width = w();
	cinfo.image_height = h();
	cinfo.input_components = 3;          /* # of color components per pixel */
	cinfo.in_color_space = JCS_RGB;

	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, quality, TRUE);

	jpeg_start_compress(&cinfo, TRUE);

	row_stride = w() * d; /* JSAMPLEs per row in image_buffer */
	while (cinfo.next_scanline < cinfo.image_height) {
		row_pointer[0] = & data[cinfo.next_scanline * row_stride];
		jpeg_write_scanlines(&cinfo, row_pointer, 1);
	}

	jpeg_finish_compress(&cinfo);
	fclose(outfile);
	jpeg_destroy_compress(&cinfo);

	return 0;
}

int
DataImage::write_tiff(const char *file) {
	TIFF *output;
	uint32 width, height;
	char *raster;

	// Open the output image
	if((output = TIFFOpen(file, "w")) == NULL){
		fprintf(stderr, "can't open %s\n", file);
		return 1;
	}

	// Write the tiff tags to the file
	TIFFSetField(output, TIFFTAG_IMAGEWIDTH, w());
	TIFFSetField(output, TIFFTAG_IMAGELENGTH, h());
	TIFFSetField(output, TIFFTAG_COMPRESSION, COMPRESSION_DEFLATE);
	TIFFSetField(output, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
	TIFFSetField(output, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
	TIFFSetField(output, TIFFTAG_BITSPERSAMPLE, 8);
	TIFFSetField(output, TIFFTAG_SAMPLESPERPIXEL, d);

	// Actually write the image
	if(TIFFWriteEncodedStrip(output, 0, data, w() * h() * d) == 0){
		fprintf(stderr, "Could not write image\n");
		return 2;
	}

	TIFFClose(output);
	return 0;
}
