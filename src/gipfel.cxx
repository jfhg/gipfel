// 
// "$Id: gipfel.cxx,v 1.18 2005/05/05 19:44:08 hofmann Exp $"
//
// flpsed program.
//
// Copyright 2004 by Johannes Hofmann
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

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_File_Chooser.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Int_Input.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Menu_Item.H>

#include "GipfelWidget.H"

char *img_file;
char *data_file;
GipfelWidget *gipf = NULL;
Fl_Slider *s_center = NULL, *s_nick = NULL, *s_scale = NULL, *s_tilt = NULL;

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
    fprintf(stderr, " == cent %f\n", gipf->get_center_angle());
    s_center->value(gipf->get_center_angle());
    s_nick->value(gipf->get_nick_angle());
    s_scale->value(gipf->get_scale());
    s_tilt->value(gipf->get_tilt_angle());
  }
}

void guess_cb(Fl_Widget *, void *) {
  if (gipf) {
    gipf->guess();
    s_center->value(gipf->get_center_angle());
    s_nick->value(gipf->get_nick_angle());
    s_scale->value(gipf->get_scale());
    s_tilt->value(gipf->get_tilt_angle());
  }
}

void about_cb() {
  fl_message("flpsed -- a pseudo PostScript editor\n"
	     "(c) Johannes Hofmann 2004, 2005\n\n"
	     "PostScript is a registered trademark of Adobe Systems");
}

Fl_Menu_Item menuitems[] = {
  { "&File",              0, 0, 0, FL_SUBMENU },
    { "&Open File...",    FL_CTRL + 'o', (Fl_Callback *)open_cb },
  {0},
  { 0 }
};
  

void usage() {
  fprintf(stderr,
	  "usage: flpsed [-hbd] [-t <tag>=<value>] [<infile>] [<outfile>]\n"
	  "   -h                 print this message\n"
	  "   -b                 batch mode (no gui)\n"
	  "   -d                 dump tags and values from a document\n"
	  "                      to stdout (this implies -b)\n"
	  "   -t <tag>=<value>   set text to <value> where tag is <tag>\n"
	  "   <infile>           optional input file; in batch mode if no\n"
	  "                      input file is given, input is read from stdin\n"
	  "   <outfile>          optional output file for batch mode; if no\n"
	  "                      output file is given, output is written to stdout\n");
}


int main(int argc, char** argv) {
  char c, *sep, *tmp, **my_argv;
  char *view_point = NULL;
  int err, bflag = 0, dflag = 0, my_argc;
  Fl_Window *win;
  Fl_Scroll *scroll;
  Fl_Menu_Bar *m;
  
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
  
  if (err) {
    usage();
    exit(1);
  }

  my_argc = argc - optind;
  my_argv = argv + optind;

  if (my_argc >= 1) {
    img_file = my_argv[0];
  }

  win = new Fl_Window(800,700);
  m = new Fl_Menu_Bar(0, 0, 800, 30);
  m->menu(menuitems);

  s_center = new Fl_Slider(50, 30, 750, 15, "direction");
  s_center->type(1);
  s_center->box(FL_THIN_DOWN_BOX);
  s_center->labelsize(10);
  s_center->step(0.00001);
  s_center->bounds(-3.14, 3.14);
  s_center->slider(FL_UP_BOX);
  s_center->callback((Fl_Callback*)angle_cb);
  s_center->align(FL_ALIGN_LEFT);

  s_scale = new Fl_Slider(100, 45, 160, 15, "scale");
  s_scale->type(1);
  s_scale->box(FL_THIN_DOWN_BOX);
  s_scale->labelsize(10);
  s_scale->step(5.0);
  s_scale->bounds(0.0, 10000.0);
  s_scale->slider(FL_UP_BOX);
  s_scale->callback((Fl_Callback*)scale_cb);
  s_scale->align(FL_ALIGN_LEFT);

  s_nick = new Fl_Slider(360, 45, 160, 15, "nick");
  s_nick->type(1);
  s_nick->box(FL_THIN_DOWN_BOX);
  s_nick->labelsize(10);
  s_nick->step(0.00001);
  s_nick->bounds(-0.5, 0.5);
  s_nick->slider(FL_UP_BOX);
  s_nick->callback((Fl_Callback*)nick_cb);
  s_nick->align(FL_ALIGN_LEFT);

  s_tilt = new Fl_Slider(50, 60, 160, 15, "tilt");
  s_tilt->type(1);
  s_tilt->box(FL_THIN_DOWN_BOX);
  s_tilt->labelsize(10);
  s_tilt->step(0.005);
  s_tilt->bounds(-0.1, 0.1);
  s_tilt->slider(FL_UP_BOX);
  s_tilt->callback((Fl_Callback*)tilt_cb);
  s_tilt->align(FL_ALIGN_LEFT);

  Fl_Slider* s_height_dist = new Fl_Slider(620, 45, 160, 15, "height-dist");
  s_height_dist->type(1);
  s_height_dist->box(FL_THIN_DOWN_BOX);
  s_height_dist->labelsize(10);
  s_height_dist->step(-0.005);
  s_height_dist->bounds(0.2, 0.01);
  s_height_dist->slider(FL_UP_BOX);
  s_height_dist->callback((Fl_Callback*)h_d_cb);
  s_height_dist->align(FL_ALIGN_LEFT);

  Fl_Button *b = new Fl_Button(200, 60, 20, 15, "comp");
  b->callback(comp_cb);
  Fl_Button *b1 = new Fl_Button(250, 60, 20, 15, "guess");
  b1->callback(guess_cb);

  scroll = new Fl_Scroll(0, 75, win->w(), win->h()-75);
  
  gipf = new GipfelWidget(0,75,500,500);

  gipf->load_image(img_file);
  gipf->load_data(data_file);
  if (view_point) {
    gipf->set_viewpoint(view_point);
  }
  scroll->end();  

  s_center->value(gipf->get_center_angle());
  s_nick->value(gipf->get_nick_angle());
  s_scale->value(gipf->get_scale());
  s_tilt->value(gipf->get_tilt_angle());

  win->resizable(scroll);
  
  win->end();
  win->show(1, argv); 
  
  return Fl::run();
}
