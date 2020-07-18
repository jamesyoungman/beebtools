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
#include "tokens.h"

#include <assert.h>   // for assert
#include <stdbool.h>  // for false, bool, true
#include <stdio.h>    // for fprintf, NULL, fputs, perror, FILE, fclose, fopen
#include <string.h>   // for strcmp

#define ARRAYSIZE(arr) (sizeof(arr)/sizeof(arr[0]))

/* We use the initializer BAD to signal that a dialect has no mapping
 * for this token value.  We interpret NULL to mean that the data
 * structure is incomplete, that is, we wrote a bug.
 */
const char invalid[] = "__invalid__";
const char line_num[] = "__line_num__";
const char fastvar[] = "__fastvar__";
const char identity[] = "__identity__";
const char end_marker[] = "__end__";
const char pdp_c8[] = "__pdp__";
#define BAD invalid

/* There are a number of other special extension tokens. */
const char ext_c6[] = "__c6__";
const char ext_c7[] = "__c7__";
const char ext_c8[] = "__c8__";
#define DO_C6 ext_c6
#define DO_C7 ext_c7
#define DO_C8 ext_c8
#define EVERYWHERE(word) word, word, word, word

/* multi_mapping describes the mapping from input byte to
 * expanded token in a form that's convenient to maintain.
 * It is used as the source data to create an instance of
 * expansion_map.
 *
 * Some of the entries in the table below are initialised
 * to |identity|. This tells the intialisation code to use
 * a value from ascii[] instead of the value in base_map.
 */
struct multi_mapping
{
  unsigned int token_value;
  /* -1 since Mac is missing. */
  const char* dialect_mappings[LAST_BASE_MAP_DIALECT+1];
};

