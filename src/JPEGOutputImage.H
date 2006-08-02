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

#ifndef JPEGOUTPUTIMAGE_H
#define JPEGOUTPUTIMAGE_H

#include <stdio.h>
extern "C" {
#include <jpeglib.h>
#undef HAVE_STDLIB_H
}

#include "OutputImage.H"

class JPEGOutputImage : OutputImage {
	private:
		unsigned char *row;
		char *file;
		struct jpeg_compress_struct cinfo;
		struct jpeg_error_mgr jerr;
		FILE *fp;
		int quality;

	public:
		JPEGOutputImage(const char *file, int quality = 90);

		~JPEGOutputImage();

	protected:
		int init_internal(int w, int h);

		int set_pixel_internal(int x, char r, char g, char b);	

		int next_line_internal();

		int done_internal();
};

#endif
