#include "general.h"

/* general module source */

long freadblock( FILE *fp, long size, void *target ) {
	return fread( target, size, 1, fp );
}
