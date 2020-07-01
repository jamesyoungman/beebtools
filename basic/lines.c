#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tokens.h"

#define STATIC_ASSERT(condition) \
do { static char assertion_arr[(condition)?1:-1]; (void)assertion_arr; } while(0)

static bool stdout_write_error()
{
  perror("stdout");
  return false;
}

static bool print_target_line_number(unsigned char b1, unsigned char b2, unsigned char b3)
{
  const unsigned char mask = 0xC0;
  unsigned char lo = b2 ^ ((unsigned char)(b1 << 2) & mask);
  unsigned char hi = b3 ^ (unsigned char)(b1 << 4);
  unsigned int n = (hi*256u) + lo;
  if (fprintf(stdout, "%u", n) < 0)
    return stdout_write_error();
  return true;
}

static bool premature_eol(unsigned int tok)
{
  fprintf(stderr, "Unexpected end-of-line immediately after token 0x%02X\n", tok);
  return false;
}


static bool is_invalid(const char *s)
{
  return s[0] == '_' && s[1] != 0;
}

static bool handle_pdp_quit(unsigned char intro,
			    const char **output,
			    const unsigned char **input,
			    unsigned char *len)
{
  /* In BBC BASIC for PDP11, 0xC9 0x98 encodes QUIT.
     0xC9 followed by any other byte encodes whatever
     it encodes in 6502 BASIC. */
  unsigned char uch;
  if (!*len)
    {
      return premature_eol(intro);
    }
  /* peek ahead one byte. */
  uch = **input;
  ++*input;
  --*len;
  switch (uch)
    {
    case 0x98:
      /* consume the peeked byte */
      *output = "QUIT";
      break;
    default:
      *output = "LOAD";
      /* back up so that the peeked byte stays in the
	 unread input. */
      --*input;
      ++*len;
      break;
    }
  return true;
}

static bool handle_special_token(unsigned char intro,
				 const char **output,
				 const unsigned char **input,
				 unsigned char *len,
				 const struct expansion_map *m)
{
  const char * const *extension_map = NULL;
  unsigned char uch;

  switch (intro)
    {
    case 0xC6: extension_map = m->c6; break;
    case 0xC7: extension_map = m->c7; break;
    case 0XC8:
      if (*output == pdp_c8)
	{
	  return handle_pdp_quit(intro, output, input, len);
	}
      else
	{
	  extension_map = m->c8;
	  break;
	}
    default:
      fprintf(stderr, "Token 0x%02X is marked for special handling, "
	      "but there is no defined handler.  This is a bug.\n",
	      intro);
      please_submit_bug_report();
      return false;
    }
  if (!*len)
    {
      return premature_eol(intro);
    }
  uch = **input;
  ++*input;
  --*len;

  assert(extension_map[uch] != NULL);
  if (is_invalid(extension_map[uch]))
    {
      fprintf(stderr, "Saw sequence 0x%02X 0x%02X, "
	      "are you sure you specified the right dialect?\n",
	      (unsigned)intro, (unsigned)uch);
      return false;
    }
  *output = extension_map[uch];
  return true;
}

static bool handle_token(unsigned char uch, long file_pos,
			 const unsigned char **input, unsigned char *len,
			 const struct expansion_map *m)

{
  const char *t = m->base[uch];
  if (NULL == t)
    {
      fprintf(stderr, "The entry in the token base map for byte 0x%02X is NULL.\n",
	      (unsigned)uch);
      please_submit_bug_report();
      return false;
    }

  /* We have "special" tokens which expand to something starting
     with an underscore, and we handle those in handle_line_num()
     (for 0x8D) or handle_special_token() (for 0xC6, 0xC7, 0xC8).

     But, the token for 0x5F also begins with _, because it is
     actually _ itself.  So, don't trigger special token handling
     for that.
  */
  if (t[0] == '_' && uch != 0x5F)
    {
      if (t == invalid)
	{
	  fprintf(stderr, "saw unexpected token 0x%02X at file position %ld (0x%02lX), "
		  "are you sure you specified the right dialect?\n",
		  (unsigned)uch, file_pos, (unsigned long)file_pos);
	  return false;
	}
      if (t == line_num)
	{
	  /* This flags an upcoming line number (e.g. in a GOTO
	   * statement).  There are three following bytes encoding
	   * the line number value.
	   */
	  if (*len < 3)
	    {
	      fprintf(stderr, "end-of-line in the middle of a line number\n");
	      return false;
	    }
	  else
	    {
	      const unsigned char* p = (unsigned char*)*input;
	      if (!print_target_line_number(p[0], p[1], p[2]))
		return false;
	      (*input) += 3;
	      assert((*len) >= 3);
	      *len = (unsigned char)(*len - 3u);
	      file_pos += 3;
	      return true;
	    }
	}
      else if (t == fastvar)
	{
	  fprintf(stderr, "This program has been 'crunched', "
		  "and its original identifiers have been mapped to "
		  "meaningless numbers.  Please run this tool on the original "
		  "source code instead.\n");
	  return false;
	}
      else if (!handle_special_token(uch, &t, input, len, m))
	{
	  fprintf(stderr, "Failed to handle token sequence beginning with 0x%02X, "
		  "are you sure you specified the right dialect?\n",
		  (unsigned)uch);
	  return false;
	}
    }
  if (fputs(t, stdout) == EOF)
    return stdout_write_error();
  return true;
}

