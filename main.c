#include <aalib.h>
#include "image.h"
#include "ui.h"
#include "config.h"

#define NAMELEN 256

char filename[NAMELEN];

static void showhelp(char *prgname)
{
  printf("Usage: %s [options] filename.p[ngbp]m\n", prgname);
  printf("Options:\n%s\n", aa_help);

}

static void showversion(char *prgname)
{
   printf("This is %s version %s\n", prgname, VERSION);
}

static void parse_cmdline( int argc, char *argv[])
{
  int i;

  filename[0] = '\0';
  for (i = 1; i < argc; i++) {
    if (!strncmp(argv[i], "--", 2)) {
      if (!strcmp(argv[i], "--help")) { showhelp(argv[0]); exit(0); }
      if (!strcmp(argv[i], "--version")) { showversion(argv[0]); exit(0); }
      printf("Unknown option. Use %s --help for help\n", argv[0]);
      exit(-1);
    }
    else {
      if (argv[i]!=NULL) {
        /* argv[i] is the filename of the image file */
        strncpy(filename, argv[i], NAMELEN);
      }
    }
  }
}

int main(int argc, char **argv)
{
    aa_parseoptions(NULL,NULL,&argc,argv);
    parse_cmdline(argc, argv);
    if (!load_image(filename))
	exit(-1);
    context = aa_autoinit(&aa_defparams);
    if (context == NULL)
	exit(-1);
    aa_autoinitkbd(context,0);
    aa_hidecursor(context);
    main_loop();
    aa_close(context);
    return(0);
}
