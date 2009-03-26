//
// Copyright 2006 Johannes Hofmann <Johannes.Hofmann@gmx.de>
//
// This software may be used and distributed according to the terms
// of the GNU General Public License, incorporated herein by reference.

#ifndef GipfelWidget_H
#define GipfelWidget_H

#include <FL/Fl_Group.H>
#include <FL/Fl_Menu_Button.H>
#include "Panorama.H"
#include "ImageMetaData.H"

class GipfelWidget : public Fl_Group {
	private:
		Fl_Image *img;
		Hill *cur_mountain, *focused_mountain;
		Hills *track_points;
		Hills *known_hills;
		Panorama *pan;
		char *img_file;
		double track_width;
		int have_gipfel_info;
		ImageMetaData *md;
		int show_hidden;
		int mouse_x, mouse_y;
		char focused_mountain_label[128];

		int handle(int event);
		Hill * find_mountain(Hills *mnts, int m_x, int m_y);
		int toggle_known_mountain(int m_x, int m_y);
		int set_mountain(int m_x, int m_y);
		void set_labels(Hills *v);
		int get_rel_track_width(Hill *m);

		static int get_pixel_nearest(Fl_Image *img, double x, double y,
			int *r, int *g, int *b);
		static int get_pixel_bicubic(Fl_Image *img, double x, double y,
			int *r, int *g, int *b);
		static int get_pixel(Fl_Image *img, int x, int y,
			int *r, int *g, int *b);
		static void find_peak_cb(Fl_Widget *o, void *f);

	public:
		typedef enum {
			NEAREST  = 0,
			BICUBIC  = 1
		} sample_mode_t;
	
		GipfelWidget(int X,int Y,int W, int H);

		int load_image(char *file);
		int save_image(char *file);
		int export_hills(const char *file, FILE *fp);
		const char * get_image_filename();
		int load_data(const char *file);
		int load_track(const char *file);
		int set_viewpoint(const char *pos);
		void set_viewpoint(const Hill *m);
		void set_center_angle(double a);
		void set_nick_angle(double a);
		void set_tilt_angle(double a);
		void set_focal_length_35mm(double s);
		void set_height_dist_ratio(double r);
		void set_hide_value(double h);
		void set_show_hidden(int h);
		void set_view_lat(double v);
		void set_view_long(double v);
		void set_view_height(double v);
		const char * get_viewpoint() { return pan->get_viewpoint(); };
		double get_center_angle() { return pan->get_center_angle(); };
		double get_nick_angle() { return pan->get_nick_angle(); };
		double get_tilt_angle() { return pan->get_tilt_angle(); };
		double get_focal_length_35mm();
		double get_height_dist_ratio() { return pan->get_height_dist_ratio(); };
		double get_view_lat() { return pan->get_view_lat(); };
		double get_view_long() { return pan->get_view_long(); };
		double get_view_height() { return pan->get_view_height(); };
		void set_track_width(double w);
		ProjectionLSQ::Projection_t projection() {
			return pan->get_projection();
		};
		void projection(ProjectionLSQ::Projection_t p);
		void get_distortion_params(double *k0, double *k1, double *x0) {
			pan->get_distortion_params(k0, k1, x0);
		};
		void set_distortion_params(double k0, double k1, double x0);
		Hills *get_mountains() { return pan->get_mountains(); };
		int comp_params();
		int get_pixel(GipfelWidget::sample_mode_t,
			double a_alph, double a_nick, int *r, int *g, int *b);
		int get_distortion_profile_name(char *buf, int buflen);
		int save_distortion_params(const char *prof_name, int force);
		int load_distortion_params(const char *prof_name);
		void draw();
};
#endif
