#include <stdio.h>

int add(int x, int y) {
   return x + y;
}

int mult(int x, int y) {
   int result = 0;	   
   for (int i = 0; i < y; i++) {
	result = add(x, result);
   }
   return result;
}

int pows(int x, int y) {
   int result = 1;
   for (int i = 0; i < y; i++) {
     result = mult(result, x);
   }
   return result;
}


int main() { 
   printf("%d+%d: %d\n", 4, 2, add(4, 2));
   printf("%d*%d: %d\n", 4, 8, mult(4, 8));
   printf("%d^%d: %d\n", 3, 4, pows(3, 4)); 
}