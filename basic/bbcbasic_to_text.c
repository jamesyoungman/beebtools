#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "tokens.h"

bool premature_eol(unsigned int tok)
{
  fprintf(stderr, "Uexpected end-of-line immediately after token 0x%02X\n", tok);
  return false;
}

int premature_eof(FILE *f)
{
  fprintf(stderr, "premature end-of-file at position %ld, "
	  "are you sure you specified the right format?\n", ftell(f));
  return 1;
}

bool handle_special_token(enum Dialect dialect, unsigned char tok,
			  const char **output, const char **input, unsigned char *len)
{
  if (dialect == mos6502_32000 || dialect == Z80_80x86)
    {
      switch (tok)
	{
	case 0xC6: *output = "AUTO";  return true;
	case 0xC7: *output = "DELETE"; return true;
	case 0xC8: *output = "LOAD"; return true;
	default:
	  abort();
	}
      return true;
    }
  else if (dialect == Mac)
    {
      switch (tok)
	{
	case 0xC6:
	  {
	    if (!*len)
	      {
		return premature_eol(tok);
	      }
	    else
	      {
		unsigned char uch = **input;
		++*input;
		*output = map_c6(dialect, uch);
	      }
	    break;
	  }

	case 0xC7: *output = "DELETE"; break;
	case 0xC8: *output = "LOAD"; break;
	}
      return true;
    }
  else
    {
      if (!*len)
	{
	  return premature_eol(tok);
	}
      else
	{
	  unsigned char uch = **input;
	  ++*input;
	  switch (tok)
	    {
	    case 0xC6: *output = map_c6(dialect, uch); break;
	    case 0xC7: *output = map_c7(dialect, uch); break;
	    case 0xC8: *output = map_c8(dialect, uch); break;
	    default:
	      fprintf(stderr, "Token 0x%02X is marked for special handling, "
		      "but there is no defined handler.  This is a bug.\n", tok);
	      return false;
	    }
	  return true;
	}
    }
}

bool print_target_line_number(unsigned char b1, unsigned char b2, unsigned char b3)
{
  unsigned char lo = b2 ^ ((b1 * 4) & 0xC0);
  unsigned char hi = b3 ^ (b1 * 16);
  unsigned int n = (hi*256) + lo;
  return fprintf(stdout, "%u", n) >= 0;
}

bool stdout_write_error()
{
  perror("stdout");
  return false;
}

int count(unsigned char needle, const char* haystack, size_t len)
{
  int n = 0;
  const unsigned char *p = (const unsigned char*)haystack;
  while (len--)
    {
      if (*p++ == needle)
	++n;
    }
  return n;
}

bool decode_line(enum Dialect dialect,
		 unsigned char line_hi, unsigned char line_lo,
		 unsigned char orig_len, const char *data,
		 long file_pos,
		 const char **token_map, int *indent, int listo)
{
  unsigned const char *p = (unsigned const char*)data;
  int outdent = 0;
  const unsigned int line_number = (256u * line_hi) + line_lo;
  unsigned char len = orig_len;

  if (fprintf(stdout, "%5u", line_number) < 0)
    return false;		/* I/O error */
  if (listo & 1)
    putchar(' ');
  if (listo & 2)		/* for...next loops */
    {
      const int next_count = count(0xED, data, orig_len);
      outdent += 2 * next_count;
    }
  if (listo & 4)		/* repeat...until loops */
    {
      const int until_count = count(0xFD, data, orig_len);
      outdent += 2 * until_count;
    }
  *indent -= outdent;
  if (*indent > 0)
    {
      if (fprintf(stdout, "%*s", (*indent), "") < 0)
	return false;		/* I/O error */
    }

  while (len--)
    {
      unsigned char uch = *p++;
      const char *t = token_map[uch];

      /* We have "special" tokens which expand to something starting
	 with an underscore, and we handle those in handle_line_num()
	 (for 0x8D) or handle_special_token() (for 0xC6, 0xC7, 0xC8).

	 But, the token for 0x5F also begins with _, because it is
	 actually _ itself.  So, don't trigger special token handling
	 for that.
      */
      if (t[0] == '_' && uch != 0x5F)
	{
	  if (0 == strcmp(t, invalid))
	    {
	      fprintf(stderr, "saw unexpected token 0x%02X at file position %ld (0x%02lX), "
		      "are you sure you specified the right dialect?\n",
		      (unsigned)uch, file_pos, (unsigned long)file_pos);
	      return false;
	    }
	  if (0 == strcmp(t, line_num))
	    {
	      /* This flags an upcoming line number (e.g. in a GOTO
	       * statement).  There are three following bytes encoding
	       * the line number value.
	       */
	      if (len < 3)
		{
		  fprintf(stderr, "end-of-line in the middle of a line number\n");
		  return false;
		}
	      else
		{
		  if (!print_target_line_number(p[0], p[1], p[2]))
		    return false;
		  p += 3;
		  len -= 3;
		  file_pos += 3;
		  continue;
		}
	    }
	  else if (!handle_special_token(dialect, uch, &t, (const char**)&p, &len))
	    return false;
	}
      if (fputs(t, stdout) == EOF)
	return stdout_write_error();
      ++file_pos;
    }
  putchar('\n');
  if (listo & 2)		/* for loops */
    {
      const int for_count = count(0xE3, data, orig_len);
      (*indent) += 2 * for_count;
    }
  if (listo & 4)
    {
      const int repeat_count = count(0xF5, data, orig_len);
      (*indent) += 2 * repeat_count;
    }
  return true;
}

