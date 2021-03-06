//
// Copyright 2007-2014 Johannes Hofmann <Johannes.Hofmann@gmx.de>
//
// This software may be used and distributed according to the terms
// of the GNU General Public License, incorporated herein by reference.

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <math.h>
#include <assert.h>
#include <algorithm>

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
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Text_Buffer.H>

#include "Fl_Value_Dial.H"
#include "Fl_Search_Chooser.H"
#include "GipfelWidget.H"
#include "JPEGOutputImage.H"
#include "TIFFOutputImage.H"
#include "PreviewOutputImage.H"
#include "Stitch.H"
#include "ScreenDump.H"
#include "choose_hill.H"
#include "../config.h"

#ifndef STD_DATADIR
#define STD_DATADIR "/usr/local/share"
#endif

#define GIPFEL_DATADIR STD_DATADIR "/" PACKAGE_NAME
#define DATAFILE "gipfel.dat"

static char *run_dir = NULL;
static char *img_file = NULL;
static char *data_file = NULL;

static GipfelWidget *gipf = NULL;
static Fl_Scroll *scroll;
static Fl_Window *control_win, *view_win;
static Fl_Dial *s_center = NULL;
static Fl_Slider *s_nick, *s_focal_length, *s_tilt, *s_height_dist, *s_track_width;
static Fl_Value_Input *i_view_lat, *i_view_long, *i_view_height;
static Fl_Value_Input *i_distortion_k0, *i_distortion_k1, *i_distortion_x0;
static Fl_Box *b_viewpoint;
static Fl_Menu_Bar *mb;

#define STITCH_PREVIEW           1
#define STITCH_JPEG              2
#define STITCH_TIFF              4

static int stitch(ScanImage::mode_t m , int b_16,
	int stitch_w, int stitch_h,
	double from, double to, int type, const char *path, int argc, char **argv);

static int export_hills(const char *export_file, double visibility);
static int export_position();

static int
confirm_overwrite(const char *f) {
    struct stat sb;

    if (stat(f, &sb) == 0)
        return fl_choice("%s exists.\n", "Cancel", "Overwrite", NULL, f);
    else
        return 1;
}

static char *
file_installed_or_local(const char *inst_dir, const char *name) {
	assert(run_dir);

	struct stat sb;
	int buflen = strlen(inst_dir) + strlen(run_dir) + strlen(name) + 1;
	char *buf = (char *) malloc (buflen);

	snprintf(buf, buflen, "%s/%s", inst_dir, name);
	if (stat(buf, &sb) == 0)
		return buf;

	snprintf(buf, buflen, "%s/../%s", run_dir, name);
	if (stat(buf, &sb) == 0)
		return buf;

	free(buf);
	return NULL;
}

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
	if (gipf->projection() == ProjectionLSQ::RECTILINEAR) {
		mb->mode(9, FL_MENU_RADIO|FL_MENU_VALUE);
		mb->mode(10, FL_MENU_RADIO);
	} else {
		mb->mode(10, FL_MENU_RADIO|FL_MENU_VALUE);
		mb->mode(9, FL_MENU_RADIO);
	}

	gipf->get_distortion_params(&k0, &k1, &x0);
	i_distortion_k0->value(k0);
	i_distortion_k1->value(k1);
	i_distortion_x0->value(x0);
}

void quit_cb() {
	if (Fl::event() == FL_SHORTCUT && Fl::event_key() == FL_Escape) 
		return; // ignore Escape
	exit(0);
}

void open_cb() {
	char *file = fl_file_chooser("Open File?", "*.jpg", img_file);
	if (file) {
		gipf->load_image(file);
		scroll->position(0, 0);
		view_win->label(file);
		view_win->redraw();
		control_win->label(file);
		set_values();
		if (img_file)
			free(img_file);
		img_file = strdup(file);
	}  
}

void track_cb() {
	char *file = fl_file_chooser("Track File?", NULL, NULL);
	if (file && gipf->load_track(file) == 0)
		s_track_width->activate();
}

void save_cb() {
	char *file = fl_file_chooser("Save Image As?", NULL, img_file);
	if (file && confirm_overwrite(file))
		if (gipf->save_image(file))
			fl_message("ERROR: Saving image %s failed.", file);	
}

