// 
// Copyright 2006 by Johannes Hofmann
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public
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

#ifndef IMAGE_META_DATA_H
#define IMAGE_META_DATA_H

class ImageMetaData {
	protected:
		double longitude;
		double latitude;
		double height;
		double direction;
		double nick;
		double tilt;
		double focallength_sensor_ratio;
		int    projection_type;

	public:
		ImageMetaData();

		virtual int load_image(char *name);
		virtual int save_image(char *in_img, char *out_img);

 		double get_longitude();
		double get_latitude();
		double get_height();
		double get_direction();
		double get_nick();
		double get_tilt();
		double get_focallength_sensor_ratio();
		int    get_projection_type();

		void set_longitude(double v);
		void set_latitude(double v);
		void set_height(double v);
		void set_direction(double v);
		void set_nick(double v);
		void set_tilt(double v);
		void set_focallength_sensor_ratio(double v);
		void set_projection_type(int v);
};
#endif
