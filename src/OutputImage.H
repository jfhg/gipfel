
#ifndef OUTPUTIMAGE_H
#define OUTPUTIMAGE_H


class OutputImage {
	private:
		int initialized;

	public:
		OutputImage();

		~OutputImage();
		
		virtual int init(int w1, int h1);

		int set_pixel(int x, char r, char g, char b);

		int next_line();

		int done();

	protected:
		int W, H, line;

		virtual int init_internal(int w1, int h1);

		virtual int set_pixel_internal(int x, char r, char g, char b);	
	
		virtual int next_line_internal();

		virtual int done_internal();
};

#endif
