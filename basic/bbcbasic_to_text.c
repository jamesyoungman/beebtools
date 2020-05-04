#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "tokens.h"
#include "decoder.h"


static void usage(FILE *f, const char *progname)
{
  fprintf(f, "usage: %s [--listo=N] [--dialect=NAME] [input-file]...\n"
	  "Use the option --help to see the program's usage in more detail.\n",
	  progname);
}

static void help(FILE *f, const char *progname)
{
  fprintf(f, "usage: %s [--listo=N] [--dialect=NAME] [input-file]...\n"
	  "If no input-file is listed, issue a usage message and exit.\n"
	  "If input-file is \"-\", read standard input.\n"
	  "Valid values for --listo are 0..7 inclusive.\n"
	  "You can list valid dialect names by specifying --dialect=help.\n"
	  "If the option --help is given, this usage message is printed and "
	  "nothing else is done.\n",
	  progname);
}

static bool set_listo(const char *s, int *listo)
{
  long converted;
  char *end;

  /* There is no need to clear errno to distinguish LONG_MAX from
     overflow, since LONG_MAX is already greater than 7 on all ISO C
     compiant platforms. */
  converted = strtol(s, &end, 10);
  if (converted >= 0 && converted <= 7)
    {
      *listo = (int)converted;
      return true;
    }
  fprintf(stderr, "Value %s is out of range; the valid range is 0 to 7.\n", s);
  return false;
}


int main(int argc, char *argv[])
{
  int exitval = 0;
  unsigned dialect;
  const char* default_dialect_name = "6502";
  const char *progname = argv[0];
  int listo = 7;
  int longindex;
  const struct option opts[] =
    {
     { "dialect", 1, NULL, 'd' },
     { "help", 0, NULL, 'h'},
     { "listo", 1, NULL, 'l' },
     { NULL, 0, NULL, 0 },
    };
  if (progname == NULL)
    {
      /* argv[0] can be NULL, you can achieve this with exec(). */
      progname = "bbcbasic_to_text";
    }
  assert(set_dialect(default_dialect_name, &dialect)); /* set the default */
  int opt;
  while ((opt=getopt_long(argc, argv, "+d:l:", opts, &longindex)) != -1)
    {
      switch (opt)
	{
	case '?':  // Unknown option or missing option argument.
	  // An error message was already issued.
	  usage(stderr, progname);
	  return 1;

	case 'h':
	  help(stdout, progname);
	  return 0;

	case 'l':
	  if (!set_listo(optarg, &listo))
	    return 1;		/* error message alreay issued. */
	  break;

	case 'd':
	  if (0 == strcmp(optarg, "help"))
	    {
	      print_dialects(stdout, default_dialect_name);
	    }
	  else if (!set_dialect(optarg, &dialect))
	    {
	      fprintf(stderr, "Unknown BASIC dialect '%s'\n", optarg);
	      print_dialects(stderr, default_dialect_name);
	      return 1;
	    }
	  break;
	}
    }

  if (optind == argc)
    {
      /* no input files listed. */
      fprintf(stderr, "You didn't specify any input files.\n");
      usage(stderr, progname);
      return 1;
    }
  for (; optind < argc; ++optind)
    {
      const char *name = argv[optind];
      FILE *f;
      if (0 == strcmp("-", argv[optind]))
	{
	  name = "standard input";
	  f = stdin;
	}
      else
	{
	  f = fopen(name, "rb");
	  if (NULL == f)
	    {
	      perror(name);
	      if (exitval < 1)
		exitval = 1;
	      continue;
	    }
	}
      struct decoder *dec = new_decoder(dialect, listo);
      if (0 == dec)
	{
	  fprintf(stderr, "failed to initialise decoder\n");
	  return 1;
	}

      if (!decode_file(dec, name, f))
	{
	  if (exitval < 1)
	    exitval = 1;
	}

      destroy_decoder(dec);
      dec = NULL;

      if (EOF == fclose(f))
	{
	  perror(name);
	  if (exitval < 1)
	    exitval = 1;
	}
    }
  return exitval;
}
