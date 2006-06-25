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

#include <Fl/fl_draw.h>

#include "DataImage.H"

DataImage::DataImage(int X, int Y, int W, int H): Fl_Widget(X, Y, W, H) {
	d = 3;
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

	return 0;
}


void
DataImage::draw() {
	fl_push_clip(x(), y(), w(), h());

	fl_draw_image(data, x(), y(), w(), h(), d);

	fl_pop_clip();
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
    






