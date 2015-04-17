#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <semaphore.h>


#define MAX_SLEEP_TIME 10

enum {
    STATE_THINKING,
    STATE_HUNGRY,
    STATE_EATING
};

struct strFil {

    int id;
    int state;
    int numRefeicoes;
    sem_t semAlterandoGrafos;
    sem_t semAguardandoGarfo;
};

typedef struct strFil Filosofo;

int numFilosofos;
Filosofo * filosofos;

//Mutex utilizado durante a impressao de estado dos filosofos
pthread_mutex_t lockImprimindoEstado;

int getSleepTime() {
    return (rand() % MAX_SLEEP_TIME) + 1;
}


void inicializaFilosofos() {
    
    filosofos = (Filosofo *) malloc(sizeof(Filosofo) * numFilosofos);

    int i;
    for(i = 0; i < numFilosofos; i++) {
        filosofos[i].id = i;
        filosofos[i].state = STATE_THINKING;
        filosofos[i].numRefeicoes = 0;
        sem_init(&filosofos[i].semAlterandoGrafos, 0, 1);
        sem_init(&filosofos[i].semAguardandoGarfo, 0, 0);
    }
}

void imprimeEstadoFilosofos() {

	pthread_mutex_lock(&lockImprimindoEstado);

	int i;
	for(i = 0; i < numFilosofos; i++) {
        
        switch(filosofos[i].state) {

        	case STATE_THINKING: 
        		printf("T ");
        		break;
    		case STATE_EATING: 
        		printf("E ");
        		break;
    		case STATE_HUNGRY: 
    			printf("H ");
    			break;
        }
    }

    printf("\n");

    pthread_mutex_unlock(&lockImprimindoEstado);
}

int possoComer(Filosofo * filosofo) {

	int idFilosofoEsquerda = (filosofo->id + (numFilosofos-1))%numFilosofos;
	int idFilosofoDireita = (filosofo->id + 1) % numFilosofos;

	//Algum dos vizinhos esta comendo
	if(filosofos[idFilosofoEsquerda].state == STATE_EATING || filosofos[idFilosofoDireita].state == STATE_EATING) {
		return 0;
	}

	//Filosofo da equerda esta com fome e comeu menos vezes do que eu
	if(filosofos[idFilosofoEsquerda].state == STATE_HUNGRY && filosofos[idFilosofoEsquerda].numRefeicoes < filosofo->numRefeicoes) {
		return 0;
	}

	//Filosofo da direita esta com fome e comeu menos vezes do que eu
	if(filosofos[idFilosofoDireita].state == STATE_HUNGRY && filosofos[idFilosofoDireita].numRefeicoes < filosofo->numRefeicoes) {
		return 0;
	}

	return 1;
}

void pensar(Filosofo * filosofo) {

	filosofo->state = STATE_THINKING;

	imprimeEstadoFilosofos();
	sleep(getSleepTime());

}

void adquirirGarfos(Filosofo * filosofo) {

	//Sinaliza que quer comer
	sem_wait(&filosofo->semAlterandoGrafos);

	filosofo->state = STATE_HUNGRY;
	imprimeEstadoFilosofos();

	int idFilosofoEsquerda = (filosofo->id + (numFilosofos-1))%numFilosofos;
	int idFilosofoDireita = (filosofo->id + 1) % numFilosofos;

	//Aguarda vez de comer
	while(!possoComer(filosofo)) {
		sem_wait(&filosofo->semAguardandoGarfo);
	}
}

void largarGarfos(Filosofo* filosofo) {

	sem_wait(&filosofo->semAlterandoGrafos);

	filosofo->state = STATE_THINKING;
	imprimeEstadoFilosofos();

	int idFilosofoEsquerda = (filosofo->id + (numFilosofos-1))%numFilosofos;
	int idFilosofoDireita = (filosofo->id + 1) % numFilosofos;

	//Sinaliza filosofo da esquerda
	if(filosofos[idFilosofoEsquerda].state == STATE_HUNGRY) {
		sem_post(&filosofos[idFilosofoEsquerda].semAguardandoGarfo);
	}

	//Sinaliza filosofo da direita
	if(filosofos[idFilosofoDireita].state == STATE_HUNGRY) {
		sem_post(&filosofos[idFilosofoDireita].semAguardandoGarfo);
	}

	sem_post(&filosofo->semAlterandoGrafos);

}

void comer(Filosofo * filosofo) {

	filosofo->state = STATE_EATING;
	imprimeEstadoFilosofos();

	//Incrementa o numero de refeicoes realizadas
	filosofo->numRefeicoes++;

	sem_post(&filosofo->semAlterandoGrafos);

	sleep(getSleepTime());

	largarGarfos(filosofo);

}


void * filosofar (void * arg) {

    Filosofo * meuFilosofo = (Filosofo *) arg;

    printf("Inicializando filosofo %d\n", meuFilosofo->id);

    while(1) {
    	pensar(meuFilosofo);
    	adquirirGarfos(meuFilosofo);
    	comer(meuFilosofo);
    }
}

int main (int argc, char **argv) {
    pthread_t * threads;

    //Numero default de threads;
    int numThreads = 3;

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

    if(numThreads < 3) {
        printf("Numero de filosofos invalido!\n");
        return 0;
    }

    printf("Usando %d filosofos!\n", numThreads);

    //Inicializa random seed
    time_t t;
    srand((unsigned) time(&t));

    //Inicializa filosofos
    numFilosofos = numThreads;
    inicializaFilosofos();

    threads = (pthread_t *) malloc(sizeof(pthread_t) * numThreads);

    //Inicializa lock de impressao de estado
    pthread_mutex_init(&lockImprimindoEstado, NULL);

    int i;
    printf("Criando threads...\n");
    //Cria threads
    for(i = 0; i < numThreads; i++) {
        pthread_create(&threads[i], NULL, filosofar, (void *) &filosofos[i]);
    }

    //Aguarda termino das threads
    for(i = 0; i < numThreads; i++) {
        pthread_join(threads[i], NULL);
    }

    free(threads);

    return 0;
}