// 
// "$Id: gipfel.cxx,v 1.24 2005/05/17 09:20:39 hofmann Exp $"
//
// gipfel program.
//
// Copyright 2005 by Johannes Hofmann
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
// USA.
//

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#include "../config.h"

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_File_Chooser.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Int_Input.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/Fl_Valuator.H>
#include <FL/Fl_Value_Slider.H>
#include "Fl_Value_Dial.H"

#include "GipfelWidget.H"

char *img_file;
char *data_file;
GipfelWidget *gipf = NULL;
Fl_Dial *s_center = NULL;
Fl_Slider *s_nick = NULL, *s_scale = NULL, *s_tilt = NULL, *s_height_dist;

void set_valuators() {
  s_center->value(gipf->get_center_angle());
  s_nick->value(gipf->get_nick_angle());
  s_scale->value(gipf->get_scale());
  s_tilt->value(gipf->get_tilt_angle());
  s_height_dist->value(gipf->get_height_dist_ratio());
}

void quit_cb() {
  exit(0);
}

void open_cb() {
  char *file = fl_file_chooser("Open File?", "*.jpeg", img_file);
  if(file != NULL) {

  }  
}

void scale_cb(Fl_Slider* o, void*) {
  if (gipf) {
    gipf->set_scale((double)(o->value()));
  }
}

void angle_cb(Fl_Slider* o, void*) {
  if (gipf) {
    gipf->set_center_angle((double)(o->value()));
  }
}

void nick_cb(Fl_Slider* o, void*) {
  if (gipf) {
    gipf->set_nick_angle((double)(o->value()));
  }
}

void tilt_cb(Fl_Slider* o, void*) {
  if (gipf) {
    gipf->set_tilt_angle((double)(o->value()));
  }
}

void h_d_cb(Fl_Slider* o, void*) {
  if (gipf) {
    gipf->set_height_dist_ratio((double)(o->value()));
  }
}

void comp_cb(Fl_Widget *, void *) {
  if (gipf) {
    gipf->comp_params();
    set_valuators();
  }
}

void guess_cb(Fl_Widget *, void *) {
  if (gipf) {
    gipf->guess();
    set_valuators();
  }
}

void about_cb() {
  fl_message("gipfel -- and you know what you see.\n"
	     "Version %s\n\n"
	     "(c) Johannes Hofmann 2005\n", VERSION);

}

Fl_Menu_Item menuitems[] = {
  { "&File",              0, 0, 0, FL_SUBMENU },
    { "&Quit", FL_CTRL + 'q', (Fl_Callback *)quit_cb, 0 },
  {0},
  { "&Help", 0, 0, 0, FL_SUBMENU },
    { "About",  0, (Fl_Callback *)about_cb },
  { 0 },
  { 0 }
};

void usage() {
  fprintf(stderr,
	  "usage: gipfel -v <viewpoint> -d <datafile> <image>\n"
	  "   -v <viewpoint>  Set point from which the picture was taken.\n"
	  "                   This must be a string that unambiguously \n"
	  "                   matches the name of an entry in the data file.\n"
	  "   -d <datafile>   Use <datafile> for GPS data.\n"
	  "      <image>      JPEG file to use.\n");
}

