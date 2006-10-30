
#ifndef PREVIEWOUTPUTIMAGE_H
#define PREVIEWOUTPUTIMAGE_H

#include <stdio.h>

#include <FL/Fl_Widget.H>
#include <FL/Fl_Image.H>

#include "OutputImage.H"

class PreviewOutputImage : OutputImage , public Fl_Widget {
	private:
		uchar *data;
		int d;

	public:
		PreviewOutputImage(int X, int Y, int W, int H);

		~PreviewOutputImage();

		void draw();

	protected:
		int init_internal(int w, int h);

		int set_pixel_internal(int x, char r, char g, char b);	

		int next_line_internal();

		int done_internal();
};

#endif
