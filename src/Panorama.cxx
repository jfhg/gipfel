//
// Copyright 2006-2009 Johannes Hofmann <Johannes.Hofmann@gmx.de>
//
// This software may be used and distributed according to the terms
// of the GNU General Public License, incorporated herein by reference.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "Panorama.H"
#include "ProjectionRectilinear.H"
#include "ProjectionCylindrical.H"

#define EARTH_RADIUS 6371000.785

Panorama::Panorama() {
	mountains = new Hills();
	close_mountains = new Hills();
	visible_mountains = new Hills();
	height_dist_ratio = 0.07;
	hide_value = 1.2;
	pi_d = asin(1.0) * 2.0;
	deg2rad = pi_d / 180.0;
	parms.a_center = 0.0;
	parms.a_nick = 0.0;
	parms.a_tilt = 0.0;
	parms.scale = 3500.0;
	parms.k0 = 0.0;
	parms.k1 = 0.0;
	parms.x0 = 0.0;
	view_name = NULL;
	view_phi = 0.0;
	view_lam = 0.0;
	view_height = 0.0;
	proj = NULL;
	set_projection(ProjectionLSQ::RECTILINEAR);
}

Panorama::~Panorama() {
	visible_mountains->clear();
	mountains->clobber();
	delete visible_mountains;
	delete mountains;
}

int
Panorama::load_data(const char *name) {
	if (mountains->load(name) != 0) {
		fprintf(stderr, "Could not load datafile %s\n", name);
		return 1;
	}

	mountains->mark_duplicates(0.00001);
	update_angles();

	return 0;
}

void
Panorama::add_hills(Hills *h) {
	mountains->add(h);

	mountains->mark_duplicates(0.00001);
	update_angles();
}

void
Panorama::remove_hills(int flags) {
	Hills *h_new = new Hills();
	Hill *m;

	for (int i = 0; i < mountains->get_num(); i++) {
		m = mountains->get(i);
		if (! (m->flags & flags))
			h_new->add(m);
	}

	delete mountains;
	mountains = h_new;
}

int
Panorama::set_viewpoint(const char *name) {
	Hill *m = get_pos(name);
	if (m == NULL) {
		fprintf(stderr, "Could not find exactly one entry for %s.\n", name);
		return 1;
	}

	set_viewpoint(m);
	return 0;
}

void
Panorama::set_viewpoint(const Hill *m) {
	if (m == NULL)
		return;

	view_phi = m->phi;
	view_lam = m->lam;
	view_height = m->height;

	if (view_name)
		free(view_name);

	view_name = strdup(m->name);

	update_angles();
}

Hills * 
Panorama::get_mountains() {
	return mountains;
}

Hills * 
Panorama::get_close_mountains() {
	return close_mountains;
}

Hills * 
Panorama::get_visible_mountains() {
	return visible_mountains;
}

int
Panorama::comp_params(Hills *h) {
	int ret;

	ret = proj->comp_params(h, &parms);
	if (ret == 0)
		update_visible_mountains(h);

	return ret;
}

void
Panorama::set_center_angle(double a) {
	parms.a_center = a * deg2rad;
	update_visible_mountains();
}

void
Panorama::set_nick_angle(double a) {
	parms.a_nick = a * deg2rad;
	update_coordinates();
}

void
Panorama::set_tilt_angle(double a) {
	parms.a_tilt = a * deg2rad;
	update_coordinates();
}

void
Panorama::set_scale(double s) {
	parms.scale = s;
	update_coordinates();
}

void
Panorama::get_distortion_params(double *k0, double *k1, double *x0) {
	*k0 = parms.k0;
	*k1 = parms.k1;
	*x0 = parms.x0;
}

void
Panorama::set_distortion_params(double k0, double k1, double x0) {
	parms.k0 = k0;
	parms.k1 = k1;
	parms.x0 = x0;
	update_coordinates();
}

