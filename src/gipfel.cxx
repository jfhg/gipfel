//
// Copyright 2006 Johannes Hofmann <Johannes.Hofmann@gmx.de>
//
// This software may be used and distributed according to the terms
// of the GNU General Public License, incorporated herein by reference.

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>

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
#include <FL/Fl_Value_Input.H>

#include "Fl_Value_Dial.H"
#include "Fl_Search_Chooser.H"
#include "GipfelWidget.H"
#include "JPEGOutputImage.H"
#include "TIFFOutputImage.H"
#include "PreviewOutputImage.H"
#include "Stitch.H"
#include "choose_hill.H"
#include "../config.h"

#ifndef DATADIR
#define DATADIR "/usr/local/share"
#endif

#define DEFAULT_DATAFILE DATADIR "/" PACKAGE_NAME "/alpinkoordinaten.dat"

char *img_file;
char *data_file = DEFAULT_DATAFILE;

GipfelWidget *gipf = NULL;
Fl_Window *control_win, *view_win;
Fl_Dial *s_center = NULL;
Fl_Slider *s_nick, *s_focal_length, *s_tilt, *s_height_dist, *s_track_width;
Fl_Value_Input *i_view_lat, *i_view_long, *i_view_height;
Fl_Value_Input *i_distortion_k0, *i_distortion_k1, *i_distortion_x0;
Fl_Box *b_viewpoint;
Fl_Menu_Bar *mb;

#define STITCH_PREVIEW           1
#define STITCH_JPEG              2
#define STITCH_TIFF              4
#define STITCH_VIGNETTE_CALIB    8

static int stitch(GipfelWidget::sample_mode_t m ,int stitch_w, int stitch_h,
	double from, double to, int type, const char *path, int argc, char **argv);

void set_values() {
	double k0 = 0.0, k1 = 0.0, x0 = 0.0;

	s_center->value(gipf->get_center_angle());
	s_nick->value(gipf->get_nick_angle());
	s_focal_length->value(gipf->get_focal_length_35mm());
	s_tilt->value(gipf->get_tilt_angle());
	s_height_dist->value(gipf->get_height_dist_ratio());
	i_view_lat->value(gipf->get_view_lat());
	i_view_long->value(gipf->get_view_long());
	i_view_height->value(gipf->get_view_height());
	b_viewpoint->label(gipf->get_viewpoint());
	if (gipf->get_projection() == ProjectionLSQ::RECTILINEAR) {
		mb->mode(8, FL_MENU_RADIO|FL_MENU_VALUE);
		mb->mode(9, FL_MENU_RADIO);
	} else {
		mb->mode(9, FL_MENU_RADIO|FL_MENU_VALUE);
		mb->mode(8, FL_MENU_RADIO);
	}

	gipf->get_distortion_params(&k0, &k1, &x0);
	i_distortion_k0->value(k0);
	i_distortion_k1->value(k1);
	i_distortion_x0->value(x0);
}

void quit_cb() {
	exit(0);
}

void open_cb() {
	char *file = fl_file_chooser("Open File?", "*.jpg", img_file);
	if(file != NULL) {
		gipf->load_image(file);
		view_win->label(file);
		control_win->label(file);
		set_values();
	}  
}

void track_cb() {
	char *file = fl_file_chooser("Track File?", NULL, NULL);
	if (gipf->load_track(file) == 0) {
		s_track_width->activate();
	}
}

void save_cb() {
	char *file = fl_file_chooser("Save Image As?", NULL, NULL);
	if (file) {
		gipf->save_image(file);
	}
}

void focal_length_cb(Fl_Slider* o, void*) {
	gipf->set_focal_length_35mm(o->value());
}

void angle_cb(Fl_Slider* o, void*) {
	gipf->set_center_angle(o->value());
}

void nick_cb(Fl_Slider* o, void*) {
	gipf->set_nick_angle(o->value());
}

void tilt_cb(Fl_Slider* o, void*) {
	gipf->set_tilt_angle(o->value());
}

void h_d_cb(Fl_Slider* o, void*) {
	gipf->set_height_dist_ratio(o->value());
}

void view_lat_cb(Fl_Value_Input* o, void*) {
	gipf->set_view_lat(o->value());
}

