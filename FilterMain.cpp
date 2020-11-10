#include <stdio.h>
#include "cs1300bmp.h"
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <cstring>


#include "Filter.h"
#include "rdtsc.h"
using namespace std;

#define USE_SSE
#define USE_DIV


Filter *readFilter(string filename);
double applyFilter(Filter *filter, cs1300bmp *input, cs1300bmp *output);

int main(int argc, char **argv)
{

  if ( argc < 2) {
    fprintf(stderr,"Usage: %s filter inputfile1 inputfile2 .... \n", argv[0]);
  }

  //
  // Convert to C++ strings to simplify manipulation
  //
  string filtername = argv[1];

  //
  // remove any ".filter" in the filtername
  //
  string filterOutputName = filtername;
  string::size_type loc = filterOutputName.find(".filter");
  if (loc != string::npos) {
    //
    // Remove the ".filter" name, which should occur on all the provided filters
    //
    filterOutputName = filtername.substr(0, loc);
  }

  Filter *filter = readFilter(filtername);

  double sum = 0.0;
  int samples = 0;

  for (int inNum = 2; inNum < argc; inNum++) {
    string inputFilename = argv[inNum];
    string outputFilename = "filtered-" + filterOutputName + "-" + inputFilename;
    struct cs1300bmp *input = new struct cs1300bmp;
    struct cs1300bmp *output = new struct cs1300bmp;
    int ok = cs1300bmp_readfile( (char *) inputFilename.c_str(), input);

    if ( ok ) {
      double sample = applyFilter(filter, input, output);
      sum += sample;
      samples++;
      cs1300bmp_writefile((char *) outputFilename.c_str(), output);
    }
    delete input;
    delete output;
  }
  fprintf(stdout, "Average cycles per sample is %f\n", sum / samples);

}

class Filter *
readFilter(string filename)
{
  ifstream input(filename.c_str());

  if ( ! input.bad() ) {
    int size = 0;
    input >> size;
    Filter *filter = new Filter(size);
    int div;
    input >> div;
    filter -> setDivisor(div);
    for (int i=0; i < size; i++) {
      for (int j=0; j < size; j++) {
	int value;
	input >> value;
	filter -> set(i,j,value);
      }
    }
    return filter;
  } else {
    cerr << "Bad input in readFilter:" << filename << endl;
    exit(-1);
  }
}


double
applyFilter(class Filter *filter, cs1300bmp *input, cs1300bmp *output)
{

  long long cycStart, cycStop;

  cycStart = rdtscll();
    //create values for frequent pointer values to decrease time by taking pointer calls out of each loop
  int filterSize=filter->getSize();
  int Fdivisor=filter->getDivisor();
  int input_width = input -> width;
  int input_height = input -> height;
  
  output->width= input_width;
  output->height=input_height;
  //unrolled loop 4 times to get 4 more values per iteration to optimize processing speed by 4. Tried using SSE and SSE2 however was a bit confused on the operations, need more time to learn. I used SIMD as an inspiration for my design, pushing 4 through each loop.
      for(int plane=0; plane<3;plane++){
       for(int row = 1; row < (input_height) - 4; row+=4) {

         for(int col = 1; col < (input_width) - 4 ; col+=4) {
             
    //4 base values for the four values being changed, will be used to decrease amount of pointer calls and constant change of ouput color value
    int v=0;
    int v1=0;
    int v2=0;
    int v3=0;
    


	for (int j = 0; j < filterSize-3; j+=4) {
	  for (int i = 0; i < filterSize-3; i+=4) {	
	    v+=input -> color[plane][row + i - 1][col + j - 1] * filter -> get(i, j);
        v1+=input ->color[plane][row + i][col + j] * filter -> get(i+1,j+1); 
        v2+=input ->color[plane][row + i + 1][col + j + 1] * filter -> get(i+2,j+2); 
        v3+=input ->color[plane][row + i + 2][col + j + 2] * filter -> get(i+3,j+4); 
        
          
     
	  }
	}
	//here I am using the if statements to assure each value is between 0-255 then implement values to color array. 'else if' will also help speed
	v=v/Fdivisor;

	if (v<0){
        v=0;
    }
    else if(v>255){
        v=255;
    }
    output->color[plane][row][col]=v;
             
    v1=v1/Fdivisor;
    if(v1<0){
        v1=0;
    }
    else if(v1>255){
        v1=255;
    }
     output->color[plane][row+1][col+1]=v1;        
     
     v2=v2/Fdivisor;
    if(v2<0){
        v2=0;
    }
    else if(v2>255){
        v2=255;
    }
     output->color[plane][row+2][col+2]=v2; 
      
     v3=v3/Fdivisor;
    if(v3<0){
        v3=0;
    }
    else if(v3>255){
        v3=255;
    }
     output->color[plane][row+3][col+3]=v3; 
             
             
         }
      }
    }


  cycStop = rdtscll();
  double diff = cycStop - cycStart;
  double diffPerPixel = diff / (output -> width * output -> height);
  fprintf(stderr, "Took %f cycles to process, or %f cycles per pixel\n",
	  diff, diff / (output -> width * output -> height));
  return diffPerPixel;
}

