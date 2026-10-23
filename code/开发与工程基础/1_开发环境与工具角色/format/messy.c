#include <stdio.h>
int sum(const int *a,int n){
int s=0;
  for(int i=0;i<n;i++)
  {
        s+=a[i];
  }
return s;}
int main(void)
{
    int v[]={3,1,4,1,5};
    if (sum(v,5)>10) printf("big\n");
    return 0;
}
