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
  fprintf(stderr, "Usage: %s -f <textfile> -w <width> -h <height> [-s <scale>] [-r <aspect ratio>] -F <font file> -C <charset file> -o <output file>\n", progname);
}

typedef struct font_s {
  unsigned char bits[FONTSIZE * FONTGEOMY] ;
  unsigned char charset[FONTSIZE];
} *font;

typedef struct image_s {
  unsigned char *bitmap;
  int width;
  int height;
  int widthBytes;
} *image;

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
  int posy = i->height - scale * FONTGEOMY;
  int bp;
  int cp=0;
  int xscale = ((int)(aspectRatio * (float)scale));
  while (text[cp]) {
    if (posx >= i->width) {
      posx = 0;
      posy -= scale * FONTGEOMY;
    }
    if (posy < 0) break;
    int isCursor = text[cp] == '\\';
    if (text[cp] == '\n') {
      posx = 0;
      posy -= scale * FONTGEOMY;
    } else if (text[cp] == '\r') {
      posx = 0;
    } else if (isCursor || (bp = findBits(f, text[cp])) != -1) {
      int x, y, ix, iy;
      for (y = 0; y < FONTGEOMY; y++)
	for (x = 0; x < FONTGEOMX; x++) {
	  int bitSet = isCursor || getBit(x, y, bp, f);
	  for (iy = 0; iy < scale; iy++)
	    for (ix = 0; ix < xscale; ix++) {
	      if (posx + xscale * x + ix < i->width && posy + scale * y + iy < i->height) {
		unsigned int idx = posx + xscale * x + ix + 8 * i->widthBytes * (i->height - (posy + scale * y + iy) - 1);
		unsigned int byteIdx = idx / 8;
		unsigned int bitIdx = 8 - (idx % 8) - 1;
		i->bitmap[byteIdx] = (i->bitmap[byteIdx] & ~((unsigned char)(1 << bitIdx))) | ((bitSet?0:1) << bitIdx);
	      }
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
  png_bytep *row_pointers;
  
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
	       i->width, i->height,
	       1,
	       PNG_COLOR_TYPE_GRAY,
	       PNG_INTERLACE_NONE,
	       PNG_COMPRESSION_TYPE_DEFAULT,
	       PNG_FILTER_TYPE_DEFAULT
	       );

  png_write_info(png, info);

  row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * i->height);
  for (int y = 0; y < i->height; y++) {
    row_pointers[y] = (png_byte*)malloc(i->widthBytes);
    for (int x = 0; x < i->widthBytes; x++) {
      row_pointers[y][x] = i->bitmap[y * (i->widthBytes) + x];
    }
  }

  png_write_image(png, row_pointers);
  png_write_end(png, NULL);
    
  for(int y = 0; y < i->height; y++) {
    free(row_pointers[y]);
  }
  free(row_pointers);
}

int main(int argc, char *argv[]) {
  int scale=1, o;
  float aRatio = 1;
  static unsigned char inbuf[BUFSIZE];
  FILE *infile=NULL, *ffile=NULL, *cfile=NULL, *ofile = NULL;
  image i = (image)malloc(sizeof(struct image_s));
  i->width = 0;
  i->height = 0;
  font f = (font)malloc(sizeof(struct font_s));
  int textlen;
  while ((o=getopt(argc, argv, "f:w:h:s:r:F:C:o:"))!=-1) {
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
      case 'w':
	i->width = atoi(optarg);
	break;
      case 'h':
	i->height = atoi(optarg);
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
  if (i->width == 0 || i->height == 0 || !infile || !ffile || !cfile || !ofile) {
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
  i->widthBytes = i->width / 8 + ((i->width % 8)?1:0);
  i->bitmap = (unsigned char *)calloc(i->height * i->widthBytes, 1);
  memset(i->bitmap, 0xFF, i->height * i->widthBytes);
  fclose(infile);
  fclose(ffile);
  fclose(cfile);

  pixWrite(inbuf, f, i, scale, aRatio);

  writepng(i, ofile);

  fclose(ofile);
  return 0;

}
