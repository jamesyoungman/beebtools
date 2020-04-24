#include "tokens.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


struct multi_mapping 
{
  unsigned int token_value;
  const char* dialect_mappings[NUM_DIALECTS];
};

/* We use the initializer BAD to signal that a dialect has no mapping
 * for this token value.  We interpret NULL to mean that the data
 * structure is incomplete, that is, we wrote a bug.
 */
#define BAD "__invalid__"
#define LINE_NUM "__line_num__"

const char invalid[] = BAD;
const char line_num[] = LINE_NUM;

/* There are a number of other special tokens. */
#define DO_C6 "__c6__"
#define DO_C7 "__c7__"
#define DO_C8 "__c8__"
#define END "__end__"

static const struct multi_mapping base_map[NUM_TOKENS] = {
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
{ 0xC6, {DO_C6,                DO_C6,            DO_C6,              "SUM"            }},
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
{ 0x100,{END,                  END,              END,                END              }},
};


/*
  compound mappings

0x42 59:BAD,               BAD,               "BY                             |
0x91 24:" TIME$",           "TIME$",          "TIME$           TIME$          |
0xB8 50:" TOP",             "TOP",            "TOP             TOP            |
0xC9 4F:" LISTO",           "LISTO         |                                    |
0xD1 24:" TIME$",           "TIME$             TIME$             TIME$          |
0xF6 24:" REPORT$",         "REPORT$           REPORT$           REPORT$        |
*/

/* Multi-byte mappings
0xC6 A9:BAD,                "              |                     SUMLEN         |
0xC6 8E:BAD,                "              |   SUM                              |
0xC6 8E A"9:",              "              |   SUMLEN                           |
0xC6 8F:BAD,                "              |   BEAT                             |
                                   +-------+-  --------------+                  |
                                   | Extra Apple Mac tokens  |                  |
0xC6:"    AUTO",            "AUTO  |                         |   SUM            |
0xC6 90:BAD,                "      |  ASK                    |                  |
0xC6 91:BAD,                "      |  ANSWER                 |                  |
0xC6 92:BAD,                "      |  SFOPENI  N             |                  |
0xC6 93:BAD,                "      |  SFOPENO  UT            |                  |
0xC6 94:BAD,                "      |  SFOPENU  P             |                  |
0xC6 95:BAD,                "      |  SFNAME$                |                  |
0xC6 96:BAD,                "      |  MENU                   |                  |
                            "      +-------------------------+------------------+
                            "      |  Extra Immediate Cmds   |                  |
                            "      +------------------------ |                  |
0xC7:"    DELETE",          "DELETE                          |   WHILE          |
0xC7 8E:BAD,                "                  APPEND        |                  |
0xC7 8F:BAD,                "                  AUTO          |                  |
0xC7 90:BAD,                "                  CRUNCH        |                  |
0xC7 91:BAD,                "                  DELETE        |                  |
0xC7 92:BAD,                "                  EDIT          |                  |
0xC7 92 4"F:",              "                  EDITO         |                  |
0xC7 93:BAD,                "                  HELP          |                  |
0xC7 94:BAD,                "                  LIST          |                  |
0xC7 94 4"F:",              "                  LISTO         |                  |
0xC7 95:BAD,                "                  LOAD          |                  |
0xC7 96:BAD,                "                  LVAR          |                  |
0xC7 97:BAD,                "                  NEW           |                  |
0xC7 98:BAD,                "                  OLD           |                  |
0xC7 99:BAD,                "                  RENUMBER      |                  |
0xC7 9A:BAD,                "                  SAVE          |                  |
0xC7 9B:BAD,                "                  TEXTLOAD      |                  |
0xC7 9C:BAD,                "                  TEXTSAVE      |                  |
0xC7 9D:BAD,                "                  TWIN          |                  |
0xC7 9E:BAD,                "                  TWINO         |                  |
0xC7 9F:BAD,                "                  INSTALL       |                  |
0xC8:"LOAD",                "LOAD          |                     CASE           |
0xC8 8E:BAD,                "              |   CASE                             |
0xC8 8F:BAD,                "              |   CIRCLE                           |
0xC8 90:BAD,                "              |   FILL                             |
0xC8 91:BAD,                "              |   ORIGIN                           |
0xC8 92:BAD,                "              |   POINT                            |
0xC8 93:BAD,                "              |   RECTANGLE                        |
0xC8 94:BAD,                "              |   SWAP                             |
0xC8 95:BAD,                "              |   WHILE                            |
0xC8 96:BAD,                "              |   WAIT                             |
0xC8 97:BAD,                "              |   MOUSE                            |
0xC8 98:BAD,                "              |   QUIT                             |
0xC8 99:BAD,                "              |   SYS                              |
0xC8 9A:BAD,                "              |   INSTALL                          |
0xC8 9B:BAD,                "              |   LIBRARY                          |
0xC8 9C:BAD,                "              |   TINT                             |
0xC8 9D:BAD,                "              |   ELLIPSE                          |
0xC8 9E:BAD,                "              |   BEATS                            |
0xC8 9F:BAD,                "              |   TEMPO                            |
0xC8 A0:BAD,                "              |   VOICES                           |
0xC8 A1:BAD,                "              |   VOICE                            |
0xC8 A2:BAD,                "              |   STEREO                           |
0xC8 A3:BAD,                "              |   OVERLAY                          |
0xC8 A4:BAD,                "              |   MANDEL                           |
0xC8 A5:BAD,                "              |   PRIVATE                          |
0xC8 A6:BAD,                "              |   EXIT                             |
*/