Fl_Window * 
create_control_window() {
  Fl_Menu_Bar *m;
  Fl_Window *win = new Fl_Window(400,250);
  m = new Fl_Menu_Bar(0, 0, 400, 30);
  m->menu(menuitems);

  s_center = new Fl_Value_Dial(40, 60, 150, 150, NULL);
  s_center->type(FL_LINE_DIAL);
  s_center->labelsize(10);
  s_center->step(0.001);
  s_center->bounds(0.0, 360.0);
  s_center->angles(180, 540);
  s_center->callback((Fl_Callback*)angle_cb);

  Fl_Box *north = new Fl_Box(95, 40, 40, 20, "North");
  Fl_Box *south = new Fl_Box(95, 210, 40, 20, "South");
  Fl_Box *east = new Fl_Box(190, 125, 40, 20, "East");
  Fl_Box *west = new Fl_Box(0, 125, 40, 20, "West");


  s_scale = new Fl_Value_Slider(235, 60, 160, 15, "Scale");
  s_scale->type(1);
  s_scale->box(FL_THIN_DOWN_BOX);
  s_scale->labelsize(10);
  s_scale->step(5.0);
  s_scale->bounds(0.0, 10000.0);
  s_scale->slider(FL_UP_BOX);
  s_scale->callback((Fl_Callback*)scale_cb);
  s_scale->align(FL_ALIGN_TOP);

  s_nick = new Fl_Value_Slider(235, 90, 160, 15, "Nick (deg.)");
  s_nick->type(1);
  s_nick->box(FL_THIN_DOWN_BOX);
  s_nick->labelsize(10);
  s_nick->step(0.001);
  s_nick->bounds(-20.0, 20.0);
  s_nick->slider(FL_UP_BOX);
  s_nick->callback((Fl_Callback*)nick_cb);
  s_nick->align(FL_ALIGN_TOP);

  s_tilt = new Fl_Value_Slider(235, 120, 160, 15, "Tilt (deg.)");
  s_tilt->type(1);
  s_tilt->box(FL_THIN_DOWN_BOX);
  s_tilt->labelsize(10);
  s_tilt->step(0.001);
  s_tilt->bounds(-10.0, 10.0);
  s_tilt->slider(FL_UP_BOX);
  s_tilt->callback((Fl_Callback*)tilt_cb);
  s_tilt->align(FL_ALIGN_TOP);

  s_height_dist = new Fl_Value_Slider(235, 150, 160, 15, "Visibility");
  s_height_dist->type(1);
  s_height_dist->box(FL_THIN_DOWN_BOX);
  s_height_dist->labelsize(10);
  s_height_dist->step(-0.005);
  s_height_dist->bounds(0.2, 0.01);
  s_height_dist->slider(FL_UP_BOX);
  s_height_dist->callback((Fl_Callback*)h_d_cb);
  s_height_dist->align(FL_ALIGN_TOP);

  Fl_Button *b = new Fl_Button(240, 180, 50, 15, "comp");
  b->color(FL_RED);
  b->callback(comp_cb);
  Fl_Button *b1 = new Fl_Button(320, 180, 50, 15, "guess");
  b1->callback(guess_cb);
  b1->color(FL_GREEN);

  win->end();
  return win;
}

int main(int argc, char** argv) {
  char c, *sep, *tmp, **my_argv;
  char *view_point = NULL;
  int err, bflag = 0, dflag = 0, my_argc;
  Fl_Window *control_win, *view_win;
  Fl_Scroll *scroll;

  
  err = 0;
  while ((c = getopt(argc, argv, "d:v:")) != EOF) {
    switch (c) {  
    case 'h':
      usage();
      exit(0);
      break;
    case 'd':
      data_file = optarg;
      break;
    case 'v':
      view_point = optarg;
      break;
    default:
      err++;
    }
  }
  

  my_argc = argc - optind;
  my_argv = argv + optind;

  if (my_argc >= 1) {
    img_file = my_argv[0];
  }

  if (data_file == NULL || view_point == NULL || img_file == NULL || err) {
    usage();
    exit(1);
  }


  control_win = create_control_window();

  view_win = new Fl_Window(700, 500);
  scroll = new Fl_Scroll(0, 0, view_win->w(), view_win->h());
  
  gipf = new GipfelWidget(0,0,700,500);

  gipf->load_image(img_file);
  gipf->load_data(data_file);
  if (view_point) {
    gipf->set_viewpoint(view_point);
  }
  scroll->end();  

  set_valuators();

  view_win->resizable(scroll);
  
  view_win->end();
  view_win->show(1, argv); 
  control_win->show(1, argv); 
  
  return Fl::run();
}