static const struct multi_mapping base_map[NUM_TOKENS] = {
/*
        6502                   Z80               ARM                 Windows
*/
{ 0x00, {BAD,                  BAD,              BAD,                BAD              }},
{ 0x01, {BAD,                  BAD,              BAD,                "CIRCLE"         }},
{ 0x02, {BAD,                  BAD,              BAD,                "ELLIPSE"        }},
{ 0x03, {BAD,                  BAD,              BAD,                "FILL"           }},
{ 0x04, {BAD,                  BAD,              BAD,                "MOUSE"          }},
{ 0x05, {BAD,                  BAD,              BAD,                "ORIGIN"         }},
{ 0x06, {BAD,                  BAD,              BAD,                "QUIT"           }},
{ 0x07, {BAD,                  BAD,              BAD,                "RECTANGLE"      }},
{ 0x08, {BAD,                  BAD,              BAD,                "SWAP"           }},
{ 0x09, {BAD,                  BAD,              BAD,                "SYS"            }},
{ 0x0A, {BAD,                  BAD,              BAD,                "TINT"           }},
{ 0x0B, {BAD,                  BAD,              BAD,                "WAIT"           }},
{ 0x0C, {BAD,                  BAD,              BAD,                "INSTALL"        }},
/*  0x0D is end-of line on all platforms, including SDL for Linux */
{ 0x0E, {BAD,                  BAD,              BAD,                "PRIVATE"        }},
{ 0x0F, {BAD,                  BAD,              BAD,                "BY"             }},
{ 0x10, {BAD,                  BAD,              BAD,                "EXIT"           }},
{ 0x18, {identity,             identity,         identity,           fastvar          }},
{ 0x19, {identity,             identity,         identity,           fastvar          }},
{ 0x1A, {identity,             identity,         identity,           fastvar          }},
{ 0x1B, {identity,             identity,         identity,           fastvar          }},
{ 0x1C, {identity,             identity,         identity,           fastvar          }},
{ 0x1D, {identity,             identity,         identity,           fastvar          }},
{ 0x1E, {identity,             identity,         identity,           fastvar          }},
{ 0x1F, {identity,             identity,         identity,           fastvar          }},
{ 0x7F, {BAD,                  BAD,              "OTHERWISE",        BAD              }},
{ 0x80, { EVERYWHERE("AND")   }},
{ 0x81, { EVERYWHERE("DIV")   }},
{ 0x82, { EVERYWHERE("EOR")   }},
{ 0x83, { EVERYWHERE("MOD")   }},
{ 0x84, { EVERYWHERE("OR")    }},
{ 0x85, { EVERYWHERE("ERROR") }},
{ 0x86, { EVERYWHERE("LINE")  }},
{ 0x87, { EVERYWHERE("OFF")   }},
{ 0x88, { EVERYWHERE("STEP")           }},
{ 0x89, { EVERYWHERE("SPC")           }},
{ 0x8A, { EVERYWHERE("TAB(")           }},
{ 0x8B, { EVERYWHERE("ELSE")           }},
{ 0x8C, { EVERYWHERE("THEN")           }},
{ 0X8D, { EVERYWHERE(line_num) }},
{ 0x8E, { EVERYWHERE("OPENIN" )}},
{ 0x8F, { EVERYWHERE("PTR"    )}},
{ 0x90, { EVERYWHERE("PAGE"   )}},
{ 0x91, { EVERYWHERE("TIME"   )}},
{ 0x92, { EVERYWHERE("LOMEM"  )}},
{ 0x93, { EVERYWHERE("HIMEM"  )}},
{ 0x94, { EVERYWHERE("ABS"    )}},
{ 0x95, { EVERYWHERE("ACS"    )}},
{ 0x96, { EVERYWHERE("ADVAL"  )}},
{ 0x97, { EVERYWHERE("ASC"    )}},
{ 0x98, { EVERYWHERE("ASN"    )}},
{ 0x99, { EVERYWHERE("ATN"    )}},
{ 0x9A, { EVERYWHERE("BGET"   )}},
{ 0x9B, { EVERYWHERE("COS"    )}},
{ 0x9C, { EVERYWHERE("COUNT"  )}},
{ 0x9D, { EVERYWHERE("DEG"    )}},
{ 0x9E, { EVERYWHERE("ERL"    )}},
{ 0x9F, { EVERYWHERE("ERR"    )}},
{ 0xA0, { EVERYWHERE("EVAL"   )}},
{ 0xA1, { EVERYWHERE("EXP"    )}},
{ 0xA2, { EVERYWHERE("EXT"    )}},
{ 0xA3, { EVERYWHERE("FALSE"  )}},
{ 0xA4, { EVERYWHERE("FN"     )}},
{ 0xA5, { EVERYWHERE("GET"    )}},
{ 0xA6, { EVERYWHERE("INKEY"  )}},
{ 0xA7, { EVERYWHERE("INSTR(" )}},
{ 0xA8, { EVERYWHERE("INT"    )}},
{ 0xA9, { EVERYWHERE("LEN"    )}},
{ 0xAA, { EVERYWHERE("LN"     )}},
{ 0xAB, { EVERYWHERE("LOG"    )}},
{ 0xAC, { EVERYWHERE("NOT"    )}},
{ 0xAD, { EVERYWHERE("OPENUP" )}},
{ 0xAE, { EVERYWHERE("OPENOUT")}},
{ 0xAF, { EVERYWHERE("PI"     )}},
{ 0xB0, { EVERYWHERE("POINT(" )}},
{ 0xB1, { EVERYWHERE("POS"    )}},
{ 0xB2, { EVERYWHERE("RAD"    )}},
{ 0xB3, { EVERYWHERE("RND"    )}},
{ 0xB4, { EVERYWHERE("SGN"    )}},
{ 0xB5, { EVERYWHERE("SIN"    )}},
{ 0xB6, { EVERYWHERE("SQR"    )}},
{ 0xB7, { EVERYWHERE("TAN"    )}},
{ 0xB8, { EVERYWHERE("TO"     )}},
{ 0xB9, { EVERYWHERE("TRUE"   )}},
{ 0xBA, { EVERYWHERE("USR"    )}},
{ 0xBB, { EVERYWHERE("VAL"    )}},
{ 0xBC, { EVERYWHERE("VPOS"   )}},
{ 0xBD, { EVERYWHERE("CHR$"   )}},
{ 0xBE, { EVERYWHERE("GET$"   )}},
{ 0xBF, { EVERYWHERE("INKEY$" )}},
{ 0xC0, { EVERYWHERE("LEFT$(" )}},
{ 0xC1, { EVERYWHERE("MID$("  )}},
{ 0xC2, { EVERYWHERE("RIGHT$(")}},
{ 0xC3, { EVERYWHERE("STR$"   )}},
{ 0xC4, { EVERYWHERE("STRING$(")}},
{ 0xC5, { EVERYWHERE("EOF")     }},
{ 0xC6, {"AUTO",               "AUTO",           DO_C6,              "SUM"            }},
{ 0xC7, {"DELETE",             "DELETE",         DO_C7,              "WHILE"          }},
{ 0xC8, {"LOAD",               "LOAD",           DO_C8,              "CASE"           }},
{ 0xC9, {"LIST",               "LIST",           "WHEN",             "WHEN"           }},
{ 0xCA, {"NEW",                "NEW",            "OF",               "OF"             }},
{ 0xCB, {"OLD",                "OLD",            "ENDCASE",          "ENDCASE"        }},
{ 0xCC, {"RENUMBER",           "RENUMBER",       "ELSE",             "OTHERWISE"      }},
{ 0xCD, {"SAVE",               "SAVE",           "ENDIF",            "ENDIF"          }},
{ 0xCE, {"EDIT",               "PUT",            "ENDWHILE",         "ENDWHILE"       }},
{ 0xCF, { EVERYWHERE("PTR") }},
{ 0xD0, { EVERYWHERE("PAGE") }},
{ 0xD1, { EVERYWHERE("TIME") }},
{ 0xD2, { EVERYWHERE("LOMEM") }},
{ 0xD3, { EVERYWHERE("HIMEM") }},
{ 0xD4, { EVERYWHERE("SOUND") }},
{ 0xD5, { EVERYWHERE("BPUT") }},
{ 0xD6, { EVERYWHERE("CALL") }},
{ 0xD7, { EVERYWHERE("CHAIN") }},
{ 0xD8, { EVERYWHERE("CLEAR") }},
{ 0xD9, { EVERYWHERE("CLOSE") }},
{ 0xDA, { EVERYWHERE("CLG") }},
{ 0xDB, { EVERYWHERE("CLS") }},
{ 0xDC, { EVERYWHERE("DATA") }},
{ 0xDD, { EVERYWHERE("DEF") }},
{ 0xDE, { EVERYWHERE("DIM") }},
{ 0xDF, { EVERYWHERE("DRAW") }},
{ 0xE0, { EVERYWHERE("END") }},
{ 0xE1, { EVERYWHERE("ENDPROC") }},
{ 0xE2, { EVERYWHERE("ENVELOPE") }},
{ 0xE3, { EVERYWHERE("FOR") }},
{ 0xE4, { EVERYWHERE("GOSUB") }},
{ 0xE5, { EVERYWHERE("GOTO") }},
{ 0xE6, { EVERYWHERE("GCOL") }},
{ 0xE7, { EVERYWHERE("IF") }},
{ 0xE8, { EVERYWHERE("INPUT") }},
{ 0xE9, { EVERYWHERE("LET") }},
{ 0xEA, { EVERYWHERE("LOCAL") }},
{ 0xEB, { EVERYWHERE("MODE") }},
{ 0xEC, { EVERYWHERE("MOVE") }},
{ 0xED, { EVERYWHERE("NEXT") }},
{ 0xEE, { EVERYWHERE("ON") }},
{ 0xEF, { EVERYWHERE("VDU") }},
{ 0xF0, { EVERYWHERE("PLOT") }},
{ 0xF1, { EVERYWHERE("PRINT") }},
{ 0xF2, { EVERYWHERE("PROC") }},
{ 0xF3, { EVERYWHERE("READ") }},
{ 0xF4, { EVERYWHERE("REM") }},
{ 0xF5, { EVERYWHERE("REPEAT") }},
{ 0xF6, { EVERYWHERE("REPORT") }},
{ 0xF7, { EVERYWHERE("RESTORE") }},
{ 0xF8, { EVERYWHERE("RETURN") }},
{ 0xF9, { EVERYWHERE("RUN") }},
{ 0xFA, { EVERYWHERE("STOP") }},
/* US BASIC maps COLOR to 0xFB, others map COLOUR to that. */
{ 0xFB, { EVERYWHERE("COLOR") }}, { 0xFB, { EVERYWHERE("COLOUR") }},
{ 0xFC, { EVERYWHERE("TRACE") }},
{ 0xFD, { EVERYWHERE("UNTIL") }},
{ 0xFE, { EVERYWHERE("WIDTH") }},
{ 0xFF, { EVERYWHERE("OSCLI") }},
{ 0x100,{end_marker,           end_marker,       end_marker,         end_marker       }},
};

