/**************************************************************************
    Travail pratique No 2 : Thread utilisateurs
    
    Ce fichier est votre implémentation de la librarie des threads utilisateurs.
         
	Systemes d'explotation GLO-2001
	Universite Laval, Quebec, Qc, Canada.
	(c) 2016 Philippe Giguere
	
				NE MODIFIEZ PAS CE FICHIER!
				NE MODIFIEZ PAS CE FICHIER!
				NE MODIFIEZ PAS CE FICHIER!
				
 **************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <sys/timeb.h>
#include "ThreadUtilisateur.h"

#define nbThread 3	//Nombre de thread

/* Structure passée à chaque thread lors de son démarrage */
typedef struct threadArg {
  int *arg1, *arg2;
  int Numero;
} threadArg;

struct timeb gTime; 

void printTimestampHeader() {
	struct timeb actualTime, diffTime;
	ftime(&actualTime);
	printf("(%ld.%03d) ",actualTime.time-gTime.time,actualTime.millitm);
}

#define TIMESTAMPED_PRINTF(x) {printTimestampHeader(); printf x ;}

void threadFunction(void *arg){
	long i, cpt;
	int nTurnToSleep = 3;
	int nTurnToYield = 2;
	// On cast le pointeur vers la structure passée lors de l'appel ThreadCreer();
	threadArg  *pArg = (threadArg*) arg;
	// Pour avoir des comportements différents entre threads
	if (ThreadId() == 2) {
		nTurnToSleep = 0;
		nTurnToYield = 1;
	}

	for (i=0; i<2; i++) {
		TIMESTAMPED_PRINTF(("Thread%d: Je tourne avec une variable sur la pile à 0x%p.\n",ThreadId(),&i));
		fflush(stdout);
		for (cpt=0; cpt < 10000000; cpt++);
		ThreadCeder();
	}
	
	for (i=0; i<nTurnToSleep; i++) {
		TIMESTAMPED_PRINTF(("Thread%d: Je tourne avec une variable sur la pile à 0x%p.\n",ThreadId(),&i));
		fflush(stdout);
		for (cpt=0; cpt < 10000000; cpt++);
		int temps = pArg->Numero+1;
		TIMESTAMPED_PRINTF(("Thread%d: Je vais dormir pendant %d secondes!\n",ThreadId(),temps));
		ThreadDormir(temps);
	}

	for (i=0; i<nTurnToYield; i++) {
		TIMESTAMPED_PRINTF(("Thread%d: Je tourne avec une variable sur la pile à 0x%p.\n",ThreadId(),&i));
		fflush(stdout);
		for (cpt=0; cpt < 300000000; cpt++);
		ThreadCeder();
	}

	TIMESTAMPED_PRINTF(("Thread%d: Je QUITTE!\n",ThreadId()));
	ThreadQuitter();
	return;
}

int main(void) {
	threadArg arg[nbThread];
	tid MyThreadIds[nbThread];
	int i;

	ftime(&gTime); // On va prendre note du temps de départ du programe.

	// Initialisation de votre libraire des threads utilisateurs
	if (ThreadInit() == -1) {
		perror("Main: Erreur d'initialisation de la librarie!\n");
	    return 1;
	}

	// Il doit maintenant y avoir un thread ID associé au main
	TIMESTAMPED_PRINTF(("Main: Le thread ID du main est %d.\n",ThreadId()));

	int y;

	// Creation des Threads
	for (i = 0; i < nbThread; i++) {
		arg[i].Numero = i;
//		printf("Main: Demarrage du thread numéro %d\n",arg[i].Numero);
	    if ((MyThreadIds[i] = ThreadCreer(threadFunction, (void *)&arg[i])) < 0) {
	    	perror("Erreur lors de la création d'un nouveau thread!\n");
	    	return 1;
	    }
	    TIMESTAMPED_PRINTF(("Main: Le thread avec ID %d a été créé.\n",MyThreadIds[i]));
	}

	
	//Attendre la fin de tout les threads avec ThreadJoindre
	for (i = 0; i < nbThread; i++) {// Creation des Threads
		TIMESTAMPED_PRINTF(("Main: Je joins le thread ID %d\n",MyThreadIds[i]));
		ThreadJoindre(MyThreadIds[i]);
		TIMESTAMPED_PRINTF(("Main: Le thread ID %d a terminé!\n",MyThreadIds[i]));
	}
	
	TIMESTAMPED_PRINTF(("Main: Tous les threads ont terminé!\n"));
	
	// On spin une fois pour s'assurer que le garbage collector fasse son travail
	ThreadCeder();
	
	TIMESTAMPED_PRINTF(("Main: je termine.\n"));

	return EXIT_SUCCESS;
}
