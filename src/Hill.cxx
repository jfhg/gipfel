//
// Copyright 2006 Johannes Hofmann <Johannes.Hofmann@gmx.de>
//
// This software may be used and distributed according to the terms
// of the GNU General Public License, incorporated herein by reference.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

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
	a_view = h.a_view;
	a_view = h.a_view;
	dist = h.dist;
	x = h.x;
	y = h.y;
	label_x = h.label_x;
	label_y = h.label_y;
	flags = h.flags;
}

Hill::Hill(int x_tmp, int y_tmp) {
	name = NULL;
	phi = 0.0;
	lam = 0.0;
	height = 0.0;
	alph = 0.0;
	x = x_tmp;
	y = y_tmp;
}

Hill::~Hill() {
	if (name) {
		free(name);
	}
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
			if (++ap >= &vals[10]) {
				break;
			}
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

	sort_phi();

	for(i=0; i<get_num();i++) {
		m = get(i);

		if (m->flags & Hill::TRACK_POINT) {
			continue;
		}

		if (m) {
			j = i + 1;
			n = get(j);
			while (n && fabs(n->phi - m->phi) <= dist) {
				if (! n->flags & Hill::DUPLICATE) {
					if (fabs(n->lam - m->lam) <= dist && 
						fabs(n->height - m->height) <= 50.0 ) {
						n->flags |= Hill::DUPLICATE;
					}
				}
				j = j + 1;
				n = get(j);
			}
		}
	}
}


Hills::~Hills() {
	if (m) {
		free(m);
	}
}


void
Hills::add(Hill *m1) {
	if (num >= cap) {
		cap = cap?cap * 2:100;
		m = (Hill **) realloc(m, cap * sizeof(Hill *));
	}

	m[num++] = m1;
}

void
Hills::add(Hills *h) {
	for(int i=0; i<h->get_num(); i++) {
		add(h->get(i));
	}
}

static int
comp_mountains(const void *n1, const void *n2) {
	Hill *m1 = *(Hill **)n1;
	Hill *m2 = *(Hill **)n2;

	if (m1 && m2) {
		if (m1->alph < m2->alph) {
			return 1;
		} else if (m1->alph > m2->alph) {
			return -1;
		} else {
			return 0;
		}
	} else {
		return 0;
	}  
}

static int
comp_mountains_phi(const void *n1, const void *n2) {
	Hill *m1 = *(Hill **)n1;
	Hill *m2 = *(Hill **)n2;

	if (m1 && m2) {
		if (m1->phi < m2->phi) {
			return 1;
		} else if (m1->phi > m2->phi) {
			return -1;
		} else {
			return 0;
		}
	} else {
		return 0;
	}  
}

static int
comp_mountains_name(const void *n1, const void *n2) {
	Hill *m1 = *(Hill **)n1;
	Hill *m2 = *(Hill **)n2;

	if (m1 && m2) {
		return strcasecmp(m1->name, m2->name);
	} else {
		return 0;
	}  
}

void
Hills::sort() {
	if (!m) {
		return;
	}

	qsort(m, num, sizeof(Hill *), comp_mountains);
}

void
Hills::sort_phi() {
	if (!m) {
		return;
	}

	qsort(m, num, sizeof(Hill *), comp_mountains_phi);
}

void
Hills::sort_name() {
	if (!m) {
		return;
	}

	qsort(m, num, sizeof(Hill *), comp_mountains_name);
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

void
Hills::clobber() {
	int i;

	for(i=0; i<get_num();i++) {
		if (get(i)) {
			delete(get(i));
		}
	}

	clear();
}

int
Hills::get_num() {
	return num;
}

Hill *
Hills::get(int n) {
	if (n < 0 || n >= num) {
		return NULL;
	} else {
		return m[n];
	}
}
