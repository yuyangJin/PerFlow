#include <iostream>

#define N 100
#define M 1000

double A[N][M] = {0};
double B[N][M] = {0};

void foo() {
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < M; j++){
            A[i][j] += i + j;
        }
    }
}

void bar() {
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < M; j++){
            B[i][j] += A[i][j] * j;
        }
    }
}


void print(double X[N][M]) {
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++){
            std::cout << X[i][j] << " ";
        }
    }
    std::cout << std::endl;
}

int main() {
    foo();
    for (int i = 0; i < N; i++) {
        bar();
    }
    print(B);    
}