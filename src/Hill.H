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

#ifndef HILL_H
#define HILL_H


class Hill;

class Hill {
 private:

 public:
  typedef enum {
    DUPLICATE   = 0x01,
    TRACK_POINT = 0x02,
    VISIBLE     = 0x04,
    HIDDEN      = 0x08
  } flags_t;

  double phi, lam;
  double height;
  double alph;
  double a_view;
  double a_nick;
  double dist;
  int x, y;
  int label_x, label_y;
  char *name;
  int flags;
 
  Hill(const char *n, double p, double l, double h);

  Hill(const Hill& h);

  Hill(int x_tmp, int y_tmp);

  ~Hill();  
};


class Hills {
 private:
  int num, cap;
  Hill **m;

 public:
  Hills();

  Hills(const Hills *h);

  ~Hills();

  int load(const char *file);

  void mark_duplicates(double dist);

  void add(Hill *m);

  void add(Hills *h);

  void sort_phi();

  void sort_name();

  void sort();
  
  void clear();

  void clobber();

  int get_num();
  
  Hill *get(int n);
};
#endif
