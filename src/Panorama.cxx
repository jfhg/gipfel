// 
// "$Id: Panorama.cxx,v 1.11 2005/04/25 20:02:56 hofmann Exp $"
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
#include "Panorama.H"

Panorama::Panorama() {
  mountains = NULL;
  visible_mountains = NULL;
  m1 = NULL;
  m2 = NULL;
  height_dist_ratio = 0.07;
  pi = asin(1.0) * 2.0;
  deg2rad = pi / 180.0;
  a_center = 0.0;
  a_nick = 0.0;
  a_tilt = 0.0;
  scale = 20.0;
}

Panorama::~Panorama() {
  if (mountains) {
    delete(mountains);
  }
}

int
Panorama::load_file(const char *name) {
  FILE *fp;
  char buf[4000];
  char *vals[10];
  char **ap, *bp;
  double phi, lam, height;
  Mountain *m;

  if (mountains) {
    delete(mountains);
  }

  mountains = NULL;
  visible_mountains = NULL;

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
    if (mountains) {
      mountains->append(m);
    } else {
      mountains = m;
    }
  }

  fclose(fp);

  update_visible_mountains();
  return 0;
}

int
Panorama::set_viewpoint(const char *name) {
  if (get_pos(name, &view_phi, &view_lam, &view_height) != 1) {
    fprintf(stderr, "Could not find exactly one entry for %s.\n");
    return 1;
  }

  update_visible_mountains();
  return 0;
}

Mountain * 
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

int
Panorama::comp_params() {
  if (m1 == NULL || m2 == NULL) {
    fprintf(stderr, "Position two mountains first.\n");
    m1 = NULL;
    m2 = NULL;
    return 1;
  }

  x2 = m2->x;
  y2 = m2->y;
  
  x1 = m1->x;
  y1 = m1->y;

  fprintf(stderr, "center = %f, scale = %f, nick=%f\n", a_center /deg2rad, scale, a_nick/deg2rad);
  a_center = comp_center_angle(m1->alph, m2->alph, x1, x2);
  scale    = comp_scale(m1->alph, m2->alph, x1, x2);
  fprintf(stderr, "center = %f, scale = %f, nick=%f\n", a_center /deg2rad, scale, a_nick/deg2rad);
  a_nick   = atan ((y1 + tan(m1->a_nick) * scale) / ( scale - y1 * tan(m1->a_nick)));
  //  a_nick = comp_center_angle(m1->a_nick, m2->a_nick, -y1, -y2) - pi;  
  fprintf(stderr, "center = %f, scale = %f, nick=%f\n", a_center /deg2rad, scale, a_nick/deg2rad);
  update_visible_mountains();
#if 0
  m1 = NULL;
  m2 = NULL;
#endif
  return 0;
}

void
Panorama::set_center_angle(double a) {
  a_center = a;
  update_visible_mountains();
}

void
Panorama::set_nick_angle(double a) {
  a_nick = a;
  fprintf(stderr, "-->nick%f\n", a_nick/deg2rad);
  update_visible_mountains();
}

void
Panorama::set_tilt_angle(double a) {
  a_tilt = a;
  fprintf(stderr, "-->tilt%f\n", a_tilt/deg2rad);
  update_visible_mountains();
}

void
Panorama::set_scale(double s) {
  scale = s;
  update_visible_mountains();
}

void
Panorama::set_height_dist_ratio(double r) {
  height_dist_ratio = r;
  update_visible_mountains();
}

int
Panorama::get_pos(const char *name, double *phi, double *lam, double *height) {
  Mountain *m = mountains;
  int found = 0;
  double p, l, h;

  while (m) {
    if (strcmp(m->name, name) == 0) {
      p = m->phi;
      l = m->lam;
      h = m->height;

      fprintf(stderr, "Found matching entry: %s (%fm)\n", m->name, m->height);
      found++;
    }
    
    m = m->get_next();
  }

  if (found == 1) {
    *phi    = p;
    *lam    = l;
    *height = h;
  }

  return found;
}

