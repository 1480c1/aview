
/* Copyright 1993 J. Remyn */
/* This program is freely distributable, but there is no warranty */
/* of any kind. */
/* version 0.3 */
/* Modified for aaflip version 1.0 by Jan Hubicka*/

#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>
#include <aalib.h>

#include "general.h"
#include "fli.h"

#define NAMELEN 256

struct OPTIONS {
	char filename[NAMELEN];	/* filename given on command line */
	char fast;		/* 1 = play without delays */
	char verbose;		/* 1 = show info about flic file */
	char release;		/* 1 = don't keep frames in memory */
	char firstp;		/* 1 = process frames while loading */
	char blank;		/* 1 = keep black screen while loading */
	char stdinp;		/* 1 = take flic file from stdin */
	int speed;		/* speed gotten from command line (-1 if nothing gotten) */
	int playrep;		/* nr of times to play animation */
} options;

struct FLI_FRAMECHUNK {
	long size;		/* Size of chunk data part */
	long file_offset;	/* Offset of chunk data part into flic file */
	int subchunks;		/* Number of subchunks in data part */
	void *cd;		/* Pointer to memory containing data part, */
				/* is NULL if data not kept in memory */
};

struct FLI {
	char filename[NAMELEN]; /* filename of fli file */
	char flc;		/* 0 = fli, 1 = flc */
	int mode;		/* vgalib mode number to be used */
	int scr_width;		/* width of screen mode used */
	int scr_height;		/* guess */
	int current;		/* current frame index */
	long time;		/* time of last processed frame in usec */
	FILE *f;		/* file pointer of opened flic file */
	struct FLI_HEADER h;	/* 128 byte fli file header structure */
	struct FLI_FRAMECHUNK *frame;
				/* pointer to an allocated 'array' of 
				   <frames> FLI_FRAMECHUNK structs */
} fli;


static aa_palette pal;
static aa_context *context;
static aa_renderparams *params;
static char *graph_mem;


static void dcd_color_64( char *data ) {
uchar start = 0;
int ops;
int count;
int i;
/*	puts( "color_64" ); */
	ops = *(short int *)data;
	data = (void *)(((short int *)data) + 1);
	while( ops-- > 0 ) {
		start += *(uchar *)data;
		data = (void *) ((uchar *)data + 1);
		if( (count = (int)*(uchar *)data) == 0 )
			count = 256;
		data = (void *) ((uchar *)data + 1);
		for( i=0; i<count; i++ ) {
			/* (s)vgalib requires a table of ints for setting a group
			 * of colors quickly, but we've got a table of chars :( 
			 * so we have to set each color individually (slow) */
			aa_setpalette(pal, start + i, 
				((int)*(uchar *)data)*4,
				((int)*((uchar *)data+1)*4),
				((int)*((uchar *)data+2)*4)
			);
			data = (void *)((uchar *)data + 3);
		}
		start += count;
	}
}

static void dcd_color_256( char *data ) {
uchar start = 0;
int ops;
int count;
int i;
/*	puts( "color_256" ); */
	ops = *(short int *)data;
	data = (void *)((short int *)data + 1); 
	while( ops-- > 0 ) {
		start += *(uchar *)data;
		data = (void *)((uchar *)data + 1);
		if( (count = (int)*(uchar *)data) == 0 )
			count = 256;
		data = (void *)((uchar *)data + 1);
		for( i=0; i<count; i++ ) {
			aa_setpalette(pal, start + i, 
				*(uchar *)data >> 2,
				*((uchar *)data+1) >> 2,
				*((uchar *)data+2) >> 2
			);
			data = (void *)((uchar *)data + 3);
		}
		start += count;
	}
}

void *memsetw( unsigned short *target, unsigned short w, int count ) {
	while( count-->0 ) {
		*target++ = w;
	}
	return( target );
}