void dump_cb(Fl_Widget * o, void*) {
	fl_cursor(FL_CURSOR_WAIT);
	ScreenDump dmp(gipf); // needs to be done before fl_file_chooser()
	fl_cursor(FL_CURSOR_DEFAULT);
	char *file = fl_file_chooser("Save Screen Dump As?", "*.jpg", NULL);
	if (file && confirm_overwrite(file)) {
		JPEGOutputImage out(file, 95);
		if (dmp.save(&out))
			fl_message("ERROR: Saving image %s failed.", file);	
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
	if (d)
		gipf->projection(ProjectionLSQ::CYLINDRICAL);
	 else 
		 gipf->projection(ProjectionLSQ::RECTILINEAR);
}

void hidden_cb(Fl_Menu_* o, void*d) {
	gipf->set_show_hidden(o->mvalue()->value() != 0); 
}

void save_distortion_cb(Fl_Widget *, void *) {
	char buf[1024];
	const char * prof_name;
	double k0, k1, x0;

	buf[0] = '\0';
	gipf->get_distortion_params(&k0, &k1, &x0);

	gipf->get_distortion_profile_name(buf, sizeof(buf));
	prof_name = fl_input("Save Distortion Profile (k0=%f, k1=%f, x0=%f)",
		buf, k0, k1, x0);

	if (prof_name == NULL)
		return;

	if (gipf->save_distortion_params(prof_name, 0) != 0) {
		if (fl_choice("A profile with this name exists.\n",
				"Cancel", "Overwrite", NULL) == 1)
			gipf->save_distortion_params(prof_name, 1);
	}
	set_values();
}

void load_distortion_cb(Fl_Widget *, void *) {
	char buf[1024];
	const char * prof_name;

	buf[0] = '\0';
	gipf->get_distortion_profile_name(buf, sizeof(buf));
	prof_name = fl_input("Load Distortion Profile", buf);

	if (prof_name == NULL)
		return;

	if (gipf->load_distortion_params(prof_name) != 0)
		fl_alert("Could not load profile %s.", prof_name);
	else
		set_values();
}

void distortion_cb(Fl_Value_Input*, void*) {
	gipf->set_distortion_params(
		i_distortion_k0->value(),
		i_distortion_k1->value(),
		i_distortion_x0->value());
}

void about_cb() {
	fl_message("gipfel -- Photogrammetry for Mountain Images.\n"
		"Version %s\n\n"
		"(c) Johannes Hofmann 2006-2014\n\n"
		"Homepage: http://flpsed.org/gipfel.html\n\n"
		"Default datafile from http://www.viewfinderpanoramas.org/ and\n"
        "http://www.alpin-koordinaten.de\n",
		VERSION);

}

void readme_cb() {
	char *readme = file_installed_or_local(DOCDIR, "README");
	if (!readme) {
		fprintf(stderr, "README file not found\n");
		return;
	}
	Fl_Window *win = new Fl_Window(800, 600, "README");
	Fl_Text_Buffer *textBuf = new Fl_Text_Buffer();
	Fl_Text_Display *textDisp = new Fl_Text_Display(0, 0, win->w(), win->h());

	textDisp->textfont(FL_COURIER);
	textDisp->buffer(textBuf);	
	win->resizable(textDisp);
	textBuf->appendfile(readme);
	free(readme);
	win->end();
	win->show();
}

void fill_menubar(Fl_Menu_Bar* mb) {
	mb->add("&File/L&oad Image", FL_CTRL+'o', (Fl_Callback*)open_cb);
	mb->add("&File/&Save Image", FL_CTRL+'s', (Fl_Callback*)save_cb);
	mb->add("&File/Choose &Viewpoint", FL_CTRL+'v', (Fl_Callback*)viewpoint_cb);
	mb->add("&File/Load &Track", FL_CTRL+'t', (Fl_Callback*)track_cb);
	mb->add("&File/Screen &Dump", FL_CTRL+'d', (Fl_Callback*)dump_cb);
	mb->add("&File/&Quit", FL_CTRL+'q', (Fl_Callback*)quit_cb);

	mb->add("&Projection/Normal Projection", 0, (Fl_Callback *)proj_cb, 
		(void *)0, FL_MENU_RADIO|FL_MENU_VALUE);
	mb->add("&Projection/Panoramic Projection", 0, (Fl_Callback *)proj_cb, 
		(void *)1, FL_MENU_RADIO);

	mb->add("&Distortion/Load Profile", 0, (Fl_Callback *)load_distortion_cb);
	mb->add("&Distortion/Save Profile", 0, (Fl_Callback *)save_distortion_cb);

	mb->add("&Option/Show Hidden", 0, (Fl_Callback *) hidden_cb, 
		(void *)0, FL_MENU_TOGGLE);

	mb->add("&Help/Readme", 0, (Fl_Callback*)readme_cb);
	mb->add("&Help/About", 0, (Fl_Callback*)about_cb);
}

void usage() {
	fprintf(stderr,
		"usage: gipfel [-v <viewpoint>] [-d <file>]\n"
		"          [-s] [-j <file>] [-t <dir] [-w <width>] [-h <height>]\n"
		"          [-e <file>] [-E] [-p]\n"
		"          [<image(s)>]\n"
		"   -v <viewpoint>  Set point from which the picture was taken.\n"
		"                   This must be a string that unambiguously \n"
		"                   matches the name of an entry in the data file.\n"
		"   -d <file>       Use <file> for GPS data.\n"
		"   -V <visibility> Set initial visibility.\n"
		"   -u <k0>,<k1>    Use distortion correction values k0,k1.\n"
		"   -s              Stitch mode.\n"
		"   -4              Create 16bit output (only with TIFF stitching).\n"
		"   -r <from>,<to>  Stitch range in degrees (e.g. 100.0,200.0).\n"
		"   -b              Use bicubic interpolation for stitching.\n"
		"   -w <width>      Width of result image.\n"
		"   -h <height>     Height of result image.\n"
		"   -j <file>       JPEG output file in Stitch mode.\n"
		"   -t <file>       TIFF output file in Stitch mode.\n"
		"   -p              Export position of image to stdout.\n"
		"   -e <file>       Export positions of hills from <file> on image.\n"
		"   -E              Export hills from default data file.\n"
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
	s_nick->bounds(-40.0, 20.0);
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

	win->end();
	return win;
}

int main(int argc, char** argv) {
	char c, **my_argv;
	char *view_point = NULL;
	int err, my_argc, sx, sy, sw, sh;
	int stitch_flag = 0, stitch_w = 2000, stitch_h = 500;
	int jpeg_flag = 0, tiff_flag = 0, distortion_flag = 0, position_flag = 0;
	int export_flag = 0;
	int bicubic_flag = 0, b_16_flag = 0;
	double stitch_from = 0.0, stitch_to = 380.0;
	double dist_k0 = 0.0, dist_k1 = 0.0, dist_x0 = 0.0;
	double visibility = 0.07;
	const char *outpath = "/tmp";
	const char *export_file = NULL;

	err = 0;
	while ((c = getopt(argc, argv, ":?d:v:sw:h:j:t:u:br:4e:V:pE")) != EOF) {
		switch (c) {  
			case '?':
				usage();
				exit(0);
				break;
			case 'd':
				data_file = optarg;
				break;
			case 'e':
				export_flag++;
				export_file = optarg;
				break;
			case 'E':
				export_flag++;
				break;
			case 'v':
				view_point = optarg;
				break;
			case 'V':
				visibility = atof(optarg);
				break;
			case 's':
				stitch_flag++;
				break;
			case 'p':
				position_flag++;
				break;
			case '4':
				b_16_flag++;
				break;
			case 'r':
				stitch_flag++;
				if (optarg && strcmp(optarg, ":")) {
					stitch_from = atof(optarg);
					if (strchr(optarg, ','))
						stitch_to = atof(strchr(optarg, ',') + 1);
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
						if (c)
							dist_x0 = atof(strchr(optarg, ',') + 1);
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
				bicubic_flag++;
				break;
			default:
				err++;
		}
	}

	my_argc = argc - optind;
	my_argv = argv + optind;

	char *exec_file = strdup(argv[0]);
	run_dir = strdup(dirname(exec_file));
	free(exec_file);

	if (my_argc >= 1)
		img_file = strdup(my_argv[0]);

	if (data_file == NULL)
		data_file = file_installed_or_local(GIPFEL_DATADIR, "gipfel.dat");

	if (data_file == NULL || err) {
		usage();
		exit(1);
	}

	if (stitch_flag) {
		int type = 0;
		if (jpeg_flag) {
			type = STITCH_JPEG;
		} else if (tiff_flag) {
			type = STITCH_TIFF;
		} else {
			type = STITCH_PREVIEW;
		}

		return stitch(bicubic_flag ? ScanImage::BICUBIC : ScanImage::NEAREST,
			b_16_flag,
			stitch_w, stitch_h, stitch_from, stitch_to,
			type, outpath, my_argc, my_argv);

	} else if (export_flag) {
		return export_hills(export_file, visibility);
	} else if (position_flag) {
		return export_position();
	}

	Fl::get_system_colors();
	if (getenv("FLTK_SCHEME"))
		Fl::scheme(NULL);
	else
		Fl::scheme("plastic");

	control_win = create_control_window();
	control_win->callback((Fl_Callback*) quit_cb);

	view_win = new Fl_Window(800, 600);
	view_win->callback((Fl_Callback*) quit_cb);

	// The Fl_Group is used to avoid FL_DAMAGE_ALL in Fl_Scroll::position 
	Fl_Group *g = new Fl_Group(0, 0, view_win->w(), view_win->h()); 
	view_win->resizable(g);
	scroll = new Fl_Scroll(0, 0, view_win->w(), view_win->h());

	gipf = new GipfelWidget(0, 0, 800, 600, set_values);
	if (img_file) {
		gipf->load_image(img_file);
		view_win->label(img_file);
		control_win->label(img_file);
	}

	if (distortion_flag)
		gipf->set_distortion_params(dist_k0, dist_k1, dist_x0);

	Fl::screen_xywh(sx, sy, sw, sh);
	view_win->size(std::min(gipf->w(), sw), std::min(gipf->h(), sh));
	scroll->size(view_win->w(), view_win->h());

	gipf->load_data(data_file);
	gipf->set_height_dist_ratio(visibility);

	scroll->end();  

	set_values();

	view_win->resizable(scroll);

	view_win->end();
	view_win->show(1, argv); 
	control_win->show(1, argv); 

	if (view_point)
		gipf->set_viewpoint(view_point);
	else if (img_file && 
		(isnan(gipf->get_view_lat()) || isnan(gipf->get_view_long())))
		viewpoint_cb(NULL, NULL);

	return Fl::run();
}

static int
stitch(ScanImage::mode_t m, int b_16,
	int stitch_w, int stitch_h, double from, double to,
	int type, const char *path, int argc, char **argv) {

	Fl_Window *win;
	Fl_Scroll *scroll;
	Stitch *st = new Stitch();

	for (int i = 0; i < argc; i++)
		st->load_image(argv[i]);

	if (type & STITCH_JPEG) {

		st->set_output(new JPEGOutputImage(path, 90));
		st->resample(m, stitch_w, stitch_h, from, to);

	} else if (type & STITCH_TIFF) {

		st->set_output(new TIFFOutputImage(path, b_16 ? 16 : 8));
		st->resample(m, stitch_w, stitch_h, from, to);

	} else {
		win = new Fl_Window(0,0, stitch_w, stitch_h);
		scroll = new Fl_Scroll(0, 0, win->w(), win->h());
		PreviewOutputImage *img =
			new PreviewOutputImage(0, 0, stitch_w, stitch_h);

		win->resizable(scroll);
		win->show(0, argv); 
		st->set_output(img);

		st->resample(m, stitch_w, stitch_h, from, to);

		img->redraw();
		Fl::run();
	}

	return 0;
}

static int
export_hills(const char *export_file, double visibility) {
	int ret;
		
	if (!img_file) {
		fprintf(stderr, "export: No image file given.\n");
		return 1;
	}

	gipf = new GipfelWidget(0,0,800,600, NULL);
	gipf->load_image(img_file);
	gipf->load_data(data_file);
	gipf->set_height_dist_ratio(visibility);
	ret = gipf->export_hills(export_file, stdout);
	delete gipf;
	gipf = NULL;

	return ret;
}

static int
export_position() {
	ImageMetaData md;

	if (!img_file) {
		fprintf(stderr, "export: No image file given.\n");
		return 1;
	}

	if (md.load_image(img_file) == 0) {
		printf(",%s,,%f,%f,%d\n", img_file,
			md.latitude(),
			md.longitude(),
			(int) rint(md.height()));

		return 0;
	} else {
		return 1;
	}
}
