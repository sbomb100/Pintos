#include <string.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include "lib/random.c"
#include "tests/lib.h"
#include "tests/main.h"

#define MAX 5

intptr_t matA[MAX][MAX];
intptr_t matB[MAX][MAX];
intptr_t result[MAX][MAX];

static void 
mult(void * arg) 
{
    int * data = (int *)arg;

    int row = data[0];
    int col = data[1];
    int sum = 0;

    for (int k = 0; k < MAX; k++) {
        sum += matA[row][k] * matB[k][col];
    }

    result[row][col] = sum;
}

void
test_main(void)
{
    for (int i = 0; i < MAX; i++) {
        for (int j = 0; j < MAX; j++) {
            matA[i][j] = random_ulong() % 10;
            matB[i][j] = random_ulong() % 10;
        }
    }

    tid_t t[MAX][MAX];
    int thread_args[MAX][MAX][2];

    for (int i = 0; i < MAX; i++) {
        for (int j = 0; j < MAX; j++) {
            thread_args[i][j][0] = i;
            thread_args[i][j][1] = j;

            t[i][j] = pthread_create(mult, (void *) &thread_args[i][j]);
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

    for (int i = 0; i < MAX; i++) {
        for (int j = 0; j < MAX; j++) {
            pthread_join(t[i][j]);
        }
    }

    printf("Result Matrix:\n");
    for (int i = 0; i < MAX; i++) {
        for (int j = 0; j < MAX; j++) {
            printf("%d ", result[i][j]);
        }
        printf("\n");
    }
}