void
Panorama::set_height_dist_ratio(double r) {
	height_dist_ratio = r;
	update_close_mountains();
}

void
Panorama::set_view_long(double v) {
	view_lam = v * deg2rad;
	update_angles();
}

void
Panorama::set_view_lat(double v) {
	view_phi = v * deg2rad;
	update_angles();
}

void
Panorama::set_view_height(double v) {
	view_height = v;
	update_angles();
}

void
Panorama::set_projection(ProjectionLSQ::Projection_t p) {
	projection_type = p;

	if (proj) {
		delete proj;
		proj = NULL;
	}

	switch(projection_type) {
		case ProjectionLSQ::RECTILINEAR:
			proj = new ProjectionRectilinear();
			break;
		case ProjectionLSQ::CYLINDRICAL:
			proj = new ProjectionCylindrical();
			break;
	}

	update_angles();
}

const char *
Panorama::get_viewpoint() {
	return view_name;
}

double
Panorama::get_center_angle() {
	return parms.a_center / deg2rad;
}

double
Panorama::get_nick_angle() {
	return parms.a_nick / deg2rad;
}

double
Panorama::get_tilt_angle() {
	return parms.a_tilt / deg2rad;
}

double
Panorama::get_scale() {
	return parms.scale;
}

double
Panorama::get_height_dist_ratio() {
	return height_dist_ratio;
}

double
Panorama::get_view_long() {
	return view_lam / deg2rad;
}

double
Panorama::get_view_lat() {
	return view_phi / deg2rad;
}

double
Panorama::get_view_height() {
	return view_height;
}

ProjectionLSQ::Projection_t
Panorama::get_projection() {
	return projection_type;
}

Hill *
Panorama::get_pos(const char *name) {
	Hill *m, *ret = NULL;

	for (int i = 0; i < mountains->get_num(); i++) {
		m = mountains->get(i);

		if (strcmp(m->name, name) == 0) {
			ret = m;
			fprintf(stderr, "Found matching entry: %s (%fm)\n",
				m->name, m->height);
		}
	}

	return ret;
}

void 
Panorama::update_angles() {
	for (int i = 0; i < mountains->get_num(); i++) {
		Hill *m = mountains->get(i);

		m->dist = distance(m->phi, m->lam);
		if (m->phi != view_phi || m->lam != view_lam)
			m->alph = alpha(m);
	}

	mountains->sort(Hills::SORT_ALPHA);

	update_close_mountains();
}

void
Panorama::set_hide_value(double h) {
	hide_value = h;
	update_visible_mountains();
}

void
Panorama::mark_hidden(Hills *hills) {
	double h;

	for (int i = 0; i < hills->get_num(); i++) {
		Hill *m = hills->get(i);

		m->flags &= ~Hill::HIDDEN;

		if (m->flags & Hill::DUPLIC)
			continue;

		for (int j = 0; j < hills->get_num(); j++) {
			Hill *n = hills->get(j);

			if (n->flags & Hill::DUPLIC || n->flags & Hill::TRACK_POINT)
				continue;

			if (m == n || fabs(m->alph - n->alph > pi_d / 2.0))
				continue;

			if (m->dist < n->dist || m->a_nick > n->a_nick)
				continue;

			h = (n->a_nick - m->a_nick) / fabs(m->alph - n->alph);
			if (isinf(h) || h > hide_value)
				m->flags |= Hill::HIDDEN;
		}
	}
}

void 
Panorama::update_close_mountains() {
	close_mountains->clear();

	for (int i = 0; i < mountains->get_num(); i++) {
		Hill *m = mountains->get(i);

		if (m->flags & Hill::TRACK_POINT ||
			((m->phi != view_phi || m->lam != view_lam) &&
			 (m->height / (m->dist * EARTH_RADIUS) > height_dist_ratio))) {

			m->a_nick = nick(m);
			close_mountains->add(m);
		}
	}

	mark_hidden(close_mountains);
	update_visible_mountains();
}