static void set_ascii_mappings(const char** out)
{
  unsigned i;
  for (i = 0x11; i < 0x80; ++i)
    {
      char *p = calloc(2, 1);
      p[0] = i;			/* the character itself */
      out[i] = p;
    }
}


static const char** build_base_mapping(unsigned dialect) 
{
  unsigned int tok, i;
  const char** out = calloc(NUM_TOKENS, sizeof(*out));
  assert(dialect < NUM_DIALECTS);
  for (i = 0; i < NUM_TOKENS; ++i)
    {
      if (0 == strcmp(base_map[i].dialect_mappings[dialect], END))
	break;
      tok = base_map[i].token_value;
      assert(tok < NUM_TOKENS);
      const char *s = base_map[i].dialect_mappings[dialect];
      out[tok] = (s && s[0]) ? s : NULL;
    }
  set_ascii_mappings(out);
  out[0x0D] = strdup("\n");
  return out;
}

const char** build_mapping(unsigned dialect) 
{
  /* The Mac mapping is only different in C6 handling. */
  return build_base_mapping(dialect == Mac ? mos6502_32000 : dialect);
}

const char *map_c6(enum Dialect d, unsigned char uch)
{
  assert(d != mos6502_32000);	/* should have already been handled */
  assert(d != Windows);		/* 0xC6 (SUM) 0xA9 (LEN) handled as separate tokens. */
  if (d == Mac)
    {
      switch (uch)
	{
	case 0x90: return "ASK";
	case 0x92: return "ANSWER";
	case 0x93: return "SFOPENIN";
	case 0x94: return "SFOPENOUT";
	case 0x95: return "SFOPENUP";
	case 0x06: return "MENU";
	default: return BAD;
	}
    }
  /* On ARM we handle 0xC6 0x8E 0xA9 as "SUM" (here) followed by
     0xA9="LEN" which we handle as an ordinary single-byte token.
     
     We do not expect to handle 0xC6 0xA9 here, because on Windows we
     handle 0xC6 as the single-byte token "SUM" and 0xA9 as the
     single-byte token "LEN".  So this function should not see those
     token sequenes except when the dialect is not Windows (in which
     case that token sequence is invalid).
   */
  switch (uch)
    {
    case 0xA9: return BAD;	/* see above. */
    case 0x8E: return (d == ARM) ? "SUM" : BAD;
    case 0x8F: return (d == ARM) ? "BEAT" : BAD;
    default: return BAD;
    }
}

const char *map_c7(enum Dialect d, unsigned char uch)
{
  assert(d == ARM);
  static const char* c7_tokens[1+0x9F-0x8E] =
    {"APPEND", "AUTO", "CRUNCH", "DELETE", "EDIT", "HELP",
     "LIST", "LOAD", "LVAR", "NEW", "OLD", "RENUMBER",
     "SAVE", "TEXTLOAD", "TEXTSAVE", "TWIN", "TWINO", "INSTALL",
    };
  if (uch < 0x8E || uch > 0x9F)
    return BAD;
  else
    return c7_tokens[uch];
}

const char *map_c8(enum Dialect d, unsigned char uch)
{
  assert(d == ARM);
  static const char* c8_tokens[1+0xA6-0x8E] =
    {
     "CASE", "CIRCLE", "FILL", "ORIGIN", "POINT",
     "RECTANGLE", "SWAP", "WHILE", "WAIT", "MOUSE", "QUIT",
     "SYS", "INSTALL", "LIBRARY", "TINT", "ELLIPSE", "BEATS",
     "TEMPO", "VOICES", "VOICE", "STEREO", "OVERLAY", "MANDEL",
     "PRIVATE", "EXIT"
    };
  if (uch < 0x8E || uch > 0xA6)
    return BAD;
  else
    return c8_tokens[uch];
}

