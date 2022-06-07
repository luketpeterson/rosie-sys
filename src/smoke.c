#include <librosie.h>

int main() {

  //Resolve the symbol address of rosie_match2, since that's new in librosie 1.3.0
  if ((void*)rosie_match2 != 0) {
    return 0;
  } else {
    return -1;
  }
}

