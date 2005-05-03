// 
// "$Id: Hill.cxx,v 1.5 2005/05/03 20:04:14 hofmann Exp $"
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "Mountain.H"

Mountain::Mountain(const char *n, double p, double l, double h) {
  name = strdup(n);
  phi = p;
  lam = l;
  height = h;
  alph = 0.0;
  x = 0;
  y = 0;
  next = NULL;
  next_visible = NULL;
}

Mountain::Mountain(int x_tmp, int y_tmp) {
  name = "";
  phi = 0.0;
  lam = 0.0;
  height = 0.0;
  alph = 0.0;
  x = x_tmp;
  y = y_tmp;
  next = NULL;
  next_visible = NULL;
}

Mountain::~Mountain() {
  if (next) {
    delete(next);
  }
  
  if (name) {
    free(name);
  }
}

void
Mountain::append(Mountain *m) {
  if (next) {
    next->append(m);
  } else {
    next = m;
  }
}

Mountain *
Mountain::get_next() {
  return next;
}

void
Mountain::append_visible(Mountain *m) {
  if (next_visible) {
    next->append_visible(m);
  } else {
    next_visible = m;
  }
}

Mountain *
Mountain::get_next_visible() {
  return next_visible;
}

void
Mountain::clear_next_visible() {
  next_visible = NULL;
}



Mountains::Mountains() {
  fprintf(stderr, "===>huhu\n");
  num = 0;
  cap = 100;
  m = (Mountain **) malloc(cap * sizeof(class Mountain *));
}

Mountains::~Mountains() {
  if (m) {
    free(m);
  }
}


void
Mountains::add(Mountain *m1) {
  if (num >= cap) {
    cap = cap?cap * 2:100;
    m = (Mountain **) realloc(m, cap * sizeof(class Mountain *));
  }

  m[num++] = m1;
}

void
Mountains::clear() {
  if (m) {
    free(m);
    m = NULL;
  }
  cap = 0;
  num = 0;
}

void
Mountains::clobber() {
  int i;
  
  for(i=0; i<get_num();i++) {
    if (get(i)) {
      delete(get(i));
    }
  }

  clear();
}

int
Mountains::get_num() {
  return num;
}

Mountain *
Mountains::get(int n) {
  if (n < 0 || n >= num) {
    return NULL;
  } else {
    return m[n];
  }
}
