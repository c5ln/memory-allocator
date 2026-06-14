#include <stdio.h>
#include <unistd.h>

int main(){
    void *current_break = sbrk(0);
    printf("program break : %p\n", current_break);    
    return 0;
}