static void dcd_delta_flc( struct FLI *fli, char *data ) {
short int line;
short int nrlines;
char *index;
short packets;
short type;
int x;
uchar lastbyte;
char setlastbyte;
/*	puts( "delta flc" ); */
	/* get nr of lines in chunk */
	nrlines = *(short int *)data;
	data = (void *)((short int *)data + 1);
	line = 0;
	setlastbyte = 0;
	lastbyte = 0;	/* this is just to get rid of a compiler warning */
	index = graph_mem;
	
	while( nrlines>0 ) { /* don't put nrlines-- here, the continues don't allow it */

		type = *(short *)data;
		data = (void *)((short *)data + 1);
		/* the 2 highest bits of type indicate how to interpret it */
		if( type<0 ) {
			if( type&0x4000 ) {
				/* add to the line number and act as if nothing
				 * happened.
				 * documentation says it's all right just to
				 * add the abs. value of the word, which is the
				 * same as subtracting the word as it is always
				 * negative.
				 */
				line -= type;
				index += fli->scr_width * (-type);
				continue;
			}
			/* the low byte contains a lastbyte (the last byte
			 * on a line in odd-width resolution flic files),
			 * this word is followed by the packet count
			 */
			setlastbyte = 1;
			lastbyte = (uchar)(type & 0x00FF);
			packets = *(short *)data;
			data = (void *)((short *)data + 1);
			/* packets can be 0 now if just the last byte
			 * changes for this line
			 */
		}
		if( type>=0 ) {
			/* the word contains the nr of packets
			 * we can just assign this to packets because the
			 * high bits are zero
			 */
			packets = type;
		}

		/* decode packets */
		x = 0;
		while( packets-->0 ) {
			/* get & decode packet */
			x += *(uchar *)data;
			data = (void *)((uchar *)data + 1);
			type = *(char *)data;
			data = (void *)((uchar *)data + 1);
			if( (char)type>=0 ) {
				/* copy ptype words */
				type <<= 1;
				memcpy( index + x, data, type );
				x += type;
				data = (void *)((uchar *)data + type);
				continue;
			}
			type = -(char)type;
			memsetw( (ushort *)(index + x), *(ushort *)data, type );
			x += type<<1;
			data = (void *)((ushort *)data + 1);
		}
		if( !setlastbyte ) {
			index = (char *)((uchar *)index + fli->scr_width);
			nrlines--;
			continue;
		}
		/* put lastbyte at end of line */
		*(uchar *)(index + fli->scr_width - 1) = lastbyte;
		setlastbyte = 0;
		index = (char *)((uchar *)index + fli->scr_width);
		nrlines--;
	}
}


static void dcd_delta_fli( struct FLI *fli, char *data ) {
short int line;
short int nrlines;
char *index;
uchar packets;
int index_x;
char type;
/*	puts( "delta fli" ); */
	line = *(short int *)data;
	data = (void *)((short int *)data + 1);
	index = graph_mem + line * fli->scr_width;
	nrlines = *(short int *)data;
	data = (void *)((short int *)data + 1);
	while( nrlines-- > 0 ) {
		index_x = 0;
		packets = *(uchar *)data;
		data = (void *)((uchar *)data + 1);
		while( packets > 0 ) {
			index_x += *(uchar *)data;
			data = (void *)((uchar *)data + 1);
			type = *(char *)data;
			data = (void *)((char *)data + 1);
			if( type >= 0 ) {
				memcpy( index + index_x, data, type );
				index_x += type;
				data = (void *)((uchar *)data + type);
				packets--;
				continue;
			}
			memset( index + index_x, *(uchar *)data, -type );
			index_x -= type;
			data = (void *)((uchar *)data + 1);
			packets--;
		}
		index += fli->scr_width;
	}
}



static void dcd_byte_run( struct FLI *fli, char *data ) {
int lines;
int width;
char type;
int index;
int index_x;
/* 	puts( "byte run" ); */
	lines = fli->h.height;
	width = fli->h.width;
	index = 0;
	while( lines-->0 ) {
		data = (void *)((uchar *)data + 1);	/* skip byte containing number of packets */
		/* start a loop to decode packets until end of line is reached */
		index_x = 0;
			while ( index_x < width ) {
				type = *((char *)data++);
				if( type<0 ) {
					/* type now contains nr of bytes to copy to video */
					memcpy( graph_mem + index + index_x, (uchar *)data, -type );
					index_x -= type;
					data = (void *)((uchar *)data - type);
				}
				else {
					memset( graph_mem + index + index_x, *(uchar *)data++, type );
					index_x += type;
				}
			}
			index += fli->scr_width;
	}
}


