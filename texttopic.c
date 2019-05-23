#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <png.h>

#define FONTSIZE 512
#define FONTGEOMX 8
#define FONTGEOMY 8
#define BUFSIZE 8192

void usage(char *progname) {
  fprintf(stderr, "Usage: %s -f <textfile> [-s <scale>] [-r <aspect ratio>] -F <font file> -C <charset file> -o <output file>\n", progname);
}

typedef struct font_s {
  unsigned char bits[FONTSIZE * FONTGEOMY] ;
  unsigned char charset[FONTSIZE];
} *font;

typedef struct image_s {
  unsigned char **bitmap;
  int maxX;
  int minX;
  int maxY;
  int minY;
  int allocRows;
  int allocBytesPerRow;
} *image;

void plot(image i, int x, int y, int white) {
  int newMaxX = i->maxX, newMinX = i->minX, newMaxY = i->maxY, newMinY = i->minY;
  if (x > newMaxX) newMaxX = x;
  if (x < newMinX) newMinX = x;
  if (y > newMaxY) newMaxY = y;
  if (y < newMinY) newMinY = y;
  int width = newMaxX - newMinX + 1;
  unsigned int bytesPerRow = width / 8 + ((width % 8)?1:0);
  if (i->allocBytesPerRow < bytesPerRow) {
    int newWidth = (int)((float)bytesPerRow * 1.5);
    for (int idx=i->minY; idx <= i->maxY; idx++) {
      i->bitmap[idx - i->minY] = realloc(i->bitmap[idx - i->minY], newWidth);
      if (newMinX < i->minX) {
	memmove(&(i->bitmap[idx - i->minY][i->minX - newMinX]), i->bitmap[idx - i->minY], i->maxX - i->minX + 1);
	memset(i->bitmap[idx - i->minY], 0xFF, i->minX - newMinX);
      }
      memset(&(i->bitmap[idx - i->minY][i->allocBytesPerRow + (i->minX - newMinX)]), 0xFF, newWidth - (i->allocBytesPerRow + (i->minX - newMinX)));
    }
    i->allocBytesPerRow = newWidth;
  }
  i->maxX = newMaxX;
  i->minX = newMinX;
  if (newMaxY - newMinY + 1 > i->allocRows) {
    int newRows = (int)((float)(newMaxY - newMinY + 1));
    i->bitmap = (unsigned char **)realloc(i->bitmap, sizeof(char*) * newRows);
    if (newMinY < i->minY) {
      memmove(&(i->bitmap[i->minY - newMinY]), i->bitmap, sizeof(char*) * (i->maxY - i->minY + 1));
      for (int idx = 0; idx < i->minY - newMinY; idx++) {
	i->bitmap[idx] = (unsigned char *)malloc(i->allocBytesPerRow);
	memset(i->bitmap[idx], 0xFF, i->allocBytesPerRow);
      }
    }
    for (int idx = i->allocRows + (i->minX - newMinX); idx < newRows; idx++) {
      i->bitmap[idx] = (unsigned char *)malloc(i->allocBytesPerRow);
      memset(i->bitmap[idx], 0xFF, i->allocBytesPerRow);
    }
    i->allocRows = newRows;
  }
  i->maxY = newMaxY;
  i->minY = newMinY;
  unsigned int byteIdx = (x - i->minX) / 8;
  unsigned int bitIdx = 8 - ((x - i->minX) % 8) - 1;
  i->bitmap[y - i->minY][byteIdx] = (i->bitmap[y - i->minY][byteIdx] & ~((unsigned char)(1 << bitIdx))) | ((white?0:1) << bitIdx);
}

void adjustImage(image i) {
  i->bitmap = (unsigned char **)realloc(i->bitmap, sizeof(unsigned char*) * (i->maxY  - i->minY + 1));
  int width = i->maxX - i->minX + 1;
  unsigned int bytesPerRow = width / 8 + ((width % 8)?1:0);
  for (int idx=i->minY; idx <= i->maxY; idx++) {
    i->bitmap[idx - i->minY] = (unsigned char *)realloc(i->bitmap[idx - i->minY], bytesPerRow);
  }
}

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

