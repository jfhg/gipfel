//
// Copyright 2006-2009 Johannes Hofmann <Johannes.Hofmann@gmx.de>
//
// This software may be used and distributed according to the terms
// of the GNU General Public License, incorporated herein by reference.

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <algorithm>

#include <FL/Fl.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/Fl_Shared_Image.H>
#include <FL/Fl_JPEG_Image.H>
#include <FL/Fl_Preferences.H>
#include <FL/fl_draw.H>

#include "Fl_Search_Chooser.H"
#include "choose_hill.H"
#include "GipfelWidget.H"

#define CROSS_SIZE 2

static double pi_d, deg2rad;

GipfelWidget::GipfelWidget(int X,int Y,int W, int H): Fl_Group(X, Y, W, H) {
	end();
	pi_d = asin(1.0) * 2.0;
	deg2rad = pi_d / 180.0;
	img = NULL;
	pan = new Panorama();
	cur_mountain = NULL;
	focused_mountain = NULL;
	known_hills = new Hills();
	img_file = NULL;
	track_width = 200.0;
	show_hidden = false;
	have_gipfel_info = false;
	md = new ImageMetaData();
	track_points = NULL;
	fl_register_images();
	mouse_x = mouse_y = 0;
}

int
GipfelWidget::load_image(char *file) {
	Fl_Image *new_img;
	double direction, nick, tilt, fl;

	new_img = new Fl_JPEG_Image(file);

	if (new_img == NULL)
		return 1;

	if (img)
		delete img;

	img = new_img;

	if (img_file)
		free(img_file);

	img_file = strdup(file);

	known_hills->clear();

	h(img->h());  
	w(img->w());  

	// try to retrieve gipfel data from JPEG meta data
	md->load_image(file);
	set_view_long(md->longitude());
	set_view_lat(md->latitude());
	set_view_height(md->height());
	projection((ProjectionLSQ::Projection_t) md->projection_type());

	have_gipfel_info = true;
	direction = md->direction();
	if (isnan(direction)) {
		set_center_angle(0.0);
		have_gipfel_info = false;
	} else {
		set_center_angle(direction);
	}

	nick = md->nick();
	if (isnan(nick)) {
		set_nick_angle(0.0);
		have_gipfel_info = false;
	} else {
		set_nick_angle(nick);
	}

	tilt = md->tilt();
	if (isnan(tilt)) {
		set_tilt_angle(0.0);
		have_gipfel_info = false;
	} else {
		set_tilt_angle(tilt);
	}

	fl = md->focal_length_35mm();
	if (isnan(fl) || fl == 0.0) {
		set_focal_length_35mm(35.0);
		have_gipfel_info = false;
	} else {
		set_focal_length_35mm(fl);
	}
	
	// try to get distortion parameters in the following ordering:
	// 1. gipfel data in JPEG comment
	// 2. matching distortion profile
	// 3. set the to 0.0, 0.0
	md->distortion_params(&pan->parms.k0, &pan->parms.k1, &pan->parms.x0);
	if (isnan(pan->parms.k0)) {
		char buf[1024];
		if (get_distortion_profile_name(buf, sizeof(buf)) == 0)
			load_distortion_params(buf);

		if (isnan(pan->parms.k0))
			pan->parms.k0 = 0.0;
		if (isnan(pan->parms.k1))
			pan->parms.k1 = 0.0;
		if (isnan(pan->parms.x0))
			pan->parms.x0 = 0.0;
	}

	return 0;
}

const char *
GipfelWidget::get_image_filename() {
	return img_file;
}

int
GipfelWidget::save_image(char *file) {
	if (img_file == NULL) {
		fprintf(stderr, "Nothing to save\n");
		return 1;
	}

	md->longitude(get_view_long());
	md->latitude(get_view_lat());
	md->height(get_view_height());
	md->direction(get_center_angle());
	md->nick(get_nick_angle());
	md->tilt(get_tilt_angle());
	md->focal_length_35mm(get_focal_length_35mm());
	md->projection_type((int) projection());
	md->distortion_params(pan->parms.k0, pan->parms.k1, pan->parms.x0);

	return  md->save_image(img_file, file);
}