static void dcd_black( struct FLI *fli, char *data ) {
/*	puts( "black" ); */
		int l;
		uchar *index;
		
		l = fli->h.height;
		index = graph_mem;
		while( l-- > 0 ) {
			memset( index, 0, fli->h.width );
			index += fli->scr_width;
		}
}

static void dcd_literal( struct FLI *fli, char *data ) {
int l;
/*	puts( "literal" ); */
	l = fli->h.height;
	{
		/* linear video memory */
		char *index;
		index = graph_mem;
		while( l-- > 0 ) {
			memcpy( index, data, fli->h.width );
			data = (void *)((uchar *)data + fli->h.width);
			index += fli->scr_width;
		}
	}
}

static void dcd_pstamp( char *data ) {
/*	puts( "pstamp" ); */
}

static void showhelp( void ) {
	puts( "FLI Player for Linux (version 0.2a) by John Remyn" );
	puts( "modified version for aalib by Jan Hubicka" );
	puts( "Now it is ascii arted FLI Player for text mode" );
	puts( "Usage:" );
	puts( " flip [switch] <filename>" );
	puts( "Valid switches are :" );
	puts( " -f          Switch off clock synchronization." );
	puts( " -v          Show information on flic file." );
	puts( " -? -h       Show help." );
	puts( " -a          Don't keep frames in memory." );
	puts( " -b          Process frames when they are loaded." );
	puts( " -c          Keep a blank screen while frames are being loaded." );
	puts( " -n <number> Play the animation sequence <n> times." );
	puts( " -s <delay>  Set delay between frames to <delay> miliseconds." );
	puts( " -           Read flic file from standard input." );
	puts( "also standard aalib options are supported" );
	puts( " -dim, -bold, -reverse, -normal for enabling attributes");
	puts( " -nodim, -nobold, -noreverse, -nonormal for disabling");
	puts( "Press H for help on keys while playing." );
}
	
static void parse_cmdln( int argc, char *argv[], struct OPTIONS *options ) {
int i,j;
	
	options->filename[0] = '\0';
	options->fast = 0;
	options->verbose = 0;
	options->release = 0;
	options->firstp = 0;
	options->blank = 0;
	options->playrep = 1;
	options->speed = -1;
	options->stdinp = 0;
	for( i=1; i<argc; i++ ) {
		if( *argv[i]=='-' ) {
			char c;
			int k;
			/* argv[i] contains options */
			if( *(argv[i]+1)=='\0' ) {
				options->stdinp = 1;
			}
			j = 1;
			k = 0;
			while( (c = *(argv[i]+j))!='\0' ) {
				switch( c ) {
					case 'f' : options->fast = 1; break;
					case 'v' : options->verbose = 1; break;
					case 'a' : options->release = 1; break;
					case 'b' : options->firstp = 1; break;
					case 'c' : options->blank = 1; break;
					case '?' :
					case 'h' : showhelp(); exit( 0 ); break;
					case 'n' : if( i+k+1<argc ) {
							k++;
							options->playrep = abs( atoi( argv[i+k] ) );
						   }
						   break;
					case 's' : if( i+k+1<argc ) {
							k++;
						   	options->speed = abs( atoi( argv[i+k] ) );
							/* adjust to some reasonable value */
							if (options->speed > 1000) options->speed = 1000;
							/* convert to usec */
							options->speed *= 1000;
						   }
						   break;
					default  : showhelp(); 
						   puts( "Unknown option." );
						   exit( -1 );
				}
				j++;
			}
			i = i+k;
		}
		else {
			if( argv[i]!=NULL ) {
				/* argv[i] is the filename of the flic file */
				strncpy( options->filename, argv[i], NAMELEN );
			}
		}
	}
	if( options->filename[0]=='\0' && options->stdinp==0 ) {
		showhelp();
		puts( "No filename given." );
		exit( -1 );
	}
}

