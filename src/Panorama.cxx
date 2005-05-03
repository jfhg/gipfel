// 
// "$Id: Panorama.cxx,v 1.23 2005/05/03 20:29:59 hofmann Exp $"
//
// PSEditWidget routines.
//
// Copyright 2004 by Johannes Hofmann
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
#include <stdlib.h>
#include <string.h>
#include <math.h>
extern "C" {
#include <ccmath.h>
}

#include "GipfelWidget.H"
#include "Panorama.H"

static int newton(double *tan_nick_view, 
		  double *tan_dir_view,
		  double *n_scale,
		  double tan_dir_m1,
		  double tan_nick_m1,
		  double tan_dir_m2,
		  double tan_nick_m2,
		  double d_m1_2, double d_m2_2, double d_m1_m2_2);

static double
comp_tilt(double tan_nick_view, double tan_dir_view, double n_scale,
	  double tan_nick_m, double tan_dir_m,
	  double x, double y);


#define EARTH_RADIUS 6371010.0

static double pi_d, deg2rad;

Panorama::Panorama() {
  m1 = NULL;
  m2 = NULL;
  mountains = new Mountains();
  visible_mountains = new Mountains();
  height_dist_ratio = 0.07;
  pi_d = asin(1.0) * 2.0;
  deg2rad = pi_d / 180.0;
  a_center = 0.0;
  a_nick = 0.0;
  a_tilt = 0.0;
  scale = 500.0;
}

Panorama::~Panorama() {
  visible_mountains->clear();
  mountains->clobber();
  delete(visible_mountains);
  delete(mountains);
}

int
Panorama::load_file(const char *name) {
  FILE *fp;
  char buf[4000];
  char *vals[10];
  char **ap, *bp;
  double phi, lam, height;
  Mountain *m;

  visible_mountains->clear();
  mountains->clobber();


  fp = fopen(name, "r");
  if (!fp) {
    perror("fopen");
    return 1;
  }
  
  while (fgets(buf, sizeof(buf), fp)) {
    bp = buf;
    for (ap = vals; (*ap = strsep(&bp, ",")) != NULL;)
      if (++ap >= &vals[10])
	break;

    phi = atof(vals[3]) * deg2rad;
    lam = atof(vals[4]) * deg2rad;
    
    height = atof(vals[5]);

    m = new Mountain(vals[1], phi, lam, height);

    mountains->add(m);
  }

  fclose(fp);

  update_angles();
  return 0;
}

int
Panorama::set_viewpoint(const char *name) {
  if (get_pos(name, &view_phi, &view_lam, &view_height) != 1) {
    fprintf(stderr, "Could not find exactly one entry for %s.\n");
    return 1;
  }

  update_angles();
  return 0;
}

Mountains * 
Panorama::get_visible_mountains() {
  return visible_mountains;
}

int
Panorama::set_mountain(Mountain *m, int x, int y) {
  if (m1 && m2 && m != m1 && m != m2) {
    fprintf(stderr, "Resetting mountains\n");
    m1 = NULL;
    m2 = NULL;
  }

  m->x = x;
  m->y = y;

  if (m1 == NULL) {
    m1 = m;
    fprintf(stderr, "m1=%s\n", m1->name);
  } else if (m2 == NULL && m != m1) {
    m2 = m;
    fprintf(stderr, "m2=%s\n", m2->name);
  } 
}

