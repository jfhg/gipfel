//
// Copyright 2006-2009 Johannes Hofmann <Johannes.Hofmann@gmx.de>
//
// This software may be used and distributed according to the terms
// of the GNU General Public License, incorporated herein by reference.

#include <stdio.h>
#include <string.h>
#include <FL/Fl.H>
#include "choose_hill.H"

Hill*
choose_hill(const Hills *hills, const char *l) {
	Fl_Search_Chooser *sc = new Fl_Search_Chooser(l?l:"Choose Hill");
	Hills *h_sort = new Hills(hills);
	Hill *ret;

	h_sort->sort(Hills::SORT_NAME);

	for (int i=0; i<h_sort->get_num(); i++) {
		char buf[256];

		Hill *m = h_sort->get(i);
		if (m->flags & (Hill::DUPLIC | Hill::TRACK_POINT))
			continue;

		snprintf(buf, sizeof(buf) - 1, "%s (%dm)", m->name, (int) m->height);
		buf[sizeof(buf) - 1] = '\0';
		sc->add(buf, m);
	} 

	delete h_sort;

	sc->show();

	while (sc->shown())
		Fl::wait();

	ret = (Hill*) sc->data();
	delete sc;

	return ret;
}