static int open_fli( struct FLI *fli, struct OPTIONS *options ) {
	/* open file and read the header */
	if( !options->stdinp ) {
		fli->f = fopen( fli->filename, "rb" );
		if( !fli->f ) {
			return 0;
		}
	} 
	else {
		fli->f = stdin;
	}
	freadblock( fli->f, sizeof( fli->h.size      ), &(fli->h.size)      );
	freadblock( fli->f, sizeof( fli->h.type      ), &(fli->h.type)      );
	freadblock( fli->f, sizeof( fli->h.frames    ), &(fli->h.frames)    );
	freadblock( fli->f, sizeof( fli->h.width     ), &(fli->h.width)     );
	freadblock( fli->f, sizeof( fli->h.height    ), &(fli->h.height)    );
	freadblock( fli->f, sizeof( fli->h.depth     ), &(fli->h.depth)     );
	freadblock( fli->f, sizeof( fli->h.flags     ), &(fli->h.flags)     );
	freadblock( fli->f, sizeof( fli->h.speed     ), &(fli->h.speed)     );
	freadblock( fli->f, sizeof( fli->h.reserved0 ), &(fli->h.reserved0) );
	freadblock( fli->f, sizeof( fli->h.created   ), &(fli->h.created)   );
	freadblock( fli->f, sizeof( fli->h.creator   ), &(fli->h.creator)   );
	freadblock( fli->f, sizeof( fli->h.updated   ), &(fli->h.updated)   );
	freadblock( fli->f, sizeof( fli->h.updater   ), &(fli->h.updater)   );
	freadblock( fli->f, sizeof( fli->h.aspectx   ), &(fli->h.aspectx)   );
	freadblock( fli->f, sizeof( fli->h.aspecty   ), &(fli->h.aspecty)   );
	freadblock( fli->f, sizeof( fli->h.reserved1 ), &(fli->h.reserved1) );
	freadblock( fli->f, sizeof( fli->h.oframe1   ), &(fli->h.oframe1)   );
	freadblock( fli->f, sizeof( fli->h.oframe2   ), &(fli->h.oframe2)   );
	freadblock( fli->f, sizeof( fli->h.reserved2 ), &(fli->h.reserved2) );

	/* now check if it's a flc or a fli and take necessary precautions */
	if( (unsigned)fli->h.type==FLITYPE ) {
		/* it's a fli. convert as much as possible to flc */
		fli->flc = 0;
		/* convert frame rate from 1/70 sec to usec */
		fli->h.speed *= 100;
		fli->h.speed /= 7;
		fli->h.speed *= 1000;
 		/* ascpect ratio is ignored, but fix it anyway */
		fli->h.aspectx = 6;
		fli->h.aspecty = 5;
		/* can't convert frame offsets */
	}
	else if( (unsigned)fli->h.type==FLCTYPE ) {
		fli->flc = 1;
		fli->h.speed *= 1000;	/* convert msec to usec */
	}
	else {
		puts( "File type not recognized." );
		exit( -1 );
	}
	return 1;
};


static void describe_fli( struct FLI *fli ) {
	printf( "Filename  %s\n",  fli->filename );
	printf( "size      %li\n", fli->h.size );
	printf( "type      %x\n",  fli->h.type );
	printf( "frames    %i\n",  fli->h.frames );
	printf( "width     %i\n",  fli->h.width );
	printf( "height    %i\n",  fli->h.height );
	printf( "depth     %i\n",  fli->h.depth );
	printf( "flags     %x\n",  fli->h.flags );
	printf( "speed     %li\n", fli->h.speed );
	printf( "aspectx   %i\n",  fli->h.aspectx );
	printf( "aspecty   %i\n",  fli->h.aspecty );
	printf( "oframe1   %li\n", fli->h.oframe1 );
	printf( "oframe2   %li\n", fli->h.oframe2 );
};


