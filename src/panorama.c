/* 
 * "$Id: panorama.c,v 1.8 2005/04/12 19:56:42 hofmann Exp $"
 *
 * flpsed program.
 *
 * Copyright 2004,2005 by Johannes Hofmann
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *  
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA.
 */


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

double pi, deg2rad;

double distance(double phi_a, double lam_a, double phi_b, double lam_b);
double sin_alpha(double lam_a, double lam_b, double phi_b, double c);
double cos_alpha(double phi_a, double phi_b, double c);
double alpha(double phi_a, double lam_a, double phi_b, double lam_b);
double center_angle(double alph_a, double alph_b, double d1, double d2);
int    read_file(double phi_a, double lam_a, char *name,
		 int dist, double h_d_ratio, double mm_per_deg,
		 double a_center);
int 
main(int argc, char **argv) {
  char c;
  char *sep, *tmp, *data_file = NULL;
  int errflg = 0;
  int dist = 100000;
  double phi_a, lam_a, phi_b, lam_b;
  double h_d_ratio = 0.1, mm_per_deg = 200.0, dist_c_m1 = 0.0;
  double dist_c_m2 = 0.0, a_center = 0.0;
  char *pos = NULL, *m1 = NULL, *m2 = NULL, *m_c = NULL;
  
  pi = asin(1.0) * 2.0;
  deg2rad = pi / 180.0;
  
  while ((c = getopt(argc, argv, "a:b:f:d:r:p:m:n:s:t:c:")) != EOF) {
    switch (c) {
    case 'a':
      tmp = strdup(optarg);
      if ((sep = strchr(tmp, ',')) == NULL) {
	errflg++;
	break;
      }
      *sep = '\0';
      phi_a = atof(tmp) * deg2rad;
      lam_a = atof(sep+1) * deg2rad;
      free(tmp);
      break;
    case 'b':
      tmp = strdup(optarg);
      if ((sep = strchr(tmp, ',')) == NULL) {
	usage();
      }
      *sep = '\0';
      phi_b = atof(tmp) * deg2rad;
      lam_b = atof(sep+1) * deg2rad;
      free(tmp);
      break;
    case 'f':
      data_file = optarg;
      break;
    case 'd':
      dist = atoi(optarg);
      break;
    case 'r':
      h_d_ratio = atof(optarg);
      break;
    case 's':
      dist_c_m1 = -atof(optarg);
      break;
    case 't':
      dist_c_m2 = -atof(optarg);
      break;
    case 'p':
      pos = optarg;
      break;
    case 'm':
      m1 = optarg;
      break;
    case 'n':
      m2 = optarg;
      break;
    case 'c':
      m_c = optarg;
      break;
    }
}

#if 0
  {
    double a1 = 0.0, a2 = 90.0;
    double d1 = -20.0, d2 = 10.0;
    
    fprintf(stderr, "==> %f\n", center_angle(a1*deg2rad, a2*deg2rad, d1, d2) / deg2rad);
    exit(1);
  }
#endif

  if (errflg) {
    usage();
    exit(1);
  }

  if (!data_file) {
    usage();
    exit(1);
  }

  if (pos){
    int n = get_pos(data_file, pos, &phi_a, &lam_a);
    if (n == 0) {
      fprintf(stderr, "No matching entry for %s in datafile.\n",
	      pos);
      exit(1);
    } else if (n > 1) {
      fprintf(stderr, "More than one  matching entry for %s in datafile.\n",
	      pos);
      exit(1);
    }
  }

  if (m1 && m2) {
    double phi, lam;
    double a1, a2, a;
    int n;

    n = get_pos(data_file, m1, &phi, &lam);
    if (n == 0) {
      fprintf(stderr, "No matching entry for %s in datafile.\n",
	      m1);
      exit(1);
    } else if (n > 1) {
      fprintf(stderr, "More than one  matching entry for %s in datafile.\n",
	      m1);
      exit(1);
    }
    a1 = alpha(phi_a, lam_a, phi, lam);

    n = get_pos(data_file, m2, &phi, &lam);
    if (n == 0) {
      fprintf(stderr, "No matching entry for %s in datafile.\n",
	      m2);
      exit(1);
    } else if (n > 1) {
      fprintf(stderr, "More than one  matching entry for %s in datafile.\n",
	      m2);
      exit(1);
    }
    a2 = alpha(phi_a, lam_a, phi, lam);

    a_center = center_angle(a1, a2, dist_c_m1, dist_c_m2);
    mm_per_deg = (dist_c_m1 - dist_c_m2) / (tan(a1 - a_center) - tan(a2 - a_center));
    fprintf(stderr, "center_angle = %f\n", a_center / deg2rad);

  }


  
  read_file(phi_a, lam_a, data_file, dist, h_d_ratio, mm_per_deg, a_center);
}


#define PS_HEADER "%!\n \
/mountain {             % expects name, height, dist, deg on stack\n \
  newpath               % Start a new path\n \
  gsave                 % Keep rotations temporary\n \
    rotate              % Rotate by degrees on stack\n \
    dup \n \
    0  moveto \n \
    add                 % End line at 20 + dist + height\n \
    dup                 % Duplicate end of line pos\n \
    0 lineto\n \
    10 add              % Start text 10 from end of line \n \
    0 moveto\n \
    show\n \
    stroke\n \
  grestore              % Get back the unrotated state\n \
} def           \n \
/Times-Roman findfont  \n \
8 scalefont            \n \
setfont  \n \
300 344 translate\n \
5 0 moveto\n \
0 0 5 0 360 arc\n \
stroke
%11 0 moveto\n"


