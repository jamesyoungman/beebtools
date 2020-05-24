#include "tokens.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARRAYSIZE(arr) (sizeof(arr)/sizeof(arr[0]))

/* We use the initializer BAD to signal that a dialect has no mapping
 * for this token value.  We interpret NULL to mean that the data
 * structure is incomplete, that is, we wrote a bug.
 */
#define BAD "__invalid__"
#define LINE_NUM "__line_num__"

const char invalid[] = BAD;
const char line_num[] = LINE_NUM;
const char fastvar[] = "__fastvar__";
const char identity[] = "__identity__";
const char end_marker[] = "__end__";

/* There are a number of other special tokens. */
#define DO_C6 "__c6__"
#define DO_C7 "__c7__"
#define DO_C8 "__c8__"


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
  const char* dialect_mappings[NUM_DIALECTS-1];
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
{ 0x80, {"AND",                "AND",            "AND",              "AND"            }},
{ 0x81, {"DIV",                "DIV",            "DIV",              "DIV"            }},
{ 0x82, {"EOR",                "EOR",            "EOR",              "EOR"            }},
{ 0x83, {"MOD",                "MOD",            "MOD",              "MOD"            }},
{ 0x84, {"OR",                 "OR",             "OR",               "OR"             }},
{ 0x85, {"ERROR",              "ERROR",          "ERROR",            "ERROR"          }},
{ 0x86, {"LINE",               "LINE",           "LINE",             "LINE"           }},
{ 0x87, {"OFF",                "OFF",            "OFF",              "OFF"            }},
{ 0x88, {"STEP",               "STEP",           "STEP",             "STEP"           }},
{ 0x89, {"SPC",                "SPC",            "SPC",              "SPC"            }},
{ 0x8A, {"TAB(",               "TAB(",           "TAB(",             "TAB("           }},
{ 0x8B, {"ELSE",               "ELSE",           "ELSE",             "ELSE"           }},
{ 0x8C, {"THEN",               "THEN",           "THEN",             "THEN"           }},
{ 0x8D, {LINE_NUM,             LINE_NUM,         LINE_NUM,           LINE_NUM         }},
{ 0x8E, {"OPENIN",             "OPENIN",         "OPENIN",           "OPENIN"         }},
{ 0x8F, {"PTR",                "PTR",            "PTR",              "PTR"            }},
{ 0x90, {"PAGE",               "PAGE",           "PAGE",             "PAGE"           }},
{ 0x91, {"TIME",               "TIME",           "TIME",             "TIME"           }},
{ 0x92, {"LOMEM",              "LOMEM",          "LOMEM",            "LOMEM"          }},
{ 0x93, {"HIMEM",              "HIMEM",          "HIMEM",            "HIMEM"          }},
{ 0x94, {"ABS",                "ABS",            "ABS",              "ABS"            }},
{ 0x95, {"ACS",                "ACS",            "ACS",              "ACS"            }},
{ 0x96, {"ADVAL",              "ADVAL",          "ADVAL",            "ADVAL"          }},
{ 0x97, {"ASC",                "ASC",            "ASC",              "ASC"            }},
{ 0x98, {"ASN",                "ASN",            "ASN",              "ASN"            }},
{ 0x99, {"ATN",                "ATN",            "ATN",              "ATN"            }},
{ 0x9A, {"BGET",               "BGET",           "BGET",             "BGET"           }},
{ 0x9B, {"COS",                "COS",            "COS",              "COS"            }},
{ 0x9C, {"COUNT",              "COUNT",          "COUNT",            "COUNT"          }},
{ 0x9D, {"DEG",                "DEG",            "DEG",              "DEG"            }},
{ 0x9E, {"ERL",                "ERL",            "ERL",              "ERL"            }},
{ 0x9F, {"ERR",                "ERR",            "ERR",              "ERR"            }},
{ 0xA0, {"EVAL",               "EVAL",           "EVAL",             "EVAL"           }},
{ 0xA1, {"EXP",                "EXP",            "EXP",              "EXP"            }},
{ 0xA2, {"EXT",                "EXT",            "EXT",              "EXT"            }},
{ 0xA3, {"FALSE",              "FALSE",          "FALSE",            "FALSE"          }},
{ 0xA4, {"FN",                 "FN",             "FN",               "FN"             }},
{ 0xA5, {"GET",                "GET",            "GET",              "GET"            }},
{ 0xA6, {"INKEY",              "INKEY",          "INKEY",            "INKEY"          }},
{ 0xA7, {"INSTR(",             "INSTR(",         "INSTR(",           "INSTR("         }},
{ 0xA8, {"INT",                "INT",            "INT",              "INT"            }},
{ 0xA9, {"LEN",                "LEN",            "LEN",              "LEN"            }},
{ 0xAA, {"LN",                 "LN",             "LN",               "LN"             }},
{ 0xAB, {"LOG",                "LOG",            "LOG",              "LOG"            }},
{ 0xAC, {"NOT",                "NOT",            "NOT",              "NOT"            }},
{ 0xAD, {"OPENUP",             "OPENUP",         "OPENUP",           "OPENUP"         }},
{ 0xAE, {"OPENOUT",            "OPENOUT",        "OPENOUT",          "OPENOUT"        }},
{ 0xAF, {"PI",                 "PI",             "PI",               "PI"             }},
{ 0xB0, {"POINT(",             "POINT(",         "POINT(",           "POINT("         }},
{ 0xB1, {"POS",                "POS",            "POS",              "POS"            }},
{ 0xB2, {"RAD",                "RAD",            "RAD",              "RAD"            }},
{ 0xB3, {"RND",                "RND",            "RND",              "RND"            }},
{ 0xB4, {"SGN",                "SGN",            "SGN",              "SGN"            }},
{ 0xB5, {"SIN",                "SIN",            "SIN",              "SIN"            }},
{ 0xB6, {"SQR",                "SQR",            "SQR",              "SQR"            }},
{ 0xB7, {"TAN",                "TAN",            "TAN",              "TAN"            }},
{ 0xB8, {"TO",                 "TO",             "TO",               "TO"             }},
{ 0xB9, {"TRUE",               "TRUE",           "TRUE",             "TRUE"           }},
{ 0xBA, {"USR",                "USR",            "USR",              "USR"            }},
{ 0xBB, {"VAL",                "VAL",            "VAL",              "VAL"            }},
{ 0xBC, {"VPOS",               "VPOS",           "VPOS",             "VPOS"           }},
{ 0xBD, {"CHR$",               "CHR$",           "CHR$",             "CHR$"           }},
{ 0xBE, {"GET$",               "GET$",           "GET$",             "GET$"           }},
{ 0xBF, {"INKEY$",             "INKEY$",         "INKEY$",           "INKEY$"         }},
{ 0xC0, {"LEFT$(",             "LEFT$(",         "LEFT$(",           "LEFT$("         }},
{ 0xC1, {"MID$(",              "MID$(",          "MID$(",            "MID$("          }},
{ 0xC2, {"RIGHT$(",            "RIGHT$(",        "RIGHT$(",          "RIGHT$("        }},
{ 0xC3, {"STR$",               "STR$",           "STR$",             "STR$"           }},
{ 0xC4, {"STRING$(",           "STRING$(",       "STRING$(",         "STRING$("       }},
{ 0xC5, {"EOF",                "EOF",            "EOF",              "EOF"            }},
{ 0xC6, {"AUTO",               "AUTO",           DO_C6,              "SUM"            }},
{ 0xC7, {"DELETE",             "DELETE",         DO_C7,              "WHILE"          }},
{ 0xC8, {"LOAD",               "LOAD",           DO_C8,              "CASE"           }},
{ 0xC9, {"LIST",               "LIST",           "WHEN",             "WHEN"           }},
{ 0xCA, {"NEW",                "NEW",            "OF",               "OF"             }},
{ 0xCB, {"OLD",                "OLD",            "ENDCASE",          "ENDCASE"        }},
{ 0xCC, {"RENUMBER",           "RENUMBER",       "ELSE",             "OTHERWISE"      }},
{ 0xCD, {"SAVE",               "SAVE",           "ENDIF",            "ENDIF"          }},
{ 0xCE, {"EDIT",               "PUT",            "ENDWHILE",         "ENDWHILE"       }},
{ 0xCF, {"PTR",                "PTR",            "PTR",              "PTR"            }},
{ 0xD0, {"PAGE",               "PAGE",           "PAGE",             "PAGE"           }},
{ 0xD1, {"TIME",               "TIME",           "TIME",             "TIME"           }},
{ 0xD2, {"LOMEM",              "LOMEM",          "LOMEM",            "LOMEM"          }},
{ 0xD3, {"HIMEM",              "HIMEM",          "HIMEM",            "HIMEM"          }},
{ 0xD4, {"SOUND",              "SOUND",          "SOUND",            "SOUND"          }},
{ 0xD5, {"BPUT",               "BPUT",           "BPUT",             "BPUT"           }},
{ 0xD6, {"CALL",               "CALL",           "CALL",             "CALL"           }},
{ 0xD7, {"CHAIN",              "CHAIN",          "CHAIN",            "CHAIN"          }},
{ 0xD8, {"CLEAR",              "CLEAR",          "CLEAR",            "CLEAR"          }},
{ 0xD9, {"CLOSE",              "CLOSE",          "CLOSE",            "CLOSE"          }},
{ 0xDA, {"CLG",                "CLG",            "CLG",              "CLG"            }},
{ 0xDB, {"CLS",                "CLS",            "CLS",              "CLS"            }},
{ 0xDC, {"DATA",               "DATA",           "DATA",             "DATA"           }},
{ 0xDD, {"DEF",                "DEF",            "DEF",              "DEF"            }},
{ 0xDE, {"DIM",                "DIM",            "DIM",              "DIM"            }},
{ 0xDF, {"DRAW",               "DRAW",           "DRAW",             "DRAW"           }},
{ 0xE0, {"END",                "END",            "END",              "END"            }},
{ 0xE1, {"ENDPROC",            "ENDPROC",        "ENDPROC",          "ENDPROC"        }},
{ 0xE2, {"ENVELOPE",           "ENVELOPE",       "ENVELOPE",         "ENVELOPE"       }},
{ 0xE3, {"FOR",                "FOR",            "FOR",              "FOR"            }},
{ 0xE4, {"GOSUB",              "GOSUB",          "GOSUB",            "GOSUB"          }},
{ 0xE5, {"GOTO",               "GOTO",           "GOTO",             "GOTO"           }},
{ 0xE6, {"GCOL",               "GCOL",           "GCOL",             "GCOL"           }},
{ 0xE7, {"IF",                 "IF",             "IF",               "IF"             }},
{ 0xE8, {"INPUT",              "INPUT",          "INPUT",            "INPUT"          }},
{ 0xE9, {"LET",                "LET",            "LET",              "LET"            }},
{ 0xEA, {"LOCAL",              "LOCAL",          "LOCAL",            "LOCAL"          }},
{ 0xEB, {"MODE",               "MODE",           "MODE",             "MODE"           }},
{ 0xEC, {"MOVE",               "MOVE",           "MOVE",             "MOVE"           }},
{ 0xED, {"NEXT",               "NEXT",           "NEXT",             "NEXT"           }},
{ 0xEE, {"ON",                 "ON",             "ON",               "ON"             }},
{ 0xEF, {"VDU",                "VDU",            "VDU",              "VDU"            }},
{ 0xF0, {"PLOT",               "PLOT",           "PLOT",             "PLOT"           }},
{ 0xF1, {"PRINT",              "PRINT",          "PRINT",            "PRINT"          }},
{ 0xF2, {"PROC",               "PROC",           "PROC",             "PROC"           }},
{ 0xF3, {"READ",               "READ",           "READ",             "READ"           }},
{ 0xF4, {"REM",                "REM",            "REM",              "REM"            }},
{ 0xF5, {"REPEAT",             "REPEAT",         "REPEAT",           "REPEAT"         }},
{ 0xF6, {"REPORT",             "REPORT",         "REPORT",           "REPORT"         }},
{ 0xF7, {"RESTORE",            "RESTORE",        "RESTORE",          "RESTORE"        }},
{ 0xF8, {"RETURN",             "RETURN",         "RETURN",           "RETURN"         }},
{ 0xF9, {"RUN",                "RUN",            "RUN",              "RUN"            }},
{ 0xFA, {"STOP",               "STOP",           "STOP",             "STOP"           }},
{ 0xFB, {"COLOUR",             "COLOUR",         "COLOUR",           "COLOUR"         }},
{ 0xFB, {"COLOR",              "COLOR",          "COLOR",            "COLOR"          }},
{ 0xFC, {"TRACE",              "TRACE",          "TRACE",            "TRACE"          }},
{ 0xFD, {"UNTIL",              "UNTIL",          "UNTIL",            "UNTIL"          }},
{ 0xFE, {"WIDTH",              "WIDTH",          "WIDTH",            "WIDTH"          }},
{ 0xFF, {"OSCLI",              "OSCLI",          "OSCLI",            "OSCLI"          }},
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
  assert(base_dialect < NUM_DIALECTS-1);

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
   {"32000", "6502", mos6502_32000},
   {"Z80", NULL, Z80_80x86},
   {"8086", "Z80", Z80_80x86},
   {"ARM", NULL, ARM},
   {"Windows", NULL, Windows},
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