int getBit(int x, int y, int fBitsIndex, font f) {
  return f->bits[fBitsIndex + (FONTGEOMY - y - 1)] & (1 << (FONTGEOMX - x - 1));
}

void pixWrite(unsigned char *text, font f, image i, int scale, float aspectRatio) {
  int posx = 0;
  int posy = 0;
  int bp;
  int cp=0;
  int xscale = ((int)(aspectRatio * (float)scale));
  while (text[cp]) {
    int isCursor = text[cp] == '\\';
    if (text[cp] == '\n') {
      posx = 0;
      posy += scale * FONTGEOMY;
    } else if (text[cp] == '\r') {
      posx = 0;
    } else if (isCursor || (bp = findBits(f, text[cp])) != -1) {
      int x, y, ix, iy;
      for (y = 0; y < FONTGEOMY; y++)
	for (x = 0; x < FONTGEOMX; x++) {
	  int bitSet = isCursor || getBit(x, y, bp, f);
	  for (iy = 0; iy < scale; iy++)
	    for (ix = 0; ix < xscale; ix++) {
	      int curX = posx + xscale * x + ix;
	      int curY = posy - scale * y + iy;
	      plot(i, curX, curY, bitSet);
	    }
	}
      posx += xscale * FONTGEOMX;
    } else { 
      posx += xscale * FONTGEOMX;
    }
    cp++;
  }
}

void writepng(image i, FILE *f) {
  png_structp png;
  png_infop info;
  
  if (!(png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL))) {
    abort();
  }

  if (!(info = png_create_info_struct(png))) {
    abort();
  }

  if (setjmp(png_jmpbuf(png))) abort();

  png_init_io(png, f);

  png_set_IHDR(
	       png,
	       info,
	       i->maxX - i->minX + 1, i->maxY - i->minY + 1,
	       1,
	       PNG_COLOR_TYPE_GRAY,
	       PNG_INTERLACE_NONE,
	       PNG_COMPRESSION_TYPE_DEFAULT,
	       PNG_FILTER_TYPE_DEFAULT
	       );

  adjustImage(i);

  png_write_info(png, info);

  png_write_image(png, i->bitmap);
  png_write_end(png, NULL);
    
}

int main(int argc, char *argv[]) {
  int scale=1, o;
  float aRatio = 1;
  static unsigned char inbuf[BUFSIZE];
  FILE *infile=NULL, *ffile=NULL, *cfile=NULL, *ofile = NULL;
  image i = (image)malloc(sizeof(struct image_s));
  i->minX = 0;
  i->maxX = 0;
  i->minY = 0;
  i->maxY = 0;
  font f = (font)malloc(sizeof(struct font_s));
  int textlen;
  while ((o=getopt(argc, argv, "f:s:r:F:C:o:"))!=-1) {
    switch (o)
      {
      case 'f':
	if (!(infile=fopen(optarg,"r"))) {
	  perror(optarg);
	  return -1;
	}
	break;
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
      case 's':
	scale = atoi(optarg);
	break;
      case 'r':
	aRatio = atof(optarg);
	break;
      default:
	usage(argv[0]);
	return -1;
	break;
      }
  }
  if (!infile || !ffile || !cfile || !ofile) {
    usage(argv[0]);
    return -1;
  }
  textlen = read(fileno(infile), inbuf, BUFSIZE);
  inbuf[textlen] = '\0';
  if (read(fileno(ffile), f->bits, sizeof(f->bits)) == -1) {
    perror("read font file");
    return -1;
  }
  if (read(fileno(cfile), f->charset, sizeof(f->charset)) == -1) {
    perror("read charset file");
    return -1;
  }
  i->allocBytesPerRow = 1;
  i->allocRows = 1;
  i->bitmap = (unsigned char **)malloc(sizeof(unsigned char *));
  i->bitmap[0] = (unsigned char *)malloc(1);
  i->bitmap[0][0] = 0xFF;
  fclose(infile);
  fclose(ffile);
  fclose(cfile);

  pixWrite(inbuf, f, i, scale, aRatio);

  writepng(i, ofile);

  fclose(ofile);
  return 0;

}