bool expect_char(FILE *f, unsigned char val_expected)
{
  int ch;
  if ((ch = getc(f)) == EOF)
    return premature_eof(f);
  if ((unsigned char)ch != val_expected)
    {
      const long int pos = ftell(f);
      fprintf(stderr, "expected to see a byte with value 0x%02X "
	      "(instead of 0x%02X) at position %ld, "
	      "are you sure you specified the right format?\n",
	      (unsigned)val_expected, (unsigned)ch, pos);
      return false;
    }
  return true;
}

bool decode_len_leading_program(FILE *f, const char *filename,
				enum Dialect dialect, const char **token_map, int listo)
{
  // In this file format lines look like this:
  // <len> <lo> <hi> tokens... 0x0D
  //
  // End of file looks like this:
  // 0x00 0xFF 0xFF
  //
  // Note that the byte ordering here is different to the 6502 dialect
  // and different to some descriptions you might find on the web.
  // However, R.T. Russell's program 6502-Z80.BBC emits the low byte
  // followed by the high byte, which is what this program expects.
  int indent = 0;
  bool empty = true;		/* file is entirely empty */
  static char buf[1024];
  long int file_pos;
  for (;;)
    {
      int ch;
      int hi, lo;
      size_t nread;
      unsigned long len;
      if ((ch = getc(f)) == EOF)
	{
	  if (empty)
	    return true;
	  else
	    return premature_eof(f);
	}
      empty = false;
      len = ch;
      if (0 == len)
	{
	  // This is logical EOF.  We still expect to see 0xFF 0xFF though.
	  expect_char(f, 0xFF);
	  expect_char(f, 0xFF);
	  // This should be followed by the physical EOF.
	  if ((ch = fgetc(f)) != EOF)
	    {
	      /* This seems to happen with at least some of the Torch
		 Z80 example BASIC programs, but I don't know if this
		 is supposed to happen or not. */
	      fprintf(stderr, "warning: expected end-of-file at position %ld but "
		      "instead we reach a byte with value 0x%02X, are you "
		      "sure you specified the right dialect?\n",
		      ftell(f), (unsigned)ch);
	      return true;	/* assume this is (perhaps unusual but) OK. */
	    }
	  return true;
	}
      if (len < 3)
	{
	  fprintf(stderr, "line at position %ld has length %lu "
		  "which is impossibly short, are you sure you specified the right "
		  "dialect?\n", ftell(f), len);
	  return false;
	}
      if ((ch = fgetc(f)) == EOF)
	return premature_eof(f);
      lo = (unsigned char)ch;
      if ((ch = fgetc(f)) == EOF)
	return premature_eof(f);
      hi = (unsigned char)ch;

      clearerr(f);
      assert(len < sizeof(buf));
      file_pos = ftell(f);
      len -= 3;
      nread = fread(buf, 1, len, f);
      if (nread < len)
	{
	  if (ferror(f))
	    {
	      perror(filename);
	      return false;
	    }
	}
      if ((len > 0) && buf[len-1] != 0x0D)
	{
	  fprintf(stderr, "expected to see character 0x0D at the end of the "
		  "line at file offset %ld, but saw 0x%02X, are you sure "
		  "you specified the correct dialect?\n",
		  file_pos, (unsigned)buf[len-1]);
	  return false;
	}
      /* decode_line already prints a newline at the end of the line, so we don't
	 want to pass it the final 0x0D, as then the newline would be doubled.
	 Hence we pass len-1 as the length.
      */
      if (!decode_line(dialect, hi, lo, len-1, buf, file_pos, token_map, &indent, listo))
	return false;
    }
}

