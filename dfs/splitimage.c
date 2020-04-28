#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

enum
  {
   SECTOR_SIZE = 256,
   SECTORS_PER_TRACK = 10,
   INTERLEAVE_SIZE = SECTOR_SIZE * SECTORS_PER_TRACK,
  };

void usage(const char *program)
{
  fprintf(stderr, "usage: %s input-image.raw "
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
  size_t bytes_read = fread(buf, 1, INTERLEAVE_SIZE, f);
  if (bytes_read == 0)
    return 0;
  if (bytes_read != INTERLEAVE_SIZE)
    {
      perror(name);
      exit(1);
    }
  return 1;
}

void must_write(const char *name, FILE *f, char buf[])
{
  check_io(name, fwrite(buf, 1, INTERLEAVE_SIZE, f), INTERLEAVE_SIZE);
}

int main(int argc, char *argv[])
{
  static char buf[SECTOR_SIZE * SECTORS_PER_TRACK];
  FILE *in, *side0, *side2;
  if (argc != 4)
    {
      usage(argv[0]);
      return 1;
    }
  in = must_open(argv[1], "rb");
  side0 = must_open(argv[2], "wb");
  side2 = must_open(argv[3], "wb");
  for (;;)
    {
      errno = 0;
      if (!try_read(argv[1], in, buf))
	break;
      must_write(argv[2], side0, buf);
      if (!try_read(argv[1], in, buf))
	{
	  fprintf(stderr, "size of %s should be a multiple of %d bytes\n",
		  argv[1], INTERLEAVE_SIZE);
	  return 1;
	}
      must_write(argv[2], side2, buf);
    }
  fclose(in);
  if (0 != fclose(side0))
    {
      perror(argv[2]);
      return 1;
    }
  if (0 != fclose(side2))
    {
      perror(argv[3]);
      return 1;
    }
  return 0;
}
