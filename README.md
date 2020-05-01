# beebtools README

## What Is This?

A set of tools for working with discs and files
from computers in the BBC Microcomputer family.

It includes

1.  dfs: a program for extracting data from Acorn DFS format floppy discs (or more typically, floppy disc images)
2.  bbcbasic_to_text: a program which converts a saved BBC basic program to text

## How Do I Build It?

The software can be built (and tested) with cmake, like this:

```sh
mkdir _build
cd _build
cmake .. && make && ctest -N
```