static void readchunkheader( FILE *f, struct FLI_CHUNK_HEADER *buf ) {
	freadblock( f, sizeof( buf->size     ), &buf->size );
	freadblock( f, sizeof( buf->type     ), &buf->type );
	freadblock( f, sizeof( buf->chunks   ), &buf->chunks );
	freadblock( f, sizeof( buf->reserved ), &buf->reserved );
}
	
static void scan_to_frame( struct FLI *fli ) {
struct FLI_CHUNK_HEADER buf;
struct FLI_FRAMECHUNK *fc;
	do {
		readchunkheader( fli->f, &buf );
		if( buf.type==0xF1FA ) break;
		fseek( fli->f, buf.size - FLI_CHUNK_HEADERLEN, SEEK_CUR );
	} while ( buf.type!=0xF1FA );
	fc = fli->frame + fli->current;
	fc->size = buf.size - FLI_CHUNK_HEADERLEN;
	fc->subchunks = buf.chunks;
	fc->file_offset = ftell( fli->f );
	fc->cd = NULL;
}

static void getframechunkdata( struct FLI *fli ) {
struct FLI_FRAMECHUNK *fc;
void *data;
	fc = fli->frame + fli->current;
	data = fc->cd;
	if( data )
		return;	/* frame chunk data already loaded */
	if(fc->size) {
	data = malloc( fc->size );
	if( !data ) {
		printf( "cannot allocate memory for frame data\n" );
		exit( 1 );
	}
	fseek( fli->f, fc->file_offset, SEEK_SET );
	freadblock( fli->f, fc->size, data );
	} else data=NULL;
	fc->cd = data;
}

static void releaseframechunkdata( struct FLI *fli ) {
struct FLI_FRAMECHUNK *fc;
	fc = fli->frame + fli->current;
	if( fc->cd ) {
		free( fc->cd );
		fc->cd = NULL;
	}
}
	
static void processframechunk( struct FLI *fli ) {
struct FLI_FRAMECHUNK *fc;
void *data;
int i;
	fc = fli->frame + fli->current;
	data = fc->cd;
	for( i=0; i<fc->subchunks; i++ ) {
		/* switch( chunktype ) */
		switch( *((short int *)((char *)data+4)) ) {
			case COLOR_256 : 
				dcd_color_256( (char *)data + 6 );
				break;
			case DELTA_FLC :
				dcd_delta_flc( fli, (char *)data + 6 );
				break;
			case COLOR_64 :
				dcd_color_64( (char *)data + 6 );
				break;
			case DELTA_FLI :
				dcd_delta_fli( fli, (char *)data + 6 );
				break;
			case BLACK :
				dcd_black( fli, (char *)data + 6 );
				break;
			case BYTE_RUN :
				dcd_byte_run( fli, (char *)data + 6 );
				break;
			case LITERAL :
				dcd_literal( fli, (char *)data + 6 );
				break;
			case PSTAMP :
				dcd_pstamp( (char *)data + 6 );
				break;
			default :
				puts( "unknown subchunk" );
		}
		data = (void *)((char *)data + *(long *)data);
	}
}
		
long gettime(void) {
struct timeval t;
        gettimeofday(&t, NULL);
        return(t.tv_usec);
}

int await() {
#define T 10000   /* process key every 10ms */
int i;
long t;
	t = gettime();
	if (t < fli.time) t = t + 1E6;
	t = fli.h.speed - (t - fli.time);
	if (t > 0) {
		if (t > T) {
			for (i = 0; i < (t / T); i++) {
				usleep(T);
				if (f_getkey() == 'q') return(1);
			}
			usleep(t % T);
		}
		else usleep(t);
	}
	return(f_getkey() == 'q');
}

