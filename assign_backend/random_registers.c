#include <stdio.h> 
#include <stdlib.h>
#include <math.h>

// This program finds the solution of a polynomial of degree 2
int main(void) {
    float InputA, InputB, InputC;
    printf("%s", "Input: ");
    scanf("%f %f %f", &InputA, &InputB, &InputC);

    float d = sqrt((InputB * InputB) - (4 * InputA * InputC));

    if (d < 0) {
        puts("Discrimantor less than 0, no solution");
        exit(1);
    }

    if (d == 0) {
        float Solution = - InputB / (2 * InputA);
        printf("Solution: %f\n", Solution);
        exit(0);
    }

    float SolutionA = (- InputB + d) / (2 * InputA);
    float SolutionB = (- InputB - d) / (2 * InputA);
    
    printf("Solutions: %f %f\n", SolutionA, SolutionB);
}