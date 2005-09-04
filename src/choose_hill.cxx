//
// Search Chooser widget for the Fast Light Tool Kit (FLTK).
// 
// Copyright by Johannes Hofmann
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Library General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
// USA.
//
//

#include <stdio.h>
#include <string.h>
#include <FL/Fl.H>
#include "choose_hill.H"

Hill*
choose_hill(const Hills *hills, const char *l) {
  Fl_Search_Chooser *sc = new Fl_Search_Chooser(l?l:"Choose Hill");
  Hills *h_sort = new Hills(hills);
  Hill *ret;

  h_sort->sort_name();
  
  for (int i=0; i<h_sort->get_num(); i++) {
    Hill *m = h_sort->get(i);
    if (m->flags & (Hill::DUPLICATE | Hill::TRACK_POINT)) {
      continue;
    } 
    sc->add(m->name, m);
  } 
  
  delete h_sort;
  
  sc->show();
  while (sc->shown()) {
    Fl::wait();
  } 
  
  ret = (Hill*) sc->data();

  delete(sc);

  return ret;
}