int
GipfelWidget::load_data(const char *file) {
	int r;

	r = pan->load_data(file);
	set_labels(pan->get_visible_mountains());

	return r;
}

int
GipfelWidget::load_track(const char *file) {
	if (track_points) {
		pan->remove_hills(Hill::TRACK_POINT);
		track_points->clobber();
		delete track_points;
	}

	track_points = new Hills();

	if (track_points->load(file) != 0) {
		delete track_points;
		track_points = NULL;
		return 1;
	}

	for (int i = 0; i < track_points->get_num(); i++)
		track_points->get(i)->flags |= Hill::TRACK_POINT;

	pan->add_hills(track_points);  
	redraw();

	return 0;
}

int
GipfelWidget::set_viewpoint(const char *pos) {
	int r;

	r = pan->set_viewpoint(pos);
	set_labels(pan->get_visible_mountains());
	redraw();
	return r;
}

void
GipfelWidget::set_viewpoint(const Hill *m) {
	pan->set_viewpoint(m);
	set_labels(pan->get_visible_mountains());
	redraw();
}

static void
draw_flag(int x, int y) {
	fl_polygon(x, y - 10, x, y - 20, x + 10, y - 15);
	fl_yxline(x, y, y - 10);
	fl_circle(x , y, 3);
}

void 
GipfelWidget::draw() {
	Hills *mnts;
	Hill *m;
	int i, height;

	if (img == NULL)
		return;

	fl_push_clip(x(), y(), w(), h());
	img->draw(x(),y(),w(),h(),0,0);

	/* hills */
	fl_font(FL_HELVETICA, 8);
	mnts = pan->get_visible_mountains();

	fl_color(FL_YELLOW);
	height = fl_height();
	for (i=0; i<mnts->get_num(); i++) {
		m = mnts->get(i);
		int m_x = w() / 2 + x() + (int) rint(m->x);
		int m_y = h() / 2 + y() + (int) rint(m->y);

		if ((m->flags & (Hill::DUPLIC|Hill::TRACK_POINT)) ||
			(!show_hidden && (m->flags & Hill::HIDDEN)) ||
			known_hills->contains(m) || m == focused_mountain)
			continue;

		fl_xyline(m_x - CROSS_SIZE, m_y, m_x + CROSS_SIZE);
		fl_yxline(m_x, m_y + m->label_y - height, m_y + CROSS_SIZE);
		fl_xyline(m_x, m_y + m->label_y - height, m_x + m->label_x);
	}

	for (i=0; i<mnts->get_num(); i++) {
		m = mnts->get(i);
		int m_x = w() / 2 + x() + (int) rint(m->x);
		int m_y = h() / 2 + y() + (int) rint(m->y);

		if ((m->flags & (Hill::DUPLIC|Hill::TRACK_POINT)) ||
			(!show_hidden && (m->flags & Hill::HIDDEN)) || m == focused_mountain)
			continue;

		if (known_hills->contains(m)) {
			if (known_hills->get_num() > 3)
				fl_color(FL_GREEN);
			else
				fl_color(FL_RED);

			draw_flag(m_x, m_y);
			fl_color(FL_BLACK);
		} else if (m->flags & Hill::HIDDEN) {
			fl_color(FL_BLUE);
		} else {
			fl_color(FL_BLACK);
		}

		fl_draw(m->name, m_x + 2, m_y + m->label_y);
	}

	if (focused_mountain) {
		m = focused_mountain;
		int m_x = w() / 2 + x() + (int) rint(m->x);
		int m_y = h() / 2 + y() + (int) rint(m->y);

		fl_color(FL_YELLOW);

		fl_rectf(m_x, m_y - height,
			(int) fl_width(focused_mountain_label) + 2, height + 2);
		fl_color(FL_BLACK);
  		fl_draw(focused_mountain_label, m_x, m_y);
	}
	

	/* track */
	if (track_points && track_points->get_num() > 0) {
		int last_x = 0, last_y = 0, last_initialized = 0;

		for (i=1; i<track_points->get_num(); i++) {
			m = track_points->get(i);
			int m_x = w() / 2 + x() + (int) rint(m->x);
			int m_y = h() / 2 + y() + (int) rint(m->y);

			if (!(track_points->get(i)->flags & Hill::VISIBLE))
				continue;

			if (track_points->get(i)->flags & Hill::HIDDEN)
				fl_color(FL_BLUE);
			else
				fl_color(FL_RED);

			fl_line_style(FL_SOLID | FL_CAP_ROUND | FL_JOIN_ROUND,
				get_rel_track_width(track_points->get(i)));
			if (last_initialized) {
				fl_begin_line();
				fl_vertex(last_x, last_y);
				fl_vertex(m_x, m_y);
				fl_end_line();
			}

			last_x = m_x;
			last_y = m_y;
			last_initialized++;
		}
		fl_line_style(0);
	}

	fl_pop_clip();
}