void 
Panorama::update_visible_mountains() {
  Mountain *m = mountains;
  visible_mountains = NULL;
  double x1, y1;

  while (m) {
 
    m->dist = distance(m->phi, m->lam);
    if ((m->phi != view_phi || m->lam != view_lam) &&
	(m->height / (m->dist * 6368000) 
	 > height_dist_ratio)) {
      
      m->alph = alpha(m->phi, m->lam);
      m->a_view = m->alph - a_center;
      if (m->a_view > pi) {
	m->a_view -= 2.0*pi;
      } else if (m->a_view < -pi) {
	m->a_view += 2.0*pi;
      }
      
      //      fprintf(stderr, "==> %s %f, dist %f km  %f\n", m->name, m->alph, distance(m->phi, m->lam)* 6368, m->a_view);
      if (m->a_view < pi / 2.0 && m->a_view > - pi / 2.0) {
	m->a_nick = nick(m->dist, m->height);
	x1 = tan(m->a_view) * scale;
	y1 = - (tan(m->a_nick - a_nick) * scale);
	// rotate by a_tilt;
	m->x = (int) (x1 * cos(a_tilt) - y1 * sin(a_tilt));
	m->y = (int) (x1 * sin(a_tilt) + y1 * cos(a_tilt));

	m->clear_next_visible();
	if (visible_mountains) {
	  visible_mountains->append_visible(m);
	} else {
	  visible_mountains = m;
	}
      }
    }
    
    m = m->get_next();
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
    alph = 2.0 * pi - acos(cos_alph);
  }


  if (alph > 2.0 * pi) {
    alph = alph - 2.0 *  pi;
  } else if (alph < 0.0) {
    alph = alph + 2.0 * pi;
  }
  return alph;
}

#if 0
double 
Panorama::center_angle(double alph_a, double alph_b, double d1, double d2) {
  double tan_a, tan_b;

  tan_a = tan(alph_a - alph_b);
  fprintf(stderr, "tan_a %f\n", tan_a);
  tan_b = (d2 - d1 + ((sqrt((d2*(d2 - (2.0*d1*(1.0 + (2.0 * tan_a * tan_a))))) + (d1*d1))))) / (2.0*d2*tan_a);

  fprintf(stderr, "tan_b=%f\n", tan_b);
  return alph_a + atan(tan_b);
}
#endif

double
Panorama::comp_center_angle(double a1, double a2, double d1, double d2) {
  double sign1 = 1.0;
  double tan_acenter, tan_a1, tan_a2;

  tan_a1 = tan(a1);
  tan_a2 = tan(a2);

  tan_acenter = (((pow(((pow((1.0 + (tan_a1 * tan_a2)), 2.0) * ((d1 * d1) + (d2 * d2))) + (2.0 * d1 * d2 * ((2.0 * ((tan_a2 * tan_a1) - (tan_a2 * tan_a2))) - ((tan_a1 * tan_a1) * (2.0 + (tan_a2 * tan_a2))) - 1.0))), (1.0 / 2.0)) * sign1) + ((1.0 - (tan_a1 * tan_a2)) * (d1 - d2))) / (2.0 * ((d2 * tan_a2) - (d1 * tan_a1))));
  
  return atan(tan_acenter) + pi;
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

  b = height + 6368000.0;
  c = view_height + 6368000.0;

  a = pow(((b * (b - (2.0 * c * cos(dist)))) + (c * c)), (1.0 / 2.0));
  beta = acos((-(b*b) + (a*a) + (c*c))/(2 * a * c));
  
  return beta - pi / 2.0;
}





//
//
//

