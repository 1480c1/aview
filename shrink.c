#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "aalib.h"
#include "image.h"
#include "shrink.h"
typedef double atype;
static atype xstep, ystep;
static atype max;
static int fast;
static int getval(atype x, atype y)
{
    atype xe = x + xstep, ye = y + ystep;
    atype ycount, xcount = 0;
    int xi, yi, xie = floor(xe), yie = floor(ye);
    ycount = 0;
    if (x < 0 || x + xstep > imgwidth || y < 0 || y + ystep > imgheight)
	return 0;
    if (fast) {
	xi = x;
	yi = y;
	xi = imgdata[xi + yi * imgwidth];
    } else {
	for (yi = y; yi <= yie; yi++) {
	    xcount = 0;
	    xi = x;
	    xcount += (((int) imgdata[xi + yi * imgwidth])) * (1.0 - x + floor(x));
	    xi++;
	    for (; xi <= xie; xi++)
		xcount += (((int) imgdata[xi + yi * imgwidth]));
	    xi--;
	    /*if(xie!=floor(xie)) */
	    xcount -= (((int) imgdata[xi + yi * imgwidth])) * (1.0 - xe + floor(xe));

	    if (yi == (int) y)
		ycount += xcount * (1.0 - y + floor(y));
	    else
		ycount += xcount;
	    /*printw("%f %f %f %i\n",xcount,ycount,x,xie); */
	}
	/*if(yie!=floor(yie)) */
	ycount -= xcount * (1.0 - ye + floor(ye));
	xi = 256 * ycount / max;
    }
    if (xi < 0)
	xi = 0;
    if (xi > 255)
	xi = 255;
    return (xi);
}
void fastscale(char *b1, char *b2, int x1, int x2, int y1, int y2, int width1, int width2)
{
    int ddx1, ddx, spx = 0, ex;
    int ddy1, ddy, spy = 0, ey;
    int x;
    char *bb1=b1;
    width2 -= x2;
    if(!x1||!x2||!y1||!y2) return;
    ddx = x1 + x1;
    ddx1 = x2 + x2;
    if (ddx1 < ddx)
	spx = ddx / ddx1, ddx %= ddx1;
    ddy = y1 + y1;
    ddy1 = y2 + y2;
    if (ddy1 < ddy)
	spy = (ddy / ddy1)*width1, ddy %= ddy1;
    ey = -ddy1;
    for (; y2; y2--) {
        ex = -ddx1;
	for (x = x2; x; x--) {
	    *b2 = *b1;
	    b2++;
	    b1 += spx;
	    ex += ddx;
	    if (ex > 0) {
		b1++;
		ex -= ddx1;
	    }
	}
	b2 += width2;
	bb1 += spy;
	ey += ddy;
	if (ey > 0) {
	    bb1 += width1;
	    ey -= ddy1;
	}
	b1=bb1;
    }
}
void shrink(aa_context * c, int fast1, int x1, int y1, int x2, int y2)
{
    int x, y;
    int width = aa_imgwidth(c);
    int height = aa_imgheight(c);
    if(x2<0||x1>=imgwidth||y2<0||y1>=imgheight) {
       memset(c->imagebuffer,0,aa_imgwidth(c)*aa_imgheight(c));
       return;
    }
    if (fast) {
        if(x1<0||y1<0||x2>=imgwidth||y2>=imgheight)
	{ int xx1=0,xx2=aa_imgwidth(c)-1,yy1=0,yy2=aa_imgheight(c)-1;
	  memset(c->imagebuffer,0,aa_imgwidth(c)*aa_imgheight(c));
          xstep = ((atype) width) / (x2 - x1 - 1) ;
          ystep = ((atype) height) / (y2 - y1 - 1) ;
	  if(x2>=imgwidth) {
	    xx2=(imgwidth-1-x1)*xstep;
	    x2=imgwidth-1;
	  }
	  if(y2>=imgheight) {
	    yy2=(imgheight-1-y1)*ystep;
	    y2=imgheight-1;
	  }
	  if(x1<0) {
	    xx1=-xstep*x1;
	    x1=0;
	  }
	  if(y1<0) {
	    yy1=-ystep*y1;
	    y1=0;
	  }
	   fastscale(imgdata + x1 + imgwidth * y1, c->imagebuffer+xx1+yy1*width, x2 - x1, xx2-xx1, y2 - y1, yy2-yy1, imgwidth, aa_imgwidth(c));
	return;
	}
	  else
	    fastscale(imgdata + x1 + imgwidth * y1, c->imagebuffer, x2 - x1, aa_imgwidth(c), y2 - y1, aa_imgheight(c), imgwidth, aa_imgwidth(c));
	return;
    }
    fast = fast1;
    xstep = (x2 - x1 - 1) / (atype) (width);
    ystep = (y2 - y1 - 1) / (atype) (height);
    max = xstep * ystep * 256;
    for (y = 0; y < height; y++) {
	for (x = 0; x < width; x++) {
	    atype xp = x1 + xstep * x, yp = y1 + ystep * y;
	    aa_putpixel(c, x, y, getval(xp, yp));
	}
    }
}
