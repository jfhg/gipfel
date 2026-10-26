// Golden dump: Stitch::resample pipeline through the original C++ Panorama +
// ScanImage (no FLTK GUI; PPM input wrapped in Fl_RGB_Image).
// Usage: stitchdump <img.ppm> <projtype> <center> <pitch> <roll> <scale>
//                    <k0> <k1> <x0> <w> <h> <from> <to> <bicubic>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <algorithm>

#include <FL/Fl.H>
#include <FL/Fl_RGB_Image.H>

#include "Panorama.H"
#include "ScanImage.H"

#define MAX_VALUE 65025

// minimal P6 PPM reader
static unsigned char *read_ppm(const char *path, int *w, int *h) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    int c;
    if (fgetc(f) != 'P' || fgetc(f) != '6' || (c = fgetc(f), !isspace(c))) {
        fclose(f); return NULL;
    }
    int skip = 0;
    while (!skip) {
        c = fgetc(f);
        if (c == '#') { while ((c = fgetc(f)) != '\n' && c != EOF); continue; }
        if (c == ' ' || c == '\n' || c == '\t' || c == '\r') continue;
        skip = 1;
        ungetc(c, f);
    }
    (void)c;
    if (fscanf(f, "%d %d %*d", w, h) != 2) { fclose(f); return NULL; }
    fgetc(f); // single whitespace
    unsigned char *buf = (unsigned char*)malloc((size_t)(*w) * (*h) * 3);
    if (fread(buf, 1, (size_t)(*w) * (*h) * 3, f) != (size_t)(*w) * (*h) * 3) {
        free(buf); fclose(f); return NULL;
    }
    fclose(f);
    return buf;
}

int main(int argc, char **argv) {
    if (argc < 15) {
        fprintf(stderr, "usage: stitchdump <img.ppm> <projtype> <center> <pitch> "
                "<roll> <scale> <k0> <k1> <x0> <w> <h> <from> <to> <bicubic>\n");
        return 1;
    }
    int iw = 0, ih = 0;
    unsigned char *px = read_ppm(argv[1], &iw, &ih);
    if (!px) { fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }

    Fl_RGB_Image img(px, iw, ih, 3);

    Panorama pan;
    pan.set_projection((ProjectionLSQ::Projection_t) atoi(argv[2]));
    pan.set_center_angle(atof(argv[3]));
    pan.set_nick_angle(atof(argv[4]));
    pan.set_tilt_angle(atof(argv[5]));
    pan.set_scale(atof(argv[6]));
    pan.parms.k0 = atof(argv[7]);
    pan.parms.k1 = atof(argv[8]);
    pan.parms.x0 = atof(argv[9]);

    int w = atoi(argv[10]), h = atoi(argv[11]);
    double view_start = atof(argv[12]) * M_PI / 180.0;
    double view_end = atof(argv[13]) * M_PI / 180.0;
    ScanImage::mode_t mode = atoi(argv[14]) ? ScanImage::BICUBIC : ScanImage::NEAREST;

    double step_view = (view_end - view_start) / w;
    int y_off = h / 2;
    double radius = (double) w / (view_end - view_start);

    for (int y = 0; y < h; y++) {
        double a_nick = atan((double)(y_off - y) / radius);
        for (int x = 0; x < w; x++) {
            double a_view = view_start + x * step_view;
            int hit = 0, r = 0, g = 0, b = 0;
            double px2, py2;
            if (pan.get_coordinates(a_view, a_nick, &px2, &py2) == 0) {
                int rr, gg, bb;
                if (ScanImage::get_pixel(&img, mode,
                        px2 + ((double) img.w()) / 2.0,
                        py2 + ((double) img.h()) / 2.0,
                        &rr, &gg, &bb) == 0) {
                    hit = 1;
                    r = std::max(std::min(rr, MAX_VALUE), 0);
                    g = std::max(std::min(gg, MAX_VALUE), 0);
                    b = std::max(std::min(bb, MAX_VALUE), 0);
                }
            }
            printf("%d %d %d %d %d %d\n", x, y, hit, r, g, b);
        }
    }

    free(px);
    return 0;
}