static void fastscale(char *b1, char *b2, int x1, int x2, int y1, int y2, int width1, int width2)
{
    int ddx1, ddx, spx = 0, ex;
    int ddy1, ddy, spy = 0, ey;
    int x;
    char *bb1 = b1;
    width2 -= x2;
    if (!x1 || !x2 || !y1 || !y2)
	return;
    ddx = x1 + x1;
    ddx1 = x2 + x2;
    if (ddx1 < ddx)
	spx = ddx / ddx1, ddx %= ddx1;
    ddy = y1 + y1;
    ddy1 = y2 + y2;
    if (ddy1 < ddy)
	spy = (ddy / ddy1) * width1, ddy %= ddy1;
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
	b1 = bb1;
    }
}
static int resized;
static void resize(aa_context * c)
{
    aa_resize(c);
    resized = 1;
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
    while ((c = tolower(aa_getkey(context, 1))) < '0' || c > '9');
    return (c - '0');
}
static void selectfont(aa_context * c)
{
    int i = 0, i1;
    char string[255];
    for (i = 0; aa_fonts[i] != NULL; i++) {
	sprintf(string, "%i - %-40s", i, aa_fonts[i]->name);
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
    char *texts[]={
      "Normal characters",
      "Half bright (dim)",
      "Double bright (bold)",
      "Bold font",
      "Reversed",
      "8 bit ascii",
      "Control characters"};
    int masks[]={AA_NORMAL_MASK,AA_DIM_MASK,AA_BOLD_MASK,AA_BOLDFONT_MASK,AA_REVERSE_MASK,AA_EIGHT,AA_ALL};
    int i;
    for(i=0;i<7;i++)
    {
      sprintf(text,"%i %-20s:%-12s",i+1,texts[i],(SUPPORTED&masks[i])?((supported&masks[i])?"On":"Off"):"Unsupported");
      aa_puts(c,0,i,AA_SPECIAL,text);
    }
    aa_puts(c,0,i,AA_SPECIAL,"8 Leave this menu");
    aa_flush(c);
    while ((ch = tolower(aa_getkey(context, 1))) < '1' || ch > '8');
    ch-='1';
    if(ch<7) supported^=masks[ch];
    } while (ch<7);

    aa_setsupported(c, supported);
}
int f_getkey(void)
{
  int c=aa_getkey(context,0);
  switch(c) {
        case 'h':
        case 'H':
	    aa_puts(context,0,0,AA_SPECIAL," aaflip - an ascii art fli/flc player           ");
	    aa_puts(context,0,1,AA_SPECIAL,"                                                ");
	    aa_puts(context,0,2,AA_SPECIAL," ';' '\\'  - gamma           '<' '>' - bright    ");
	    aa_puts(context,0,3,AA_SPECIAL," '[' ']'  - Random dithering',' '.' - contrast  ");
	    aa_puts(context,0,4,AA_SPECIAL," 'I' 'i'  - inversion                           ");
	    aa_puts(context,0,5,AA_SPECIAL,"                                                ");
	    aa_puts(context,0,6,AA_SPECIAL,"   'm'    - Dithering method  'q'   - quit      ");
	    aa_puts(context,0,7,AA_SPECIAL,"   'u'    - Select attributes 'g'   - font      ");
	    aa_flush(context);
	    aa_getkey(context,1);
	    break;
        case ';':
            params->gamma /= 1.05;
            break;
        case '\'':
            params->gamma *= 1.05;
            break;
        case '<':
            params->bright -= 4;
            break;
        case '>':
            params->bright += 4;
            break;
        case '[':
            params->randomval -= 4;
            break;
        case ']':
            params->randomval += 4;
            break;
        case ',':
            params->contrast -= 2;
            break;
        case '.':
            params->contrast += 2;
            break;
        case 'm':
            params->dither = (params->dither + 1) % AA_DITHERTYPES;
            break;
        case 'i':
            params->inversion = 1;
	    break;
        case 'I':
            params->inversion = 0;
	    break;
        case 'u':
	    selectsupported(context);
	    break;
        case 'g':
	    selectfont(context);
	    break;
  }
  return(c);
}

