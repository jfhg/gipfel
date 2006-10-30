//
// Copyright 2006 Johannes Hofmann <Johannes.Hofmann@gmx.de>
//
// This software may be used and distributed according to the terms
// of the GNU General Public License, incorporated herein by reference.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "Panorama.H"
#include "ProjectionTangential.H"
#include "ProjectionSphaeric.H"

#define EARTH_RADIUS 6371010.0

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
	view_name = NULL;
	view_phi = 0.0;
	view_lam = 0.0;
	view_height = 0.0;
	proj = NULL;
	set_projection(Projection::TANGENTIAL);
}

Panorama::~Panorama() {
	visible_mountains->clear();
	mountains->clobber();
	delete(visible_mountains);
	delete(mountains);
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
Panorama::remove_trackpoints() {
	Hills *h_new = new Hills();
	Hill *m;

	for(int i=0; i<mountains->get_num(); i++) {
		m = mountains->get(i);
		if (! (m->flags & Hill::TRACK_POINT)) {
			h_new->add(m);
		}
	}

	delete mountains;
	mountains = h_new;
}


int
Panorama::set_viewpoint(const char *name) {
	Hill *m = get_pos(name);
	if (m == NULL) {
		fprintf(stderr, "Could not find exactly one entry for %s.\n");
		return 1;
	}

	set_viewpoint(m);
	return 0;
}

void
Panorama::set_viewpoint(const Hill *m) {
	if (m == NULL) {
		return;
	}

	view_phi = m->phi;
	view_lam = m->lam;
	view_height = m->height;

	if (view_name) {
		free(view_name);
	}

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

double
Panorama::get_value(Hills *p) {
	int i, j;
	Hill *m;
	double v = 0.0, d_min, d;

	if (isnan(parms.scale) || isnan(parms.a_center) || isnan(parms.a_tilt) || isnan(parms.a_nick) ||
		parms.scale < 500.0 || parms.scale > 100000.0 || 
		parms.a_nick > pi_d/4.0 || parms.a_nick < - pi_d/4.0 || 
		parms.a_tilt > pi_d/16.0 || parms.a_tilt < - pi_d/16.0) {
		return 10000000.0;
	}


	for (i=0; i<p->get_num(); i++) {
		d_min = 1000.0;
		for (j=0; j<visible_mountains->get_num(); j++) {
			d = pow(p->get(i)->x - visible_mountains->get(j)->x, 2.0) + 
				pow(p->get(i)->y - visible_mountains->get(j)->y, 2.0);
			if (d < d_min) {
				d_min = d;
			}
		}
		v = v + d_min;
	}

	return v;
}

int 
Panorama::guess(Hills *p, Hill *m1) {
	Hill *p2, *m_tmp1, *m_tmp2;
	Hill *m2;
	double best = 100000000.0, v;
	double a_center_best, a_nick_best, a_tilt_best, scale_best;
	int x1_sav, y1_sav;
	int i, j;

	if (m1 == NULL) {
		fprintf(stderr, "Position one mountain first.\n");
		return 1;
	}

	m_tmp1 = m1;
	x1_sav = m1->x;
	y1_sav = m1->y;

	for (i=0; i<p->get_num(); i++) {
		p2 = p->get(i);
		for (j=0; j<close_mountains->get_num(); j++) {
			m_tmp2 = close_mountains->get(j);

			m1 = m_tmp1;
			m1->x = x1_sav;
			m1->y = y1_sav;

			if (m_tmp2->flags & Hill::TRACK_POINT || 
				m1 == m_tmp2 || fabs(m1->alph - m_tmp2->alph) > pi_d *0.7) {
				continue;
			}

			m2 = m_tmp2;
			m2->x = p2->x;
			m2->y = p2->y;

			comp_params(m1, m2);

			v = get_value(p);

			if (v < best) {
				best = v;
				a_center_best = parms.a_center;
				a_nick_best = parms.a_nick;
				a_tilt_best = parms.a_tilt;
				scale_best = parms.scale;
			}
		}     
	}

	if (best < 4000.0) {
		parms.a_center = a_center_best;
		parms.a_nick = a_nick_best;
		parms.a_tilt = a_tilt_best;
		parms.scale = scale_best;
	} else {
		fprintf(stderr, "No solution found.\n");
	}
	update_visible_mountains();
	return 0;
}

int
Panorama::comp_params(Hill *m1, Hill *m2) {
	int ret;

	ret = proj->comp_params(m1, m2, &parms);
	update_visible_mountains();
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
Panorama::set_projection(Projection::Projection_t p) {
	projection_type = p;

	if (proj) {
		delete proj;
	}

	switch(projection_type) {
		case Projection::TANGENTIAL:
			proj = new ProjectionTangential();
			view_angle = pi_d / 3.0;
			break;
		case Projection::SPHAERIC:
			proj = new ProjectionSphaeric();
			view_angle = pi_d * 2.0;
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

Projection::Projection_t
Panorama::get_projection() {
	return projection_type;
}

Hill *
Panorama::get_pos(const char *name) {
	int i;
	int found = 0;
	double p, l, h;
	Hill *m, *ret;

	for (i=0; i<mountains->get_num(); i++) {
		m = mountains->get(i);

		if (strcmp(m->name, name) == 0) {
			ret = m;
			fprintf(stderr, "Found matching entry: %s (%fm)\n", m->name, m->height);
			found++;
		}
	}

	if (found == 1) {
		return ret;
	}

	return NULL;
}

void 
Panorama::update_angles() {
	int i;
	Hill *m;

	for (i=0; i<mountains->get_num(); i++) {
		m = mountains->get(i);

		m->dist = distance(m->phi, m->lam);
		if (m->phi != view_phi || m->lam != view_lam) {

			m->alph = alpha(m->phi, m->lam);
			m->a_nick = nick(m->dist, m->height);
		}
	}


	mountains->sort();

	update_close_mountains();
}

void
Panorama::set_hide_value(double h) {
	hide_value = h;
	update_visible_mountains();
}

void
Panorama::mark_hidden(Hills *hills) {
	int i, j;
	Hill *m, *n;
	double h;

	for (i=0; i<hills->get_num(); i++) {
		m = hills->get(i);

		m->flags &= ~Hill::HIDDEN;

		if (m->flags & Hill::DUPLICATE) {
			continue;
		}

		for (j=0; j<hills->get_num(); j++) {
			n = hills->get(j);

			if (n->flags & Hill::DUPLICATE || n->flags & Hill::TRACK_POINT) {
				continue;
			}
			if (m == n || fabs(m->alph - n->alph > pi_d / 2.0)) {
				continue;
			}
			if (m->dist < n->dist || m->a_nick > n->a_nick) {
				continue;
			}

			h = (n->a_nick - m->a_nick) / fabs(m->alph - n->alph);
			if (isinf(h) || h > hide_value) {
				m->flags |= Hill::HIDDEN;
			}
		}

	}

}

void 
Panorama::update_close_mountains() {
	int i;
	Hill *m;

	close_mountains->clear();

	for (i=0; i<mountains->get_num(); i++) {
		m = mountains->get(i);

		if (m->flags & Hill::TRACK_POINT ||
			((m->phi != view_phi || m->lam != view_lam) &&
			 (m->height / (m->dist * get_earth_radius(m)) 
			  > height_dist_ratio))) {

			close_mountains->add(m);
		}
	}

	mark_hidden(close_mountains);
	update_visible_mountains();
}

void 
Panorama::update_visible_mountains() {
	int i;
	Hill *m;

	visible_mountains->clear();

	for (i=0; i<close_mountains->get_num(); i++) {
		m = close_mountains->get(i);

		m->a_view = m->alph - parms.a_center;

		if (m->a_view > pi_d) {
			m->a_view -= 2.0*pi_d;
		} else if (m->a_view < -pi_d) {
			m->a_view += 2.0*pi_d;
		}

		if (m->a_view < view_angle && m->a_view > - view_angle) {
			visible_mountains->add(m);
			m->flags |= Hill::VISIBLE;
		} else {
			m->flags &= ~Hill::VISIBLE;
		}
	}

	update_coordinates();
}

void
Panorama::update_coordinates() {
	Hill *m;

	for (int i=0; i<visible_mountains->get_num(); i++) {
		m = visible_mountains->get(i);
		double tmp_x, tmp_y;

		proj->get_coordinates(m->a_view, m->a_nick, &parms, &tmp_x, &tmp_y);
		m->x = (int) rint(tmp_x);
		m->y = (int) rint(tmp_y);
	}
}

double 
Panorama::distance(double phi, double lam) {
	return acos(sin(view_phi) * sin(phi) + 
		cos(view_phi) * cos(phi) * cos(view_lam - lam));
}

double 
Panorama::sin_alpha(double lam, double phi, double c) {
	return sin(lam - view_lam) * cos(phi) / sin(c);
}


double
Panorama::cos_alpha(double phi, double c) {
	return (sin(phi) - sin(view_phi) * cos(c)) / (cos(view_phi) * sin(c));
}


double 
Panorama::alpha(double phi, double lam) {
	double dist, sin_alph, cos_alph, alph;

	dist = distance(phi, lam);
	sin_alph = sin_alpha(lam, phi, dist);
	cos_alph = cos_alpha(phi, dist);

	if (sin_alph > 0) {
		alph = acos(cos_alph);
	} else {
		alph = 2.0 * pi_d - acos(cos_alph);
	}


	if (alph > 2.0 * pi_d) {
		alph = alph - 2.0 *  pi_d;
	} else if (alph < 0.0) {
		alph = alph + 2.0 * pi_d;
	}
	return alph;
}


double
Panorama::nick(double dist, double height) {
	double a, b, c;
	double beta;

	b = height + EARTH_RADIUS;
	c = view_height + EARTH_RADIUS;

	a = pow(((b * (b - (2.0 * c * cos(dist)))) + (c * c)), (1.0 / 2.0));
	beta = acos((-(b*b) + (a*a) + (c*c))/(2 * a * c));

	return beta - pi_d / 2.0;
}

double
Panorama::get_earth_radius(Hill *m) {
	return EARTH_RADIUS;
}

double
Panorama::get_real_distance(Hill *m) {
	double a, b, c, gam;

	a = view_height + get_earth_radius(m); // using m here is not quite right
	b = m->height + get_earth_radius(m); 
	gam = m->dist;

	c = sqrt(pow(a, 2.0) + pow(b, 2.0) - 2.0 * a * b * cos(gam));
	return c;
}

int
Panorama::get_coordinates(double a_view, double a_nick, double *x, double *y) {
	a_view = a_view - parms.a_center;

	if (a_view > pi_d) {
		a_view -= 2.0*pi_d;
	} else if (a_view < -pi_d) {
		a_view += 2.0*pi_d;
	}

	if (a_view > view_angle || a_view < - view_angle) {
		return 1;
	}

	proj->get_coordinates(a_view, a_nick, &parms, x, y);

	return 0;
}