void view_long_cb(Fl_Value_Input* o, void*) {
	gipf->set_view_long(o->value());
}

void view_height_cb(Fl_Value_Input* o, void*) {
	gipf->set_view_height(o->value());
}

void track_width_cb(Fl_Value_Input* o, void*) {
	gipf->set_track_width(o->value());
}

void viewpoint_cb(Fl_Value_Input* o, void*) {
	Hill *m = choose_hill(gipf->get_mountains(), "Choose Viewpoint");
	if (m) {
		gipf->set_viewpoint(m);
		set_values();
	}
}

void proj_cb(Fl_Value_Input* o, void*d) {
	if(d == NULL) {
		gipf->set_projection(ProjectionLSQ::RECTILINEAR);
	} else {
		gipf->set_projection(ProjectionLSQ::CYLINDRICAL);
	}
}

void hidden_cb(Fl_Menu_* o, void*d) {
	gipf->set_show_hidden(o->mvalue()->value()); 
}

void comp_cb(Fl_Widget *, void *) {
	gipf->comp_params();
	set_values();
}

void save_distortion_cb(Fl_Widget *, void *) {
	char buf[1024];
	const char * prof_name;
	double k0, k1, x0;

	gipf->get_distortion_params(&k0, &k1, &x0);
	gipf->get_distortion_profile_name(buf, sizeof(buf));
	prof_name = fl_input("Save Distortion Profile (k0=%f, k1=%f, x0=%f)",
		buf, k0, k1, x0);

	if (prof_name == NULL) {
		return;
	}

	gipf->save_distortion_params(prof_name);
	set_values();
}

void load_distortion_cb(Fl_Widget *, void *) {
	char buf[1024];
	const char * prof_name;

	gipf->get_distortion_profile_name(buf, sizeof(buf));
	prof_name = fl_input("Load Distortion Profile", buf);

	if (prof_name == NULL) {
		return;
	}

	if (gipf->load_distortion_params(prof_name) != 0) {
		fl_alert("Could not load profile %s.", prof_name);
	} else {
		set_values();
	}
}

void distortion_cb(Fl_Value_Input*, void*) {
	gipf->set_distortion_params(
		i_distortion_k0->value(),
		i_distortion_k1->value(),
		i_distortion_x0->value());
}

void about_cb() {
	fl_message("gipfel -- and you know what you see.\n"
		"Version %s\n\n"
		"(c) Johannes Hofmann 2006\n\n"
		"Default datafile by http://www.alpin-koordinaten.de\n",
		VERSION);

}

void fill_menubar(Fl_Menu_Bar* mb) {
	mb->add("&File/L&oad Image", FL_CTRL+'o', (Fl_Callback*)open_cb);
	mb->add("&File/&Save Image", FL_CTRL+'s', (Fl_Callback*)save_cb);
	mb->add("&File/Choose &Viewpoint", FL_CTRL+'v', (Fl_Callback*)viewpoint_cb);
	mb->add("&File/Load &Track", FL_CTRL+'t', (Fl_Callback*)track_cb);
	mb->add("&File/&Quit", FL_CTRL+'q', (Fl_Callback*)quit_cb);

	mb->add("&Projection/Normal Projection", 0, (Fl_Callback *)proj_cb, 
		(void *)0, FL_MENU_RADIO|FL_MENU_VALUE);
	mb->add("&Projection/Panoramic Projection", 0, (Fl_Callback *)proj_cb, 
		(void *)1, FL_MENU_RADIO);

	mb->add("&Distortion/Load Profile", 0, (Fl_Callback *)load_distortion_cb);
	mb->add("&Distortion/Save Profile", 0, (Fl_Callback *)save_distortion_cb);

	mb->add("&Option/Show Hidden", 0, (Fl_Callback *) hidden_cb, 
		(void *)0, FL_MENU_TOGGLE);

	mb->add("&Help/About", 0, (Fl_Callback*)about_cb);
}

