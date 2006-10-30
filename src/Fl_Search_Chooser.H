//
// Copyright 2006 Johannes Hofmann <Johannes.Hofmann@gmx.de>
//
// This software may be used and distributed according to the terms
// of the GNU General Public License, incorporated herein by reference.

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
