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
	private:
		int find_prefix(const char *p, int s, int e);

	public:
		Fl_Search_Browser(int X, int Y, int W, int H, const char *c):Fl_Select_Browser(X,Y,W,H,c){};

		int find_prefix(const char *p);
};

class Fl_Search_Chooser : public Fl_Window {
	private:
		Fl_Search_Browser *sb;
		int visible_focus;
	
		static void input_cb(Fl_Input* in, void*c);	
		static void ok_cb(Fl_Input* in, void*c);	
		static void cancel_cb(Fl_Input* in, void*c);	

	public:
		Fl_Search_Chooser(const char *title);

		void close();
		void add(const char *t, void *d);
		void *data();
		int handle(int event);
};

#endif
