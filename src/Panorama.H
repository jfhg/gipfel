//
// Copyright 2006 Johannes Hofmann <Johannes.Hofmann@gmx.de>
//
// This software may be used and distributed according to the terms
// of the GNU General Public License, incorporated herein by reference.

#ifndef PANORAMA_H
#define PANORAMA_H

#include "Hill.H"
#include "ProjectionLSQ.H"
#include "ViewParams.H"



class Panorama {
	private:
		double view_phi, view_lam, view_height;
		char *view_name;
		double height_dist_ratio;
		double hide_value;
		Hills *mountains;
		Hills *close_mountains;
		Hills *visible_mountains;
		ProjectionLSQ *proj;
		ProjectionLSQ::Projection_t projection_type;
		double pi_d, deg2rad;

		Hill * get_pos(const char *name);
		void update_angles();
		void update_coordinates();
		void update_close_mountains();
		void update_visible_mountains();
		void mark_hidden(Hills *hills);
		double distance(double phi, double lam);
		double alpha(const Hill *m);
		double nick(const Hill *m);
		double refraction_change(const Hill *m);
		double comp_center_angle(double alph_a, double alph_b, double d1, double d2);
		double comp_scale(double alph_a, double alph_b, double d1, double d2);
		int get_matrix(double m[]);
		int optimize(Hill *m1, Hill *m2);
		int is_visible(double a_alph);

	public:
		ViewParams parms;

		Panorama();
		~Panorama();
		int load_data(const char *name);
		void add_hills(Hills *h);
		void remove_hills(int flags);
		int set_viewpoint(const char *pos);  
		void set_viewpoint(const Hill *m);  
		void set_height_dist_ratio(double r);
		void set_hide_value(double h);
		Hills * get_mountains();
		Hills * get_close_mountains();
		Hills * get_visible_mountains();
		void set_center_angle(double a);
		void set_nick_angle(double a);
		void set_tilt_angle(double a);
		void set_scale(double s);
		void set_view_lat(double v);
		void set_view_long(double v);
		void set_view_height(double v);
		const char * get_viewpoint();  
		double get_center_angle();
		double get_nick_angle();
		double get_tilt_angle();
		double get_scale();
		double get_height_dist_ratio();
		double get_view_lat();
		double get_view_long();
		double get_view_height();
		double get_earth_radius(double latitude);
		double get_real_distance(const Hill *m);
		int comp_params(Hills *h);
		ProjectionLSQ::Projection_t get_projection();
		void set_projection(ProjectionLSQ::Projection_t p);
		void get_distortion_params(double *k0, double *k1, double *x0);
		void set_distortion_params(double k0, double k1, double x0);
		int get_coordinates(double a_alph, double a_nick, double *x, double *y);
};
#endif
