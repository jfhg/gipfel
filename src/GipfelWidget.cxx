//
// Copyright 2006 Johannes Hofmann <Johannes.Hofmann@gmx.de>
//
// This software may be used and distributed according to the terms
// of the GNU General Public License, incorporated herein by reference.

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>

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


#define MAX(A,B) ((A)>(B)?(A):(B))

static double pi_d, deg2rad;

static void center_cb(Fl_Widget *o, void *f);

GipfelWidget::GipfelWidget(int X,int Y,int W, int H): Fl_Widget(X, Y, W, H) {
	int i;

	pi_d = asin(1.0) * 2.0;
	deg2rad = pi_d / 180.0;
	img = NULL;
	pan = new Panorama();
	cur_mountain = NULL;
	mb = NULL;
	known_hills = new Hills();
	img_file = NULL;
	track_width = 200.0;
	show_hidden = 0;
	md = new ImageMetaData();
	track_points = NULL;
	fl_register_images();
}

int
GipfelWidget::load_image(char *file) {
	Fl_Image *new_img;

	new_img = new Fl_JPEG_Image(file);

	if (new_img == NULL) {
		return 1;
	}

	if (img) {
		delete img;
	} 

	img = new_img;

	if (img_file) {
		free(img_file);
	}

	img_file = strdup(file);

	h(img->h());  
	w(img->w());  

	mb = new Fl_Menu_Button(x(),y(),w()+x(),h()+y(),"&popup");
	mb->type(Fl_Menu_Button::POPUP3);
	mb->box(FL_NO_BOX);
	mb->add("Center Peak", 0, (Fl_Callback*) center_cb, this);

	// try to retrieve gipfel data from JPEG meta data
	md->load_image(file, img->w());
	set_view_long(md->get_longitude());
	set_view_lat(md->get_latitude());
	set_view_height(md->get_height());
	set_center_angle(md->get_direction());
	set_nick_angle(md->get_nick());
	set_tilt_angle(md->get_tilt());
	set_projection((ProjectionLSQ::Projection_t) md->get_projection_type());
	set_focal_length_35mm(md->get_focal_length_35mm());
	
	// try to get distortion parameters in the following ordering:
	// 1. gipfel data in JPEG comment
	// 2. matching distortion profile
	// 3. set the to 0.0, 0.0
	md->get_distortion_params(&pan->parms.k0, &pan->parms.k1, &pan->parms.x0);
	if (isnan(pan->parms.k0)) {
		char buf[1024];
		get_distortion_profile_name(buf, sizeof(buf));
		load_distortion_params(buf);
		if (isnan(pan->parms.k0)) {
			pan->parms.k0 = 0.0;
			pan->parms.k1 = 0.0;
		}
	}

	return 0;
}

const char *
GipfelWidget::get_image_filename() {
	return img_file;
}

int
GipfelWidget::save_image(char *file) {
	ImageMetaData *md;
	int ret;

	if (img_file == NULL) {
		fprintf(stderr, "Nothing to save\n");
		return 1;
	}

	md = new ImageMetaData();

	md->set_longitude(get_view_long());
	md->set_latitude(get_view_lat());
	md->set_height(get_view_height());
	md->set_direction(get_center_angle());
	md->set_nick(get_nick_angle());
	md->set_tilt(get_tilt_angle());
	md->set_focal_length_35mm(get_focal_length_35mm());
	md->set_projection_type((int) get_projection());
	md->set_distortion_params(pan->parms.k0, pan->parms.k1, pan->parms.x0);

	ret = md->save_image(img_file, file);
	delete md;

	return ret;
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
		pan->remove_trackpoints();
		track_points->clobber();
		delete track_points;
	}

	track_points = new Hills();

	if (track_points->load(file) != 0) {
		delete track_points;
		track_points = NULL;
		return 1;
	}

	for (int i=0; i<track_points->get_num(); i++) {
		track_points->get(i)->flags |= Hill::TRACK_POINT;
	}

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
draw_flag(int x, int y, char *s) {
	Fl_Color c = fl_color();

	fl_polygon(x, y - 10, x, y - 20, x + 10, y - 15);
	fl_yxline(x, y, y - 10);


	if (s) {
		fl_color(FL_WHITE);
		fl_draw(s, x , y - 12);
		fl_color(c);
	}	
}

