#include<stdio.h>

#define MAX_CLIENT 120
int random_key[MAX_CLIENT];

int main(){

FILE *fp;
int i,counter=0;
double j;  
    
fp = fopen("throughput.txt", "r"); 
    if (fp == NULL) {
         printf("I couldn't open the file for reading.\n");
         exit(0);
      }

int sum=0;
  while (fscanf(fp, "%d,", &i) == 1){  
		  sum += i;		
          random_key[counter] = i;		  
		  counter++;	  
  }
fclose(fp);
int k;
int  totops = 0, minops = 1 << 30, maxops = 0;		 
for (k=0; k<counter;k++)
{
        if (random_key[k] > maxops) maxops = random_key[k];
        if (random_key[k] < minops) minops = random_key[k];		
}
			double seconds = 25;
			printf(" num cli = %d, Throughput = %f, latency = %f, fairness=%f maxops=%f  \n",counter,  sum/seconds , (double) (seconds / sum )* 1000000 * counter  , (double)maxops/minops, (double) maxops ); 
			fflush(stdout);	
			system("rm -r throughput.txt");
	return 0;
}
