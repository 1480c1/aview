
#define FLITYPE 0xAF11
#define FLCTYPE 0xAF12

#define COLOR_256	4
#define DELTA_FLC	7
#define COLOR_64	11
#define DELTA_FLI	12
#define BLACK		13
#define BYTE_RUN	15
#define LITERAL		16
#define PSTAMP		18

/* Structure of flic file header. Fields with + before the comment are FLC only. */
struct FLI_HEADER {
	long			size;		/*  size of entire file */
	unsigned short int	type;		/*  0xAF12 = FLC, 0xAF11 = FLI */
	short int		frames;		/*  number of frames in flic */
	short int 		width;		/*  screen width in pixels */
	short int 		height;		/*  sceen height in pixels */
	short int 		depth;		/*  bits per pixel (always 8) */
	unsigned short int	flags;		/*  set to 0x0003 */
	long			speed;		/*  time delay between frames, FLI: 1/70thsec, FLC: msec */
	short int 		reserved0;	/*  set to 0 */
	long			created;	/*+ MS-DOS formatted date and time of creation */
	long			creator;	/*+ Animator Pro serial number */
	long			updated;	/*+ date of last update */
	long 			updater;	/*+ Serial number of updating program */
	short int		aspectx;	/*+ ratio of screen used to create flic */
	short int 		aspecty;	/*+ */
	char  			reserved1[38];	/*+ 38 bytes set to 0 */
	long			oframe1;	/*+ file offset of first frame */
	long			oframe2;	/*+ file offset of second frame */
	char                    reserved2[40];	/*  40 bytes set to 0 */
};	
	
#define FLI_HEADERLEN ( sizeof( struct FLI_HEADER ) )

struct FLI_CHUNK_HEADER {
	long 			size;		/* size of chunk */
	short unsigned int	type;		/* chunk type */
	short unsigned int 	chunks;		/* number of subchunks */
	char 			reserved[8];	/* set to 0 */
};

#define FLI_CHUNK_HEADERLEN ( sizeof( struct FLI_CHUNK_HEADER ) )