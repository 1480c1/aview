#include <aalib.h>
#include "image.h"
#include "ui.h"

extern struct aa_driver curses_d;
int main(int argc, char **argv)
{
    aa_parseoptions(NULL,NULL,&argc,argv);
    if (argc != 2) {
	printf("Usage: %s [options] filename.p[ngbp]m\n", argv[0]);
        printf("    or %s [options] -stdin\n", argv[0]);
	printf("Options:\n%s\n",aa_help);
	exit(1);
    }
    if (!load_image(argv[1]))
	exit(-1);
    context = aa_autoinit(&aa_defparams);
    if (context == NULL)
	exit();
    aa_autoinitkbd(context,0);
    aa_hidecursor(context);
    main_loop();
    aa_close(context);
    return 0;
}
