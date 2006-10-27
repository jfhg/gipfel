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

#ifndef EXIF_IMAGE_META_DATA_H
#define EXIF_IMAGE_META_DATA_H

class ExifImageMetaData {
	ExifImageMetaData();

	virtual int load_image(const char *name);

 	virtual double get_longitude();
 	virtual double get_latitude();
 	virtual double get_height();
 	virtual double get_direction();
 	virtual double get_nick();
 	virtual double get_tilt();
 	virtual double get_focallength_sensor_ratio();
	virtual int    get_projection_type();

 	virtual void set_longitude(double v);
 	virtual void set_latitude(double v);
 	virtual void set_height(double v);
 	virtual void set_direction(double v);
 	virtual void set_nick(double v);
 	virtual void set_tilt(double v);
 	virtual void set_focallength_sensor_ratio(double v);
	virtual int  set_projection_type(int v);
};
#endif
