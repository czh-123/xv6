#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  // int i;

  if(argc != 2){
    fprintf(2, "sleep args error ...\n");
    exit(1);
  }

  if (sleep(atoi(argv[1])) < 0) {
      fprintf(2, "sleep error return < 0");
  }

  exit(0);
}
