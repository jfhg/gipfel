//
// Copyright 2006 Johannes Hofmann <Johannes.Hofmann@gmx.de>
//
// This software may be used and distributed according to the terms
// of the GNU General Public License, incorporated herein by reference.

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
}

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

void
Fl_Search_Chooser::input_cb(Fl_Input* in, void*c) {
	Fl_Search_Browser *sb = ((Fl_Search_Chooser *) c)->sb;
	sb->find_prefix(in->value()); 
}    

void
Fl_Search_Chooser::ok_cb(Fl_Input* in, void*c) {
	Fl_Search_Chooser *sc = (Fl_Search_Chooser *) c;
	sc->close();
} 

void
Fl_Search_Chooser::cancel_cb(Fl_Input* in, void*c) {
	Fl_Search_Chooser *sc = (Fl_Search_Chooser *) c;
	sc->sb->deselect();
	sc->close();
}    

Fl_Search_Chooser::Fl_Search_Chooser(const char *title) : Fl_Window(320, 320, title?title:"Choose") {
	callback((Fl_Callback*) cancel_cb, this);
	visible_focus = Fl::visible_focus();
	Fl::visible_focus(0);
	Fl_Group *g = new Fl_Group(10, 10, w() - 10, h() - 10);
	sb = new Fl_Search_Browser(g->x(), g->y(), g->w() , g->h() - 100, NULL);
	sb->type(FL_HOLD_BROWSER);
	Fl_Input *in = new Fl_Input(g->x()+50, g->h()-80, g->w()-80, 20, "Search");
	in->callback((Fl_Callback*) input_cb, this);
	in->when(FL_WHEN_CHANGED);
	Fl_Button *cancel_b = new Fl_Button(g->w()-200, g->h()-30, 80, 25, "Cancel");
	cancel_b->callback((Fl_Callback*) cancel_cb, this);
	Fl_Button *ok_b = new Fl_Button(g->w()-100, g->h()-30, 80, 25, "Ok");
	ok_b->callback((Fl_Callback*) ok_cb, this);
	Fl::focus(in);
	g->end();
	end();
}

void
Fl_Search_Chooser::close() {
	hide();
	Fl::visible_focus(visible_focus);
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

int
Fl_Search_Chooser::handle(int event) {
	switch(event) {
		case FL_KEYBOARD:
			int key = Fl::event_key();

			if (key == FL_Up || key == FL_Down) {
				return sb->handle(event);
			} else if (key == FL_Enter) {
				close();
			}
	}

	return Fl_Window::handle(event);
}
