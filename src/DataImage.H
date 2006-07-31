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

#ifndef DATAIMAGE_H
#define DATAIMAGE_H

#include <FL/Fl_Widget.H>
#include <FL/Fl_Image.H>

class DataImage : public Fl_Widget {
	private:
		int d;
		uchar *data;
		

	public:
		DataImage(int X, int Y, int W, int H);

		~DataImage();
		
		int set_pixel(int x, int y, char r, char g, char b);

		void draw();

		int write_jpeg(const char *file, int quality);

		static int get_pixel(Fl_Image *img, int x, int y,
			char *r, char *g, char *b);

		static int get_pixel_bilinear(Fl_Image *img, double x, double y,
			char *r, char *g, char *b);

		static int get_pixel_nearest(Fl_Image *img, double x, double y,
			char *r, char *g, char *b);
};

#endif
