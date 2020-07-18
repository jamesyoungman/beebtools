# Features

The BBC BASIC de-tokeniser `bbcbasic_to_text` is quite comprehensive:

1. It covers less well-known implementations (Mac Classic, PDP-11) as
   well as the traditional (BBC Micro, Archimedes, Z80) and modern
   (Windows, SDL) implementatons.
2. It handles a number of situations better than some of the
   alternative programs:
   1. Line numbers
   2. Lines lacking line numbers
   3. It incorporates (and documents) some obscure issues not
      documented elsewhere

The `dfs` program deals well with a number of less-common cases:

1. Double-density disks
1. Support for Opus DDOS and Watford DFS discs as well as, of course,
   Acorn DFS floppies.
1. As well as SSD, SDD, DSD and DDD image files, MMB files and HFE
   versions 1 and 3 are suported.
1. All supported image file formats are supported compressed (with
   gzip).
1. Generally, file system type and disc geometry guessing are quite
   thorough.  For example, the code can successfully identify an Opus
   DDOS 35-track double-density disc image.
1. The code explains (optionally, via --verbose) why it does or does
   not believe a particular image file contains any of the supported
   file systems.

Manual pages and regression tests are included.
