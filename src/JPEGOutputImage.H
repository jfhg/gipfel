
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
