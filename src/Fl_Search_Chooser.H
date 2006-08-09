//
// Value dial header file for the Fast Light Tool Kit (FLTK).
//
// Copyright 1998-2004 by Bill Spitzak and others.
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

#ifndef FL_SEARCH_CHOOSER_H
#define FL_SEARCH_CHOOSER_H

#include <FL/Fl_Window.H>
#include <FL/Fl_Select_Browser.H>
#include <FL/Fl_Input.H>

class Fl_Search_Browser : public Fl_Select_Browser {
  public:
    
   Fl_Search_Browser(int X, int Y, int W, int H, const char *c):Fl_Select_Browser(X,Y,W,H,c){};

   int find_prefix(const char *p);
  private:
   int find_prefix(const char *p, int s, int e);
};

class Fl_Search_Chooser {
  private:
  public:
    Fl_Window *w;
    Fl_Search_Browser *sb;

    Fl_Search_Chooser(const char *title);
    ~Fl_Search_Chooser();

    void add(const char *t, void *d);
    void *data();
    void show();
    int shown();
};

#endif