int
Panorama::get_matrix(double m[]) {
  
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


int 
Panorama::newton() {
  double a[9];
  double b[3];
  double a_x0[3], f_x0 [3], x0[3];
  int ret;

  tan_nick_view = tan(a_nick);
  tan_dir_view = tan(a_center);
  n_scale = scale;
  tan_dir_m1 = tan(m1->alph);
  tan_nick_m1 = tan(m1->a_nick);
  tan_dir_m2 = tan(m2->alph);
  tan_nick_m2 = tan(m2->a_nick);

  fprintf(stderr, "m1: %d, %d; m2: %d, %d\n", x1, y1, x2, y2);
  d_m1_2 = pow(x1, 2.0) + pow(y1, 2.0);
  d_m2_2 = pow(x2, 2.0) + pow(y2, 2.0);
  d_m1_m2_2 = pow(x1 - x2, 2.0) + pow(y1 - y2, 2.0);

  fprintf(stderr, "d_m1_2 %f, d_m2_2 %f, d_m1_m2_2 %f\n", 
	  d_m1_2, d_m2_2, d_m1_m2_2);

  get_matrix(a);

  f_x0[0] = d_m1_2 - (pow((tan_nick_view-tan_nick_m1),2.0)/pow((tan_nick_m1*tan_nick_view+1), 2.0)+pow((tan_dir_view-tan_dir_m1),2.0)/pow((tan_dir_m1*tan_dir_view+1),2.0))*pow(n_scale, 2.0);

  f_x0[1] = d_m2_2 - (pow((tan_nick_view-tan_nick_m2),2.0)/pow((tan_nick_m2*tan_nick_view+1),2.0)+pow((tan_dir_view-tan_dir_m2),2.0)/pow((tan_dir_m2*tan_dir_view+1),2.0))*pow(n_scale, 2.0);

  f_x0[2] = d_m1_m2_2 - (pow((- (((tan_dir_view - tan_dir_m1) * n_scale) / (tan_dir_m1 * tan_dir_view + 1.0)) + (((tan_dir_view - tan_dir_m2) * n_scale) / (tan_dir_m2 * tan_dir_view + 1))), 2.0) + pow((- (((tan_nick_view - tan_nick_m1) * n_scale) / (tan_nick_m1 * tan_nick_view + 1)) + ((tan_nick_view - tan_nick_m2) * n_scale) / (tan_nick_m2 * tan_nick_view + 1)), 2.0));

  fprintf(stderr, "f_x0[0] %f, f_x0[1] %f, f_x0[2] %f\n", 
	  f_x0[0], f_x0[1], f_x0[2]);

  x0[0] = tan_nick_view;
  x0[1] = tan_dir_view;
  x0[2] = n_scale;

  rmmult(a_x0, a, x0, 3, 3, 1);
    
  b[0] = a_x0[0] - f_x0[0];
  b[1] = a_x0[1] - f_x0[1];
  b[2] = a_x0[2] - f_x0[2];

  ret = solv(a, b, 3);
  fprintf(stderr, "solv returned %d\n", ret);

  a_nick = atan(b[0]);
  a_center = atan(b[1]);
  fprintf(stderr, "==> m1->alph %f, a_center = %f\n", m1->alph/deg2rad,
	  a_center/deg2rad);
  if (fabs(a_center - m1->alph) > pi/2.0) {
    a_center = a_center + pi;
    fprintf(stderr, "==> adding pi => %f\n", a_center);
  }
  if (a_center > 2.0 * pi) {
    a_center = a_center - 2.0 *  pi;
  } else if (a_center < 0.0) {
    a_center = a_center + 2.0 * pi;
  }
  
  scale = b[2];

  a_tilt = comp_tilt();
  fprintf(stderr, "center = %f, scale = %f, nick=%f tilt %f\n", a_center /deg2rad, scale, a_nick/deg2rad, a_tilt/deg2rad);
  update_visible_mountains();
  return 0;
}


double
Panorama::comp_tilt() {
  double sin_a_tilt, sign1 = 1.0, res;
  
 sin_a_tilt = (((pow((1.0 + (tan_nick_m1 * tan_nick_view)), 2.0) * y1 * ((tan_dir_view * (1.0 - (tan_dir_m1 * tan_dir_m1))) + (tan_dir_m1 * ((tan_dir_view * tan_dir_view) - 1.0)))) + (pow(((pow((scale * y1), 2.0) * pow((1.0 + (tan_dir_view * tan_dir_m1)), 4.0) * ((2.0 * (tan_nick_view - pow(tan_nick_view, 3.0)) * (tan_nick_m1 - pow(tan_nick_m1, 3.0))) - ((tan_nick_view * tan_nick_view) * (1.0 + pow(tan_nick_m1, 4.0))) + ((tan_nick_m1 * tan_nick_m1) * ((4.0 * (tan_nick_view * tan_nick_view)) - 1.0 - pow(tan_nick_view, 4.0))))) + (pow(scale, 4.0) * ((2.0 * ((pow(tan_nick_view, 3.0) * ((pow(tan_nick_m1, 3.0) * (((tan_dir_view * tan_dir_view) * ((4.0 * (tan_dir_m1 * tan_dir_m1)) - pow(tan_dir_m1, 4.0) - 1.0)) - ((tan_dir_m1 * tan_dir_m1) * (pow(tan_dir_view, 4.0) + 1.0)) + (2.0 * (tan_dir_m1 - pow(tan_dir_m1, 3.0)) * (tan_dir_view - pow(tan_dir_view, 3.0))))) + (tan_nick_m1 * (((tan_dir_view * tan_dir_view) * (pow(tan_dir_m1, 4.0) + 1.0 - (16.0 * (tan_dir_m1 * tan_dir_m1)))) + (pow(tan_dir_view, 4.0) * ((tan_dir_m1 * tan_dir_m1) - (2.0 * pow(tan_dir_m1, 4.0)))) + (2.0 * ((tan_dir_view * (pow(tan_dir_m1, 3.0) - (5.0 * tan_dir_m1))) + (pow(tan_dir_view, 3.0) * (tan_dir_m1 - (5.0 * pow(tan_dir_m1, 3.0)))) - 1.0)) + (tan_dir_m1 * tan_dir_m1))))) + (tan_nick_view * ((pow(tan_nick_m1, 3.0) * (((tan_dir_view * tan_dir_view) * (pow(tan_dir_m1, 4.0) + 1.0 - (16.0 * (tan_dir_m1 * tan_dir_m1)))) + (pow(tan_dir_view, 4.0) * ((tan_dir_m1 * tan_dir_m1) - (2.0 * pow(tan_dir_m1, 4.0)))) + (2.0 * ((tan_dir_view * (pow(tan_dir_m1, 3.0) - (5.0 * tan_dir_m1))) + (pow(tan_dir_view, 3.0) * (tan_dir_m1 - (5.0 * pow(tan_dir_m1, 3.0)))) - 1.0)) + (tan_dir_m1 * tan_dir_m1))) + (tan_nick_m1 * (((tan_dir_view * tan_dir_view) * ((4.0 * (tan_dir_m1 * tan_dir_m1)) - pow(tan_dir_m1, 4.0) - 1.0)) - ((tan_dir_m1 * tan_dir_m1) * (pow(tan_dir_view, 4.0) + 1.0)) + (2.0 * (tan_dir_m1 - pow(tan_dir_m1, 3.0)) * (tan_dir_view - pow(tan_dir_view, 3.0))))))))) + ((tan_nick_view * tan_nick_view) * ((pow(tan_nick_m1, 4.0) * ((2.0 * (pow(tan_dir_m1, 3.0) - tan_dir_m1) * (tan_dir_view - pow(tan_dir_view, 3.0))) + ((tan_dir_view * tan_dir_view) * (pow(tan_dir_m1, 4.0) + 1.0 - (4.0 * (tan_dir_m1 * tan_dir_m1)))) + ((tan_dir_m1 * tan_dir_m1) * (pow(tan_dir_view, 4.0) + 1.0)))) + ((tan_nick_m1 * tan_nick_m1) * ((8.0 * ((tan_dir_view * ((4.0 * tan_dir_m1) - pow(tan_dir_m1, 3.0))) + (pow(tan_dir_view, 3.0) * ((4.0 * pow(tan_dir_m1, 3.0)) - tan_dir_m1)))) + (4.0 * (((tan_dir_view * tan_dir_view) * ((13.0 * (tan_dir_m1 * tan_dir_m1)) - pow(tan_dir_m1, 4.0) - 1.0)) - (tan_dir_m1 * tan_dir_m1))) + (pow(tan_dir_view, 4.0) * ((6.0 * pow(tan_dir_m1, 4.0)) - (4.0 * (tan_dir_m1 * tan_dir_m1)))) + 6.0)) + (2.0 * (pow(tan_dir_m1, 3.0) - tan_dir_m1) * (tan_dir_view - pow(tan_dir_view, 3.0))) + ((tan_dir_view * tan_dir_view) * (pow(tan_dir_m1, 4.0) + 1.0 - (4.0 * (tan_dir_m1 * tan_dir_m1)))) + ((tan_dir_m1 * tan_dir_m1) * (pow(tan_dir_view, 4.0) + 1.0)))) + (pow((1.0 + (tan_dir_m1 * tan_dir_view)), 4.0) * (pow(tan_nick_m1, 4.0) + pow(tan_nick_view, 4.0))) + ((tan_nick_m1 * tan_nick_m1) * (((tan_dir_view * tan_dir_view) * (1.0 + pow(tan_dir_m1, 4.0))) + ((tan_dir_m1 * tan_dir_m1) * (1.0 - (4.0 * (tan_dir_view * tan_dir_view)) + pow(tan_dir_view, 4.0))) + (2.0 * (pow(tan_dir_view, 3.0) - tan_dir_view) * (tan_dir_m1 - pow(tan_dir_m1, 3.0)))) * (pow(tan_nick_view, 4.0) + 1.0))))), (1.0 / 2.0)) * sign1 / scale)) / (scale * ((2.0 * ((tan_nick_view * tan_nick_m1 * ((4.0 * tan_dir_m1 * tan_dir_view) + (((tan_dir_view * tan_dir_view) - 1.0) * ((tan_dir_m1 * tan_dir_m1) - 1.0)))) + (tan_dir_m1 * tan_dir_view))) - ((tan_nick_m1 * tan_nick_m1) * (pow((tan_nick_view * (tan_dir_m1 - tan_dir_view)), 2.0) + (2.0 * tan_dir_m1 * tan_dir_view) + pow((tan_dir_m1 * tan_dir_view), 2.0) + 1.0)) - pow((tan_nick_view * (1.0 + (tan_dir_m1 * tan_dir_view))), 2.0) - (tan_dir_m1 * tan_dir_m1) - (tan_dir_view * tan_dir_view))));

 res = asin(sin_a_tilt);

 if (res > pi / 4.0) {
   res = res - pi / 2.0;
 } else if (res < -pi / 4.0) {
   res = res + pi / 2.0;
 }
 
 return res;
}
