//
// Copyright 2006 Johannes Hofmann <Johannes.Hofmann@gmx.de>
//
// This software may be used and distributed according to the terms
// of the GNU General Public License, incorporated herein by reference.

#ifndef IMAGE_META_DATA_H
#define IMAGE_META_DATA_H

class ImageMetaData {
	private:
		char *manufacturer;
		char *model;
		double longitude;
		double latitude;
		double height;
		double direction;
		double nick;
		double tilt;
		double k0;
		double k1;
		double x0;
		double focal_length;
		double focal_length_35mm;
		double scale;
		int    projection_type;
		int    have_gipfel_info;

		int load_image_jpgcom(char *name);
		int save_image_jpgcom(char *in_img, char *out_img);
		int load_image_exif(char *name);
		void clear();

	public:
		ImageMetaData();
		~ImageMetaData();

		int load_image(char *name);
		int save_image(char *in_img, char *out_img);

		const char *get_manufacturer() {return manufacturer;};
		const char *get_model() {return model;};
 		double get_longitude() {return longitude;};
		double get_latitude() {return latitude;};
		double get_height() {return height;};
		double get_direction() {return direction;};
		double get_nick() {return nick;};
		double get_tilt() {return tilt;};
		double get_focal_length() {return focal_length;};
		double get_focal_length_35mm() {return focal_length_35mm;};
		int    get_projection_type() {return projection_type;};
		void get_distortion_params(double *_k0, double *_k1, double *_x0);

		void set_longitude(double v) {longitude = v;};
		void set_latitude(double v) {latitude = v;};
		void set_height(double v) {height = v;};
		void set_direction(double v) {direction = v;};
		void set_nick(double v) {nick = v;};
		void set_tilt(double v) {tilt = v;};
		void set_focal_length_35mm(double v) {focal_length_35mm = v;};
		void set_projection_type(int v) {projection_type = v;};
		void set_distortion_params(double _k0, double _k1, double _x0);
};
#endif