int main( int argc, char *argv[] ) {
int quit = 0;
int playstartframe = 0;
int first=1;
long t, tdelta;
	aa_parseoptions(NULL,NULL,&argc,argv);
	parse_cmdln( argc, argv, &options );
	strcpy( fli.filename, options.filename );

	/* open flic and get header information */	
	if( !open_fli( &fli, &options ) ) {
		puts( "cannot open fli file" );
		exit( 1 );
	}
	
	/* show header */
	if( options.verbose==1 ) {
		describe_fli( &fli );
	}

	/* optionally set delays */
	if( options.speed>=0 ) {
		fli.h.speed = options.speed;
	}

	/* optionally reduce delays to 0 (this overrides the set delay option) */
	if( options.fast==1 ) {
		fli.h.speed = 0;
	}
	

	/* silly but might happen */
	if( options.playrep<=0 ) {
		exit( 0 );
	}
	
	/* determine graphics mode to use */
	/* set mode */
	context=aa_autoinit(&aa_defparams);
	if(context==NULL) {
	  printf("Failed to initialize aalib\n");
	  exit(1);
	}
	aa_hidecursor(context);
	aa_autoinitkbd(context,0);
	aa_resizehandler(context,resize);
	params=aa_getrenderparams();
	fli.scr_width = fli.h.width;
	fli.scr_height = fli.h.height;
	graph_mem=malloc(fli.scr_width*fli.scr_height);

	fli.frame = (struct FLI_FRAMECHUNK *)malloc( (fli.h.frames+1) * sizeof( struct FLI_FRAMECHUNK ) );
	if( fli.frame==NULL ) {
		puts( "cannot allocate enough memory to store frame information\n" );
		exit( 1 );
	}

	fli.current = 0;
	/* preloading */
	/* first the 1st frame which is a full screen frame */
	scan_to_frame( &fli );

	if( !options.blank ) {
		getframechunkdata( &fli );
		processframechunk( &fli );
		releaseframechunkdata( &fli );
		playstartframe = 1;
	}
	else {
		getframechunkdata( &fli );
		playstartframe = 0;
		if( options.release ) {
			releaseframechunkdata( &fli );
		}
	}
	fli.current++;
	first=1;
	while( fli.current<=fli.h.frames && !quit ) {
		scan_to_frame( &fli );
		getframechunkdata( &fli );
		if( options.firstp ) {
			fli.time = gettime();
			processframechunk( &fli );
		}
		if( options.release ) {
			releaseframechunkdata( &fli );
		}
		if(first||options.firstp) {
			fastscale(graph_mem,context->imagebuffer,
			  	  fli.scr_width,aa_imgwidth(context),
			  	  fli.scr_height,aa_imgheight(context),
			  	  fli.scr_width,aa_imgwidth(context));
			aa_renderpalette(context,pal,params,0,0,aa_imgwidth(context),aa_imgheight(context));
			if(first&&!options.firstp)
		  	  aa_puts(context,0,0,AA_SPECIAL,"Preloading...");
			aa_flush(context);
		}
		fli.current++;
		first=0;
		if( f_getkey()=='q' ) quit = 1;
                if(options.firstp&&!quit) quit = await();
	}

	if(!quit) {
	
 	if( options.firstp ) {
		options.playrep--;
		if( options.playrep==0 ) {
			quit = 1;
		}
	}
		
	fli.current = playstartframe;
	while( !quit ) {
		fli.time = gettime();
		getframechunkdata( &fli );
		processframechunk( &fli );
		if( options.release ) {
			releaseframechunkdata( &fli );
		}
		fli.current++;
		fastscale(graph_mem,context->imagebuffer,
			  fli.scr_width,aa_imgwidth(context),
			  fli.scr_height,aa_imgheight(context),
			  fli.scr_width,aa_imgwidth(context));
		aa_renderpalette(context,pal,params,0,0,aa_imgwidth(context),aa_imgheight(context));
		aa_flush(context);
		if( fli.current>fli.h.frames ) {
			fli.current = 1;
			options.playrep--;
			if( options.playrep==0 ) {
				quit = 1;
				break;
			}
		}
                quit = await();

	}
	}
		
	/* restore textmode */
	aa_close(context);

	/* close flic */
	if( !options.stdinp ) {
		fclose( fli.f );
	}
	fli.f = NULL;
	return 0;
};

	

