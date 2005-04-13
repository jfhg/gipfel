// 
// "$Id: Panorama.cxx,v 1.2 2005/04/13 19:09:19 hofmann Exp $"
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

#include "Panorama.H"

Panorama::Panorama() {
  mountains = NULL;
  visible_mountains = NULL;
  height_dist_ratio = 0.1;
  pi = asin(1.0) * 2.0;
  deg2rad = pi / 180.0;
  center_angle = 0.0;
  scale = 200.0;
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
  if (get_pos(name, &view_phi, &view_lam) != 1) {
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
Panorama::get_x(Mountain *m) {
  return (int) (tan(m->alph - center_angle) * scale);
}

int
Panorama::get_pos(const char *name, double *phi, double *lam) {
  Mountain *m = mountains;
  int found = 0;
  double p, l;

  while (m) {
    if (strstr(m->name, name)) {
      p = m->phi;
      l = m->lam;

      fprintf(stderr, "Found matching entry: %s (%fm)\n", m->name, m->height);
      found++;
    }
    
    m = m->get_next();
  }

  if (found == 1) {
    *phi = p;
    *lam = l;
  }

  return found;
}

void 
Panorama::update_visible_mountains() {
  Mountain *m = mountains;
  visible_mountains = NULL;

  while (m) {

    if (m->height / (distance(m->phi, m->lam)* 6368000) 
	> height_dist_ratio) {
      m ->alph = alpha(m->phi, m->lam);

      if (m->alph < pi / 2.0 &&  m->alph > - pi / 2.0) {
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

  return alph;
}

