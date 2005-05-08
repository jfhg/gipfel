// 
// "$Id: Hill.H,v 1.11 2005/05/08 18:00:11 hofmann Exp $"
//
// X11 header file for the Fast Light Tool Kit (FLTK).
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

#ifndef HILL_H
#define HILL_H

class Mountain;

class Mountain {
 private:

 public:
  double phi, lam;
  double height;
  double alph;
  double a_view;
  double a_nick;
  double dist;
  int x, y;
  int label_x, label_y;
  char *name;
 
  Mountain(const char *n, double p, double l, double h);

  Mountain(int x_tmp, int y_tmp);

  ~Mountain();  
};


class Mountains {
 private:
  int num, cap;
  Mountain **m;

 public:
  Mountains();

  ~Mountains();

  void add(Mountain *m);

  void sort();
  
  void clear();

  void clobber();

  int get_num();
  
  Mountain *get(int n);
};
#endif
