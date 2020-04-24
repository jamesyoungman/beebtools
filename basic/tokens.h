enum Dialect
  {
   MIN_DIALECT = 0, mos6502_32000 = MIN_DIALECT,
   Z80_80x86 = 1,
   ARM = 2,
   Windows = 3,
   Mac = 4,
   NUM_DIALECTS,
  };
enum { NUM_TOKENS = 0x100 };

extern const char invalid[];
extern const char line_num[];
const char** build_mapping(unsigned dialect);
const char *map_c6(enum Dialect, unsigned char);
const char *map_c7(enum Dialect, unsigned char);
const char *map_c8(enum Dialect, unsigned char);