static int 
overlap(double x1, double l1, double x2, double l2) {
	return x1 <= x2 + l2 && x1 + l1 >= x2;
}

void 
GipfelWidget::set_labels(Hills *v) {
	int height;

	fl_font(FL_HELVETICA, 8);
	height = fl_height();

	for (int i = 0; i < v->get_num(); i++) {
		Hill *m = v->get(i);
		Hills colliding;

		if (m->flags & (Hill::DUPLIC | Hill::TRACK_POINT))
			continue;

		if (!show_hidden && (m->flags & Hill::HIDDEN))
			continue;

		m->label_x = (int) fl_width(m->name) + 1;
		m->label_y = 0;

		for (int j = i - 1; j >= 0; j--) {
			Hill *n = v->get(j);

			if (n->flags & (Hill::DUPLIC | Hill::TRACK_POINT))
				continue;

			if (!show_hidden && (n->flags & Hill::HIDDEN))
				continue;

			if (overlap(m->x, m->label_x, n->x - CROSS_SIZE, n->label_x))
				colliding.add(n);
			else
				break;
		}

		colliding.sort(Hills::SORT_LABEL_Y);

		for (int j = 0; j < colliding.get_num(); j++) {
			Hill *n = colliding.get(j);

			// Check for overlapping labels and
			// overlaps between labels and peak markers
			if (overlap(m->y + m->label_y - height, height,
					n->y + n->label_y - height, height) ||
				overlap(m->y + m->label_y - height,
					height, n->y - CROSS_SIZE, 2 * CROSS_SIZE)) {

				m->label_y = (int) rint(n->y + n->label_y - m->y - height - 2);
			}
		}
	}
}

Hill *
GipfelWidget::find_mountain(Hills *mnts, int m_x, int m_y) {
    Hill *m;
    int center_x = w() / 2;
    int center_y = h() / 2;

    for (int i = 0; i < mnts->get_num(); i++) {
        m = mnts->get(i);

        if (m_x - center_x >= m->x - 2 && m_x - center_x < m->x + 2 &&
            m_y - center_y >= m->y - 2 && m_y - center_y < m->y + 2)
			return m;
	}

	return NULL;
}

int
GipfelWidget::toggle_known_mountain(int m_x, int m_y) {
	Hills *mnts = pan->get_visible_mountains();
	int center_x = w() / 2;
	int center_y = h() / 2;

	for (int i = 0; i < mnts->get_num(); i++) {
		Hill *m = mnts->get(i); 

		if (m->flags & (Hill::DUPLIC | Hill::TRACK_POINT))
			continue;

		if (m_x - center_x >= m->x - 2 && m_x - center_x < m->x + 2 &&
			m_y - center_y >= m->y - 2 && m_y - center_y < m->y + 2) {


			if (known_hills->contains(m))
				known_hills->remove(m);
			else
				known_hills->add(m);

			redraw();
			return 0;
		}
	}

	cur_mountain = NULL;
	redraw();
	return 1;
}

