//
//   Copyright 2020 James Youngman
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.
//
#include <assert.h>            // for assert
#include <unistd.h>	       // for optarg, optind
#include <getopt.h>            // for getopt_long, option
#include <stdbool.h>           // for bool, false, true
#include <stdio.h>             // for fprintf, perror, stderr, NULL, fflush
#include <stdlib.h>            // for strtol
#include <string.h>            // for strcmp

#include "decoder.h"           // for decode_file, destroy_decoder, new_decoder
#include "tokens.h"            // for set_dialect, print_dialects, etc...


static bool usage(FILE *f, const char *progname)
{
  return fprintf(f, "usage: %s [--listo=N] [--dialect=NAME] [input-file]...\n"
		 "Use the option --help to see the program's usage in more detail.\n",
		 progname) >= 0;
}

static bool help(FILE *f, const char *progname)
{
  return fprintf(f, "usage: %s [--listo=N] [--dialect=NAME] [input-file]...\n"
		 "If no input-file is listed, issue a usage message and exit.\n"
		 "If input-file is \"-\", read standard input.\n"
		 "Valid values for --listo are 0..7 inclusive.\n"
		 "You can list valid dialect names by specifying --dialect=help.\n"
		 "If the option --help is given, this usage message is printed and "
		 "nothing else is done.\n",
		 progname) >= 0;
}

static bool set_listo(const char *s, int *listo)
{
  long converted;
  char *end;

  /* There is no need to clear errno to distinguish LONG_MAX from
     overflow, since LONG_MAX is already greater than 7 on all ISO C
     compiant platforms.   Likewise 0 and LONG_MIN. */
  converted = strtol(s, &end, 10);
  if (s == end)
    {
      fprintf(stderr, "Value %s should be an integer.\n", s);
      return false;
    }
  if (*end)
    {
      fprintf(stderr, "Value %s should be an integer, but it was followed by trailing junk %s.\n", s, end);
      return false;
    }
  if (converted >= 0 && converted <= 7)
    {
      *listo = (int)converted;
      return true;
    }
  fprintf(stderr, "Value %s is out of range; the valid range is 0 to 7.\n", s);
  return false;
}


int wrapped_main(int argc, char *argv[])
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
     { "dump-token-maps", 0, NULL, 'D' },
     { NULL, 0, NULL, 0 },
    };
  if (progname == NULL)
    {
      /* argv[0] can be NULL, you can achieve this with exec(). */
      progname = "bbcbasic_to_text";
    }
  assert(set_dialect(default_dialect_name, &dialect)); /* set the default */
  int opt;
  while ((opt=getopt_long(argc, argv, "+d:D:l:", opts, &longindex)) != -1)
    {
      switch (opt)
	{
	case '?':  // Unknown option or missing option argument.
	  // An error message was already issued.
	  usage(stderr, progname);
	  return 1;

	case 'D':
	  /* We don't document this option because we don't want to
	   * have to maintain backward compatibility in the output
	   * format.  This option is mainly for regression testing.
	   */
	  if (!internal_dump_all_dialects(optarg))
	    return 1;
	  return 0;

	case 'h':
	  if (!help(stdout, progname))
	    return 1;
	  return 0;

	case 'l':
	  if (!set_listo(optarg, &listo))
	    return 1;		/* error message already issued. */
	  break;

	case 'd':
	  if (0 == strcmp(optarg, "help"))
	    {
	      if (!print_dialects(stdout, default_dialect_name))
		return 1;
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

int main(int argc, char *argv[])
{
  /* We use a wrapper like this to that various parts of the main
   * function can just "return 0" and still take advantage of the I/O
   * failure detection below.
   */
  int exitval = wrapped_main(argc, argv);

  /* Our formatted program went to stdout, so there is likely some
   * buffered data to be flushed.  The C library's exit handling would
   * flush it, but if those operations fail they won't change the exit
   * status of the program.  We, on the other hand, would want to let
   * the user know there was a problem, so we flush the data
   * ourselves.
   */
  if (0 != fflush(stdout))
    {
      perror("stdout");
      exitval = 1;
    }
  if (0 != fflush(stderr))
    {
      perror("stderr");  /* likely useless, but worthwhile attempt */
      exitval = 1;       /* but this is useful */
    }
  return exitval;
}
