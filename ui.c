#include <stdio.h>
#include <ctype.h>
#include <malloc.h>
#include <string.h>
#include <aalib.h>
#include "shrink.h"
#include "image.h"
aa_context *context;
aa_renderparams *renderparams;
static int x1, y1, x2, y2;
static int fast = 1;
static int resized;
static char getletter(int i)
{
    if(i<9) return('1'+i);
    if(i==9) return('0');
    if(i>9) return('A'+i-10);
}
static void do_display(void)
{
    aa_puts(context, 0, 0, AA_SPECIAL, "Calculating..");
    aa_flush(context);
    shrink(context, fast, x1, y1, x2, y2);
    aa_render(context, renderparams, 0, 0, aa_scrwidth(context), aa_scrheight(context));
    aa_flush(context);
}
static void resize(aa_context * c)
{
    aa_resize(c);
    resized = 1;
    do_display();
}

static int yesno(int x, int y, char *string)
{
    char c;
    aa_puts(context, x, y, AA_SPECIAL, string);
    aa_flush(context);
    while ((c = tolower(aa_getkey(context, 1))) != 'y' && c != 'n');
    return (c == 'y');
}
static int getnum(char *string)
{
    unsigned char c;
    aa_puts(context, 0, 0, AA_SPECIAL, string);
    aa_flush(context);
    while (((c = tolower(aa_getkey(context, 1))) < '0' || c > '9') && (c<'A' || c>'Z') && (c<'a' || c>'z'));
    if(c=='0') return 9;
    if(c<='9'&&c>='1') return (c-'1');
    if(c>='a'&&c<='z') return (c-'a'+10);
    if(c>='A'&&c<='Z') return (c-'A'+10);
    return (c - '0');
}
static void selectfont(aa_context * c)
{
    int i = 0, i1;
    char string[255];
    for (i = 0; aa_fonts[i] != NULL; i++) {
	sprintf(string, "%c - %-40s", getletter(i), aa_fonts[i]->name);
	aa_puts(context, 0, i, AA_SPECIAL, string);
    }
    i1 = getnum("");
    if (i1 < i)
	aa_setfont(c, *(aa_fonts + i1));
}
#if AA_LIB_VERSION==1 && AA_LIB_MINNOR==0
#define SUPPORTED c->driver->params.supported
#else
#define SUPPORTED c->driverparams.supported
#endif
static void selectsupported(aa_context * c)
{
    int supported = c->params.supported;
    char text[40];
    int ch;
    do {
	static char *texts[] =
	{
	    "Normal characters",
	    "Half bright (dim)",
	    "Double bright (bold)",
	    "Bold font",
	    "Reversed",
	    "8 bit ascii",
	    "Control characters"};
	int masks[] =
	{AA_NORMAL_MASK, AA_DIM_MASK, AA_BOLD_MASK, AA_BOLDFONT_MASK, AA_REVERSE_MASK, AA_EIGHT, AA_ALL};
	int i;
	for (i = 0; i < 7; i++) {
	    sprintf(text, "%i %-20s:%-12s", i + 1, texts[i], (SUPPORTED & masks[i]) ? ((supported & masks[i]) ? "On" : "Off") : "Unsupported");
	    aa_puts(context, 0, i, AA_SPECIAL, text);
	}
	aa_puts(context, 0, i, AA_SPECIAL, "8 Leave this menu");
	aa_flush(context);
	while ((ch = tolower(aa_getkey(context, 1))) < '1' || ch > '8');
	ch -= '1';
	if (ch < 7)
	    supported ^= masks[ch];
    } while (ch < 7);

    aa_setsupported(c, supported);
}
static void save(void)
{
    int i = 0, i1;
    struct aa_savedata data;
    aa_context *c;
    char string[255];
    char name[256];
    for (i = 0; aa_formats[i] != NULL; i++) {
	sprintf(string, "%c - %-40s", getletter(i), aa_formats[i]->formatname);
	aa_puts(context, 0, i, AA_SPECIAL, string);
    }
    while ((i1 = getnum("")) >= i);
    do_display();
    data.format = aa_formats[i1];
    if (data.format->flags & AA_USE_PAGES) {
	sprintf(string, "Page size is:%ix%i", data.format->pagewidth, data.format->pageheight);
	aa_puts(context, 0, 0, AA_SPECIAL, string);
    }
    aa_puts(context, 0, 1, AA_SPECIAL, "Width of image:");
    sprintf(string, "%i", data.format->width);
    do {
	aa_edit(context, 0, 2, 5, string, 256);
    } while (sscanf(string, "%i", &aa_defparams.width) != 1 || aa_defparams.width <= 0 || aa_defparams.width >= 65000);
    aa_puts(context, 0, 3, AA_SPECIAL, "Height of image:");
    sprintf(string, "%i", data.format->height);
    do {
	aa_edit(context, 0, 4, 5, string, 256);
    } while (sscanf(string, "%i", &aa_defparams.height) != 1 || aa_defparams.height <= 0 || aa_defparams.height >= 65000);
    name[0] = 0;
    data.name = name;
    aa_puts(context, 0, 5, AA_SPECIAL, "Filename:");
    aa_edit(context, 0, 6, 20, name, 256);
    strcat(name,"%c%e");
    do_display();
    aa_defparams.minwidth = 0;
    aa_defparams.minheight = 0;
    aa_defparams.maxwidth = 0;
    aa_defparams.maxheight = 0;
    c = aa_init(&save_d, &aa_defparams, &data);
    if (c == NULL) {
	return;
    }
    if (data.format->font == NULL)
	selectfont(c);
    else
	aa_setfont(c, data.format->font);
    do_display();
    selectsupported(c);
    do_display();
    aa_gotoxy(context, 0, 0);
    if (yesno(0, 0, "Save just current view? "))
	shrink(c, fast, x1, y1, x2, y2);
    else
	shrink(c, fast, 0, 0, imgwidth, imgheight);
    aa_puts(context, 0, 1, AA_SPECIAL, "saving..");
    aa_flush(context);
    aa_render(c, renderparams, 0, 0, aa_scrwidth(c), aa_scrheight(c));
    aa_flush(c);
    aa_close(c);
    do_display();
}
static void ui_help(void)
{
    aa_puts(context, 0, 1, AA_SPECIAL, " asciiarted image viewer by Jan Hubicka                            ");
    aa_puts(context, 0, 2, AA_SPECIAL, " a,w,d,x - move image one row/column  A,W,D,X - move image one page");
    aa_puts(context, 0, 3, AA_SPECIAL, " z       - unzoom                     Z       - zoom               ");
    aa_puts(context, 0, 4, AA_SPECIAL, " s       - Save image                                              ");
    aa_puts(context, 0, 5, AA_SPECIAL, " m       - change dithering mode      q       - quit               ");
    aa_puts(context, 0, 6, AA_SPECIAL, " i       - turns inversion on         I       - intversion off     ");
    aa_puts(context, 0, 7, AA_SPECIAL, " u       - select attributes          f       - select font        ");
    aa_puts(context, 0, 8, AA_SPECIAL, " space   - redraw                     ',','.' - change contrast    ");
    aa_puts(context, 0, 9, AA_SPECIAL, " ';',''' - change gamma               '<','>' - change bright      ");
    aa_puts(context, 0, 10, AA_SPECIAL, " '+','-' - zoom/unzoom                '<','>' - change bright      ");
}
void main_loop(void)
{
    int c, i;
    int quit = 0;
    char string[255];
    float xmul = ((float) aa_mmheight(context)) / aa_mmwidth(context);
    float ymul = ((float) aa_mmwidth(context)) / aa_mmheight(context);
    resized = 1;
    renderparams = aa_getrenderparams();
    aa_resizehandler(context, resize);
    while (!quit) {
	if (resized) {
	    xmul = ((float) aa_mmheight(context)) / aa_mmwidth(context);
	    ymul = ((float) aa_mmwidth(context)) / aa_mmheight(context);
	    x1 = 0;
	    y1 = 0;
	    x2 = imgwidth;
	    y2 = imgheight;
	    if (imgwidth * xmul > imgheight)
		y2 = imgwidth * xmul;
	    else
		x2 = imgheight * ymul;
	    x1 = (imgwidth - x2) / 2;
	    x2 += x1;
	    y1 = (imgheight - y2) / 2;
	    y2 += y1;
	    do_display();
	    resized = 0;
	}
	aa_flush(context);
	c = aa_getevent(context, 1);
	switch (c) {
	case 'h':
	    ui_help();
	    break;
	case 'z':
	    x1 = 0;
	    y1 = 0;
	    x2 = aa_imgwidth(context);
	    y2 = aa_imgwidth(context) * xmul;
	    do_display();
	    break;
	case 'Z':
	    x1 = 0;
	    y1 = 0;
	    x2 = imgwidth;
	    y2 = imgheight;
	    if (imgwidth * xmul > imgheight)
		y2 = imgwidth * xmul;
	    else
		x2 = imgheight * ymul;
	    do_display();
	    break;
	case 'd':
	    if (x2 < imgwidth - 1) {
		x1 += context->mulx;
		x2 += context->muly;
		do_display();
	    }
	    break;
	case AA_RIGHT:
	case 'D':
	    {
		int step = (x2 - x1) / 5;
		if (x2 < imgwidth - step) {
		    x1 += step;
		    x2 += step;
		} else {
		    x1 = imgwidth - x2 + x1;
		    x2 = imgwidth;
		}
	    }
	    do_display();
	    break;
	case 'a':
	    if (x1 > 1) {
		x1 -= context->mulx;
		x2 -= context->muly;
		do_display();
	    }
	    break;
	case AA_LEFT:
	case 'A':
	    {
		int step = (x2 - x1) / 5;
		if (x1 > step) {
		    x1 -= step;
		    x2 -= step;
		} else {
		    x2 = x2 - x1;
		    x1 = 0;
		}
		do_display();
	    }
	    break;
	case 'x':
	    if (y2 < imgheight - 1) {
		y1 += context->mulx;
		y2 += context->muly;
		do_display();
	    }
	    break;
	case AA_DOWN:
	case 'X':
	    {
		int step = (y2 - y1) / 5;
		if (y2 < imgheight - step) {
		    y1 += step;
		    y2 += step;
		} else {
		    y1 = imgheight - y2 + y1;
		    y2 = imgheight;
		}
	    }
	    do_display();
	    break;
	case 'w':
	    if (y1 > 2) {
		y1 -= context->mulx;
		y2 -= context->muly;
	    }
	    do_display();
	    break;
	case AA_UP:
	case 'W':
	    {
		int step = (y2 - y1) / 5;
		if (y1 > step) {
		    y1 -= step;
		    y2 -= step;
		} else {
		    y2 = y2 - y1;
		    y1 = 0;
		}
	    }
	    do_display();
	    break;
	case 'q':
	    quit = 1;
	    break;
	case 'm':
	    renderparams->dither = (renderparams->dither + 1) % AA_DITHERTYPES;
	    do_display();
	    aa_puts(context, 0, 0, AA_SPECIAL, aa_dithernames[renderparams->dither]);
	    break;
	case ' ':
	    do_display();
	    break;
	case 'Q':{
		renderparams->dither = AA_FLOYD_S;
		do_display();
		for (i = 2000; i > 5; i -= 7) {
		    renderparams->randomval = i;
		    aa_render(context, renderparams, 0, 0, aa_scrwidth(context), aa_scrheight(context));
		    aa_flush(context);
		}
	    }
	    renderparams->dither = AA_FLOYD_S;
	    aa_render(context, renderparams, 0, 0, aa_scrwidth(context), aa_scrheight(context));
	    aa_flush(context);
	    break;
	case 'E':{
		do_display();
		for (i = 0; i < 256; i += 50) {
		    renderparams->bright = i;
		    aa_render(context, renderparams, 0, 0, aa_scrwidth(context), aa_scrheight(context));
		    aa_flush(context);
		}
	    }
	    renderparams->bright = 255;
	    aa_render(context, renderparams, 0, 0, aa_scrwidth(context), aa_scrheight(context));
	    aa_flush(context);
	    renderparams->bright = 0;
	    renderparams->dither = AA_FLOYD_S;
	    aa_render(context, renderparams, 0, 0, aa_scrwidth(context), aa_scrheight(context));
	    aa_flush(context);
	    break;
	case 'R':{
		do_display();
		for (i = -256; i < 0; i += 5) {
		    renderparams->bright = i;
		    aa_render(context, renderparams, 0, 0, aa_scrwidth(context), aa_scrheight(context));
		    aa_flush(context);
		}
	    }
	    renderparams->bright = 0;
	    aa_render(context, renderparams, 0, 0, aa_scrwidth(context), aa_scrheight(context));
	    aa_flush(context);
	    break;
	case 'T':{
		do_display();
		for (i = -256; i < 256; i += 5) {
		    renderparams->bright = i;
		    aa_render(context, renderparams, 0, 0, aa_scrwidth(context), aa_scrheight(context));
		    aa_flush(context);
		}
	    }
	    renderparams->bright = 0;
	    aa_render(context, renderparams, 0, 0, aa_scrwidth(context), aa_scrheight(context));
	    aa_flush(context);
	    break;
	case 'i':
	    renderparams->inversion = 1;
	    do_display();
	    break;
	case 'I':
	    renderparams->inversion = 0;
	    do_display();
	    break;
	case 's':
	    save();
	    break;
	case 'F':
	    fast = 1;
	    do_display();
	    break;
	case 'f':
	    fast = 0;
	    do_display();
	    break;
	case 'u':
	    selectsupported(context);
	    do_display();
	    break;
	case 'g':
	    selectfont(context);
	    do_display();
	    break;
	case ';':
	    renderparams->gamma /= 1.05;
	    do_display();
	    sprintf(string, "gamma:%f", renderparams->gamma);
	    aa_puts(context, 0, 0, AA_SPECIAL, string);
	    break;
	case '\'':
	    renderparams->gamma *= 1.05;
	    do_display();
	    sprintf(string, "gamma:%f", renderparams->gamma);
	    aa_puts(context, 0, 0, AA_SPECIAL, string);
	    break;
	case '<':
	    renderparams->bright -= 4;
	    do_display();
	    sprintf(string, "bright:%f", renderparams->bright / 255.0);
	    aa_puts(context, 0, 0, AA_SPECIAL, string);
	    break;
	case '>':
	    renderparams->bright += 4;
	    do_display();
	    sprintf(string, "bright:%f", renderparams->bright / 255.0);
	    aa_puts(context, 0, 0, AA_SPECIAL, string);
	    break;
	case '[':
	    renderparams->randomval -= 4;
	    do_display();
	    sprintf(string, "bright:%f", renderparams->bright / 255.0);
	    aa_puts(context, 0, 0, AA_SPECIAL, string);
	    break;
	case ']':
	    renderparams->randomval += 4;
	    do_display();
	    sprintf(string, "bright:%f", renderparams->bright / 255.0);
	    aa_puts(context, 0, 0, AA_SPECIAL, string);
	    break;
	case ',':
	    renderparams->contrast -= 2;
	    do_display();
	    sprintf(string, "contrast:%f", renderparams->contrast / 127.0);
	    aa_puts(context, 0, 0, AA_SPECIAL, string);
	    break;
	case '.':
	    renderparams->contrast += 2;
	    do_display();
	    sprintf(string, "contrast:%f", renderparams->contrast / 127.0);
	    aa_puts(context, 0, 0, AA_SPECIAL, string);
	    break;
	case '-':
	    {
		int tmp = (x2 - x1 + 7) / 8;
		x1 -= tmp;
		x2 += tmp;
		tmp = (y2 - y1) / 8;
		y1 -= tmp;
		y2 = y1 + (x2 - x1) * xmul;
		do_display();
	    } break;
	case '+':
	    {
		int tmp = (x2 - x1) / 8;
		x1 += tmp;
		x2 -= tmp;
		tmp = (y2 - y1) / 8;
		y1 += tmp;
		y2 = y1 + (x2 - x1) * xmul;
		do_display();
	    } break;
	}
    }
}