void usage() {
	fprintf(stderr,
		"usage: gipfel [-v <viewpoint>] [-d <datafile>]\n"
		"          [-s] [-j <outfile>] [-t <outdir] [-w <width>] [-h <height>]\n"
		"          [<image(s)>]\n"
		"   -v <viewpoint>  Set point from which the picture was taken.\n"
		"                   This must be a string that unambiguously \n"
		"                   matches the name of an entry in the data file.\n"
		"   -d <datafile>   Use <datafile> for GPS data.\n"
		"   -u <k0>,<k1>    Use distortion correction values k0,k1.\n"
		"   -s              Stitch mode.\n"
		"   -r <from>,<to>  Stitch range in degrees (e.g. 100.0,200.0).\n"
		"   -b              Use bilinear interpolation for stitching.\n"
		"   -w <width>      Width of result image.\n"
		"   -h <height>     Height of result image.\n"
		"   -j <outfile>    JPEG output file for Stitch mode.\n"
		"   -t <outdir>     Output directory for TIFF images in Stitch mode.\n"
		"      <image(s)>   JPEG file(s) to use.\n");
}

Fl_Window * 
create_control_window() {
	Fl_Window *win = new Fl_Window(400,400);
	mb = new Fl_Menu_Bar(0, 0, 400, 30);
	fill_menubar(mb);

	s_center = new Fl_Value_Dial(40, 60, 150, 150, NULL);
	s_center->type(FL_LINE_DIAL);
	s_center->labelsize(10);
	s_center->step(0.01);
	s_center->bounds(0.0, 360.0);
	s_center->angles(180, 540);
	s_center->callback((Fl_Callback*)angle_cb);

	win->add(new Fl_Box(95, 40, 40, 20, "North"));
	win->add(new Fl_Box(95, 210, 40, 20, "South"));
	win->add(new Fl_Box(190, 125, 40, 20, "East"));
	win->add(new Fl_Box(0, 125, 40, 20, "West"));

	s_focal_length = new Fl_Value_Slider(235, 60, 160, 15, "Focal Length in 35mm");
	s_focal_length->type(1);
	s_focal_length->box(FL_THIN_DOWN_BOX);
	s_focal_length->labelsize(10);
	s_focal_length->step(0.01);
	s_focal_length->bounds(1.0, 300.0);
	s_focal_length->slider(FL_UP_BOX);
	s_focal_length->callback((Fl_Callback*)focal_length_cb);
	s_focal_length->align(FL_ALIGN_TOP);

	s_nick = new Fl_Value_Slider(235, 90, 160, 15, "Nick (deg.)");
	s_nick->type(1);
	s_nick->box(FL_THIN_DOWN_BOX);
	s_nick->labelsize(10);
	s_nick->step(0.01);
	s_nick->bounds(-20.0, 20.0);
	s_nick->slider(FL_UP_BOX);
	s_nick->callback((Fl_Callback*)nick_cb);
	s_nick->align(FL_ALIGN_TOP);

	s_tilt = new Fl_Value_Slider(235, 120, 160, 15, "Tilt (deg.)");
	s_tilt->type(1);
	s_tilt->box(FL_THIN_DOWN_BOX);
	s_tilt->labelsize(10);
	s_tilt->step(0.01);
	s_tilt->bounds(-10.0, 10.0);
	s_tilt->slider(FL_UP_BOX);
	s_tilt->callback((Fl_Callback*)tilt_cb);
	s_tilt->align(FL_ALIGN_TOP);

	s_height_dist = new Fl_Value_Slider(235, 150, 160, 15, "Visibility");
	s_height_dist->type(1);
	s_height_dist->box(FL_THIN_DOWN_BOX);
	s_height_dist->labelsize(10);
	s_height_dist->step(-0.001);
	s_height_dist->bounds(0.1, 0.01);
	s_height_dist->slider(FL_UP_BOX);
	s_height_dist->callback((Fl_Callback*)h_d_cb);
	s_height_dist->align(FL_ALIGN_TOP);

	s_track_width = new Fl_Value_Slider(235, 180, 160, 15, "Track Width");
	s_track_width->type(1);
	s_track_width->box(FL_THIN_DOWN_BOX);
	s_track_width->labelsize(10);
	s_track_width->step(1.0);
	s_track_width->bounds(1.0, 500.0);
	s_track_width->value(200.0);
	s_track_width->slider(FL_UP_BOX);
	s_track_width->callback((Fl_Callback*)track_width_cb);
	s_track_width->align(FL_ALIGN_TOP);
	s_track_width->deactivate();

	// Viewpoint Stuff
	b_viewpoint = new Fl_Box(FL_DOWN_BOX, 30, 305, 300, 80, "");
	b_viewpoint->align(FL_ALIGN_TOP);

	i_view_lat = new Fl_Value_Input(40, 320, 100, 20, "Latitude");
	i_view_lat->labelsize(10);
	i_view_lat->align(FL_ALIGN_TOP);
	i_view_lat->when(FL_WHEN_ENTER_KEY);
	i_view_lat->callback((Fl_Callback*)view_lat_cb);

	i_view_long = new Fl_Value_Input(200, 320, 100, 20, "Longitude");
	i_view_long->labelsize(10);
	i_view_long->align(FL_ALIGN_TOP);
	i_view_long->when(FL_WHEN_ENTER_KEY);
	i_view_long->callback((Fl_Callback*)view_long_cb);

	i_view_height = new Fl_Value_Input(40, 360, 80, 20, "Height");
	i_view_height->labelsize(10);
	i_view_height->align(FL_ALIGN_TOP);
	i_view_height->when(FL_WHEN_ENTER_KEY);
	i_view_height->callback((Fl_Callback*)view_height_cb);

	i_distortion_k0 = new Fl_Value_Input(250, 200, 80, 20, "k0");
	i_distortion_k0->labelsize(10);
	i_distortion_k0->textsize(10);
	i_distortion_k0->align(FL_ALIGN_LEFT);
	i_distortion_k0->when(FL_WHEN_ENTER_KEY);
	i_distortion_k0->callback((Fl_Callback*)distortion_cb);

	i_distortion_k1 = new Fl_Value_Input(250, 225, 80,  20, "k1");
	i_distortion_k1->labelsize(10);
	i_distortion_k1->textsize(10);
	i_distortion_k1->align(FL_ALIGN_LEFT);
	i_distortion_k1->when(FL_WHEN_ENTER_KEY);
	i_distortion_k1->callback((Fl_Callback*)distortion_cb);

	i_distortion_x0 = new Fl_Value_Input(250, 250, 80,  20, "x0");
	i_distortion_x0->labelsize(10);
	i_distortion_x0->textsize(10);
	i_distortion_x0->align(FL_ALIGN_LEFT);
	i_distortion_x0->when(FL_WHEN_ENTER_KEY);
	i_distortion_x0->callback((Fl_Callback*)distortion_cb);



	// Buttons
	Fl_Button *b = new Fl_Button(280, 280, 100, 20, "comp");
	b->color(FL_RED);
	b->tooltip("compute view parameter from given mountains");
	b->callback(comp_cb);

	win->end();
	return win;
}

