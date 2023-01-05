#include "common.h"

#include <unistd.h>

int main(){
    printf("%s\n", get_process_name(getpid()).c_str());
}
