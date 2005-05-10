// 
// "$Id: Hill.cxx,v 1.13 2005/05/10 17:16:54 hofmann Exp $"
//
// Hill routines.
//
// Copyright 2005 by Johannes Hofmann
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "Hill.H"

static double pi_d, deg2rad;

Hill::Hill(const char *n, double p, double l, double h) {
  name = strdup(n);
  phi = p;
  lam = l;
  height = h;
  alph = 0.0;
  x = 0;
  y = 0;
}

Hill::Hill(int x_tmp, int y_tmp) {
  name = "";
  phi = 0.0;
  lam = 0.0;
  height = 0.0;
  alph = 0.0;
  x = x_tmp;
  y = y_tmp;
}

Hill::~Hill() {
  if (name) {
    free(name);
  }
}


Hills::Hills() {
  num = 0;
  cap = 100;
  m = (Hill **) malloc(cap * sizeof(Hill *));

  pi_d = asin(1.0) * 2.0;
  deg2rad = pi_d / 180.0;
}

int
Hills::load(const char *file) {
  FILE *fp;
  char buf[4000];
  char *vals[10];
  char **ap, *bp;
  double phi, lam, height;
  Hill *m;

  fp = fopen(file, "r");
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

    m = new Hill(vals[1], phi, lam, height);

    add(m);
  }

  fclose(fp);

  return 0;
}


Hills::~Hills() {
  if (m) {
    free(m);
  }
}


void
Hills::add(Hill *m1) {
  if (num >= cap) {
    cap = cap?cap * 2:100;
    m = (Hill **) realloc(m, cap * sizeof(Hill *));
  }

  m[num++] = m1;
}


static int
comp_mountains(const void *n1, const void *n2) {
  Hill *m1 = *(Hill **)n1;
  Hill *m2 = *(Hill **)n2;
  
  if (m1 && m2) {
    if (m1->alph < m2->alph) {
      return 1;
    } else if (m1->alph > m2->alph) {
      return -1;
    } else {
      return 0;
    }
  } else {
    return 0;
  }  
}

void
Hills::sort() {
  if (!m) {
    return;
  }

  qsort(m, num, sizeof(Hill *), comp_mountains);
}

void
Hills::clear() {
  if (m) {
    free(m);
    m = NULL;
  }
  cap = 0;
  num = 0;
}

void
Hills::clobber() {
  int i;
  
  for(i=0; i<get_num();i++) {
    if (get(i)) {
      delete(get(i));
    }
  }

  clear();
}

int
Hills::get_num() {
  return num;
}

Hill *
Hills::get(int n) {
  if (n < 0 || n >= num) {
    return NULL;
  } else {
    return m[n];
  }
}