void 
GipfelWidget::draw() {
	Hills *mnts;
	Hill *m;
	int i;

	if (img == NULL) {
		return;
	}

	fl_push_clip(x(), y(), w(), h());
	img->draw(x(),y(),w(),h(),0,0);

	/* hills */
	fl_font(FL_HELVETICA, 10);
	mnts = pan->get_visible_mountains();
	for (i=0; i<mnts->get_num(); i++) {
		m = mnts->get(i);
		int m_x = w() / 2 + x() + (int) rint(m->x);
		int m_y = h() / 2 + y() + (int) rint(m->y);

		if (m->flags & (Hill::DUPLICATE|Hill::TRACK_POINT)) {
			continue;
		}

		if (!show_hidden && (m->flags & Hill::HIDDEN)) {
			continue;
		}

		if (known_hills->contains(m)) {
			if (known_hills->get_num() > 3) {
				fl_color(FL_GREEN);
			} else {
				fl_color(FL_RED);
			}
			draw_flag(m_x, m_y, "");
		} else if (m->flags & Hill::HIDDEN) {
			fl_color(FL_BLUE);
		} else {
			fl_color(FL_BLACK);
		}

		fl_xyline(m_x- CROSS_SIZE, m_y, m_x + CROSS_SIZE);
		fl_yxline(m_x, m_y + m->label_y - CROSS_SIZE, m_y + CROSS_SIZE);

		fl_draw(m->name, m_x , m_y + m->label_y);
	}

	/* track */
	if (track_points && track_points->get_num() > 0) {
		int last_x = 0, last_y = 0, last_initialized = 0;

		for (i=1; i<track_points->get_num(); i++) {
			m = track_points->get(i);
			int m_x = w() / 2 + x() + (int) rint(m->x);
			int m_y = h() / 2 + y() + (int) rint(m->y);
			if (!(track_points->get(i)->flags & Hill::VISIBLE)) {
				continue;
			}

			if (track_points->get(i)->flags & Hill::HIDDEN) {
				fl_color(FL_BLUE);
			} else {
				fl_color(FL_RED);
			}

			fl_line_style(FL_SOLID|FL_CAP_ROUND|FL_JOIN_ROUND,
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
overlap(double m1, double n1, double m2, double n2) {
	return m1 <= n2 && n1 >= m2; 
}

void 
GipfelWidget::set_labels(Hills *v) {
	int i, j, width, height;
	Hill *m, *n;

	fl_font(FL_HELVETICA, 10);
	height = fl_height();

	for (i=0; i<v->get_num(); i++) {
		m = v->get(i);

		if (m->flags & (Hill::DUPLICATE|Hill::TRACK_POINT)) {
			continue;
		}

		if (!show_hidden && (m->flags & Hill::HIDDEN)) {
			continue;
		}

		width = (int) ceilf(fl_width(m->name));
		m->label_x = width;
		m->label_y = 0;
		for (j=0; j < i; j++) {
			n = v->get(j);

			if (n->flags & (Hill::DUPLICATE | Hill::TRACK_POINT)) {
				continue;
			}

			if (!show_hidden && (n->flags & Hill::HIDDEN)) {
				continue;
			}

			// Check for overlapping labels and
			// overlaps between labels and peak markers
			if ((overlap(m->x, m->x + m->label_x, n->x, n->x + n->label_x) &&
					overlap(m->y + m->label_y - height, m->y + m->label_y, n->y + n->label_y - height, n->y + n->label_y)) ||
				(overlap(m->x, m->x + m->label_x, n->x - 2, n->x + 2) &&
				 overlap(m->y + m->label_y - height, m->y + m->label_y, n->y - 2, n->y + 2))) {
				m->label_y = (int) rint(n->y + n->label_y - m->y - height - 1);
			}
		}
	}
}

int
GipfelWidget::set_cur_mountain(int m_x, int m_y) {
    Hill *m;
    int center_x = w() / 2;
    int center_y = h() / 2;
    int i;

    for (i=0; i<known_hills->get_num(); i++) {
        m = known_hills->get(i);

        if (m_x - center_x >= m->x - 2 && m_x - center_x < m->x + 2 &&
            m_y - center_y >= m->y - 2 && m_y - center_y < m->y + 2) {

			cur_mountain = m;
			return 0;
		}
	}

	return 1;
}


int
GipfelWidget::toggle_known_mountain(int m_x, int m_y) {
	Hills *mnts = pan->get_visible_mountains();
	Hill *m;
	int center_x = w() / 2;
	int center_y = h() / 2;
	int i;

	for (i=0; i<mnts->get_num(); i++) {
		m = mnts->get(i); 
		if (m->flags & (Hill::DUPLICATE | Hill::TRACK_POINT)) {
			continue;
		}

		if (m_x - center_x >= m->x - 2 && m_x - center_x < m->x + 2 &&
			m_y - center_y >= m->y - 2 && m_y - center_y < m->y + 2) {


			if (known_hills->contains(m)) {
				known_hills->remove(m);
			} else {
				known_hills->add(m);
			}

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

	if (cur_mountain == NULL) {
		return 1;
	}

	old_x = (int) rint(cur_mountain->x);
	old_y = (int) rint(cur_mountain->y);
	old_label_y = cur_mountain->label_y;

	cur_mountain->x = m_x - center_x;
	cur_mountain->y = m_y - center_y;
	cur_mountain->label_y = 0;

	damage(4, center_x + x() + old_x - 2*CROSS_SIZE - 1,
		center_y + y() + old_y + old_label_y - 2*CROSS_SIZE - 20,
		MAX(20, cur_mountain->label_x) + 2*CROSS_SIZE + 2,
		MAX(20, old_label_y) + 22 ); 
	damage(4,
		(int) rint(center_x + x() + cur_mountain->x - 2*CROSS_SIZE - 1),
		(int) rint(center_y + y() + cur_mountain->y + cur_mountain->label_y - 2*CROSS_SIZE - 20),
		MAX(20, cur_mountain->label_x) + 2*CROSS_SIZE + 2,
		MAX(20, cur_mountain->label_y) + 22 ); 

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
GipfelWidget::set_projection(ProjectionLSQ::Projection_t p) {
	pan->set_projection(p);
	set_labels(pan->get_visible_mountains());
	redraw();
}

void
GipfelWidget::get_distortion_params(double *k0, double *k1, double *x0) {
	pan->get_distortion_params(k0, k1, x0);
}

void
GipfelWidget::set_distortion_params(double k0, double k1, double x0) {
	pan->set_distortion_params(k0, k1, x0);
	redraw();
}

ProjectionLSQ::Projection_t
GipfelWidget::get_projection() {
	return pan->get_projection();
}

double
GipfelWidget::get_center_angle() {
	return pan->get_center_angle();
}

double
GipfelWidget::get_nick_angle() {
	return pan->get_nick_angle();
}

double
GipfelWidget::get_tilt_angle() {
	return pan->get_tilt_angle();
}

double
GipfelWidget::get_focal_length_35mm() {
	if (img == NULL) {
		return NAN;
	} else {
		return pan->get_scale() * 35.0 / (double) img->w();
	}
}

double
GipfelWidget::get_height_dist_ratio() {
	return pan->get_height_dist_ratio();
}

const char *
GipfelWidget::get_viewpoint() {
	return pan->get_viewpoint();
}

double
GipfelWidget::get_view_lat() {
	return pan->get_view_lat();
}

double
GipfelWidget::get_view_long() {
	return pan->get_view_long();
}

double
GipfelWidget::get_view_height() {
	return pan->get_view_height();
}

void 
GipfelWidget::center() {
	Hill *m = choose_hill(pan->get_close_mountains(), "Center Peak");
	if (m) {
		set_center_angle(m->alph / deg2rad);
			
		if (!known_hills->contains(m)) {
			known_hills->add(m);
		}
	
	}
}

static void
center_cb(Fl_Widget *o, void *f) {
	((GipfelWidget*)f)->center();
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
GipfelWidget::set_show_hidden(int h) {
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

Hills*
GipfelWidget::get_mountains() {
	return pan->get_mountains();
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

	return (int) rint(MAX((pan->get_scale()*track_width)/(dist*10.0), 1.0));
}

void
GipfelWidget::set_track_width(double w) {
	track_width = w;
	redraw();
}


int
GipfelWidget::handle(int event) {
	int mark_x, mark_y;

	switch(event) {
		case FL_PUSH:    
			mark_x = Fl::event_x()-x();
			mark_y = Fl::event_y()-y();
			if (Fl::event_button() == 1) {
				set_cur_mountain(mark_x, mark_y);
			} else if (Fl::event_button() == 2) {
				toggle_known_mountain(mark_x, mark_y);
			}

			Fl::focus(this);
			return 1;
			break;
		case FL_DRAG:
			set_mountain(Fl::event_x()-x(), Fl::event_y()-y());
			return 1;
			break;
		case FL_RELEASE:
			cur_mountain = NULL;
			return 1;
			break;
		case FL_FOCUS:
			return 1;
			break;
		case FL_UNFOCUS:
			return 0;
			break;
	}
	return 0;
}

int
GipfelWidget::get_pixel(GipfelWidget::sample_mode_t m,
	double a_alph, double a_nick, uchar *r, uchar *g, uchar *b) {
	double px, py;

	if (img == NULL) {
		return 1;
	}

	if (pan->get_coordinates(a_alph, a_nick, &px, &py) != 0) {
		return 1;
	}

	if (m == GipfelWidget::BILINEAR) {
		return get_pixel_bilinear(img, px + ((double) img->w()) / 2.0,
			py + ((double) img->h()) / 2.0, r, g, b);
	} else {
		return get_pixel_nearest(img, px + ((double) img->w()) / 2.0,
			py + ((double) img->h()) / 2.0, r, g, b);
	}
}

int
GipfelWidget::get_pixel_nearest(Fl_Image *img, double x, double y,
	uchar *r, uchar *g, uchar *b) {
	if (isnan(x) || isnan(y)) {
		return 1;
	} else {
		return get_pixel(img, (int) rint(x), (int) rint(y), r, g, b);
	}
}

int
GipfelWidget::get_pixel_bilinear(Fl_Image *img, double x, double y,
	uchar *r, uchar *g, uchar *b) {
	uchar a_r[4] = {0, 0, 0, 0};
	uchar a_g[4] = {0, 0, 0, 0};
	uchar a_b[4] = {0, 0, 0, 0};
	float v0 , v1;
	int fl_x = (int) floor(x);
	int fl_y = (int) floor(y);
	int i, n;

	i = 0;
	n = 0;
	for (int iy = 0; iy <= 1; iy++) {
		for (int ix = 0; ix <= 1; ix++) {
			n += get_pixel(img, fl_x + ix, fl_y + iy, &(a_r[i]), &(a_g[i]), &(a_b[i]));
			i++;
		}
	}

	v0 = a_r[0] * (1 - (x - fl_x)) + a_r[1] * (x - fl_x);
	v1 = a_r[2] * (1 - (x - fl_x)) + a_r[3] * (x - fl_x);
	*r = (uchar) rint(v0 * (1 - (y - fl_y)) + v1 * (y - fl_y));

	v0 = a_g[0] * (1 - (x - fl_x)) + a_g[1] * (x - fl_x);
	v1 = a_g[2] * (1 - (x - fl_x)) + a_g[3] * (x - fl_x);
	*g = (uchar) rint(v0 * (1 - (y - fl_y)) + v1 * (y - fl_y));

	v0 = a_b[0] * (1 - (x - fl_x)) + a_b[1] * (x - fl_x);
	v1 = a_b[2] * (1 - (x - fl_x)) + a_b[3] * (x - fl_x);
	*b = (uchar) rint(v0 * (1 - (y - fl_y)) + v1 * (y - fl_y));

	if (n >= 1) {
		return 1;
	} else {
		return 0;
	}
}

int
GipfelWidget::get_pixel(Fl_Image *img, int x, int y,
                     uchar *r, uchar *g, uchar *b) {
	if ( img->d() == 0 ) {
		return 1;
	}

	if (x < 0 || x >=img->w() || y < 0 || y >= img->h()) {
		return 1;
	}
	long index = (y * img->w() * img->d()) + (x * img->d()); // X/Y -> buf index  
	switch (img->count()) {
		case 1:
		{                                            // bitmap
			const char *buf = img->data()[0];
			switch (img->d())
			{
				case 1:
					*r = *g = *b = *(buf+index);
					break;
				case 3:                              // 24bit
					*r = *(buf+index+0);
					*g = *(buf+index+1);
					*b = *(buf+index+2);
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

	return 0;

}

int
GipfelWidget::get_distortion_profile_name(char *buf, int buflen) {
	int n;
	
	if (md && md->get_manufacturer() && md->get_model()) {
		n = snprintf(buf, buflen, "%s_%s_%.2f_mm",
			md->get_manufacturer(), md->get_model(), md->get_focal_length());

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
GipfelWidget::save_distortion_params(const char *prof_name) {

	Fl_Preferences prof(dist_prefs, prof_name);
	prof.set("k0", pan->parms.k0);
	prof.set("k1", pan->parms.k1);
	prof.set("x0", pan->parms.x0);

	return 0;
}
