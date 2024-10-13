#include <stdio.h>
#include <time.h>
#include <stdint.h>
 
int main(void)
{
    time_t t;
    time_t result = time(&t);
    if(result != (time_t)(-1)){
        printf("The current time is %s(%jd seconds since the Epoch)\n",
               asctime(gmtime(&result)), (intmax_t)result);
        printf("The current time is %s(%jd seconds since the Epoch)\n",
               asctime(gmtime(&t)), (intmax_t)t);
    }
}