#include "stdio.h"
#include "stdlib.h"
#include "iostream"
#include "string"

using namespace std;
const string build = "compile and link: c++ -o rmcr rmcr.cc";
const string usage = "usage: rmcr < oldfile-with-cr > newfile-without-cr";

int main(int argc, char** argv) {
  //clog<<"argc: "<<argc<<endl;
  //clog<<"argv: "<<*argv<<endl;
  if( argc > 1 ) {
    cout<<build<<endl;
    cout<<usage<<endl;
    exit(0);
  }
  int i= 0, c= getchar();
  while( c != EOF ) {
    if( c != 13 ) {
      putchar(c);
      ++i;
    }
    c = getchar();
  }

  clog<<"rmcr> character cnt: "<<--i<<endl;
  return 0;
} 
