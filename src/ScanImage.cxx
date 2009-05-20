//
// Copyright 2007-2009 Johannes Hofmann <Johannes.Hofmann@gmx.de>
//
// This software may be used and distributed according to the terms
// of the GNU General Public License, incorporated herein by reference.

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include <Fl/Fl_Image.H>
#include "ScanImage.H"

int
ScanImage::get_pixel(Fl_Image *img, mode_t mode,
	double x, double y, int *r, int *g, int *b) {
	if (mode == BICUBIC)
		return get_pixel_bicubic(img, x, y, r, g, b);
	else
		return get_pixel_nearest(img, x, y, r, g, b);
}

int
ScanImage::get_pixel_nearest(Fl_Image *img, double x, double y,
    int *r, int *g, int *b) {

    if (isnan(x) || isnan(y))
        return 1;
    else
        return get_pixel(img, (int) rint(x), (int) rint(y), r, g, b);
}

static inline double
interp_cubic(double x, double x2, double x3, double *v) {
    double a0, a1, a2, a3;

    a0 = v[3] - v[2] - v[0] + v[1];
    a1 = v[0] - v[1] - a0;
    a2 = v[2] - v[0];
    a3 = v[1];

    return a0 * x3 + a1 * x2 + a2 * x + a3;
}

int
ScanImage::get_pixel_bicubic(Fl_Image *img, double x, double y,
    int *r, int *g, int *b) {

    double fl_x = floor(x);
    double fl_y = floor(y);
    double dx = x - fl_x, dx2 = dx * dx, dx3 = dx2 * dx;
    double dy = y - fl_y, dy2 = dy * dy, dy3 = dy2 * dy;
    int ic[3];
    double c[3][4];
    double c1[3][4];

    for (int iy = 0; iy < 4; iy++) {
        for (int ix = 0; ix < 4; ix++) {

            if (get_pixel(img, (int) fl_x + ix - 1, (int) fl_y + iy - 1,
                    &ic[0], &ic[1], &ic[2]) != 0)
                return 1;

            for (int l = 0; l < 3; l++)
                c[l][ix] = (double) ic[l];
        }

        for (int l = 0; l < 3; l++)
            c1[l][iy] = interp_cubic(dx, dx2, dx3, c[l]);
    }

    *r = (int) rint(interp_cubic(dy, dy2, dy3, c1[0]));
    *g = (int) rint(interp_cubic(dy, dy2, dy3, c1[1]));
    *b = (int) rint(interp_cubic(dy, dy2, dy3, c1[2]));
    return 0;
}

int
ScanImage::get_pixel(Fl_Image *img, int x, int y,
                     int *r, int *g, int *b) {
    if ( img->d() == 0 )
        return 1;

    if (x < 0 || x >=img->w() || y < 0 || y >= img->h())
        return 1;

    long index = (y * img->w() * img->d()) + (x * img->d()); // X/Y -> buf index  
    switch (img->count()) {
        case 1:
        {                                            // bitmap
            const unsigned char *buf = (const unsigned char*) img->data()[0];
            switch (img->d())
            {
                case 1:
                    *r = *g = *b = *(buf+index);
                    break;
                case 3:                              // 24bit
                    *r = (int) *(buf+index+0);
                    *g = (int) *(buf+index+1);
                    *b = (int) *(buf+index+2);
                    break;
                default:                             // ??
                    printf("Not supported: chans=%d\n", img->d());
                    return 1;
            }
            break;
        }
        default:                                             // ?? pixmap, bit vals
        printf("Not supported: count=%d\n", img->count());
        return 1;
    }

    *r = *r * 255;
    *g = *g * 255;
    *b = *b * 255;

    return 0;
}