double
Panorama::get_value(Mountains *p) {
  int i, j;
  Mountain *m;
  double v = 0.0, d_min, d;

  if (isnan(scale) || isnan(a_center) || isnan(a_tilt) || isnan(a_nick) ||
      scale < 500.0 || scale > 100000.0 || 
      a_nick > pi_d/4.0 || a_nick < - pi_d/4.0 || 
      a_tilt > pi_d/16.0 || a_tilt < - pi_d/16.0) {
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

extern GipfelWidget *gipf;



int 
Panorama::guess(Mountains *p) {
  Mountain *p2, *m_tmp1, *m_tmp2;
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
    for (j=0; j<mountains->get_num(); j++) {
      m_tmp2 = mountains->get(j);
      m1 = m_tmp1;
      m1->x = x1_sav;
      m1->y = y1_sav;

      if (fabs(m1->alph - m_tmp2->alph) < pi_d / 2.0 &&
	  m_tmp2->height / (m_tmp2->dist * EARTH_RADIUS) > 
	  height_dist_ratio) {
	
	if (m1 != m_tmp2) {
	
	  m_tmp2->x = p2->x;
	  m_tmp2->y = p2->y;
	  m2 = m_tmp2;
	  
	  comp_params();
	  
	  v = get_value(p);
	  
	  if (v < best) {
	    best = v;
	    a_center_best = a_center;
	    a_nick_best = a_nick;
	    a_tilt_best = a_tilt;
	    scale_best = scale;
	    gipf->update();
	    
	    fprintf(stderr, "best %f\n", best);
	  }	    
	}
      }
    }     
  }

  a_center = a_center_best;
  a_nick = a_nick_best;
  a_tilt = a_tilt_best;
  scale = scale_best;
  fprintf(stderr, "best %f\n", best);
  fprintf(stderr, "center = %f, scale = %f, nick=%f\n", a_center /deg2rad, scale, a_nick/deg2rad);
  update_coordinates();
  return 0;
}

int
Panorama::comp_params() {

  if (m1 == NULL || m2 == NULL) {
    fprintf(stderr, "Position two mountains first.\n");
    m1 = NULL;
    m2 = NULL;
    return 1;
  }

  x1 = m1->x;
  y1 = m1->y;

  x2 = m2->x;
  y2 = m2->y;
  
  a_center = comp_center_angle(m1->alph, m2->alph, x1, x2);
  scale    = comp_scale(m1->alph, m2->alph, x1, x2);
  a_nick   = atan ((y1 + tan(m1->a_nick) * scale) / ( scale - y1 * tan(m1->a_nick)));


  optimize();

  update_coordinates();

  return 0;
}

int
Panorama::optimize() {
  int i;
  double tan_nick_view, tan_dir_view, n_scale;
  double tan_nick_m1, tan_dir_m1;
  double tan_nick_m2, tan_dir_m2;
  double d_m1_2, d_m2_2, d_m1_m2_2;

  d_m1_2 = pow(x1, 2.0) + pow(y1, 2.0);
  d_m2_2 = pow(x2, 2.0) + pow(y2, 2.0);
  d_m1_m2_2 = pow(x1 - x2, 2.0) + pow(y1 - y2, 2.0);

  tan_nick_view = tan(a_nick);
  tan_dir_view = tan(a_center);
  n_scale = scale;
  tan_dir_m1 = tan(m1->alph);
  tan_nick_m1 = tan(m1->a_nick);
  tan_dir_m2 = tan(m2->alph);
  tan_nick_m2 = tan(m2->a_nick);

  //  fprintf(stderr, "m1: %d, %d; m2: %d, %d\n", x1, y1, x2, y2);
  d_m1_2 = pow(x1, 2.0) + pow(y1, 2.0);
  d_m2_2 = pow(x2, 2.0) + pow(y2, 2.0);
  d_m1_m2_2 = pow(x1 - x2, 2.0) + pow(y1 - y2, 2.0);

  //  fprintf(stderr, "d_m1_2 %f, d_m2_2 %f, d_m1_m2_2 %f\n", 
  //  d_m1_2, d_m2_2, d_m1_m2_2);

  for (i=0; i<5; i++) {
    newton(&tan_nick_view, &tan_dir_view, &n_scale, 
	   tan_dir_m1, tan_nick_m1, tan_dir_m2, tan_nick_m2,
	   d_m1_2, d_m2_2, d_m1_m2_2);
  }

  a_nick = atan(tan_nick_view);
  a_center = atan(tan_dir_view);

  if (fabs(a_center - m1->alph) > pi_d/2.0) {
    a_center = a_center + pi_d;
  }
  if (a_center > 2.0 * pi_d) {
    a_center = a_center - 2.0 *  pi_d;
  } else if (a_center < 0.0) {
    a_center = a_center + 2.0 * pi_d;
  }
  
  scale = n_scale;

  // use the point with greater distance from center for tilt computation 
  if (d_m1_2 > d_m2_2) {
      a_tilt = comp_tilt(tan_nick_view, tan_dir_view, n_scale, 
			 tan_nick_m1, tan_dir_m1,
			 (double) x1, (double) y1);
  } else {
    a_tilt = comp_tilt(tan_nick_view, tan_dir_view, n_scale, 
		       tan_nick_m2, tan_dir_m2,
		       (double) x2, (double) y2);
  }

 
  return 0;
}

void
Panorama::set_center_angle(double a) {
  a_center = a;
  update_coordinates();
}

void
Panorama::set_nick_angle(double a) {
  a_nick = a;
  fprintf(stderr, "-->nick%f\n", a_nick/deg2rad);
  update_coordinates();
}

void
Panorama::set_tilt_angle(double a) {
  a_tilt = a;
  fprintf(stderr, "-->tilt%f\n", a_tilt/deg2rad);
  update_coordinates();
}

void
Panorama::set_scale(double s) {
  scale = s;
  update_coordinates();
}

void
Panorama::set_height_dist_ratio(double r) {
  height_dist_ratio = r;
  update_visible_mountains();
}

int
Panorama::get_pos(const char *name, double *phi, double *lam, double *height) {
  int i;
  int found = 0;
  double p, l, h;
  Mountain *m;

  for (i=0; i<mountains->get_num(); i++) {
    m = mountains->get(i);

    if (strcmp(m->name, name) == 0) {
      p = m->phi;
      l = m->lam;
      h = m->height;

      fprintf(stderr, "Found matching entry: %s (%fm)\n", m->name, m->height);
      found++;
    }
  }

  if (found == 1) {
    *phi    = p;
    *lam    = l;
    *height = h;
  }

  return found;
}

void 
Panorama::update_angles() {
  int i;
  Mountain *m;

  for (i=0; i<mountains->get_num(); i++) {
    m = mountains->get(i);
 
    m->dist = distance(m->phi, m->lam);
    if (m->phi != view_phi || m->lam != view_lam) {
      
      m->alph = alpha(m->phi, m->lam);
      m->a_nick = nick(m->dist, m->height);
    }
  }
  
  update_visible_mountains();
}

void 
Panorama::update_visible_mountains() {
  int i;
  Mountain *m;

  visible_mountains->clear();

  for (i=0; i<mountains->get_num(); i++) {
    m = mountains->get(i);
 
    if ((m->phi != view_phi || m->lam != view_lam) &&
	(m->height / (m->dist * EARTH_RADIUS) 
	 > height_dist_ratio)) {
      
      if (m->a_view < pi_d / 2.0 && m->a_view > - pi_d / 2.0) {
	visible_mountains->add(m);
      }
    }
  }

  visible_mountains->sort();
  update_coordinates();
}

void 
Panorama::update_coordinates() {
  int i;
  double x_tmp, y_tmp;
  Mountain *m;

  for (i=0; i<visible_mountains->get_num(); i++) {
    m = visible_mountains->get(i);
        
    m->a_view = m->alph - a_center;
    if (m->a_view > pi_d) {
      m->a_view -= 2.0*pi_d;
    } else if (m->a_view < -pi_d) {
      m->a_view += 2.0*pi_d;
    }
    
    x_tmp = tan(m->a_view) * scale;
    y_tmp = - (tan(m->a_nick - a_nick) * scale);
    // rotate by a_tilt;
    m->x = (int) rint(x_tmp * cos(a_tilt) - y_tmp * sin(a_tilt));
    m->y = (int) rint(x_tmp * sin(a_tilt) + y_tmp * cos(a_tilt));
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
Panorama::comp_center_angle(double a1, double a2, double d1, double d2) {
  double sign1 = 1.0;
  double tan_acenter, tan_a1, tan_a2;

  tan_a1 = tan(a1);
  tan_a2 = tan(a2);

  tan_acenter = (((pow(((pow((1.0 + (tan_a1 * tan_a2)), 2.0) * ((d1 * d1) + (d2 * d2))) + (2.0 * d1 * d2 * ((2.0 * ((tan_a2 * tan_a1) - (tan_a2 * tan_a2))) - ((tan_a1 * tan_a1) * (2.0 + (tan_a2 * tan_a2))) - 1.0))), (1.0 / 2.0)) * sign1) + ((1.0 - (tan_a1 * tan_a2)) * (d1 - d2))) / (2.0 * ((d2 * tan_a2) - (d1 * tan_a1))));
  
  return atan(tan_acenter) + pi_d;
}

double
Panorama::comp_scale(double a1, double a2, double d1, double d2) {
  double sign1 = 1.0;
  double sc, tan_a1, tan_a2;
  
  tan_a1 = tan(a1);
  tan_a2 = tan(a2);
  
  sc = ((((1.0 + (tan_a1 * tan_a2)) * (d1 - d2)) - (sign1 * pow((((1.0 + pow((tan_a1 * tan_a2), 2.0)) * ((d1 * d1) + (d2 * d2))) + (2.0 * ((tan_a1 * tan_a2 * pow((d1 + d2), 2.0)) - (d1 * d2 * (((tan_a1 * tan_a1) * (2.0 + (tan_a2 * tan_a2))) + 1.0 + (2.0 * (tan_a2 * tan_a2))))))), (1.0 / 2.0)))) / (2.0 * (tan_a1 - tan_a2)));

  return sc;
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





//
//
//

static int
get_matrix(double m[], 
	   double tan_nick_view, double tan_dir_view, double n_scale,
	   double tan_dir_m1, double tan_nick_m1,
	   double tan_dir_m2, double tan_nick_m2) {
  
  m[0] = pow(n_scale,2.0)*(1.0/pow((tan_nick_m1*tan_nick_view + 1.0),2.0)*(2.0*tan_nick_m1 - 2.0 * tan_nick_view) + 2.0*tan_nick_m1*pow((tan_nick_m1 - tan_nick_view), 2.0)/pow((tan_nick_m1*tan_nick_view + 1.0), 3.0));

  m[1] = pow(n_scale, 2.0) *(1.0/pow((tan_dir_m1*tan_dir_view + 1.0), 2.0) * (2.0*tan_dir_m1 - 2.0*tan_dir_view) + 2.0*tan_dir_m1*pow((tan_dir_m1 - tan_dir_view),2.0) / pow((tan_dir_m1*tan_dir_view + 1.0), 3.0));

  m[2] = -2.0*n_scale*(pow((tan_dir_m1 - tan_dir_view), 2.0)/pow((tan_dir_m1*tan_dir_view + 1.0), 2.0) + pow((tan_nick_m1 - tan_nick_view), 2.0)/pow((tan_nick_m1*tan_nick_view + 1.0), 2.0));

  m[3] = pow(n_scale, 2.0)*(1.0/pow((tan_nick_m2*tan_nick_view + 1.0), 2.0)*(2.0*tan_nick_m2 - 2.0*tan_nick_view) + 2.0*tan_nick_m2*pow((tan_nick_m2 - tan_nick_view), 2.0)/pow((tan_nick_m2*tan_nick_view + 1.0), 3.0));

  m[4] = pow(n_scale, 2.0)*(1.0/pow((tan_dir_m2*tan_dir_view + 1.0), 2.0)*(2.0*tan_dir_m2 - 2.0*tan_dir_view) + 2.0*tan_dir_m2*pow((tan_dir_m2 - tan_dir_view), 2.0)/pow((tan_dir_m2*tan_dir_view + 1.0), 3.0));

  m[5] = -2.0*n_scale*(pow((tan_dir_m2 - tan_dir_view), 2.0)/pow((tan_dir_m2*tan_dir_view + 1.0), 2.0) + pow((tan_nick_m2 - tan_nick_view), 2.0)/pow((tan_nick_m2*tan_nick_view + 1.0), 2.0));

  m[6] = 2.0*(n_scale*(tan_nick_m1 - tan_nick_view)/(tan_nick_m1*tan_nick_view + 1.0) - n_scale*(tan_nick_m2 - tan_nick_view)/(tan_nick_m2*tan_nick_view + 1.0))*(n_scale/(tan_nick_m1*tan_nick_view + 1.0) - n_scale/(tan_nick_m2*tan_nick_view + 1.0) + tan_nick_m1*n_scale*(tan_nick_m1 - tan_nick_view)/pow((tan_nick_m1*tan_nick_view + 1.0), 2.0) - tan_nick_m2*n_scale*(tan_nick_m2 - tan_nick_view)/pow((tan_nick_m2*tan_nick_view + 1.0),2.0));

  m[7] = 2.0*(n_scale*(tan_dir_m1 - tan_dir_view)/(tan_dir_m1*tan_dir_view + 1.0) - n_scale*(tan_dir_m2 - tan_dir_view)/(tan_dir_m2*tan_dir_view + 1.0))*(n_scale/(tan_dir_m1*tan_dir_view + 1.0) - n_scale/(tan_dir_m2*tan_dir_view + 1.0) + tan_dir_m1*n_scale*(tan_dir_m1 - tan_dir_view)/pow((tan_dir_m1*tan_dir_view + 1.0), 2.0) - tan_dir_m2*n_scale*(tan_dir_m2 - tan_dir_view)/pow((tan_dir_m2*tan_dir_view + 1.0), 2.0));

  m[8] = - 2.0*(n_scale*(tan_dir_m1 - tan_dir_view)/(tan_dir_m1*tan_dir_view + 1.0) - n_scale*(tan_dir_m2 - tan_dir_view)/(tan_dir_m2*tan_dir_view + 1.0))*((tan_dir_m1 - tan_dir_view)/(tan_dir_m1*tan_dir_view + 1.0) - (tan_dir_m2 - tan_dir_view)/(tan_dir_m2*tan_dir_view + 1.0)) - 2.0*(n_scale*(tan_nick_m1 - tan_nick_view)/(tan_nick_m1*tan_nick_view + 1.0) - n_scale*(tan_nick_m2 - tan_nick_view)/(tan_nick_m2*tan_nick_view + 1.0))*((tan_nick_m1 - tan_nick_view)/(tan_nick_m1*tan_nick_view + 1.0) - (tan_nick_m2 - tan_nick_view)/(tan_nick_m2*tan_nick_view + 1.0));

  return 0;
}


static int newton(double *tan_nick_view, 
		  double *tan_dir_view,
		  double *n_scale,
		  double tan_dir_m1,
		  double tan_nick_m1,
		  double tan_dir_m2,
		  double tan_nick_m2,
		  double d_m1_2, double d_m2_2, double d_m1_m2_2) {
  double a[9];
  double b[3];
  double a_x0[3], f_x0 [3], x0[3];
  int ret;

  get_matrix(a, *tan_nick_view, *tan_dir_view, *n_scale, 
	     tan_dir_m1, tan_nick_m1, tan_dir_m2, tan_nick_m2);

  f_x0[0] = d_m1_2 - (pow((*tan_nick_view-tan_nick_m1),2.0)/pow((tan_nick_m1**tan_nick_view+1), 2.0)+pow((*tan_dir_view-tan_dir_m1),2.0)/pow((tan_dir_m1**tan_dir_view+1),2.0))*pow(*n_scale, 2.0);

  f_x0[1] = d_m2_2 - (pow((*tan_nick_view-tan_nick_m2),2.0)/pow((tan_nick_m2**tan_nick_view+1),2.0)+pow((*tan_dir_view-tan_dir_m2),2.0)/pow((tan_dir_m2**tan_dir_view+1),2.0))*pow(*n_scale, 2.0);

  f_x0[2] = d_m1_m2_2 - (pow((- (((*tan_dir_view - tan_dir_m1) * *n_scale) / (tan_dir_m1 * *tan_dir_view + 1.0)) + (((*tan_dir_view - tan_dir_m2) * *n_scale) / (tan_dir_m2 * *tan_dir_view + 1))), 2.0) + pow((- (((*tan_nick_view - tan_nick_m1) * *n_scale) / (tan_nick_m1 * *tan_nick_view + 1)) + ((*tan_nick_view - tan_nick_m2) * *n_scale) / (tan_nick_m2 * *tan_nick_view + 1)), 2.0));

  //  fprintf(stderr, "f_x0[0] %f, f_x0[1] %f, f_x0[2] %f\n", 
  //	  f_x0[0], f_x0[1], f_x0[2]);

  x0[0] = *tan_nick_view;
  x0[1] = *tan_dir_view;
  x0[2] = *n_scale;

  rmmult(a_x0, a, x0, 3, 3, 1);
    
  b[0] = a_x0[0] - f_x0[0];
  b[1] = a_x0[1] - f_x0[1];
  b[2] = a_x0[2] - f_x0[2];

  ret = solv(a, b, 3);

  *tan_nick_view = b[0];
  *tan_dir_view  = b[1];
  *n_scale       = b[2];

  return 0;
}


static double
comp_tilt(double tan_nick_view, double tan_dir_view, double n_scale,
	  double tan_nick_m, double tan_dir_m,
	  double x, double y) {
  double y_tmp, x_tmp, sin_a_tilt1, sin_a_tilt2, sin_a_tilt, res;

  y_tmp = - (((tan_nick_view - tan_nick_m) * n_scale) / 
	     (tan_nick_m * tan_nick_view + 1));
  x_tmp = - (((tan_dir_view - tan_dir_m) * n_scale) / 
	     (tan_dir_m * tan_dir_view + 1));


  sin_a_tilt1 = - (y * - pow(x*x + y*y - y_tmp*y_tmp, 0.5) - x * y_tmp) /
    (x*x + y*y);

  sin_a_tilt2 = - (y * pow(x*x + y*y - y_tmp*y_tmp, 0.5) - x * y_tmp) / 
    (x*x + y*y);

  sin_a_tilt = fabs(sin_a_tilt1) < fabs(sin_a_tilt2)?sin_a_tilt1:sin_a_tilt2;

  res = asin(sin_a_tilt);

  if (res > pi_d / 4.0) {
    res = res - pi_d / 2.0;
  } else if (res < -pi_d / 4.0) {
    res = res + pi_d / 2.0;
  }

  return res;
}
