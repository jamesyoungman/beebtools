# References - DFS

[https://www.hermannseib.com/documents/floppy.pdf](https://www.hermannseib.com/documents/floppy.pdf)
provides a general introduction to floppy disks.

## Image File Formats

* APD - TODO: put URL here
* FDI - [Specification](http://www.oldskool.org/disk2fdi/files/FDISPEC.pdf)
* HFE - See [HFE file format](https://hxc2001.com/download/floppy_drive_emulator/SDCard_HxC_Floppy_Emulator_HFE_file_format.pdf).
* IPF [Unofficial specification](http://info-coach.fr/atari/documents/_mydoc/IPF-Documentation.pdf).
* MMB - The MMB file format is described in [DUTILS Guide](https://nanopdf.com/download/http-mmbeebmysitewanadoo-memberscouk-dutils-guide-04_pdf)
* Sector Dump Formats
   * There is no official standard for the SSD, SDD, DSD, DDD
     formats.  Relevant info:
      * [Stardot: Re: b-em & interleaved double-density
        files](https://stardot.org.uk/forums/viewtopic.php?f=4&t=19742&p=275195&hilit=DSD+format#p275601)
	  * [Stardot: Re: Interleaved and non-interleaved disc
        images](https://stardot.org.uk/forums/viewtopic.php?f=2&t=9815&p=116413&hilit=sequential+sided+disc#p116413)
      * [Stardot: Re: SSDs in Strange
        Sizes](https://stardot.org.uk/forums/viewtopic.php?f=4&t=15651&p=214122&hilit=sequential+sided+disc#p214122)
        (which points out that SSD files may contain _two_ sides).


## Acorn DFS and Related File System Descriptions

### Acorn DFS
* [Acorn Disc Filing System USER
     GUIDE](http://chrisacorns.computinghistory.org.uk/docs/Acorn/Manuals/Acorn_DiscSystemUGI2.pdf)
   * The Catalogue format is described on page 86 (Chapter 12).
* [BeebWiki page _Acorn DFS disc format_](http://beebwiki.mdfs.net/Acorn_DFS_disc_format)

### Watford DFS
Watford Electronics DFS Manual, ISBN 0948663006

* Includes both user manual and format specification
* [HTML](https://acorn.huininga.nl/pub/unsorted/manuals/Watford%20DFS-Manual/WE_DFS_manual.html)
* [PDF (in a zip file)](http://bbc.nvg.org/doc/WatfordDFS-manual.zip)

### Opus DDOS
* [Disc Manual for the BBC Micro using Opus Peripherals](http://chrisacorns.computinghistory.org.uk/docs/Opus/Opus_DDOS.pdf)

### Solidisk DFS
[Solidisk DDFS User
Manual](http://chrisacorns.computinghistory.org.uk/docs/Solidisk/Solidisk_DiskFilingSystem.pdf)
- does not document the on-disc format.

### HDFS - "A Hierarchical Disc Filing System for the BBC Microcomputer"

* Includes both user manual and format specification
* [HTML](http://knackered.org/angus/beeb/hdfs.html)
* [Compressed TAR file containing PDF and ROM image](ftp://ftp.knackered.org/pub/angus/BBC/hdfs.tar.gz)


## User Documentation

   * _BBC Microcomputer System Disc Filing System User Guide_, March 1985.


## Physical Floppies

### Floppy Disk Hardware

* [Shugart SA800/801 Theory of
  Operations](http://www.mirrorservice.org/sites/www.bitsavers.org/pdf/shugart/SA8xx/50664-0_SA800_801_Theory_of_Operations_Apr76.pdf) -
  this documentation for the Shugart SA800/SA801 floppy drives explains in detail how the floppy hardware works. It also includes a diagram of the track format (page 7 as marked, page 11 of the PDF) but doesn't explain the gap signalling in detail.
* Herb Johnson has a [website describing many arcane
  details](http://www.retrotechnology.com/herbs_stuff/drive.html),
  including why it might have been useful to use a 35-track format.



### Floppy Disk Data Representation / Signals

* FM - [IBM GA21-9182-3 Diskette General Information Manual, September
  1977](http://www.bitsavers.org/pdf/ibm/floppy/GA21-9182-3_Diskette_General_Information_Manual_Sep77.pdf) -
  includes a description of the track format
* MFM - IBM System 34 Format - there should, surely, be a canonical
  description from IBM
* [Control Data Corporation Application Note 75897469 Jan 1980, "5.25 inch FDD format considerations and controller compatibilities"](https://archive.org/details/bitsavers_cdcdiscsflDFmtJan80_3167476/mode/2up)

### Floppy Disk Controller Data Sheets

* [Intel 8271](http://www.nj7p.org/Manuals/PDFs/Intel/AFN-00223B.pdf)
* [WD 1770](https://datasheetspdf.com/datasheet/WD1770.html)
* [WD 1772](https://datasheetspdf.com/pdf/1311813/WesternDigital/WD1772/1)
* [WD 1791 datasheet](http://www.proteus-international.fr/userfiles/downloads/Datasheets/Western%20Digital%20FD1791.pdf) - describes some non-IBM gap formats
* [TI TMS279 Floppy Disc Controller](https://pdf1.alldatasheet.com/datasheet-pdf/view/29028/TI/TMS279X.html) - gives diagrams of the IBM System 34 (256 byte/sector) format
* [NEC 765](http://www.classiccmp.org/dunfield/r/765.pdf)

## ECMA Standards

*  8"
   *  [ECMA-54 - Data interchange on 200 mm flexible disk cartridges using two-frequency recording at 13 262 ftprad on one side](http://dev.ecma-international.org/publications-and-standards/standards/ecma-54/)
   *  [ECMA-58 - 200mm Flexible Disk Cartridge Labelling and File Structure for Information Interchange](http://dev.ecma-international.org/publications-and-standards/standards/ecma-58/)
   *  [ECMA-59 - Data interchange on 200 mm flexible disk cartridges using two-frequency recording at 13 262 ftprad on both sides](http://dev.ecma-international.org/publications-and-standards/standards/ecma-59/)
* 5.25"
   *  [ECMA-66 - Data interchange on 130 mm flexible disk cartridges using two-frequency recording at 7 958 ftprad on one side](http://dev.ecma-international.org/publications-and-standards/standards/ecma-66/)
   *  [ECMA-70 - Data interchange on 130 mm flexible disk cartridges using MFM recording at 7 958 ftprad on 40 tracks on each side](http://dev.ecma-international.org/publications-and-standards/standards/ecma-70/)
   *  [ECMA-99 - Data interchange on 130 mm flexible disk cartridges using MFM recording at 13 262 ftprad on both sides, 3,8 tracks per mm](http://dev.ecma-international.org/publications-and-standards/standards/ecma-99/)

## Other

* [_Standard Archive Format_ by Mark de Weger](
  http://archive.retro-kit.co.uk/bbc.nvg.org/std-format.php3.html)