void please_submit_bug_report()
{
  fprintf(stderr,
	  "We think this is a bug in this program.\n"
	  "Please submit a bug report, and include both the input file\n"
	  "and a correct ASCII listing of the program if you can get it.\n"
	  "Please email your bug report to james@youngman.org.\n");
}

/* is_fastvar returns true if this is a byte used by
 * BBC BASIC for SDL (or Windows) 2.0 to represent
 * a fast (REM!Fast) variable/FN/PROC.
 */
bool is_fastvar(unsigned int i)
{
  switch (i)
    {
    case 0x18:
    case 0x19:
    case 0x1A:
    case 0x1B:
    case 0x1C:
    case 0x1D:
    case 0x1E:
    case 0x1F:
      return true;
    default:
      return false;
    }
}

bool build_mapping(unsigned dialect, struct expansion_map *m)
{
  unsigned int tok;
  enum Dialect base_dialect = dialect;
  assert(dialect < NUM_DIALECTS);
  if (dialect == Mac)
    {
      /* We have no entries in base_map for Mac, but it is
       * similar to ARM except for the extension mappings
       * which are not in base_map anyway.
       */
      base_dialect = ARM;
    }
  else if (dialect == PDP11)
    {
      base_dialect = mos6502_32000;
    }

  assert(base_dialect <= LAST_BASE_MAP_DIALECT);

  for (char i = 0; ; ++i)
    {
      m->ascii[(unsigned int)i][0] = i;
      m->ascii[(unsigned int)i][1] = 0;
      // char may not be able to represent 0x80, so we can't use a normal for loop.
      if (i == 0x7F)
	break;
    }
  /* Set up ASCII identity mappings. */
  for (char i = 0x11; i < 0x7F; ++i)
    {
      m->base[(unsigned int)i] = m->ascii[(unsigned int)i];	/* some of these values will be overwritten. */
    }

  for (unsigned int i = 0; i < NUM_TOKENS; ++i)
    {
      tok = base_map[i].token_value;
      const char *s = base_map[i].dialect_mappings[base_dialect];
      assert(s != NULL);
      if (s == end_marker)
	{
	  break;
	}
      assert(tok < NUM_TOKENS);
      if (s == identity)
	{
	  /* This tells us we want an identity mapping for this input byte,
	   * but the identity mapping should have already been set up by
	   * the loop above.  If this assertion fails, you probably need to
	   * change the loop bounds on the ASCII identity mapping setup,
	   * above.
	   */
	  assert(m->base[tok][0] == m->ascii[tok][0]);
	  assert(m->base[tok][1] == 0);
	}
      else
	{
	  assert(s[0]);		/* empty mappings not allowed */
	  m->base[tok] = s;
	}
      if (i == NUM_TOKENS-1)
	break;
    }

  if (PDP11 == dialect)
    m->base[0xC8] = pdp_c8;
  m->base[0x7F] = (ARM == dialect || Mac == dialect) ? "OTHERWISE" : m->ascii[0x7F];
  m->base[0x0D] = m->ascii[0x0D];
  build_map_c6(dialect, m->c6);
  build_map_c7(dialect, m->c7);
  build_map_c8(dialect, m->c8);
  return true;
}

