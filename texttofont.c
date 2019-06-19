#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <float.h>
#include <math.h>

#define FONTSIZE 512
#define FONTGEOMX 8
#define FONTGEOMY 8
#define BUFSIZE 8192

void usage(char *progname) {
  fprintf(stderr, "Usage: %s -F <font file> -C <charset file> -o <output file>\n", progname);
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