int main(int argc, char** argv) {
	char c, **my_argv;
	char *view_point = NULL;
	int err, my_argc;
	int stitch_flag = 0, stitch_w = 2000, stitch_h = 500;
	int jpeg_flag = 0, tiff_flag = 0, distortion_flag = 0;
	int bilinear_flag = 0, vignette_flag = 0;
	double stitch_from = 0.0, stitch_to = 380.0;
	double dist_k0 = 0.0, dist_k1 = 0.0, dist_x0 = 0.0;
	char *outpath = "/tmp";
	Fl_Scroll *scroll;


	err = 0;
	while ((c = getopt(argc, argv, ":?d:v:sw:h:j:t:u:br:V")) != EOF) {
		switch (c) {  
			case '?':
				usage();
				exit(0);
				break;
			case 'd':
				data_file = optarg;
				break;
			case 'v':
				view_point = optarg;
				break;
			case 's':
				stitch_flag++;
				break;
			case 'V':
				vignette_flag++;
				break;
			case 'r':
				stitch_flag++;
				if (optarg && strcmp(optarg, ":")) {
					stitch_from = atof(optarg);
					if (strchr(optarg, ',')) {
						stitch_to = atof(strchr(optarg, ',') + 1);
					}
				}
				break;
			case 'u':
				char *c;
				distortion_flag++;
				if (optarg && strcmp(optarg, ":")) {
					dist_k0 = atof(optarg);
					c = strchr(optarg, ',');
					if (c) {
						dist_k1 = atof(strchr(optarg, ',') + 1);
						c = strchr(c + 1, ',');
						if (c) {
							dist_x0 = atof(strchr(optarg, ',') + 1);
						}
					}
				}
				break;
			case 'j':
				jpeg_flag++;
				outpath = optarg;
				break;
			case 't':
				tiff_flag++;
				outpath = optarg;
				break;
			case 'w':
				stitch_w = atoi(optarg);
				break;
			case 'h':
				stitch_h = atoi(optarg);
				break;
			case 'b':
				bilinear_flag++;
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

	if (data_file == NULL || err) {
		usage();
		exit(1);
	}

	if (stitch_flag) {
		int type = STITCH_PREVIEW;
		if (jpeg_flag) {
			type = STITCH_JPEG;
		} else if (tiff_flag) {
			type = STITCH_TIFF;
		} else if (vignette_flag) {
			type = STITCH_VIGNETTE_CALIB;
		}

		stitch(bilinear_flag?GipfelWidget::BILINEAR:GipfelWidget::NEAREST,
			stitch_w, stitch_h, stitch_from, stitch_to,
			type, outpath, my_argc, my_argv);

		exit(0);
	}

	Fl::get_system_colors();
	if (getenv("FLTK_SCHEME")) {
		Fl::scheme(NULL);
	} else {
		Fl::scheme("plastic");
	}

	control_win = create_control_window();

	view_win = new Fl_Window(800, 600);

	// The Fl_Group is used to avoid FL_DAMAGE_ALL in Fl_Scroll::position 
	Fl_Group *g = new Fl_Group(0, 0, view_win->w(), view_win->h()); 
	view_win->resizable(g);
	scroll = new Fl_Scroll(0, 0, view_win->w(), view_win->h());

	gipf = new GipfelWidget(0,0,800,600);
	if (img_file) {
		gipf->load_image(img_file);
		view_win->label(img_file);
		control_win->label(img_file);
	}

	if (distortion_flag) {
		gipf->set_distortion_params(dist_k0, dist_k1, dist_x0);
	}

	view_win->size(gipf->w(), gipf->h());
	scroll->size(gipf->w(), gipf->h());

	gipf->load_data(data_file);
	scroll->end();  

	set_values();

	view_win->resizable(scroll);

	view_win->end();
	view_win->show(1, argv); 
	control_win->show(1, argv); 

	if (view_point) {
		gipf->set_viewpoint(view_point);
	} else if (img_file && 
		(isnan(gipf->get_view_lat()) || isnan(gipf->get_view_long()))) {
		viewpoint_cb(NULL, NULL);
	}

	return Fl::run();
}

static int
stitch(GipfelWidget::sample_mode_t m,
	int stitch_w, int stitch_h, double from, double to,
	int type, const char *path, int argc, char **argv) {

	Fl_Window *win;
	Fl_Scroll *scroll;
	Stitch *st = new Stitch();

	for (int i=0; i<argc; i++) {
		st->load_image(argv[i]);
	}

	if (type == STITCH_JPEG) {

		st->set_output((OutputImage*) new JPEGOutputImage(path, 90));
		st->resample(m, stitch_w, stitch_h, from, to);

	} else if (type == STITCH_TIFF) {

		for (int i=0; i<argc; i++) {
			char buf[1024];
			char *dot;

			snprintf(buf, sizeof(buf), "%s/%s", path, argv[i]);
			dot = strrchr(buf, '.');
			*dot = '\0';
			strncat(buf, ".tiff", sizeof(buf));

			st->set_output(argv[i], (OutputImage*) new TIFFOutputImage(buf));
		}

		st->resample(m, stitch_w, stitch_h, from, to);

	} else {
		win = new Fl_Window(0,0, stitch_w, stitch_h);
		scroll = new Fl_Scroll(0, 0, win->w(), win->h());
		PreviewOutputImage *img =
			new PreviewOutputImage(0, 0, stitch_w, stitch_h);

		win->resizable(scroll);
		win->show(0, argv); 
		st->set_output((OutputImage*) img);

		if (type == STITCH_VIGNETTE_CALIB) {
			st->color_calib(m, stitch_w, stitch_h, from, to);
			//st->vignette_calib(m, stitch_w, stitch_h, from, to);
			st->resample(m, stitch_w, stitch_h, from, to);
		} else {
			st->resample(m, stitch_w, stitch_h, from, to);
		}

		img->redraw();
		Fl::run();
	}

	return 0;
}
