//
// Copyright 2006 Johannes Hofmann <Johannes.Hofmann@gmx.de>
//
// This software may be used and distributed according to the terms
// of the GNU General Public License, incorporated herein by reference.

#ifndef HILL_H
#define HILL_H


class Hill;

class Hill {
	private:

	public:
		typedef enum {
			DUPLICATE   = 0x01,
			TRACK_POINT = 0x02,
			VISIBLE     = 0x04,
			HIDDEN      = 0x08
		} flags_t;

		double phi, lam;
		double height;
		double alph;
		double a_nick;
		double dist;
		double x, y;
		int label_x, label_y;
		char *name;
		int flags;

		Hill(const char *n, double p, double l, double h);

		Hill(const Hill& h);

		Hill(double x_tmp, double y_tmp);

		~Hill();  
};


class Hills {
	private:
		int num, cap;
		Hill **m;

	public:
		Hills();

		Hills(const Hills *h);

		~Hills();

		int load(const char *file);

		void mark_duplicates(double dist);

		void add(Hill *m);

		void remove(const Hill *m);

		void add(Hills *h);

		void sort_phi();

		void sort_name();

		void sort();

		void clear();

		void clobber();
	
		int contains(const Hill *m) const;

		int get_num() const;

		Hill *get(int n) const;
};
#endif
