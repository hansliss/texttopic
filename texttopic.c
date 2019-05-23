#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <png.h>
#include <float.h>
#include <math.h>

#define FONTSIZE 512
#define FONTGEOMX 8
#define FONTGEOMY 8
#define BUFSIZE 8192

void usage(char *progname) {
  fprintf(stderr, "Usage: %s -f <textfile> [-s <scale>] [-r <aspect ratio>] -F <font file> -C <charset file> -o <output file> [-S (svg)]\n", progname);
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

typedef struct vecdata_s {
  float x;
  float y;
  float width;
  float height;
  struct vecdata_s *next;
} *vecdata;

void addRect(vecdata *v, float x, float y, float width, float height) {
  if (*v) addRect(&((*v)->next), x, y, width, height);
  else {
    vecdata tmp = (vecdata)malloc(sizeof(struct vecdata_s));
    tmp->next=NULL;
    tmp->x = x;
    tmp->y = y;
    tmp->width = width;
    tmp->height = height;
    *v = tmp;
  }
}

void vecFree(vecdata *v) {
  if (*v) {
    vecFree(&((*v)->next));
    free(*v);
    *v = NULL;
  }
}

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
    int newRows = newMaxY - newMinY + 1;
    i->bitmap = (unsigned char **)realloc(i->bitmap, sizeof(char*) * newRows);
    if (newMinY < i->minY) {
      memmove(&(i->bitmap[i->minY - newMinY]), i->bitmap, sizeof(char*) * (i->maxY - i->minY + 1));
      for (int idx = 0; idx < i->minY - newMinY; idx++) {
	i->bitmap[idx] = (unsigned char *)malloc(i->allocBytesPerRow);
	memset(i->bitmap[idx], 0xFF, i->allocBytesPerRow);
      }
    }
    for (int idx = i->allocRows + (i->minY - newMinY); idx < newRows; idx++) {
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
  for (int idx=i->maxY - i->minY + 1; idx < i->allocRows; i++) {
    free(i->bitmap[idx]);
  }
  i->allocRows = i->maxY - i->minY + 1;
  i->bitmap = (unsigned char **)realloc(i->bitmap, sizeof(unsigned char*) * i->allocRows);
  int width = i->maxX - i->minX + 1;
  unsigned int bytesPerRow = width / 8 + ((width % 8)?1:0);
  for (int idx=0; idx < i->allocRows; idx++) {
    i->bitmap[idx] = (unsigned char *)realloc(i->bitmap[idx], bytesPerRow);
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
  int bp=0;
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

void vecCollect(unsigned char *text, font f, vecdata *v, int scale) {
  int posx = 0;
  int posy = 0;
  int bp=0;
  int cp=0;
  while (text[cp]) {
    int isCursor = text[cp] == '\\';
    if (text[cp] == '\n') {
      posx = 0;
      posy += scale * FONTGEOMY;
    } else if (text[cp] == '\r') {
      posx = 0;
    } else if (isCursor || (bp = findBits(f, text[cp])) != -1) {
      int x, y;
      for (y = 0; y < FONTGEOMY; y++)
	for (x = 0; x < FONTGEOMX; x++)
	  if (isCursor || getBit(x, y, bp, f)) {
	    addRect(v, posx + scale * x, posy - y * scale, scale, scale);
	  }
      posx += scale * FONTGEOMX;
    } else { 
      posx += scale * FONTGEOMX;
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

int simplifyFrom(vecdata this, vecdata *v) {
  if (!(*v)) return 0;
  else if ((*v) != this) {
    if (fabsf((*v)->x - (this->x + this->width)) < 1 &&
	fabsf((*v)->y - this->y) < 1 &&
	fabsf((*v)->height - this->height) < 1) {
      this->width += (*v)->width;
      vecdata tmp = *v;
      *v = (*v)->next;
      free(tmp);
      return 1;
    } else if (fabsf((*v)->y - (this->y + this->height)) < 1 &&
	fabsf((*v)->x - this->x) < 1 &&
	fabsf((*v)->width - this->width) < 1) {
      this->height += (*v)->height;
      vecdata tmp = *v;
      *v = (*v)->next;
      free(tmp);
      return 1;
      } else {
      return simplifyFrom(this, &((*v)->next));
    }
  } else return simplifyFrom(this, &((*v)->next));
  return 0;	
}

void printVecdata(vecdata v) {
  if (v) {
    fprintf(stderr, "v: (%g, %g) (%g, %g)\n", v->x, v->y, v->width, v->height);
    printVecdata(v->next);
  }
}

void simplify(vecdata *v) {
  int changed;
  do {
    changed=0;
    vecdata tmp = *v;
    //    printVecdata(*v);
    while (tmp) {
      if (simplifyFrom(tmp, v)) {
	changed = 1;
      }
      tmp = tmp->next;
    }
  } while (changed);
}

void findExtremes(vecdata v, float *xMin, float *xMax, float *yMin, float *yMax) {
  if (v) {
    findExtremes(v->next, xMin, xMax, yMin, yMax);
    if (v->x < *xMin) *xMin = v->x;
    if (v->x + v->width > *xMax) *xMax = v->x + v->width;
    if (v->y < *yMin) *yMin = v->y;
    if (v->y + v->height > *yMax) *yMax = v->y + v->height;
  } else {
    *xMin = *yMin = FLT_MAX;
    *xMax = *yMax = FLT_MIN;
  }
}

void reCal(vecdata v, float xMin, float yMin, float scaleX, float scaleY) {
  if (v) {
    reCal(v->next, xMin, yMin, scaleX, scaleY);
    v->x = (v->x - xMin) * scaleX;
    v->y = (v->y - yMin) * scaleY;
    v->width *= scaleX;
    v->height *= scaleY;
  }
}

void writeRects(vecdata v, FILE *outfile) {
  if (v) {
    fprintf(outfile, "  <rect x=\"%g\" y=\"%g\" width=\"%g\" height=\"%g\" />\n", v->x, v->y, v->width, v->height);
    writeRects(v->next, outfile);
  }
}
  

void writeSVG(vecdata *v, FILE *outfile, float aspectRatio) {
  float xMin, xMax, yMin, yMax;
  float SVGHeight = 600;
  float scaleX, scaleY;
  simplify(v);
  findExtremes(*v, &xMin, &xMax, &yMin, &yMax);
  float SVGWidth = aspectRatio * ((xMax - xMin) * SVGHeight / (yMax - yMin));
  scaleX = SVGWidth / (xMax - xMin);
  scaleY = SVGHeight / (yMax - yMin);
  reCal(*v, xMin, yMin, scaleX, scaleY);

  fprintf(outfile, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
  fprintf(outfile, "<svg viewBox=\"0 0 %g %g\" xmlns=\"http://www.w3.org/2000/svg\">\n", SVGWidth, SVGHeight);
  writeRects(*v, outfile);
  fprintf(outfile, "</svg>\n");
}

int main(int argc, char *argv[]) {
  int scale=1, o, svg=0;
  float aRatio = 1;
  static unsigned char inbuf[BUFSIZE];
  FILE *infile=NULL, *ffile=NULL, *cfile=NULL, *ofile = NULL;
  font f = (font)malloc(sizeof(struct font_s));
  int textlen;
  while ((o=getopt(argc, argv, "f:s:r:F:C:o:S"))!=-1) {
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
      case 'S':
	svg = 1;
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
  fclose(infile);
  fclose(ffile);
  fclose(cfile);
  
  if (!svg) {
    image i = (image)malloc(sizeof(struct image_s));
    i->minX = 0;
    i->maxX = 0;
    i->minY = 0;
    i->maxY = 0;
    i->allocBytesPerRow = 1;
    i->allocRows = 1;
    i->bitmap = (unsigned char **)malloc(sizeof(unsigned char *));
    i->bitmap[0] = (unsigned char *)malloc(1);
    i->bitmap[0][0] = 0xFF;
    
    pixWrite(inbuf, f, i, scale, aRatio);
    
    writepng(i, ofile);
    for (int idx = 0; idx < i->allocRows; idx++) {
      free(i->bitmap[idx]);
    }
    free(i->bitmap);
    free(i);
  } else {
    vecdata v = NULL;
    vecCollect(inbuf, f, &v, scale);
    writeSVG(&v, ofile, aRatio);
    vecFree(&v);
  }
  fclose(ofile);
  return 0;

}
