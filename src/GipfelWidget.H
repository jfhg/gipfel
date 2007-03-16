//
// Copyright 2006 Johannes Hofmann <Johannes.Hofmann@gmx.de>
//
// This software may be used and distributed according to the terms
// of the GNU General Public License, incorporated herein by reference.

#ifndef GipfelWidget_H
#define GipfelWidget_H

#include <FL/Fl_Widget.H>
#include <FL/Fl_Menu_Button.H>
#include "Panorama.H"
#include "ImageMetaData.H"

class GipfelWidget : public Fl_Widget {
	private:
		Fl_Image *img;
		Hill *cur_mountain;
		Hills *track_points;
		Hills *known_hills;
		Panorama *pan;
		Fl_Menu_Button *mb;
		char *img_file;
		double track_width;
		ImageMetaData *md;

		int show_hidden;

		int handle(int event);

		int set_cur_mountain(int m_x, int m_y);

		int toggle_known_mountain(int m_x, int m_y);

		int set_mountain(int m_x, int m_y);

		void set_labels(Hills *v);

		int get_rel_track_width(Hill *m);

		static int get_pixel_nearest(Fl_Image *img, double x, double y,
			int *r, int *g, int *b);

		static int get_pixel_bilinear(Fl_Image *img, double x, double y,
			int *r, int *g, int *b);

		static int get_pixel(Fl_Image *img, int x, int y,
			int *r, int *g, int *b);

	public:
		typedef enum {
			NEAREST  = 0,
			BILINEAR = 1
		} sample_mode_t;
	
		GipfelWidget(int X,int Y,int W, int H);

		void center();

		int load_image(char *file);

		int save_image(char *file);

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

		const char * get_viewpoint();

		double get_center_angle();

		double get_nick_angle();

		double get_tilt_angle();

		double get_focal_length_35mm();

		double get_height_dist_ratio();

		double get_view_lat();

		double get_view_long();

		double get_view_height();

		void set_track_width(double w);

		ProjectionLSQ::Projection_t get_projection();

		void set_projection(ProjectionLSQ::Projection_t p);

		void get_distortion_params(double *k0, double *k1i, double *x0);

		void set_distortion_params(double k0, double k1, double x0);

		Hills *get_mountains();

		int comp_params();

		int get_pixel(GipfelWidget::sample_mode_t,
			double a_alph, double a_nick, int *r, int *g, int *b);

		double get_angle_off(double view, double nick);

		int get_distortion_profile_name(char *buf, int buflen);

		int save_distortion_params(const char *prof_name);

		int load_distortion_params(const char *prof_name);

		void draw();
};
#endif
