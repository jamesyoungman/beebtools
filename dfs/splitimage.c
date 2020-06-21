#include <errno.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
  {
   SECTOR_SIZE = 256,
  };

unsigned long int spt = 0;
unsigned long int interleave_size = 0;
char * buf = NULL;


bool set_spt(const char *arg)
{
  errno = 0;
  char *end = (char*)arg;
  unsigned long val = strtoul(arg, &end, 10);
  if (val == 0 && end == arg)
    {
      fprintf(stderr, "Argument to option -s was empty but should should have been a positive decimal integer\n");
      return false;
    }
  else if ((val == ULONG_MAX || val == 0uL) && errno)
    {
      perror(arg);
      return false;
    }
  else if (*end)
    {
      fprintf(stderr, "Argument of option -s, '%s', is non-decimal or has unexpected suffix '%s'\n",
	      arg, end);
      return false;
    }
  else if (val > (unsigned long)LONG_MAX && strchr(arg, '-'))
    {
      fprintf(stderr, "Argument to option -s was '%s' but should should have been a positive decimal integer\n", arg);
      return false;
    }
  else
    {
      spt = val;
      interleave_size = spt * SECTOR_SIZE;
      return true;
    }
}


void usage(const char *program)
{
  fprintf(stderr, "usage: %s -s NN input-image.raw "
	  "output-side0.sdd output-side2.sdd\n",
	  program);
}

FILE *must_open(const char *name, const char *mode)
{
  FILE *f = fopen(name, mode);
  if (f == NULL)
    {
      perror(name);
      exit(1);
    }
  return f;
}

void check_io(const char *name, size_t result, size_t expected)
{
  if (result != expected)
    {
      perror(name);
      exit(1);
    }
}

int try_read(const char *name, FILE* f, char buf[])
{
  size_t bytes_read = fread(buf, 1, interleave_size, f);
  if (bytes_read == 0)
    return 0;
  if (bytes_read != interleave_size)
    {
      perror(name);
      exit(1);
    }
  return 1;
}

void must_write(const char *name, FILE *f, char buf[])
{
  check_io(name, fwrite(buf, 1, interleave_size, f), interleave_size);
}

int do_everything(int argc, char *argv[])
{
  FILE *in, *side0, *side2;
  const char *input_name, *output0, *output2;
  int opt;

  while ((opt=getopt(argc, argv, "s:")) != -1)
    {
      switch (opt)
	{
	case 's':
	  if (!set_spt(optarg))
	    return 1;
	  break;
	default:
	  usage(argv[0]);
	  return 1;
	}
    }
  if (0uL == spt)
    {
      fprintf(stderr, "Please use the -s option to specify the number of sectors per track in the input file\n");
      return 1;
    }

  buf = calloc(spt, SECTOR_SIZE);
  if (buf == NULL)
    {
      perror("malloc");
      return 1;
    }

  if ((argc - optind) != 3)
    {
      fprintf(stderr, "expected 3 non-option arguments, got %d\n", (argc-optind));
      usage(argv[0]);
      return 1;
    }
  input_name = argv[optind];
  output0 = argv[optind+1];
  output2 = argv[optind+2];
  in = must_open(argv[optind], "rb");
  side0 = must_open(argv[optind+1], "wb");
  side2 = must_open(argv[optind+2], "wb");
  for (;;)
    {
      errno = 0;
      if (!try_read(input_name, in, buf))
	break;
      must_write(output0, side0, buf);
      if (!try_read(input_name, in, buf))
	{
	  fprintf(stderr, "size of %s should be a multiple of %lu bytes\n",
		  argv[1], interleave_size);
	  return 1;
	}
      must_write(output2, side2, buf);
    }
  fclose(in);
  if (0 != fclose(side0))
    {
      perror(output0);
      return 1;
    }
  if (0 != fclose(side2))
    {
      perror(output2);
      return 1;
    }
  return 0;
}

int main(int argc, char *argv[])
{
  int rv = do_everything(argc, argv);
  if (buf)
    free(buf);
  return rv;
}