static int count(unsigned char needle, const char* haystack, size_t len)
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

static bool decode_line(unsigned char line_hi, unsigned char line_lo,
			unsigned char orig_len, const char *data,
			long orig_file_pos,
			const struct expansion_map *m, int *indent, int listo)
{
  long file_pos = orig_file_pos;
  unsigned const char *p = (unsigned const char*)data;
  int outdent = 0;
  const unsigned int line_number = (256u * line_hi) + line_lo;
  unsigned char len = orig_len;
  bool in_string = false;

  // Print the line number as a space-padded right-aligned
  // decimal number.  If there is no line number, just print
  // five spaces.
  if (line_number != 0)
    {
      if (printf("%5u", line_number) < 0)
	return stdout_write_error();
    }
  else
    {
      if (printf("%5s", "") < 0)
	return stdout_write_error();
    }
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
      if (printf("%*s", (*indent), "") < 0)
	return stdout_write_error();
    }

  assert(((unsigned const char*)data + orig_len) == (p + len));
  while (len)
    {
      unsigned char uch = *p++;
      --len;
      assert(((unsigned const char*)data + orig_len) == (p + len));
      if (uch == 0)
	{
	  /* Probably we had incremented p without decrementing len. */
	  fprintf(stderr, "It looks like decode_line ran over the end of the input.\n");
	  please_submit_bug_report();
	  return false;
	}
      if (in_string)
	{
	  /* We don't expand tokens inside strings.  Some programs, for
	     example, include Mode 7 control characters inside strings.
	     So 0x86 (decimal 134) inside a string is passed through literally
	     (where in mode 7 it would turn text cyan) while outside
	     a string it would expand to the keyword LINE.
	  */
	  if (EOF == putchar(uch))
	    return stdout_write_error();
	}
      else
	{
	  if (!handle_token(uch, file_pos, &p, &len, m))
	    return false;
	}
      if (uch == '"')
	in_string = !in_string;
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

static bool premature_eof(FILE *f)
{
  fprintf(stderr, "premature end-of-file at position %ld, "
	  "are you sure you specified the right format?\n", ftell(f));
  return false;
}

static bool expect_char(FILE *f, unsigned char val_expected)
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

bool decode_little_endian_program(FILE *f, const char *filename,
				  const struct expansion_map *m, int listo)
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
  enum { BufSize = 1024 };
  static char buf[BufSize];
  long int file_pos;
  for (;;)
    {
      int ch;
      unsigned char hi, lo;
      size_t nread;
      unsigned char len;
      // BufSize must be at least UCHAR_MAX, so len < BufSize always.
      STATIC_ASSERT(UCHAR_MAX < BufSize);
      if ((ch = getc(f)) == EOF)
	{
	  if (empty)
	    return true;
	  else
	    return premature_eof(f);
	}
      empty = false;
      len = (unsigned char)ch;
      if (0 == len)
	{
	  // This is logical EOF.  We still expect to see 0xFF 0xFF though.
	  if (!expect_char(f, 0xFF) || !expect_char(f, 0xFF))
	    return false;
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
	  fprintf(stderr, "line at position %ld has length %u "
		  "which is impossibly short, are you sure you specified the right "
		  "dialect?\n", ftell(f), (unsigned int)len);
	  return false;
	}
      if ((ch = fgetc(f)) == EOF)
	return premature_eof(f);
      lo = (unsigned char)ch;
      if ((ch = fgetc(f)) == EOF)
	return premature_eof(f);
      hi = (unsigned char)ch;

      clearerr(f);
      file_pos = ftell(f);
      len = (unsigned char)(len - 3u);  /* len is unsigned so a wrap here generates a large value. */
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
	 Hence we pass len-1 as the length.   But if len is already zero, subtracting
	 1 would cause an unsigned arithmetic wrap, and besides it seems pointless
	 to call decode_line when the line is empty.

	 We see examples where len==0 in the .bbc file in the (zipfile) compiler output
	 of R. T. Russell's BBC BASIC for SDL.
      */
      if (len)
	{
	  --len;
	  if (!decode_line(hi, lo, len, buf, file_pos, m, &indent, listo))
	    return false;
	}
    }
}

bool decode_big_endian_program(FILE *f, const char *filename,
			       const struct expansion_map *m, int listo)
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
  unsigned char len;
  static char buf[1024];
  // Ensure that len can never overflow buf. */
  STATIC_ASSERT(UCHAR_MAX < sizeof(buf));
  for (;;)
    {
      int ch;
      unsigned char hi, lo;
      size_t nread;
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
		  "format?\n", pos-1, (unsigned)ch);
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
			  "you specified the right format?\n", ftell(f)-1);
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
	  fprintf(stderr, "line at position %ld has length %u "
		  "which is impossibly short, are you sure you specified the right "
		  "format?\n", ftell(f), (unsigned int)len);
	  return false;
	}
      len = (unsigned char)(len - 4u);
      clearerr(f);
      file_pos = ftell(f);
      nread = fread(buf, 1, len, f);
      if (nread < len)
	{
	  if (ferror(f))
	    {
	      perror(filename);
	      return false;
	    }
	  else
	    {
	      return premature_eof(f);
	    }
	}
      if (!decode_line(hi, lo, len, buf, file_pos, m, &indent, listo))
	return false;
    }
}
