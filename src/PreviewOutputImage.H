//
// Copyright 2006-2009 Johannes Hofmann <Johannes.Hofmann@gmx.de>
//
// This software may be used and distributed according to the terms
// of the GNU General Public License, incorporated herein by reference.

#ifndef PREVIEWOUTPUTIMAGE_H
#define PREVIEWOUTPUTIMAGE_H

#include <stdio.h>

#include <FL/Fl_Widget.H>
#include <FL/Fl_Image.H>

#include "OutputImage.H"

class PreviewOutputImage : public OutputImage , public Fl_Widget {
	private:
		uchar *data;
		int d;

	protected:
		int init_internal();
		int set_pixel_internal(int x, int r, int g, int b);	
		int next_line_internal();

	public:
		PreviewOutputImage(int X, int Y, int W, int H);
		~PreviewOutputImage();

		void draw();
};

#endif
