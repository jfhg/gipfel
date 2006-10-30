
#ifndef IMAGE_META_DATA_H
#define IMAGE_META_DATA_H

class ImageMetaData {
	private:
		double longitude;
		double latitude;
		double height;
		double direction;
		double nick;
		double tilt;
		double focal_length_35mm;
		double scale;
		int    projection_type;

		int load_image_jpgcom(char *name);
		int save_image_jpgcom(char *in_img, char *out_img);
		int load_image_exif(char *name);

	public:
		ImageMetaData();

		int load_image(char *name, int img_width);
		int save_image(char *in_img, char *out_img);

 		double get_longitude();
		double get_latitude();
		double get_height();
		double get_direction();
		double get_nick();
		double get_tilt();
		double get_focal_length_35mm();
		int    get_projection_type();

		void set_longitude(double v);
		void set_latitude(double v);
		void set_height(double v);
		void set_direction(double v);
		void set_nick(double v);
		void set_tilt(double v);
		void set_focal_length_35mm(double v);
		void set_projection_type(int v);
};
#endif
