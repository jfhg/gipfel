//
// Copyright 2006-2009 Johannes Hofmann <Johannes.Hofmann@gmx.de>
//
// This software may be used and distributed according to the terms
// of the GNU General Public License, incorporated herein by reference.

#ifndef IMAGE_META_DATA_H
#define IMAGE_META_DATA_H

class ImageMetaData {
	private:
		char *_manufacturer;
		char *_model;
		double _longitude;
		double _latitude;
		double _height;
		double _direction;
		double _nick;
		double _tilt;
		double _k0;
		double _k1;
		double _x0;
		double _focal_length;
		double _focal_length_35mm;
		double _scale;
		int _projection_type;
		int _have_gipfel_info;

		int load_image_jpgcom(char *name);
		int save_image_jpgcom(char *in_img, char *out_img);
		int load_image_exif(char *name);
		void clear();

	public:
		ImageMetaData();
		~ImageMetaData();

		int load_image(char *name);
		int save_image(char *in_img, char *out_img);

		const char *manufacturer() {return _manufacturer;};
		const char *model() {return _model;};
 		double longitude() {return _longitude;};
		double latitude() {return _latitude;};
		double height() {return _height;};
		double direction() {return _direction;};
		double nick() {return _nick;};
		double tilt() {return _tilt;};
		double focal_length() {return _focal_length;};
		double focal_length_35mm() {return _focal_length_35mm;};
		int projection_type() {return _projection_type;};
		void distortion_params(double *_k0, double *_k1, double *_x0);

		void longitude(double v) {_longitude = v;};
		void latitude(double v) {_latitude = v;};
		void height(double v) {_height = v;};
		void direction(double v) {_direction = v;};
		void nick(double v) {_nick = v;};
		void tilt(double v) {_tilt = v;};
		void focal_length_35mm(double v) {_focal_length_35mm = v;};
		void projection_type(int v) {_projection_type = v;};
		void distortion_params(double k0, double k1, double x0);
};
#endif
