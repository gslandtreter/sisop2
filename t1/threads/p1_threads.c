#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUFFER_SIZE 5 * 1024 * 1024

struct structMatrix {

    int numLinhas;
    int numColunas;

    int **dados;
};

typedef struct structMatrix Matrix;

struct task {
    int linha;
    int coluna;

    Matrix * matriz1;
    Matrix * matriz2;

    int * posResultado;
};

typedef struct task MultiplyTask;


struct thTask {
    int threadId;
    int numTasks;
    MultiplyTask * tasks;
};

typedef struct thTask ThreadTask;

Matrix * createMatrix(int numLinhas, int numColunas) {
    Matrix * novaMatrix = (Matrix *) malloc(sizeof(Matrix));

    novaMatrix->numLinhas = numLinhas;
    novaMatrix->numColunas = numColunas;

    //Aloca espaco para dados
    novaMatrix->dados = (int**) malloc(numLinhas * sizeof(int *));

    int i, j;
    for (i = 0; i < numLinhas; i++) {
        novaMatrix->dados[i] = (int*) malloc (numColunas * sizeof(int));
        for (j = 0; j < numColunas; j++) {
            novaMatrix->dados[i][j] = -1;
        }
    }

    return novaMatrix;
}

Matrix * generateRandomMatrix(int numLinhas, int numColunas, int maxVal) {
    //Inicializa semente
    time_t t;
    srand((unsigned) time(&t));

    Matrix * novaMatrix = (Matrix *) malloc(sizeof(Matrix));

    novaMatrix->numLinhas = numLinhas;
    novaMatrix->numColunas = numColunas;

    //Aloca espaco para dados
    novaMatrix->dados = (int**) malloc(numLinhas * sizeof(int *));

    int i, j;
    for (i = 0; i < numLinhas; i++) {
        novaMatrix->dados[i] = (int*) malloc (numColunas * sizeof(int));
        for (j = 0; j < numColunas; j++) {
            novaMatrix->dados[i][j] = rand() % maxVal;
        }
    }

    return novaMatrix;
}

Matrix * destroyMatrix(Matrix * matrix) {
    if(!matrix)
        return NULL;

    int i;
    for (i = 0; i < matrix->numLinhas; i++) {
        free(matrix->dados[i]);
    }

    free(matrix->dados);
    free(matrix);

    return NULL;
}

void printMatrixInfo (Matrix * matrix) {
    if(!matrix)
        return;

     printf("Numero de Linhas: %d\nNumero de Colunas: %d\n", matrix->numLinhas, matrix->numColunas);
 }

void printMatrix(Matrix * matrix) {
    if(!matrix)
        return;

    printMatrixInfo(matrix);

    int i, j;

    for (i = 0; i < matrix->numLinhas; i++) {
        for (j = 0; j < matrix->numColunas; j++) {
            printf("%d ", matrix->dados[i][j]);
        }
        printf("\n");
    }
}

Matrix * createFromFile(const char * fileName) {

    char buffer[BUFFER_SIZE];

    FILE* pFile = fopen(fileName, "r");

    if(!pFile)
        return NULL;

    char* token;
    char* pointerToBufferCopy;

    //Le primeira linha
    fgets(buffer, BUFFER_SIZE, pFile);
    pointerToBufferCopy = strdup(buffer);

    token = strsep(&pointerToBufferCopy, "=");
    token = strsep(&pointerToBufferCopy, "=");

    int numLinhas = atoi(token);

     //Le segunda linha
    fgets(buffer, BUFFER_SIZE, pFile);
    pointerToBufferCopy = strdup(buffer);

    token = strsep(&pointerToBufferCopy, "=");
    token = strsep(&pointerToBufferCopy, "=");

    int numColunas = atoi(token);

    //Cria a matriz

    Matrix * novaMatrix = createMatrix(numLinhas, numColunas);
    if(!novaMatrix)
        return NULL;

    //Le o resto da matriz
    int i, j;
    for(i = 0; i < numLinhas; i++) {
        fgets(buffer, BUFFER_SIZE, pFile);
        pointerToBufferCopy = strdup(buffer);

        for(j = 0; j < numColunas; j++) {
            token = strsep(&pointerToBufferCopy, " ");
            novaMatrix->dados[i][j] = atoi(token);
        }

    }

    fclose(pFile);

    return novaMatrix;
}

int writeMatrixToFile (Matrix * matriz, const char * fileName) {
    if(!matriz)
        return 1;

    FILE * filePointer = fopen(fileName, "w");

    if(!filePointer)
        return 1;

    fprintf(filePointer, "LINHAS = %d\r\n", matriz->numLinhas);
    fprintf(filePointer, "COLUNAS = %d\r\n", matriz->numColunas);

    int i, j;

    for (i = 0; i < matriz->numLinhas; i++) {
        for (j = 0; j < matriz->numColunas; j++) {
            fprintf(filePointer, "%d ", matriz->dados[i][j]);
        }
        fprintf(filePointer, "\r\n");
    }

    fclose(filePointer);
    return 0;
}

void multiplyWithPointer (Matrix * matriz1, Matrix * matriz2, int linha, int coluna, int * resultPointer) {

    *resultPointer = 0;
    int i;

    if(!matriz1 || !matriz2)
        return;

    for(i = 0; i < matriz1->numColunas; i++) {
        *resultPointer += (matriz1->dados[linha][i] * matriz2->dados[i][coluna]);
    }
}

