#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <complex>
#include <complex.h>

#include "sobel.h"

using namespace std;

//#define MYTEST
#ifdef MYTEST

int main () {
	  short int src[HEIGHT*WIDTH] = {1};
	  short int dst[(HEIGHT-2)*(WIDTH-2)] = {1};

	  sobel(src, dst, HEIGHT, WIDTH);

	  return 0;
}

#else

int main () {
  
  static short int src[HEIGHT*WIDTH];
  static short int dst[(HEIGHT-2)*(WIDTH-2)];

  ifstream file("data.txt");
  string line;
  short int s1;
  int i=0;
  
  while(getline(file,line) && (i < (HEIGHT*WIDTH)))
  {
    istringstream sin(line);
    sin >> s1;
    src[i] = (short int)s1;
    i = i + 1;
  }

  sobel(src, dst, HEIGHT, WIDTH);

  int tf = 0;
  
  ifstream fileo("dst.txt");
  string lineo;
  short int s2;
  int j=0;
  
  while(getline(fileo,lineo) && (j < (HEIGHT-2)*(WIDTH-2)))
  {
    istringstream sino(lineo);
    sino >> s2;
    if (dst[j] != s2)
    {
      tf = j;
      break;
    }
    j = j + 1;
  }

  if (tf != 0)
  {
	fprintf(stdout, "%d\n", tf);
    fprintf(stdout, "*******************************************\n");
    fprintf(stdout, "FAIL: Output DOES NOT match the golden output\n");
    fprintf(stdout, "*******************************************\n");
    return 1;
  } 
  else 
  {
    fprintf(stdout, "*******************************************\n");
    fprintf(stdout, "PASS: The output matches the golden output!\n");
    fprintf(stdout, "*******************************************\n");
    return 0;
  }

}

#endif

