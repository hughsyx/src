/* Smooth signal division. */
/*
  Copyright (C) 2004 University of Texas at Austin
  
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <rsf.h>

#include "divn.h"

int main (int argc, char* argv[])
{
    int  i, n12, niter, dim, n[SF_MAX_DIM], rect[SF_MAX_DIM];
    float *num, *den, *rat;
    char key[6];
    sf_file in, out, denom;

    sf_init (argc,argv);
    in = sf_input("in");
    denom = sf_input("den");
    out = sf_output("out");

    if (SF_FLOAT != sf_gettype(in)) sf_error("Need float input");

    dim = sf_filedims (in,n);
    n12 = 1;
    for (i=0; i < dim; i++) {
	snprintf(key,6,"rect%d",i+1);
	if (!sf_getint(key,rect+i)) rect[i]=1;
	n12 *= n[i];
    }

    num = sf_floatalloc(n12);
    den = sf_floatalloc(n12);
    rat = sf_floatalloc(n12);

    if (!sf_getint("niter",&niter)) niter=100;
    /* number of iterations */

    divn_init(dim, n12, n, rect, niter);

    sf_floatread(num,n12,in);
    sf_floatread(den,n12,denom);

    divn (num, den, rat);

    sf_floatwrite(rat,n12,out);
    sf_close();
    exit(0);
}

/* 	$Id: Menvelope.c 696 2004-07-06 23:17:31Z fomels $	 */
