//
// Copyright 2006-2009 Johannes Hofmann <Johannes.Hofmann@gmx.de>
//
// This software may be used and distributed according to the terms
// of the GNU General Public License, incorporated herein by reference.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
extern "C" {
#include "strsep.h"
}
#include "Hill.H"

static double pi_d, deg2rad;

Hill::Hill(const char *n, double p, double l, double h) {
	name = strdup(n);
	phi = p;
	lam = l;
	height = h;
	alph = 0.0;
	x = 0;
	y = 0;
	flags = 0;
}

Hill::Hill(const Hill& h) {
	name = strdup(h.name);
	phi = h.phi;
	lam = h.lam;
	height = h.height;
	alph = h.alph;
	dist = h.dist;
	x = h.x;
	y = h.y;
	label_x = h.label_x;
	label_y = h.label_y;
	flags = h.flags;
}

Hill::Hill(double x_tmp, double y_tmp) {
	name = NULL;
	phi = 0.0;
	lam = 0.0;
	height = 0.0;
	alph = 0.0;
	x = x_tmp;
	y = y_tmp;
}

Hill::~Hill() {
	if (name)
		free(name);
}

Hills::Hills() {
	num = 0;
	cap = 100;
	m = (Hill **) malloc(cap * sizeof(Hill *));

	pi_d = asin(1.0) * 2.0;
	deg2rad = pi_d / 180.0;
}

Hills::Hills(const Hills *h) {
	num = h->num;
	cap = h->cap;
	m = (Hill **) malloc(cap * sizeof(Hill *));
	memcpy(m, h->m, cap * sizeof(Hill *));

	pi_d = asin(1.0) * 2.0;
	deg2rad = pi_d / 180.0;
}

int
Hills::load(const char *file) {
	FILE *fp;
	char buf[4000];
	char *vals[10];
	char **ap, *bp;
	double phi, lam, height;
	Hill *m;
	int n;

	fp = fopen(file, "r");
	if (!fp) {
		perror("fopen");
		return 1;
	}

	while (fgets(buf, sizeof(buf), fp)) {
		bp = buf;
		memset(vals, 0, sizeof(vals));
		n = 0;
		for (ap = vals; (*ap = strsep(&bp, ",")) != NULL;) {
			n++;
			if (++ap >= &vals[10])
				break;
		}

		// standard format including name and description
		if (n == 6 && vals[1] && vals [3] && vals[4] && vals[5]) {
			phi = atof(vals[3]) * deg2rad;
			lam = atof(vals[4]) * deg2rad;
			height = atof(vals[5]);

			m = new Hill(vals[1], phi, lam, height);

			add(m);
			// track point format
		} else if (n == 3 && vals[0] && vals[1] && vals[2]) {
			phi = atof(vals[0]) * deg2rad;
			lam = atof(vals[1]) * deg2rad;
			height = atof(vals[2]);

			m = new Hill("", phi, lam, height);

			add(m);
		}
	}

	fclose(fp);

	return 0;
}

void Hills::mark_duplicates(double dist) {
	Hill *m, *n;
	int i, j;

	sort(SORT_PHI);

	for(i = 0; i < get_num(); i++) {
		m = get(i);

		if (m) {
			if (m->flags & Hill::TRACK_POINT)
				continue;

			j = i + 1;
			n = get(j);
			while (n && fabs(n->phi - m->phi) <= dist) {
				if (! (n->flags & Hill::DUPLIC)) {
					if (fabs(n->lam - m->lam) <= dist && 
						fabs(n->height - m->height) <= 50.0 ) {
						n->flags |= Hill::DUPLIC;
					}
				}
				j = j + 1;
				n = get(j);
			}
		}
	}
}

Hills::~Hills() {
	if (m)
		free(m);
}

void
Hills::add(Hill *m1) {
	if (num >= cap) {
		cap = cap ? cap * 2 : 100;
		m = (Hill **) realloc(m, cap * sizeof(Hill *));
	}

	m[num++] = m1;
}

void
Hills::add(Hills *h) {
	for (int i=0; i<h->get_num(); i++)
		add(h->get(i));
}

static int
comp_mountains_alpha(const void *n1, const void *n2) {
	Hill *m1 = *(Hill **)n1;
	Hill *m2 = *(Hill **)n2;

	if (m1 && m2) {
		if (m1->alph < m2->alph)
			return 1;
		else if (m1->alph > m2->alph)
			return -1;
		else
			return 0;
	} else {
		return 0;
	}  
}

static int
comp_mountains_phi(const void *n1, const void *n2) {
	Hill *m1 = *(Hill **)n1;
	Hill *m2 = *(Hill **)n2;

	if (m1 && m2) {
		if (m1->phi < m2->phi)
			return 1;
		else if (m1->phi > m2->phi)
			return -1;
		else
			return 0;
	} else {
		return 0;
	}  
}

static int
comp_mountains_name(const void *n1, const void *n2) {
	Hill *m1 = *(Hill **)n1;
	Hill *m2 = *(Hill **)n2;
	int r;

	if (m1 && m2) {
		r = strcasecmp(m1->name, m2->name);
		if (r == 0)
			return (int) (m1->height - m2->height);
		else
			return r;
	} else {
		return 0;
	}
}

static int
comp_mountains_label_y(const void *n1, const void *n2) {
	Hill *m1 = *(Hill **)n1;
	Hill *m2 = *(Hill **)n2;

	if (m1 && m2) {
		if ((m2->y + m2->label_y) > (m1->y + m1->label_y))
			return 1;
		else if ((m2->y + m2->label_y) < (m1->y + m1->label_y))
			return -1;
		else
			return 0;
	} else {
		return 0;
	}
}

static int
comp_mountains_x(const void *n1, const void *n2) {
    Hill *m1 = *(Hill **)n1;
    Hill *m2 = *(Hill **)n2;

    if (m1 && m2) {
		if (m2->x < m1->x)
			return 1;
		else if (m2->x > m1->x)
			return -1;
		else
			return 0;
    } else {
        return 0;
    }
}

void
Hills::sort(SortType t) {
	int (*cmp)(const void *, const void *);
	
	if (!m || num < 2)
		return;

	switch (t) {
		case SORT_ALPHA:
			cmp = comp_mountains_alpha;
			break;
		case SORT_PHI:
			cmp = comp_mountains_phi;
			break;
		case SORT_NAME:
			cmp = comp_mountains_name;
			break;
		case SORT_LABEL_Y:
			cmp = comp_mountains_label_y;
			break;
		case SORT_X:
			cmp = comp_mountains_x;
			break;
		default:
			fprintf(stderr, "ERROR: Unknown sort type %d\n", t);
			return;
	}

	qsort(m, num, sizeof(Hill *), cmp);
}

void
Hills::clear() {
	if (m) {
		free(m);
		m = NULL;
	}
	cap = 0;
	num = 0;
}

int
Hills::contains(const Hill *m) const {
	for  (int i = 0; i < get_num(); i++)
		if (get(i) == m)
			return 1;

	return 0;
}

void
Hills::remove(const Hill *h) {
	for (int i = 0; i < get_num(); i++) {
		if (get(i) == h) {
			memmove(&m[i], &m[i+1], (get_num() - i - 1) * sizeof(Hill*));
			num--;
		}
	}
}

void
Hills::clobber() {
	int i;

	for (i = 0; i < get_num(); i++)
		if (get(i))
			delete(get(i));

	clear();
}

Hill *
Hills::get(int n) const {
	if (n < 0 || n >= num)
		return NULL;
	else
		return m[n];
}