int
GipfelWidget::set_mountain(int m_x, int m_y) {
	int old_x, old_y, old_label_y;
	int center_x = w() / 2;
	int center_y = h() / 2;

	if (!cur_mountain)
		return 1;

	old_x = (int) rint(cur_mountain->x);
	old_y = (int) rint(cur_mountain->y);
	old_label_y = cur_mountain->label_y;

	cur_mountain->x = m_x - center_x;
	cur_mountain->y = m_y - center_y;
	cur_mountain->label_y = 0;

	damage(4, center_x + x() + old_x - 2*CROSS_SIZE - 1,
		center_y + y() + old_y + old_label_y - 2*CROSS_SIZE - 20,
		std::max(20, cur_mountain->label_x) + 2*CROSS_SIZE + 4,
		std::max(20, old_label_y) + 22 ); 
	damage(4,
		(int) rint(center_x + x() + cur_mountain->x - 2*CROSS_SIZE - 1),
		(int) rint(center_y + y() + cur_mountain->y + cur_mountain->label_y - 2*CROSS_SIZE - 20),
		std::max(20, cur_mountain->label_x) + 2*CROSS_SIZE + 4,
		std::max(20, cur_mountain->label_y) + 22 ); 

	return 0;
}

void
GipfelWidget::set_center_angle(double a) {
	pan->set_center_angle(a);
	set_labels(pan->get_visible_mountains());
	redraw();
}

void
GipfelWidget::set_nick_angle(double a) {
	pan->set_nick_angle(a);
	set_labels(pan->get_visible_mountains());
	redraw();
}

void
GipfelWidget::set_tilt_angle(double a) {
	pan->set_tilt_angle(a);
	set_labels(pan->get_visible_mountains());
	redraw();
}

void
GipfelWidget::set_focal_length_35mm(double s) {
	pan->set_scale(s * (double) img->w() / 35.0);
	set_labels(pan->get_visible_mountains());
	redraw();
}

void
GipfelWidget::projection(ProjectionLSQ::Projection_t p) {
	pan->set_projection(p);
	set_labels(pan->get_visible_mountains());
	redraw();
}

void
GipfelWidget::set_distortion_params(double k0, double k1, double x0) {
	pan->set_distortion_params(k0, k1, x0);
	redraw();
}

double
GipfelWidget::get_focal_length_35mm() {
	if (img == NULL)
		return NAN;
	else
		return pan->get_scale() * 35.0 / (double) img->w();
}

void 
GipfelWidget::find_peak_cb(Fl_Widget *, void *f) {
	GipfelWidget *g = (GipfelWidget*) f;

	Hill *m = choose_hill(g->pan->get_close_mountains(), "Find Peak");
	if (m) {
		if (!g->known_hills->contains(m))
			g->known_hills->add(m);

		m->flags |= Hill::VISIBLE;
		if (! g->pan->get_visible_mountains()->contains(m))
			g->pan->get_visible_mountains()->add(m);
		g->set_labels(g->pan->get_visible_mountains());

		g->cur_mountain = m;
		g->set_mountain(g->mouse_x, g->mouse_y);
	}
}

void
GipfelWidget::set_height_dist_ratio(double r) {
	pan->set_height_dist_ratio(r);
	set_labels(pan->get_visible_mountains());
	redraw();
}

void
GipfelWidget::set_hide_value(double h) {
	pan->set_hide_value(h);
	set_labels(pan->get_visible_mountains());
	redraw();
}

void
GipfelWidget::set_show_hidden(bool h) {
	show_hidden = h;
	set_labels(pan->get_visible_mountains());
	redraw();
}

void
GipfelWidget::set_view_lat(double v) {
	pan->set_view_lat(v);
	set_labels(pan->get_visible_mountains());
	redraw();
}

void
GipfelWidget::set_view_long(double v) {
	pan->set_view_long(v);
	set_labels(pan->get_visible_mountains());
	redraw();
}

void
GipfelWidget::set_view_height(double v) {
	pan->set_view_height(v);
	set_labels(pan->get_visible_mountains());
	redraw();
}

int
GipfelWidget::comp_params() {
	int ret;

	fl_cursor(FL_CURSOR_WAIT);
	ret = pan->comp_params(known_hills);
	set_labels(pan->get_visible_mountains());
	redraw();
	fl_cursor(FL_CURSOR_DEFAULT);

	return ret;
}

int
GipfelWidget::get_rel_track_width(Hill *m) {
	double dist = pan->get_real_distance(m);

	return (int) rint(std::max((pan->get_scale()*track_width)/(dist*10.0), 1.0));
}

void
GipfelWidget::set_track_width(double w) {
	track_width = w;
	redraw();
}

