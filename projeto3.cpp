/*
Nome: Bruno Hideki Amadeu Ogata
RA: 140884
Programação Concorrente e Distribuída
Atividade 3
*/

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string.h>
#include <mpi.h>


int Soma_serial(int **geracao, int linhas, int colunas) {
    int cont = 0, i, j;
    for (i = 0; i <= linhas; i++) {
        for (j = 0; j < colunas; j++) {
            cont = cont + geracao[i][j];
        }
    }
    return cont;
}



int getNeighbors(int** geracao, int linhas, int colunas, int x, int y) {
    int CIMA = (y - 1 + colunas) % colunas;
    int BAIXO = (y + 1) % colunas;
    int ESQUERDA = (x - 1 + colunas) % colunas;
    int DIREITA = (x + 1) % colunas;

    return  geracao[ESQUERDA][CIMA] + geracao[x][CIMA] + geracao[DIREITA][CIMA] +
        geracao[ESQUERDA][y] + 0 + geracao[DIREITA][y] +
        geracao[ESQUERDA][BAIXO] + geracao[x][BAIXO] + geracao[DIREITA][BAIXO];
}



int main(int argc, char* argv[]) {
    int N = 2048;
    int NUM_GENERATIONS = 2000;

    int NUM_PROCESSES;
    int PID;

    double start;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &NUM_PROCESSES);
    MPI_Comm_rank(MPI_COMM_WORLD, &PID);

    MPI_Barrier(MPI_COMM_WORLD);
    start = MPI_Wtime();


    int BLOCK_SIZE = (N / NUM_PROCESSES) + (N % NUM_PROCESSES > PID);

    MPI_Barrier(MPI_COMM_WORLD);

    // first and last rows are for messaging
    int** geracao = (int**)malloc((1 + BLOCK_SIZE + 1) * sizeof(int*));
    int** prox = (int**)malloc((1 + BLOCK_SIZE + 1) * sizeof(int*));

    int i, j;
    for (i = 0; i < (1 + BLOCK_SIZE + 1); i++) {
        geracao[i] = (int*)malloc(N * sizeof(int));
        prox[i] = (int*)malloc(N * sizeof(int));
    }

    //inicializa tabuleiro
    int i0 = PID * BLOCK_SIZE;
    int iN = i0 + BLOCK_SIZE;
    for (i = 0; i < N; i++) {
        int local_i = (i % BLOCK_SIZE) + 1;

        for (j = 0; j < N; j++) {
            if (i0 <= i && i < iN) {
                geracao[local_i][j] = 0;
            }
            else {
                geracao[local_i][j] = 0;
            }
        }
    }
    if (PID == 0) {
        for (i = 0; i < N; i++) {
            for (j = 0; j < N; j++) {
                int lin = 1, col = 1;
                geracao[lin][col + 1] = 1;
                geracao[lin + 1][col + 2] = 1;
                geracao[lin + 2][col] = 1;
                geracao[lin + 2][col + 1] = 1;
                geracao[lin + 2][col + 2] = 1;

                lin = 10; col = 30;
                geracao[lin][col + 1] = 1;
                geracao[lin][col + 2] = 1;
                geracao[lin + 1][col] = 1;
                geracao[lin + 1][col + 1] = 1;
                geracao[lin + 2][col + 1] = 1;
            }
        }
    }


    MPI_Request REQ;
    int k;
    for (k = 0; k < NUM_GENERATIONS; k++) {
        MPI_Barrier(MPI_COMM_WORLD);
        double start = MPI_Wtime();

        int PROC_ANT = (PID - 1 + NUM_PROCESSES) % NUM_PROCESSES; // processo anterior
        int PROX_PROC = (PID + 1) % NUM_PROCESSES; // proximo processo

        MPI_Isend(geracao[1], N, MPI_INT, PROC_ANT, 10, MPI_COMM_WORLD, &REQ); // envia linha 1 para o anterior
        MPI_Isend(geracao[BLOCK_SIZE], N, MPI_INT, PROX_PROC, 10, MPI_COMM_WORLD, &REQ); // envia linha M para o proximo

        MPI_Recv(geracao[BLOCK_SIZE + 1], N, MPI_INT, PROX_PROC, 10, MPI_COMM_WORLD, MPI_STATUS_IGNORE); // recebe linha 1 do proximo e coloca em M+1
        MPI_Recv(geracao[0], N, MPI_INT, PROC_ANT, 10, MPI_COMM_WORLD, MPI_STATUS_IGNORE); // recebe linha M do anterior e coloca em 0

        // calculate next generation
        int i, j;
        for (i = 1; i <= BLOCK_SIZE; i++) {
            for (j = 0; j <= N; j++) {
                // cant move code below, its considerably faster executing here
                // than creating another function and calling it
                int neighborhood = getNeighbors(geracao, BLOCK_SIZE, N, i, j);

                int controle = geracao[i][j];
                if (controle == 1) { // vivo
                    if (neighborhood < 2) controle = 0; // morte por abandono
                    else if (neighborhood >= 4) controle = 0; // morte por superpopulacao
                }
                else { // morto
                    if (neighborhood == 3) controle = 1; // revive
                }

                prox[i][j] = controle;
            }
        }

        // avança geracao
        int** tmp = geracao;
        geracao = prox;
        prox = tmp;

        // contagem parcial do bloco
        int localCount = Soma_serial(geracao, BLOCK_SIZE, N);

        // aguarda todos os processos
        MPI_Barrier(MPI_COMM_WORLD);


        if (PID == 0) {
            int generationCount = localCount;
            int externalCount;

            // soma contadores de outros processos
            int p;
            for (p = 1; p < NUM_PROCESSES; p++) {
                MPI_Recv(&externalCount, 1, MPI_INT, p, 10, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                generationCount += externalCount;
            }

            printf("-/%04d           %d        %.02fms\n", k + 1, generationCount, (MPI_Wtime() - start) * 1000.0);
        }
        else {
            MPI_Isend(&localCount, 1, MPI_INT, 0, 10, MPI_COMM_WORLD, &REQ);
        }
    }


    // contagem parcial do bloco
    int count = Soma_serial(geracao, BLOCK_SIZE, N);

    if (PID == 0) {
        int externalCount;

        // soma contadore de outros processos
        int p;
        for (p = 1; p < NUM_PROCESSES; p++) {
            MPI_Recv(&externalCount, 1, MPI_INT, p, 10, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            count += externalCount;
        }

        printf("\ncont (%04d)     %d        %.02fms\n", NUM_GENERATIONS, count, (MPI_Wtime() - start) * 1000.0);

    }
    else {
        MPI_Isend(&count, 1, MPI_INT, 0, 10, MPI_COMM_WORLD, &REQ);
    }


    MPI_Finalize();


    return 0;
}