static void build_invalid_map(const char **output)
{
  unsigned int i;
  for (i = 0u; i < NUM_TOKENS; ++i)
    output[i] = invalid;
}

void build_map_c6(enum Dialect d, const char **output)
{
  build_invalid_map(output);
  if (d == mos6502_32000 || d == Z80_80x86 || d == Windows)
    {
      return;
    }

  if (ARM == d || Mac == d)
    {
      /* On ARM we handle 0xC6 0x8E 0xA9 as "SUM" (here) followed by
	 0xA9="LEN" which we handle as an ordinary single-byte token.

	 We do not expect to handle 0xC6 0xA9 here, because on Windows
	 we handle 0xC6 as the single-byte token "SUM" and 0xA9 as
	 the single-byte token "LEN".  So on Windows we shouldn't
	 be looking at an extension map at all in that case.
      */
      output[0x8E] = "SUM";
      output[0x8F] = "BEAT";
    }
  if (d == Mac)
    {
      output[0x90] = "ASK";
      output[0x91] = "ANSWER";
      output[0x92] = "SFOPENIN";
      output[0x93] = "SFOPENOUT";
      output[0x94] = "SFOPENUP";
      output[0x95] = "SFNAME$";
      output[0x96] = "MENU";
    }
}

void build_map_c7(enum Dialect d, const char **output)
{
  bool arm = (d == ARM);
  build_invalid_map(output);
  if (d == ARM || d == Mac)
    {
      output[0x8E] = "APPEND";
      output[0x8F] = "AUTO";
      /*                    ARM          Mac */
      output[0x90] = arm ? "CRUNCH"   : "DELETE";
      output[0x91] = arm ? "DELETE"   : "EDIT";
      output[0x92] = arm ? "EDIT"     : "HELP";
      output[0x93] = arm ? "HELP"     : "LIST";
      output[0x94] = arm ? "LIST"     : "LOAD";
      output[0x95] = arm ? "LOAD"     : "LVAR";
      output[0x96] = arm ? "LVAR"     : "NEW";
      output[0x97] = arm ? "NEW"      : "OLD";
      output[0x98] = arm ? "OLD"      : "RENUMBER";
      output[0x99] = arm ? "RENUMBER" : "SAVE";
      output[0x9A] = arm ? "SAVE"     : "TWIN";
      output[0x9B] = arm ? "TEXTLOAD" : "TWINO";
      output[0x9C] = arm ? "TEXTSAVE" : invalid;
      output[0x9D] = arm ? "TWIN"     : invalid;
      output[0x9E] = arm ? "TWINO"    : invalid;
      output[0x9F] = arm ? "INSTALL"  : invalid;
    }
}

