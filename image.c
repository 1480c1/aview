#include <stdio.h>
#include <unistd.h>
#include <malloc.h>
#include "config.h"

int imgwidth, imgheight;
unsigned char *imgdata;
#define PBM 0
#define PGM 1
#define PPM 2

int load_image(char *name)
{
    char data[65536];
    int i, c, max;
    int raw = 0;
    int type;
    FILE *file;

    if (name[0]=='\0') {
#ifdef HAVE_ISATTY
      if (isatty(STDIN_FILENO)) {
        printf("Missing filename.\n Use --help for list of options.\n");
        return 0;
      }
#endif
      file = stdin;
    }
    else {
      if ((file = fopen(name, "rb")) == NULL) {
        printf("File not found\n");
        return 0;
      }
    }
    if (getc(file) != 'P') {
	printf("Invalid magic-not p?m family format\n");
	return 0;
    }
    c = getc(file);
    if (c < '1' || c > '6') {
	printf("Invalid magic-unknown p?m family format\n");
	return 0;
    }
    if (c >= '4')
	raw = 1, c -= 3;
    type = c - '1';
    c = getc(file);
    while (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '#') {
	if (c == '#')
	    fgets(data, 65536, file);
	c = getc(file);
    }
    ungetc(c, file);

    if (fscanf(file, "%i %i", &imgwidth, &imgheight) != 2) {
	printf("Parse error\n");
	return 0;
    }
    switch (type) {
    case PGM:
    case PPM:
	if (fscanf(file, "%i", &max) != 1) {
	    printf("Parse error\n");
	    return 0;
	}
	break;
    case PBM:
	max=1;
	break;
    }
    c = getc(file);
    if ((imgdata = malloc(imgwidth * imgheight)) == NULL) {
	printf("Out of memory\n");
	return 0;
    }
    if (!raw) {
        if(type==PPM) {
	  int r,g,b;
	  for (i = 0; i < imgwidth * imgheight; i++) 
	    fscanf(file, "%i %i %i", &r,&g,&b), 
	    imgdata[i]=(r*76+g*150+b*29)/max;
	  max=255;
	} else
	for (i = 0; i < imgwidth * imgheight; i++)
	    fscanf(file, "%i\n", &c), imgdata[i] = c;
    } else {
        if(type==PBM) {
	  for(i=0;i<imgwidth*imgheight;)
	  {int n;
	    c=getc(file);
            for(n=128;n&&i<imgwidth*imgheight;n>>=1,i++) 
	      imgdata[i]=(n&c)!=0;
	  }
	} else
	if(type==PPM) {
	  for(i=0;i<imgwidth*imgheight;i++)
	  {
	  int r,g,b;
	  r=getc(file);
	  g=getc(file);
	  b=getc(file);
	  imgdata[i]=(r*76+g*150+b*29)/max;
	  }
	  max=255;
	} else
	fread(imgdata, 1, imgwidth * imgheight, file);
    }
    for (i = 0; i < imgwidth * imgheight; i++)
	imgdata[i] = imgdata[i] * 255 / max;
    if (name[0]!='\0') fclose(file);
    return 1;
}
