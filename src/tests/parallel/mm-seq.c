#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include "lib/random.c"
#include "tests/lib.h"
#include "tests/main.h"

#define MAX 8

int matA[MAX][MAX];
int matB[MAX][MAX];
int result[MAX][MAX];

void
test_main(void)
{
    for (int i = 0; i < MAX; i++) {
        for (int j = 0; j < MAX; j++) {
            matA[i][j] = random_ulong() % 10;
            matB[i][j] = random_ulong() % 10;
        }
    }

    for (int i = 0; i < MAX; i++) {
        for (int j = 0; j < MAX; j++) {
            int sum = 0;
            for (int k = 0; k < MAX; k++) {
                sum += matA[i][k] * matB[k][j];
            }
            result[i][j] = sum;
        }
    }

    printf("Matrix A:\n");
    for (int i = 0; i < MAX; i++) {
        for (int j = 0; j < MAX; j++) {
            printf("%d ", matA[i][j]);
        }
        printf("\n");
    }

    printf("Matrix B:\n");
    for (int i = 0; i < MAX; i++) {
        for (int j = 0; j < MAX; j++) {
            printf("%d ", matB[i][j]);
        }
        printf("\n");
    }

    printf("Result Matrix:\n");
    for (int i = 0; i < MAX; i++) {
        for (int j = 0; j < MAX; j++) {
            printf("%d ", result[i][j]);
        }
        printf("\n");
    }
}