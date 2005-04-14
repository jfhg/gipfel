// 
// "$Id: gipfel.cxx,v 1.7 2005/04/14 19:54:58 hofmann Exp $"
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
GipfelWidget *gipf;

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

void h_d_cb(Fl_Slider* o, void*) {
  if (gipf) {
    gipf->set_height_dist_ratio((double)(o->value()));
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

  win = new Fl_Window(600,700);
  m = new Fl_Menu_Bar(0, 0, 600, 30);
  m->menu(menuitems);
  Fl_Slider* s = new Fl_Slider(0, 30, 160, 15, "scale");
  s->type(1);
  s->box(FL_THIN_DOWN_BOX);
  s->labelsize(10);
  s->step(10.0);
  s->bounds(0.0, 2000.0);
  s->slider(FL_UP_BOX);
  s->callback((Fl_Callback*)scale_cb);
  Fl_Slider* a = new Fl_Slider(160, 30, 160, 15, "angle");
  a->type(1);
  a->box(FL_THIN_DOWN_BOX);
  a->labelsize(10);
  a->step(0.0001);
  a->bounds(-3.14, 3.14);
  a->slider(FL_UP_BOX);
  a->callback((Fl_Callback*)angle_cb);
  Fl_Slider* r = new Fl_Slider(320, 30, 160, 15, "height-dist");
  r->type(1);
  r->box(FL_THIN_DOWN_BOX);
  r->labelsize(10);
  r->step(-0.005);
  r->bounds(0.2, 0.02);
  r->slider(FL_UP_BOX);
  r->callback((Fl_Callback*)h_d_cb);


  scroll = new Fl_Scroll(0, 60, win->w(), win->h()-60);
  
  gipf = new GipfelWidget(0,60,500,500);

  gipf->load_image(img_file);
  gipf->load_data(data_file);
  if (view_point) {
    gipf->set_viewpoint(view_point);
  }
  scroll->end();  
    
  win->resizable(scroll);
  
  win->end();
  win->show(1, argv); 
  
  return Fl::run();
}
