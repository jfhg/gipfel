// 
// Copyright 2006 by Johannes Hofmann
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Library General Public
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

#include <stdio.h>
#include <math.h>

#include "ImageMetaData.H"

ImageMetaData::ImageMetaData() {
	longitude = NAN;
	latitude = NAN;
	height = NAN;
	direction = NAN;
	nick = NAN;
	tilt = NAN;
	focallength_sensor_ratio = NAN;
	projection_type = -1;
}

int
ImageMetaData::load_image(char *name) {
	return 1;
}

int
ImageMetaData::save_image(char *in_img, char *out_img) {
	return 1;
}

double
ImageMetaData::get_longitude() {
	return longitude;
}

double
ImageMetaData::get_latitude() {
	return latitude;
}

double
ImageMetaData::get_height() {
	return height;
}

double
ImageMetaData::get_direction() {
	return direction;
}

double
ImageMetaData::get_nick() {
	return nick;
}

double
ImageMetaData::get_tilt() {
	return tilt;
}

double
ImageMetaData::get_focallength_sensor_ratio() {
	return focallength_sensor_ratio;
}

int
ImageMetaData::get_projection_type() {
	return projection_type;
}

void
ImageMetaData::set_longitude(double v) {
	longitude = v;
}

void
ImageMetaData::set_latitude(double v) {
	latitude = v;
}

void
ImageMetaData::set_height(double v) {
	height = v;
}

void
ImageMetaData::set_direction(double v) {
	direction = v;
}

void
ImageMetaData::set_nick(double v) {
	nick = v;
}

void
ImageMetaData::set_tilt(double v) {
	tilt = v;
}

void
ImageMetaData::set_focallength_sensor_ratio(double v) {
	focallength_sensor_ratio = v;
}

void
ImageMetaData::set_projection_type(int v) {
	projection_type = v;
}