int multiply (Matrix * matriz1, Matrix * matriz2, int linha, int coluna) {

    int result = 0;
    int i;

    if(!matriz1 || !matriz2)
        return 0;

    for(i = 0; i < matriz1->numColunas; i++) {
        result += (matriz1->dados[linha][i] * matriz2->dados[i][coluna]);
    }

    return result;
}

void initializeTasks (ThreadTask * tasks, int tasksLength, int maxTasksPerThread) {
    int i, j;
    for(i = 0; i < tasksLength; i++) {
        tasks[i].tasks = (MultiplyTask *) malloc(sizeof(MultiplyTask) * maxTasksPerThread);
        tasks[i].numTasks = 0;
        tasks[i].threadId = i;

        for(j = 0; j < maxTasksPerThread; j++) {
            tasks[i].tasks[j].linha = tasks[i].tasks[j].coluna = -1;
        }
    }
}

ThreadTask * destroyTasks (ThreadTask * tasks) {
    if(!tasks)
        return NULL;

    int i;
    for (i = 0; i < tasks->numTasks; i++) {
        free(tasks->tasks);
    }

    free(tasks);
}

void * threadWork (void * arg) {

    ThreadTask *myTasks = (ThreadTask *) arg;

    //printf("Thread realizando %d tarefas!\n", myTasks->numTasks);

    int i;

    for(i = 0; i < myTasks->numTasks; i++) {
        //Realiza multiplicacao
        //printf("Thread [%d] -> Multiplicando linha [%d], coluna [%d]\n", myTasks->threadId, myTasks->tasks[i].linha, myTasks->tasks[i].coluna);
        multiplyWithPointer(myTasks->tasks[i].matriz1, myTasks->tasks[i].matriz2, myTasks->tasks[i].linha, myTasks->tasks[i].coluna, myTasks->tasks[i].posResultado);
    }

}

int main (int argc, char **argv) {
    pthread_t * threads;


    /*Matrix * matrix100 = generateRandomMatrix(1000, 1000, 100);
    Matrix * matrix101 = generateRandomMatrix(1000, 1000, 100);

    writeMatrixToFile(matrix100, "matrizgrande1.txt");
    writeMatrixToFile(matrix101, "matrizgrande2.txt");*/

    //Numero default de threads;
    int numThreads = 1;

    //Parsing de parametros
    int c;
    while ((c = getopt (argc, argv, "n:")) != -1) {
        switch (c)
        {
            case 'n':
                numThreads = atoi(optarg);
                break;
        }
    }

    if(numThreads < 1) {
        printf("Numero de threads invalido!");
    }

    printf("Usando %d threads!\n", numThreads);

    threads = (pthread_t *) malloc(sizeof(pthread_t) * numThreads);

    Matrix * matriz1 = createFromFile("in1.txt");
    Matrix * matriz2 = createFromFile("in2.txt");

    if(!matriz1 || !matriz2)
        return 1;

    printf("Matriz 1 carregada:\n");
    printMatrixInfo(matriz1);

    printf("Matriz 2 carregada:\n");
    printMatrixInfo(matriz2);


    if(matriz1->numColunas != matriz2->numLinhas) {
        printf("As matrizes nao sao multiplicaveis! ERRROU!\n");
        return 1;
    }

    //Cria matriz resultado
    Matrix * resultado = createMatrix(matriz1->numLinhas, matriz2->numColunas);

    int i,j;

    int totalTaskNumber = matriz1->numLinhas * matriz2->numColunas;
    int maxTasksPerThread = (totalTaskNumber / numThreads);

    if(totalTaskNumber % numThreads != 0) {
        maxTasksPerThread++;
    }

    ThreadTask * tasks = (ThreadTask *) malloc(sizeof(ThreadTask) * numThreads);
    initializeTasks(tasks, numThreads, maxTasksPerThread);

    printf("Total tasks: %d, maxPerThread: %d!\n", totalTaskNumber, maxTasksPerThread);

    int threadAssigned = 0;
    for(i = 0; i < matriz1->numLinhas; i++) {
        for(j = 0; j < matriz2->numColunas; j++) {

            //Passa task para thread
            int taskIndex = tasks[threadAssigned].numTasks;

            tasks[threadAssigned].tasks[taskIndex].linha = i;
            tasks[threadAssigned].tasks[taskIndex].coluna = j;
            tasks[threadAssigned].tasks[taskIndex].matriz1 = matriz1;
            tasks[threadAssigned].tasks[taskIndex].matriz2 = matriz2;
            tasks[threadAssigned].tasks[taskIndex].posResultado =  &resultado->dados[i][j];

            tasks[threadAssigned].numTasks++;

            //printf("Designando linha %d e coluna %d para Thread %d, tarefa %d\n", i, j, threadAssigned, taskIndex);

            //Passa tarefa para proxima thread
            threadAssigned = (threadAssigned + 1) % numThreads;
        }
    }

    printf("Criando threads...\n");
    //Cria threads
    for(i = 0; i < numThreads; i++) {
        pthread_create(&threads[i], NULL, threadWork, (void *) &tasks[i]);
    }

    printf("Threads criadas! Aguardando termino das threads...\n");
    //Aguarda termino das threads
    for(i = 0; i < numThreads; i++) {
        pthread_join(threads[i], NULL);
    }

    free(tasks);
    free(threads);

    writeMatrixToFile(resultado, "out.txt");

    printf("Resultado:\n");
    printMatrixInfo(resultado);

    destroyMatrix(matriz1);
    destroyMatrix(matriz2);
	destroyMatrix(resultado);

	printf("Matriz resultante calculada!\n");

    return 0;
}