void build_map_c8(enum Dialect d, const char **output)
{
  build_invalid_map(output);
  if (d == ARM || d == Mac)
    {
      output[0x8E] = "CASE";
      output[0x8F] = "CIRCLE";
      output[0x90] = "FILL";
      output[0x91] = "ORIGIN";
      output[0x92] = "POINT";
      output[0x93] = "RECTANGLE";
      output[0x94] = "SWAP";
      output[0x95] = "WHILE";
      output[0x96] = "WAIT";
      output[0x97] = "MOUSE";
      output[0x98] = "QUIT";
    }
  if (d == ARM)			/* but not Mac */
    {
      output[0x99] = "SYS";
      output[0x9A] = "INSTALL";
      output[0x9B] = "LIBRARY";
      output[0x9C] = "TINT";
      output[0x9D] = "ELLIPSE";
      output[0x9E] = "BEATS";
      output[0x9F] = "TEMPO";
      output[0xA0] = "VOICES";
      output[0xA1] = "VOICE";
      output[0xA2] = "STEREO";
      output[0xA3] = "OVERLAY";
      output[0xA4] = "MANDEL";
      output[0xA5] = "PRIVATE";
      output[0xA6] = "EXIT";
    }
}

struct dialect_mapping
{
  const char *name;
  const char *synonym_for;
  enum Dialect value;
};