#define PS_HEADER_PHOTO "%!\n \
 /mountain {             % expects name, height, dist, deg on stack\n \
   newpath\n \
   0.352778 div          % convert from mm to PS points\n \
   dup\n \
   dup\n \
   200 exch moveto\n \
   500 exch lineto\n \
   500 exch moveto\n \
   pop \n \
   pop \n \
   show\n \
   stroke\n \
} def\n \
\n \
 /Times-Roman findfont \n \
 6 scalefont           \n \
 setfont  \n
 0 420 translate \n"


int get_pos(char *filename, char *name, double *phi, double *lam) {
  FILE *fp;
  char buf[1024];
  char *vals[10];
  char **ap, *bp;
  double height;
  int found = 0;

  fp = fopen(filename, "r");
  if (!fp) {
    perror("fopen");
    return 1;
  }
  
  while (fgets(buf, sizeof(buf), fp)) {
    bp = buf;
    for (ap = vals; (*ap = strsep(&bp, ",")) != NULL;)
      if (++ap >= &vals[10])
	break;
    
    if (strstr(vals[1], name)) {
      *phi = atof(vals[3]) * deg2rad;
      *lam = atof(vals[4]) * deg2rad;
      height = atof(vals[5]);
      fprintf(stderr, "Found matching entry: %s (%fm)\n", vals[1], height);
      found++;
    }
  }

  fclose(fp);
  return found;
}



int read_file(double phi_a, double lam_a, char *name,
	      int dist, double h_d_ratio, double mm_per_deg, double a_center) {
  FILE *fp;
  char buf[1024];
  char *vals[10];
  char **ap, *bp;
  double phi_b, lam_b, height, alph, dist_center;

  fp = fopen(name, "r");
  if (!fp) {
    perror("fopen");
    return 1;
  }
  
  printf("%s", PS_HEADER_PHOTO);
  printf("/mm_per_deg %f def\n", mm_per_deg);
  printf("/a_center %f def\n",   a_center);
  while (fgets(buf, sizeof(buf), fp)) {
    bp = buf;
    for (ap = vals; (*ap = strsep(&bp, ",")) != NULL;)
      if (++ap >= &vals[10])
	break;

    phi_b = atof(vals[3]) * deg2rad;
    lam_b = atof(vals[4]) * deg2rad;
    
    if (phi_a == phi_b && lam_a == lam_b) {
      continue;
    }

    height = atof(vals[5]);

    if (distance(phi_a, lam_a, phi_b, lam_b) * 6368000.0 > dist) {
      continue;
    }


    if (height / (distance(phi_a, lam_a, phi_b, lam_b)* 6368000) < h_d_ratio) {
      continue;
    }

    alph = alpha(phi_a, lam_a, phi_b, lam_b);
    alph = alph - a_center;
    if (alph > pi / 2.0 || alph < - pi /2.0) {
      continue;
    }

    dist_center = tan(alph) * mm_per_deg;

    printf("(%s) %f %f %f mountain\n", vals[1], 
	   height / 20.0,
	   distance(phi_a, lam_a, phi_b, lam_b) * 25000.0,
	   dist_center);
   

#if 0
    printf("%f %f %s %fm\n", 
	   alpha(phi_a, lam_a, phi_b, lam_b),  
	   distance(phi_a, lam_a, phi_b, lam_b) * 6368000,
	   vals[1],
	   height);
#endif
  }

  printf("showpage\n");

  fclose(fp);
  return 0;
}


double
distance(double phi_a, double lam_a, double phi_b, double lam_b) {
  return acos(sin(phi_a)*sin(phi_b) + cos(phi_a)*cos(phi_b)*cos(lam_a - lam_b));
}

double
sin_alpha(double lam_a, double lam_b, double phi_b, double c) {
  return sin(lam_b - lam_a) * cos(phi_b) / sin(c);
}

double
cos_alpha(double phi_a, double phi_b, double c) {
  return (sin(phi_b) - sin(phi_a) * cos(c)) / (cos(phi_a) * sin(c));
}

double alpha(double phi_a, double lam_a, double phi_b, double lam_b) {
  double dist, sin_alph, cos_alph, alph;
  
  dist = distance(phi_a, lam_a, phi_b, lam_b);
  sin_alph = sin_alpha(lam_a, lam_b, phi_b, dist);
  cos_alph = cos_alpha(phi_a, phi_b, dist);

  if (sin_alph > 0) {
    alph = acos(cos_alph);
  } else {
    alph = 2.0 * pi - acos(cos_alph);
  }

  return alph;
}

double 
center_angle(double alph_a, double alph_b, double d1, double d2) {
  double tan_a, tan_b;

  fprintf(stderr, "a=%f, b=%f d1=%f d2=%f\n", 
	  alph_a / deg2rad, alph_b / deg2rad, d1, d2);

  tan_a = tan(alph_a - alph_b);
  fprintf(stderr, "tan_a %f\n", tan_a);
  tan_b = (d2 - d1 + ((sqrt((d2*(d2 - (2.0*d1*(1.0 + (2.0 * tan_a * tan_a))))) + (d1*d1))))) / (2.0*d2*tan_a);

  fprintf(stderr, "tan_b=%f\n", tan_b);
  return alph_a + atan(tan_b);
}
  
void 
usage() {
  fprintf(stderr,
	  "usage: panorama -a <phi>,<lambda>\n"
	  "                -b <phi>,<lambda>\n");
}
