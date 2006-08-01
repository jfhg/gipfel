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

#ifndef OUTPUTIMAGE_H
#define OUTPUTIMAGE_H


class OutputImage {
	public:
		OutputImage();

		~OutputImage();
		
		virtual int init(int w, int h);

		int set_pixel(int x, char r, char g, char b);

		int next_line();

	protected:
		virtual int set_pixel_internal(int x, char r, char g, char b);	
	
		virtual int next_line_internal();
};

class OutputImages {
	public:
		OutputImages();

		~OutputImages();

		int add(OutputImage *i);

		int remove(OutputImage *i);

		int init(int w, int h);

		int set_pixel(int x, char r, char g, char b);

		int next_line();
}

#endif
