//
// Search Chooser widget for the Fast Light Tool Kit (FLTK).
// 
// Copyright by Johannes Hofmann
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
//

#include <stdio.h>
#include <string.h>
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Button.H>
#include "Fl_Search_Chooser.H"

int
Fl_Search_Browser::find_prefix(const char *p) {
  int i = find_prefix(p, 1, size());
  if (i == -1) {
    return 1;
  } else {
    deselect();
    middleline(i);
    select(i);
    return 0;
  }
};

int
Fl_Search_Browser::find_prefix(const char *p, int s, int e) {
  if (s < 0 || e > size() || s > e) {
    fprintf(stderr, "Invalid search range %d %d\n", s, e);
    return 1;
  } else if (e - s <= 1) {
    if (strncasecmp(p, text(s), strlen(p)) == 0) {
       return s;
    } else if (strncasecmp(p, text(e), strlen(p)) == 0){
       return e;
    } else {
       return -1;
    }
  } else {
    int med = s + (e - s) / 2;
    if (strncasecmp(p, text(med), strlen(p)) > 0) {
      return find_prefix(p, med, e);
    } else {
      return find_prefix(p, s, med);
    }
  }
} 


static void input_cb(Fl_Input* in, void*c) {
  Fl_Search_Browser *sb = ((Fl_Search_Chooser *) c)->sb;
  sb->find_prefix(in->value()); 
}    


static void ok_cb(Fl_Input* in, void*c) {
  Fl_Search_Chooser *sc = (Fl_Search_Chooser *) c;
  sc->w->hide();
} 


static void cancel_cb(Fl_Input* in, void*c) {
  Fl_Search_Chooser *sc = (Fl_Search_Chooser *) c;
  sc->sb->deselect();
  sc->w->hide();
}    

Fl_Search_Chooser::Fl_Search_Chooser(const char *title) {
  w = new Fl_Window(320, 320, title?title:"Choose");
  Fl_Group *g = new Fl_Group(10, 10, w->w() - 10, w->h() - 10);
  sb = new Fl_Search_Browser(g->x(), g->y(), g->w() , g->h() - 100, NULL);
  sb->type(FL_HOLD_BROWSER);
  Fl_Input *in = new Fl_Input(g->x()+50, g->h()-80, g->w()-80, 20, "Search");
  in->callback((Fl_Callback*) input_cb, this);
  in->when(FL_WHEN_CHANGED);
  Fl_Button *cancel_b = new Fl_Button(g->w()-200, g->h()-30, 80, 20, "Cancel");
  cancel_b->callback((Fl_Callback*) cancel_cb, this);
  Fl_Button *ok_b = new Fl_Button(g->w()-100, g->h()-30, 80, 20, "Ok");
  ok_b->callback((Fl_Callback*) ok_cb, this);
  Fl::focus(in);
  g->end();
  w->end();
}

Fl_Search_Chooser::~Fl_Search_Chooser() {
  delete sb;
  delete w;
}

void
Fl_Search_Chooser::add(const char *t, void *d) {
  sb->add(t, d);
}

void *
Fl_Search_Chooser::data() {
  int v = sb->value();
  if (v) {
    return sb->data(v);
  } else {
    return NULL;
  }
}

void
Fl_Search_Chooser::show() {
  w->show();
}

int
Fl_Search_Chooser::shown() {
  return w->shown();
}

