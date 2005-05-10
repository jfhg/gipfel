// 
// "$Id: Hill.H,v 1.13 2005/05/10 17:05:32 hofmann Exp $"
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
  double phi, lam;
  double height;
  double alph;
  double a_view;
  double a_nick;
  double dist;
  int x, y;
  int label_x, label_y;
  char *name;
 
  Hill(const char *n, double p, double l, double h);

  Hill(int x_tmp, int y_tmp);

  ~Hill();  
};


class Hills {
 private:
  int num, cap;
  Hill **m;

 public:
  Hills();

  ~Hills();

  void add(Hill *m);

  void sort();
  
  void clear();

  void clobber();

  int get_num();
  
  Hill *get(int n);
};
#endif
