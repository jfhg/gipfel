//
// Copyright 2006 Johannes Hofmann <Johannes.Hofmann@gmx.de>
//
// This software may be used and distributed according to the terms
// of the GNU General Public License, incorporated herein by reference.

#ifndef OUTPUTIMAGE_H
#define OUTPUTIMAGE_H


class OutputImage {
	private:
		int initialized;

	public:
		OutputImage();

		virtual ~OutputImage();
		
		virtual int init(int w1, int h1);

		int set_pixel(int x, int r, int g, int b);

		int next_line();

		int done();

	protected:
		int W, H, line;

		virtual int init_internal(int w1, int h1);

		virtual int set_pixel_internal(int x, int r, int g, int b);	
	
		virtual int next_line_internal();

		virtual int done_internal();
};

#endif
