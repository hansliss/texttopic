#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <float.h>
#include <math.h>
#include <types.h>
#include <stdint.h>

#define FONTSIZE 512
#define FONTGEOMX 8
#define FONTGEOMY 8
#define BUFSIZE 8192

void usage(char *progname) {
  fprintf(stderr, "Usage: %s -F <font file> -C <charset file> -o <output file>\n", progname);
}

typedef struct { // Data stored PER GLYPH
  uint16_t bitmapOffset;     // Pointer into GFXfont->bitmap
  uint8_t  width, height;    // Bitmap dimensions in pixels
  uint8_t  xAdvance;         // Distance to advance cursor (x axis)
  int8_t   xOffset, yOffset; // Dist from cursor pos to UL corner
} GFXglyph;

typedef struct { // Data stored for FONT AS A WHOLE:
  uint8_t  *bitmap;      // Glyph bitmaps, concatenated
  GFXglyph *glyph;       // Glyph array
  uint8_t   first, last; // ASCII extents
  uint8_t   yAdvance;    // Newline distance (y axis)
} GFXfont;

typedef struct _fontinfo {
  unsigned char character;
  uint8_t *bitmap;
  uint8_t width, height, xAdvance, xOffset, yOffset;
  struct _fontinfo *next;
} *fontinfo;

void addFi(fontinfo *fi, unsigned char c, uint8_t width, uint8_t height, uint8_t xAdvance, uint8_t xOffset, uint8_t yOffset) {
  if (*fi && ((*fi)->character < c)) {
    addFi(&((*fi)->next), width, height, xAdvance, xOffset, yOffset);
  } else {
    fontinfo tmp = (fontinfo)malloc(sizeof(struct _fontinfo));
    tmp->width = width;
    tmp->height = height;
    tmp->xAdvance = xAdvance;
    tmp->xOffset = xOffset;
    tmp->yOffset = yOffset;
    tmp->next = *fi;
    *fi = tmp;
  }
}

fontinfo findFi(fontinfo fi, unsigned char c) {
  if (fi && (fi->character != c)) {
    return findFi(fi->next, c);
  } else {
    return fi;
  }
}

typedef struct font_s {
  unsigned char bits[FONTSIZE * FONTGEOMY] ;
  unsigned char charset[FONTSIZE];
} *font;

int findBits(font f, unsigned char c) {
  int i=0;
  int ready=0;
  while (!ready && i<FONTSIZE) {
    if (c == f->charset[i]) {
      ready = 1;
    } else {
      i++;
    }
  }
  if (ready) {
    return i * FONTGEOMY;
  } else {
    return -1;
  }
}

int checkColumn(font f, int fBitsIndex, int column) {
  int i;
  for (i=0; i<FONTGEOMY; i++) {
    if (f[fBitsIndex + i] & (1 << (7 - column))) {
      return 1;
    }
  }
  return 0;
}

void findlimits(font f, int fBitsIndex, int &l, int &r, int &t, int &b) {
  l=0;
  r=FONTGEOMX-1;
  t=0;
  b=FONTGEOMY-1;
  while ((t < FONTGEOMY) && !f[fBitsIndex + t]) {
    t++;
  }
  while ((b >= t) && !f[fBitsIndex + b]) {
    b--;
  }
  while ((l < FONTGEOMX) && !checkColumn(f, fBitsIndex, l)) {
    l++;
  }
  while ((r >= l) && !checkColumn(f, fBitsIndex, r))) {
    r--;
  }
}

int getBit(int x, int y, int fBitsIndex, font f) {
  return f->bits[fBitsIndex + (FONTGEOMY - y - 1)] & (1 << (FONTGEOMX - x - 1));
}

int main(int argc, char *argv[]) {
  int o;
  FILE *ffile=NULL, *cfile=NULL, *ofile = NULL;
  unsigned char ascii;
  font f = (font)malloc(sizeof(struct font_s));
  while ((o=getopt(argc, argv, "F:C:o:"))!=-1) {
    switch (o)
      {
      case 'F':
	if (!(ffile=fopen(optarg,"r"))) {
	  perror(optarg);
	  return -1;
	}
	break;
      case 'C':
	if (!(cfile=fopen(optarg,"r"))) {
	  perror(optarg);
	  return -1;
	}
	break;
      case 'o':
	if (!(ofile=fopen(optarg,"wb"))) {
	  perror(optarg);
	  return -1;
	}
	break;
      default:
	usage(argv[0]);
	return -1;
	break;
      }
  }
  if (!ffile || !cfile || !ofile) {
    usage(argv[0]);
    return -1;
  }
  if (read(fileno(ffile), f->bits, sizeof(f->bits)) == -1) {
    perror("read font file");
    return -1;
  }
  if (read(fileno(cfile), f->charset, sizeof(f->charset)) == -1) {
    perror("read charset file");
    return -1;
  }
  fclose(ffile);
  fclose(cfile);

  for (ascii = 0x20; ascii <= 0x7E; ascii++) {
    int off = findBits(f, ascii);
    fprintf(stderr, "%02X(%c): %d\n", ascii, ascii, off);
  }
  
  fclose(ofile);
  return 0;

}