bool decode_cr_leading_program(FILE *f, const char *filename,
			       enum Dialect dialect, const char **token_map, int listo)
{
  // In this file format lines look like this:
  // 0x0D <hi> <lo> <len> tokens...
  // Here <hi> and <lo> are the high and low bytes of the line number
  // and <len> is the total length of the line (starting from the intial
  // 0x0D.  The number of bytes of tokens is therefore <len>-4.
  //
  // End of file looks like this:
  // 0x0D 0xFF
  bool warned = false;
  bool empty = true;
  int indent = 0;
  long int file_pos;
  static char buf[1024];
  for (;;)
    {
      int ch;
      int hi, lo;
      size_t nread;
      unsigned long len;
      if ((ch = getc(f)) == EOF)
	{
	  if (empty)
	    return true;
	  else
	    return premature_eof(f);
	}
      empty = false;
      if (ch != 0x0D)
	{
	  long pos = ftell(f);
	  fprintf(stderr, "line at position %ld did not start with 0x0D "
		  "(instead 0x%02X) are you sure you specified the right "
		  "format?\n", pos, (unsigned)ch);
	  return false;
	}
      if ((ch = fgetc(f)) == EOF)
	return premature_eof(f);
      hi = (unsigned char)ch;
      if (hi == 0xFF)
	{
	  /* 0x0D 0xFF signals EOF. */
	  if ((ch = getc(f)) == EOF)
	    {
	      return true;
	    }
	  else
	    {
	      /* Slightly unexpected, perhaps a very large line number. */
	      if (!warned)
		{
		  fprintf(stderr, "Saw 0xFF at position %ld as the high byte "
			  "of a line number; this is unusual, are you sure "
			  "you specified the right format?\n", ftell(f));
		  warned = true;
		}
	    }
	}
      else
	{
	  if ((ch = fgetc(f)) == EOF)
	    return premature_eof(f);
	}
      lo = (unsigned char)ch;
      if ((ch = fgetc(f)) == EOF)
	return premature_eof(f);
      len = (unsigned char)ch;
      /* len counts from the initial 0x0D, and we already read 4 characters. */
      if (len < 4u)
	{
	  fprintf(stderr, "line at position %ld has length %lu "
		  "which is impossibly short, are you sure you specified the right "
		  "format?\n", ftell(f), len);
	  return false;
	}
      len -= 4;
      clearerr(f);
      assert(len < sizeof(buf));
      file_pos = ftell(f);
      nread = fread(buf, 1, len, f);
      if (nread < len)
	{
	  if (ferror(f))
	    {
	      perror(filename);
	      return false;
	    }
	}
      if (!decode_line(dialect, hi, lo, len, buf, file_pos, token_map, &indent, listo))
	return false;
    }
}

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
  const char **token_map;
  int listo = 7;
  bool cr_leading;
  bool (*decoder)(FILE *f, const char *filename,
		  enum Dialect dialect, const char **token_map, int listo);
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
  cr_leading = (dialect == mos6502_32000 || dialect == Windows || dialect == ARM);
  decoder = cr_leading ? decode_cr_leading_program : decode_len_leading_program;
  token_map = build_mapping(dialect);
  if (NULL == token_map)
    {
      perror("building token mapping");
      return 1;
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
      if (!decoder(f, name, dialect, token_map, listo))
	{
	  if (exitval < 1)
	    exitval = 1;
	}
      if (EOF == fclose(f))
	{
	  perror(name);
	  if (exitval < 1)
	    exitval = 1;
	}
    }
  return exitval;
}