void 
Panorama::update_visible_mountains(Hills *excluded_hills) {
	visible_mountains->clear();

	for (int i = 0; i < close_mountains->get_num(); i++) {
		Hill *m = close_mountains->get(i);

		if (is_visible(m->alph)) {
			visible_mountains->add(m);
			m->flags |= Hill::VISIBLE;
		} else {
			m->flags &= ~Hill::VISIBLE;
		}
	}

	update_coordinates(excluded_hills);
}

void
Panorama::update_coordinates(Hills *excluded_hills) {
	for (int i = 0; i < visible_mountains->get_num(); i++) {
		Hill *m = visible_mountains->get(i);
		if (!excluded_hills || !excluded_hills->contains(m))
			proj->get_coordinates(m->alph, m->a_nick, &parms, &m->x, &m->y);
	}
}

double 
Panorama::distance(double phi, double lam) {
	double d_lam = view_lam - lam;

	return atan2(sqrt(pow(cos(phi) * sin(d_lam), 2.0) +
				pow(cos(view_phi) * sin(phi) - sin(view_phi) * cos(phi) * cos(d_lam), 2.0)),
		sin(view_phi) * sin(phi) + cos(view_phi) * cos(phi) * cos(d_lam));
}

double 
Panorama::alpha(const Hill *m) {
	double sin_alph, cos_alph;

	sin_alph = sin(m->lam - view_lam) * cos(m->phi) / sin(m->dist);
	cos_alph = (sin(m->phi) - sin(view_phi) * cos(m->dist)) /
		(cos(view_phi) * sin(m->dist));

	return fmod(atan2(sin_alph, cos_alph) + 2.0 * pi_d, 2.0 * pi_d);
}

// approximation of refraction effect as described by Tom Chester at
// http://tchester.org/sgm/analysis/peaks/refraction.html
double
Panorama::refraction(const Hill *m) {
	double a, b, c, alpha = 6.5, T0 = 10.0;

	a = 2.9e-4 * exp (-view_height / 10000.0) / (1.0 + 2.9 * T0 / 760.0);
	b = 2.9 * alpha / (760.0 * (1.0 + 2.9 * T0 / 760.0));
	c = a * (b - 1.0 / 10.0);

	return c * get_real_distance(m) / (2000.0 * (1.0 + a));
}

double
Panorama::nick(const Hill *m) {
	double b, c, theta = refraction(m);

	b = m->height + get_earth_radius(m->phi);
	c = view_height + get_earth_radius(view_phi);

	return atan((cos(m->dist) * b - c) / (sin(m->dist) * b)) - theta;
}

// return local distance to center of WGS84 ellipsoid
double
Panorama::get_earth_radius(double phi) {
	double a = 6378137.000;
	double b = 6356752.315;
	double r;
	double ata = tan(phi);

	r = a*pow(pow(ata,2)+1,1.0/2.0)*fabs(b)*pow(pow(b,2)+pow(a,2)*pow(ata,2),-1.0/2.0);
	return r;
}

double
Panorama::get_real_distance(const Hill *m) {
	double a, b, c, gam;

	a = view_height + get_earth_radius(view_phi);
	b = m->height + get_earth_radius(m->phi); 
	gam = m->dist;

	c = sqrt(pow(a, 2.0) + pow(b, 2.0) - 2.0 * a * b * cos(gam));
	return c;
}

int
Panorama::is_visible(double a_alph) {
	double center_dist;

	center_dist = fabs(a_alph - parms.a_center);
	if (center_dist > pi_d) {
		center_dist = 2*pi_d - center_dist;
	}

	return center_dist < proj->get_view_angle();
}

int
Panorama::get_coordinates(double a_alph, double a_nick, double *x, double *y) {
	if (is_visible(a_alph)) {
		proj->get_coordinates(a_alph, a_nick, &parms, x, y);
		return 0;
	} else {
		return 1;
	}
}