int
GipfelWidget::handle(int event) {
	Hill *m;

	switch(event) {
		case FL_PUSH:    
			mouse_x = Fl::event_x()-x();
			mouse_y = Fl::event_y()-y();
			if (Fl::event_button() == FL_LEFT_MOUSE) {
				m = find_mountain(known_hills, mouse_x, mouse_y);
				if (m)
					cur_mountain = m;
			} else if (Fl::event_button() == FL_MIDDLE_MOUSE) {
				toggle_known_mountain(mouse_x, mouse_y);
				if (focused_mountain) {
					focused_mountain = NULL;
					redraw();
				}
			} else if (Fl::event_button() == FL_RIGHT_MOUSE) {
				Fl_Menu_Item rclick_menu[] = {
					{"Find Peak", 0, find_peak_cb,  this},
					{0}
				};
				const Fl_Menu_Item *mi =
					rclick_menu->popup(Fl::event_x(), Fl::event_y(), 0, 0, 0);

				if (mi)
					mi->do_callback(0, mi->user_data());
			}

			Fl::focus(this);
			return 1;
		case FL_DRAG:
			set_mountain(Fl::event_x()-x(), Fl::event_y()-y());
			return 1;
		case FL_RELEASE:
			cur_mountain = NULL;
			return 1;
		case FL_ENTER:
			return 1;
		case FL_LEAVE:
			return 1;
		case FL_MOVE:
			m = find_mountain(pan->get_visible_mountains(), Fl::event_x()-x(), Fl::event_y()-y());
			if (m != focused_mountain && (!m || !known_hills->contains(m))) {
				if (m)
					snprintf(focused_mountain_label,
						sizeof(focused_mountain_label) - 1,
						"%s (%dm), distance %.2fkm",
						m->name, (int) m->height, pan->get_real_distance(m) / 1000.0);

				focused_mountain = m;
				redraw();
			}
			return 1;
		case FL_FOCUS:
			return 1;
		case FL_UNFOCUS:
			return 0;
	}
	return 0;
}

int
GipfelWidget::export_hills(const char *file, FILE *fp) {
	Hills export_hills, *mnts;

	if (!have_gipfel_info) {
		fprintf(stderr, "No gipfel info available for %s.\n", img_file);
		return 0;
	}

	if (file) {
		if (export_hills.load(file) != 0)
			return 1;

		for (int i = 0; i < export_hills.get_num(); i++)
			export_hills.get(i)->flags |= Hill::EXPORT;

		pan->add_hills(&export_hills);  
	}

	fprintf(fp, "#\n# name\theight\tx\ty\tdistance\tflags\n#\n");

	mnts = pan->get_visible_mountains();
	for (int i = 0; i < mnts->get_num(); i++) {
		Hill *m = mnts->get(i);
		int _x = (int) rint(m->x) + w() / 2;
		int _y = (int) rint(m->y) + h() / 2;

		if (m->flags & Hill::DUPLIC || m->flags & Hill::HIDDEN ||
			file && !(m->flags & Hill::EXPORT)) {
			continue;
		}

		if (_x < 0 || _x > w() || _y < 0 || _y > h())
			continue;

		fprintf(fp, "%s\t%d\t%d\t%d\t%d\n",
			m->name, (int) rint(m->height), _x, _y,
			(int) rint(pan->get_real_distance(m)));
	}

	pan->remove_hills(Hill::EXPORT);
	
	return 0;
}

int
GipfelWidget::get_pixel(GipfelWidget::sample_mode_t m,
	double a_alph, double a_nick, int *r, int *g, int *b) {
	double px, py;
	int ret;

	if (img == NULL)
		return 1;

	if (pan->get_coordinates(a_alph, a_nick, &px, &py) != 0)
		return 1;

	if (m == GipfelWidget::BICUBIC)
		ret = get_pixel_bicubic(img, px + ((double) img->w()) / 2.0,
			py + ((double) img->h()) / 2.0, r, g, b);
	else 
		ret = get_pixel_nearest(img, px + ((double) img->w()) / 2.0,
			py + ((double) img->h()) / 2.0, r, g, b);

	return ret;
}

