#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>

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

    int posResultado;
};

typedef struct task MultiplyTask;


struct thTask {
    int threadId;
    int numTasks;
    MultiplyTask * tasks;
};

typedef struct thTask ThreadTask;

int sharedSegmentId = 0;

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

void multiplyWithSharedSegment (Matrix * matriz1, Matrix * matriz2, int linha, int coluna, int resultIndex) {

    int result = multiply (matriz1, matriz2, linha, coluna);

    int * shared_memory = (int *) shmat(sharedSegmentId, NULL, 0);
    shared_memory[resultIndex] = result;
    shmdt(shared_memory);
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

void threadWork (ThreadTask * tasks) {

    ThreadTask *myTasks = tasks;

    //printf("Processo realizando %d tarefas!\n", myTasks->numTasks);

    int i;

    for(i = 0; i < myTasks->numTasks; i++) {
        //Realiza multiplicacao
        //printf("Thread [%d] -> Multiplicando linha [%d], coluna [%d]\n", myTasks->threadId, myTasks->tasks[i].linha, myTasks->tasks[i].coluna);
        multiplyWithSharedSegment(myTasks->tasks[i].matriz1, myTasks->tasks[i].matriz2, myTasks->tasks[i].linha, myTasks->tasks[i].coluna, myTasks->tasks[i].posResultado);

    }

}

int main (int argc, char **argv) {
    pid_t * pids;


    /*
    //Gera matriz aleatoria

    Matrix * matrix100 = generateRandomMatrix(12, 6, 100);
    Matrix * matrix101 = generateRandomMatrix(6, 9, 100);

    writeMatrixToFile(matrix100, "matrizgrande11.txt");
    writeMatrixToFile(matrix101, "matrizgrande21.txt");

    return(0);
    */

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
        printf("Numero de processos invalido!");
    }

    printf("Usando %d processos!\n", numThreads);

    pids = (pid_t *) malloc(sizeof(pid_t) * numThreads);

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

    printf("Total tasks: %d, maxPerProcess: %d!\n", totalTaskNumber, maxTasksPerThread);

    int threadAssigned = 0;
    for(i = 0; i < matriz1->numLinhas; i++) {
        for(j = 0; j < matriz2->numColunas; j++) {

            //Passa task para thread
            int taskIndex = tasks[threadAssigned].numTasks;

            tasks[threadAssigned].tasks[taskIndex].linha = i;
            tasks[threadAssigned].tasks[taskIndex].coluna = j;
            tasks[threadAssigned].tasks[taskIndex].matriz1 = matriz1;
            tasks[threadAssigned].tasks[taskIndex].matriz2 = matriz2;

            //Coloca index do shared segment como posicao onde o resultaod deve ser gravadp
            tasks[threadAssigned].tasks[taskIndex].posResultado = (i * matriz1->numLinhas) + j;

            tasks[threadAssigned].numTasks++;

            //printf("Designando linha %d e coluna %d para Thread %d, tarefa %d\n", i, j, threadAssigned, taskIndex);

            //Passa tarefa para proxima thread
            threadAssigned = (threadAssigned + 1) % numThreads;
        }
    }

    //Criar segmento de memoria compartilhado
    printf("Criando segmento de memoria compartilhado...\n");

    int sharedMemorySize = sizeof(int) * matriz1->numLinhas * matriz2->numColunas;
    sharedSegmentId = shmget(IPC_PRIVATE, sharedMemorySize, S_IRUSR | S_IWUSR);

    printf("Criando processos...\n");
    int currentPidIndex = 0;

    //Cria threads
    for(i = 0; i < numThreads; i++) {

        pid_t newProcessPid = fork();

        if(newProcessPid == 0) {
            //Child
            threadWork(&tasks[currentPidIndex]);
            exit(0);
        }
        else if (newProcessPid > 0) {
            //Parent
            printf("Processo criado: %d\n", newProcessPid);

            pids[currentPidIndex] = newProcessPid;
            currentPidIndex++;
        }
        else {
            //Error
            printf("Erro no Fork!\n");
            exit(1);
        }
    }

    printf("Processos criados! Aguardando termino dos processos...\n");
    //Aguarda termino dos processos

    for(i = 0; i < numThreads; i++) {
        while(0<waitpid(pids[i],NULL,0));
    }

    free(tasks);
    free(pids);

    //Popula matirz resultado com base no segmento de memoria compartilhado
    int *shared_memory;
    shared_memory = (int *) shmat(sharedSegmentId, NULL, 0);
    
    for(i = 0; i < matriz1->numLinhas; i++) {
        for(j = 0; j < matriz2->numColunas; j++) {
            resultado->dados[i][j] = shared_memory[(i * matriz1->numLinhas) + j];
        }
    }

    shmdt(shared_memory);

    //Remove segmento compartilhado
    shmctl(sharedSegmentId, IPC_RMID, NULL);

    writeMatrixToFile(resultado, "out.txt");

    printf("Resultado:\n");
    printMatrixInfo(resultado);

    destroyMatrix(matriz1);
    destroyMatrix(matriz2);
	destroyMatrix(resultado);

	printf("Matriz resultante calculada!\n");

    return 0;
}