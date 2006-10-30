
#ifndef STITCH_H
#define STITCH_H

#include "GipfelWidget.H"
#include "OutputImage.H"

#define MAX_PICS 256

		
class Stitch {
	private:
		GipfelWidget *gipf[MAX_PICS];
		OutputImage *single_images[MAX_PICS];
		OutputImage *merged_image;
		

	public:
		Stitch();

		~Stitch();
		
		int load_image(char *file);

		OutputImage * set_output(OutputImage *img);

		OutputImage * set_output(const char *file, OutputImage *img);

		int resample(int w, int h,
			double view_start, double view_end);
};

#endif