static struct dialect_mapping dialects[] =
  {
   { "6502", NULL, mos6502_32000},
   { "PDP11", NULL, PDP11 },
   {"32000", "6502", mos6502_32000},
   {"Z80", NULL, Z80_80x86},
   {"8086", "Z80", Z80_80x86},
   {"ARM", NULL, ARM},
   {"Windows", NULL, Windows},
   {"SDL", "Windows", Windows},
   {"MacOSX", "Windows", Windows},
   {"Mac", NULL, Mac},
   {NULL, NULL, NUM_DIALECTS},
  };

bool set_dialect(const char* name, enum Dialect* d)
{
  const struct dialect_mapping *m;
  for (m = dialects; m->name != NULL; ++m)
    {
      if (0 == strcmp(name, m->name))
	{
	  *d = m->value;
	  return true;
	}
    }
  return false;
}

static bool any_tokens_valid(const char *map[NUM_TOKENS])
{
  for (unsigned int i = 0; i < NUM_TOKENS; ++i)
    {
      if (map[i] != invalid)
	return true;
    }
  return false;
}

static bool dump_map(FILE *f, const char* dialect_name, const char* map_name, const char *map[NUM_TOKENS])
{
  if (!any_tokens_valid(map))
    {
      if (fprintf(f, "%s (%s map): dialect has no valid tokens in the %s map\n",
		  dialect_name, map_name, map_name) < 0)
	return false;
      return true;
    }
  for (unsigned int i = 0; i < NUM_TOKENS; ++i)
    {
      const char *dest = map[i];
      if (map[i][0] == (char)i && map[i][1] == 0)
	dest = "(maps to itself)";
      if (fprintf(f, "%s (%s map): 0x%02X->%s\n", dialect_name, map_name, i, dest) < 0)
	return false;
    }
  return true;
}

static bool internal_dump_all_dialects_to_file(FILE *f)
{
  const struct dialect_mapping *m;
  for (m = dialects; m->name != NULL; ++m)
    {
      struct expansion_map xmap;
      if (m->synonym_for != NULL)
	{
	  fprintf(f, "dialect %s=%s\n", m->name, m->synonym_for);
	  continue;
	}
      fprintf(f, "dialect %s:\n", m->name);
      if (!build_mapping(m->value, &xmap))
	return false;
      if (!(dump_map(f, m->name, "base", xmap.base) &&
	    dump_map(f, m->name, "c6", xmap.c6) &&
	    dump_map(f, m->name, "c7", xmap.c7) &&
	    dump_map(f, m->name, "c8", xmap.c8)))
	{
	  return false;
	}
    }
  return true;
}

bool internal_dump_all_dialects(const char *file_name)
{
  if (0 == strcmp(file_name, "-"))
    {
      return internal_dump_all_dialects_to_file(stdout);
    }
  else
    {
      bool ok = true;
      FILE *f = fopen(file_name, "w");
      if (NULL == f)
	{
	  perror(file_name);
	  return false;
	}
      ok = internal_dump_all_dialects_to_file(f);
      if (EOF == fclose(f))
	{
	  perror(file_name);
	  ok = false;
	}
      return ok;
    }
}

bool print_dialects(FILE *f, const char *default_dialect_name)
{
  const struct dialect_mapping *m;
  bool first;
  if (fprintf(f, "Known dialects are: ") < 0)
    return false;
  for (first = true, m = dialects; m->name != NULL; ++m, first=false)
    {
      if (!first)
	{
	  if (fputs(", ", f) < 0)
	    return false;
	}
      if (fprintf(f, "\"%s\"", m->name) < 0)
	return false;
      if (0 == strcmp(default_dialect_name, m->name))
	{
	  if (fputs(" (this is the default)", f) < 0)
	    return false;
	}
      if (m->synonym_for != NULL)
	{
	  if (fprintf(f, " (this is a synonym for \"%s\")", m->synonym_for) < 0)
	    return false;
	}
    }
  if (fputc('\n', f) == EOF)
    return false;
  return true;
}

/* Local Variables: */
/* fill-column: 200 */
/* End: */