int
GipfelWidget::get_pixel_nearest(Fl_Image *img, double x, double y,
	int *r, int *g, int *b) {

	if (isnan(x) || isnan(y))
		return 1;
	else
		return get_pixel(img, (int) rint(x), (int) rint(y), r, g, b);
}

static inline double
interp_cubic(double x, double x2, double x3, double *v) {
	double a0, a1, a2, a3;

	a0 = v[3] - v[2] - v[0] + v[1];
	a1 = v[0] - v[1] - a0;
	a2 = v[2] - v[0];
	a3 = v[1];

	return a0 * x3 + a1 * x2 + a2 * x + a3;
}

int
GipfelWidget::get_pixel_bicubic(Fl_Image *img, double x, double y,
	int *r, int *g, int *b) {

	double fl_x = floor(x);
	double fl_y = floor(y);
	double dx = x - fl_x, dx2 = dx * dx, dx3 = dx2 * dx;
	double dy = y - fl_y, dy2 = dy * dy, dy3 = dy2 * dy;
	int ic[3];
	double c[3][4];
	double c1[3][4];

	for (int iy = 0; iy < 4; iy++) {
		for (int ix = 0; ix < 4; ix++) {

			if (get_pixel(img, (int) fl_x + ix - 1, (int) fl_y + iy - 1,
					&ic[0], &ic[1], &ic[2]) != 0)
				return 1;

			for (int l = 0; l < 3; l++)
				c[l][ix] = (double) ic[l];
		}

		for (int l = 0; l < 3; l++)
			c1[l][iy] = interp_cubic(dx, dx2, dx3, c[l]);
	}

	*r = (int) rint(interp_cubic(dy, dy2, dy3, c1[0]));
	*g = (int) rint(interp_cubic(dy, dy2, dy3, c1[1]));
	*b = (int) rint(interp_cubic(dy, dy2, dy3, c1[2]));
	return 0;
}

int
GipfelWidget::get_pixel(Fl_Image *img, int x, int y,
                     int *r, int *g, int *b) {
	if ( img->d() == 0 )
		return 1;

	if (x < 0 || x >=img->w() || y < 0 || y >= img->h())
		return 1;

	long index = (y * img->w() * img->d()) + (x * img->d()); // X/Y -> buf index  
	switch (img->count()) {
		case 1:
		{                                            // bitmap
			const unsigned char *buf = (const unsigned char*) img->data()[0];
			switch (img->d())
			{
				case 1:
					*r = *g = *b = *(buf+index);
					break;
				case 3:                              // 24bit
					*r = (int) *(buf+index+0);
					*g = (int) *(buf+index+1);
					*b = (int) *(buf+index+2);
					break;
				default:                             // ??
					printf("Not supported: chans=%d\n", img->d());
					return 1;
			}
			break;
		}
		default:                                             // ?? pixmap, bit vals
		printf("Not supported: count=%d\n", img->count());
		return 1;
	}

	*r = *r * 255;
	*g = *g * 255;
	*b = *b * 255;

	return 0;
}

int
GipfelWidget::get_distortion_profile_name(char *buf, int buflen) {
	int n;
	
	if (md && md->manufacturer() && md->model()) {
		n = snprintf(buf, buflen, "%s_%s_%.2f_mm",
			md->manufacturer(), md->model(), md->focal_length());

		return n > buflen;
	} else {
		return 1;
	}
}

static Fl_Preferences dist_prefs(Fl_Preferences::USER,
	"Johannes.HofmannATgmx.de", "gipfel/DistortionProfiles");

int
GipfelWidget::load_distortion_params(const char *prof_name) {
	int ret = 0;

	Fl_Preferences prof(dist_prefs, prof_name);
	ret += prof.get("k0", pan->parms.k0, pan->parms.k0);
	ret += prof.get("k1", pan->parms.k1, pan->parms.k1);
	ret += prof.get("x0", pan->parms.x0, pan->parms.x0);

	return !ret;
}

int
GipfelWidget::save_distortion_params(const char *prof_name, int force) {
	Fl_Preferences prof(dist_prefs, prof_name);

	if (!force && prof.entryExists("k0"))
		return 1;

	prof.set("k0", pan->parms.k0);
	prof.set("k1", pan->parms.k1);
	prof.set("x0", pan->parms.x0);

	return 0;
}
