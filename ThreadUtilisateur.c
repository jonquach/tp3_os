/**************************************************************************
    Travail pratique No 2 : Thread utilisateurs
    
    Ce fichier est votre implémentation de la librarie des threads utilisateurs.
         
	Systemes d'explotation GLO-2001
	Universite Laval, Quebec, Qc, Canada.
	(c) 2016 Philippe Giguere
 **************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ucontext.h>
#include <time.h>
#include <signal.h>
#include <sys/time.h>
#include <valgrind/valgrind.h>
#include "ThreadUtilisateur.h"

/* Définitions privées, donc pas dans le .h, car l'utilisateur n'a pas besoin de
   savoir ces détails d'implémentation. OBLIGATOIRE. */
typedef enum { 
  THREAD_EXECUTE=0,
  THREAD_PRET,
  THREAD_BLOQUE,
  THREAD_TERMINE
} EtatThread;

#define TAILLE_PILE 8192   // Taille de la pile utilisée pour les threads
#define STACK_SIZE TAILLE_PILE

/* Structure de données pour créer une liste chaînée simple sur les threads qui ont fait un join.
   Facultatif */
typedef struct WaitList {
  struct TCB *pThreadWaiting;
  struct WaitList *pNext;
} WaitList;

/* TCB : Thread Control Block. Cette structure de données est utilisée pour stocker l'information
   pour un thread. Elle permet aussi d'implémenter une liste doublement chaînée de TCB, ce qui
   facilite la gestion et permet de faire un ordonnanceur tourniquet sans grand effort.  */
typedef struct TCB {  // Important d'avoir le nom TCB ici, sinon le compilateur se plaint.
  tid                id;        // Numero du thread
  EtatThread         etat;      // Etat du thread
  ucontext_t         ctx;       // Endroit où stocker le contexte du thread
  time_t             WakeupTime; // Instant quand réveiller le thread, s'il dort, en epoch time.
  struct TCB         *pSuivant;   // Liste doublement chaînée, pour faire un buffer circulaire
  struct TCB         *pPrecedant; // Liste doublement chaînée, pour faire un buffer circulaire
  struct WaitList    *pWaitListJoinedThreads; // Liste chaînée simple des threads en attente.
} TCB;

// Pour que les variables soient absolument cachées à l'utilisateur, on va les déclarer static
static TCB *gpThreadCourant = NULL;  // Thread en cours d'execution
static TCB *gpNextToExecuteInCircularBuffer = NULL;
static int gNumberOfThreadInCircularBuffer = 0;
static int gNextThreadIDToAllocate = 0;
static WaitList *gpWaitTimerList = NULL; 
static TCB *gThreadTable[MAX_THREADS]; // Utilisé par la fonction ThreadID()

//mes variable
static WaitList *gpLastWaitTimerList = NULL;


struct TCB *makeNewTCB() {
  struct TCB *newTcb = (TCB *) malloc(sizeof(struct TCB));

  newTcb->pSuivant = NULL;
  newTcb->pPrecedant = NULL;

  return newTcb;
}

char getStatusToChar(EtatThread status) {
  char c;
  switch(status) {
    case THREAD_EXECUTE:
      c = 'E';
      break;
    case THREAD_PRET:
      c = 'P';
      break;
    case THREAD_BLOQUE:
      c = 'B';
      break;
    case THREAD_TERMINE:
      c = 'T';
      break;
  }

  return c;
}

/* Cette fonction ne fait rien d'autre que de spinner un tour et céder sa place. C'est l'équivalent 
   pour un système de se tourner les pouces. */
void IdleThreadFunction(void *arg) {
  struct timespec SleepTime, TimeRemaining;
  SleepTime.tv_sec = 0;
  SleepTime.tv_nsec = 250000000;
  while (1) {
    printf("                #########  Idle Thread 0 s'exécute et va prendre une pose de 250 ms... #######\n");
    /* On va dormir un peu, pour ne pas surcharger inutilement le processus/l'affichage. Dans un
       vrai système d'exploitation, s'il n'y a pas d'autres threads d'actifs, ce thread demanderait au
       CPU de faire une pause, car il n'y a rien à faire. */    
      nanosleep(&SleepTime,&TimeRemaining); // nanosleep interfere moins avec les alarmes.
    ThreadCeder();
  }
}


/* ****************************************************************************************** 
                                   T h r e a d I n i t
   ******************************************************************************************/
int ThreadInit(void){
  printf("\n  ******************************** ThreadInit()  ******************************** \n");

  // Idle thread, id 0;
  ThreadCreer(*IdleThreadFunction, 0);

  // Should be thread 1, main thread here
  struct TCB *tcb = (TCB *) malloc(sizeof(struct TCB));

  tcb->id = gNextThreadIDToAllocate;
  // tcb->etat = THREAD_PRET;
  // tcb->ctx = NULL;
  // tcb->WakupTime
  gThreadTable[gNextThreadIDToAllocate] = tcb;
  gNextThreadIDToAllocate += 1;

  // Link thread 0 and thread 1 to ring buffer
  tcb->pSuivant = gThreadTable[0];
  tcb->pPrecedant = gThreadTable[0];
  tcb->pSuivant->pPrecedant = tcb;
  tcb->pPrecedant->pSuivant = tcb;
  // tcb->pWaitListJoinedThreads = NULL;

  // Init ucontext
  getcontext(&tcb->ctx);
  // Each thread must posses it's own stack
  char *pile = (char *) malloc(TAILLE_PILE);
  // Affect new stack to new thread, change ESP register 
  tcb->ctx.uc_stack.ss_sp = pile;
  tcb->ctx.uc_stack.ss_size = TAILLE_PILE;

  gNumberOfThreadInCircularBuffer += 1;

  // Mark thread 1 as current thread
  gpThreadCourant = tcb;

  return 0;
}


/* ****************************************************************************************** 
                                   T h r e a d C r e e r
   ******************************************************************************************/
tid ThreadCreer(void (*pFuncThread)(void *), void *arg) {
  printf("\n  ******************************** ThreadCreer(%p,%p) ******************************** \n",pFuncThread,arg);

  struct TCB *tcb = (TCB *) malloc(sizeof(struct TCB));

  tcb->id = gNextThreadIDToAllocate;
  tcb->pWaitListJoinedThreads = NULL;
  gThreadTable[gNextThreadIDToAllocate] = tcb;
  gNextThreadIDToAllocate += 1;

  //store the VALGRIND_STACK_REGISTER return values
  // struct TCB *valgrind_ret = malloc(sizeof(struct TCB));

  // Init ucontext
  getcontext(&tcb->ctx);

  // Each thread must posses it's own stack
  char *pile = (char *) malloc(TAILLE_PILE);

  VALGRIND_STACK_REGISTER(pile, pile + STACK_SIZE);
  // valgrind stack deregister
  // VALGRIND_STACK_DEREGISTER(valgrind_ret[i]);

  // Affect new stack to new thread, change ESP register
  tcb->ctx.uc_stack.ss_sp = pile;
  tcb->ctx.uc_stack.ss_size = TAILLE_PILE;

  // Affect function to context for when needed
  makecontext(&tcb->ctx, (void *)pFuncThread, 1, arg);

  // Insert in ring buffer

  // printf("tcb id %d\n", tcb->id);
  // if very first
  if (tcb->id == 0) {
    tcb->pSuivant = NULL;
    tcb->pPrecedant = NULL;
  } else {
    tcb->pSuivant = gpThreadCourant->pSuivant;
    tcb->pPrecedant = gpThreadCourant;
    tcb->pSuivant->pPrecedant = tcb;
    tcb->pPrecedant->pSuivant = tcb;
  }

  gNumberOfThreadInCircularBuffer += 1;
  printf("gNumberOfThreadInCircularBuffer %d\n", gNumberOfThreadInCircularBuffer);
  gpNextToExecuteInCircularBuffer = tcb;

  // Thread ready
  tcb->etat = THREAD_PRET;

  return tcb->id;
}

/* ****************************************************************************************** 
                                   T h r e a d C e d e r
   ******************************************************************************************/
void ThreadCeder(void) {
  printf("\n  ******************************** ThreadCeder()  ******************************** \n");

  // Print scheduler status
  printf("----- Etat de l'ordonnanceur avec %d threads -----\n", gNumberOfThreadInCircularBuffer);

  // struct TCB *end = gpNextToExecuteInCircularBuffer->pPrecedant;
  struct TCB *start = gpNextToExecuteInCircularBuffer;

  // printf("end id %d\n", end->id);
  // printf("start id %d\n", start->id);
  for (int i = 0; i < gNumberOfThreadInCircularBuffer; ++i) {
    if (start == gpNextToExecuteInCircularBuffer) {
      printf("|\tprochain->ThreadID:%d\tÉtat:%c\tWaitList", start->id, getStatusToChar(start->etat));
    }
    else if (start->id == 0) {
      printf("|\t\t  ThreadID:%d\tÉtat:%c\t**Special Idle Thread*\tWaitList",
             start->id, getStatusToChar(start->etat));
    }
    else {
      printf("|\t\t  ThreadID:%d\tÉtat:%c\tWaitList", start->id, getStatusToChar(start->etat));
    }

    if (start->pWaitListJoinedThreads != NULL) {
      for (struct WaitList *waitList = start->pWaitListJoinedThreads;
           waitList != NULL; waitList = waitList->pNext) {
        printf("-->(%d)", waitList->pThreadWaiting->id);
      }
      // printf("-->() // NOTE: TO BE IMPLEMENTED");
    }

    printf("\n");
    start = start->pSuivant;
    // i++;
  }

  printf("----- Liste des threads qui dorment, epoch time=%d -----\n", (int) time(NULL));
  printf("-------------------------------------------------\n");
  // NOTE: Core function, like clock top

  // Need to loop through gpWaitTimerList, check if a thread need to be wakeup

  // Need garbage collection, with THREAD_TERMINE
  while (gpNextToExecuteInCircularBuffer->etat == THREAD_TERMINE) {

    printf("\n\n==== ==== ===== ==== === === === === === === ==== \n");
    printf("ThreadCeder: Garbage collection sur le thread %d\n", gpNextToExecuteInCircularBuffer->id);

    // Save next TCB pointer before free memory
    struct TCB *pNextExec = gpNextToExecuteInCircularBuffer->pSuivant;

    // Remove from thread table index
    gThreadTable[gpNextToExecuteInCircularBuffer->id] = NULL;

    // printf("AFTER GTHREAD, BEFORE FREEE\n");
    // Free ucontext stack
    free(gpNextToExecuteInCircularBuffer->ctx.uc_stack.ss_sp);
    // Free WaitList, if pWaitList NULL nothing is performed, cf man free
    free(gpNextToExecuteInCircularBuffer->pWaitListJoinedThreads);

    // Remove from ring buffer
    // printf("AFTER FREE, REMOVING FROM RING BUFFER\n");
    gpNextToExecuteInCircularBuffer->pPrecedant->pSuivant = gpNextToExecuteInCircularBuffer->pSuivant;
    gpNextToExecuteInCircularBuffer->pSuivant->pPrecedant = gpNextToExecuteInCircularBuffer->pPrecedant;
    gpNextToExecuteInCircularBuffer->pSuivant = NULL;
    gpNextToExecuteInCircularBuffer->pPrecedant = NULL;
    gNumberOfThreadInCircularBuffer -= 1;

    // Destroy the TCB
    free(gpNextToExecuteInCircularBuffer);
    // printf("TCB DESTROYED\n");

    // Pass next TCB pointer
    gpNextToExecuteInCircularBuffer = pNextExec;
  }


  WaitList *previousNode = NULL;
  //on parcour la liste de threads qui wait et on les affichent
  WaitList *cur = gpWaitTimerList;
  for (; cur->pNext != NULL; cur = cur->pNext)
  {

	  printf("/t/t  ThreadID:%d/tÉtat:%c/tWakeTime=%d  WaitList", cur->pThreadWaiting->id, getStatusToChar(cur->pThreadWaiting->etat), cur->pThreadWaiting->WakeupTime);
	  if (start->pWaitListJoinedThreads != NULL) {
	        for (struct WaitList *waitList = start->pWaitListJoinedThreads;
	             waitList != NULL; waitList = waitList->pNext) {
	          printf("-->(%d)", waitList->pThreadWaiting->id);
	        }
	  }
	  printf("\n");

	  //vérification de si il est pret a etre réveillé
	  if (time(NULL) >= cur->pThreadWaiting->WakeupTime) {
		  //on ajoute le noeud apres l'actuel.
		  TCB *next = gpThreadCourant->pSuivant;
		  next->pPrecedant = cur->pThreadWaiting;
		  gpThreadCourant->pSuivant = cur->pThreadWaiting;
		  cur->pThreadWaiting->pSuivant = next;
		  cur->pThreadWaiting->pPrecedant = gpThreadCourant;

		  //on le vire de la liste des waitings
		  if (previousNode == NULL)//si c le 1 node
		  {
			  if (gpWaitTimerList->pNext == NULL)// si y a que 1 node
			  {
				  gpWaitTimerList = NULL;
			  }
			  else
			  {
				  gpWaitTimerList = gpWaitTimerList->pNext;
			  }
		  }
		  else
		  {
			  previousNode->pNext = cur->pNext; //on bind le previous au suivant
		  }

	  }
	  previousNode = cur;
  }



  // Select next thread to be executed
  // swapcontext(ucontext_t *oucp, const ucontext_t *ucp);

  TCB *oucp = gpThreadCourant;

  printf("gpThreadCourant id == %d\n", gpThreadCourant->id);
  printf("gpThreadCourant status == %c\n", getStatusToChar(gpThreadCourant->etat));

  if (oucp->etat == THREAD_EXECUTE)
    oucp->etat = THREAD_PRET;

  // Search next thread ready to be executed
  for (;gpNextToExecuteInCircularBuffer->etat != THREAD_PRET;
        gpNextToExecuteInCircularBuffer = gpNextToExecuteInCircularBuffer->pSuivant);

  // Mark as executed before swapping context
  gpNextToExecuteInCircularBuffer->etat = THREAD_EXECUTE;
  gpThreadCourant = gpNextToExecuteInCircularBuffer;

  printf("========\n");
  printf("gpThreadCourant id == %d\n", gpThreadCourant->id);
  printf("gpThreadCourant status == %c\n", getStatusToChar(gpThreadCourant->etat));

  // Reposition next to be executed
  gpNextToExecuteInCircularBuffer = gpThreadCourant->pSuivant;

  swapcontext(&oucp->ctx, &gpThreadCourant->ctx);
}


/* ****************************************************************************************** 
                                   T h r e a d J o i n d r e
   ******************************************************************************************/
int ThreadJoindre(tid ThreadID){
  printf("\n  ******************************** ThreadJoindre(%d)  ******************************* \n",ThreadID);

  TCB *tcbToJoin = gThreadTable[(unsigned int)ThreadID];

  if (tcbToJoin == NULL || tcbToJoin->etat == THREAD_TERMINE) {
    printf("SOMETHING TERRIBLE IS GOING ON\n");
  }

  // printf("blah id %d\n", ThreadID);
  printf("tcbToJoin id %d\n", tcbToJoin->id);
  printf("gpThreadCourant id == %d\n", gpThreadCourant->id);

  // Mark current thread as blocked
  gpThreadCourant->etat = THREAD_BLOQUE;
  
  // Remove from ring buffer
  gpThreadCourant->pPrecedant->pSuivant = gpThreadCourant->pSuivant;
  gpThreadCourant->pSuivant->pPrecedant = gpThreadCourant->pPrecedant;
  gpThreadCourant->pSuivant = NULL;
  gpThreadCourant->pPrecedant = NULL;
  gNumberOfThreadInCircularBuffer -= 1;

  // New item in wait list
  WaitList *newWaitList = (WaitList *) malloc(sizeof(struct WaitList));

  newWaitList->pThreadWaiting = gpThreadCourant;
  newWaitList->pNext = tcbToJoin->pWaitListJoinedThreads;

  // Add waiting list on tcb to be joined with
  tcbToJoin->pWaitListJoinedThreads = newWaitList;

  // Exec another thread
  ThreadCeder();

  return 1;
}


/* ****************************************************************************************** 
                                   T h r e a d Q u i t t e r
   ******************************************************************************************/
void ThreadQuitter(void){
  printf("  ******************************** ThreadQuitter(%d)  ******************************** \n",gpThreadCourant->id);

  if (gpThreadCourant->pWaitListJoinedThreads != NULL) {
    for (;gpThreadCourant->pWaitListJoinedThreads != NULL;
          gpThreadCourant->pWaitListJoinedThreads = gpThreadCourant->pWaitListJoinedThreads->pNext) {

      struct TCB *pThreadWaiting = gpThreadCourant->pWaitListJoinedThreads->pThreadWaiting;
      printf("ThreadQuitter: je reveille le thread %d\n",
             pThreadWaiting->id);

      // Add to ring buffer
      pThreadWaiting->pSuivant = gpThreadCourant->pSuivant;
      pThreadWaiting->pPrecedant = gpThreadCourant;
      pThreadWaiting->pSuivant->pPrecedant = pThreadWaiting;
      pThreadWaiting->pPrecedant->pSuivant = pThreadWaiting;
      gNumberOfThreadInCircularBuffer += 1;
      gpNextToExecuteInCircularBuffer = pThreadWaiting;
      pThreadWaiting->etat = THREAD_PRET;
    }
  }

  gpThreadCourant->pWaitListJoinedThreads = NULL;
  gpThreadCourant->etat = THREAD_TERMINE;

  // Go to next thread
  ThreadCeder();
  printf(" ThreadQuitter:Je ne devrais jamais m'exectuer! Si je m'exécute, vous avez un bug!\n");
  return;
}

/* ****************************************************************************************** 
                                   T h r e a d I d
   ******************************************************************************************/
tid ThreadId(void) {
  // Libre à vous de la modifier. Mais c'est ce que j'ai fait dans mon code, en toute simplicité.
  return gpThreadCourant->id;
}

/* ****************************************************************************************** 
                                   T h r e a d D o r m i r
   ******************************************************************************************/
void ThreadDormir(int secondes) {

  printf("\n  ******************************** ThreadDormir(%d)  ******************************** \n",secondes);
  TCB *next;
  TCB *previous;

  gpThreadCourant->etat = THREAD_BLOQUE;
  if (gpWaitTimerList == NULL)
  {
	  gpWaitTimerList = malloc(1 * sizeof(WaitList));

	  gpWaitTimerList->pThreadWaiting = gpThreadCourant;
	  gpWaitTimerList->pNext = NULL;
	  gpLastWaitTimerList = gpWaitTimerList; //on met un pointeur sur le derniere elem de la liste car g la flemme de faire une boucle
  }
  else
  {
	  //On remplit le nouveau maillon
	  WaitList *newElem = malloc(1 * sizeof(WaitList));
	  newElem->pThreadWaiting = gpThreadCourant;
	  newElem->pNext = NULL;

	  //On set le dernier maillon de la liste sur le nouveau
	  gpLastWaitTimerList->pNext = newElem;

	  //on set le pointeur sur le dernier maillon (qui est le nouveau qu'on vient d'ajouter)
	  gpLastWaitTimerList = newElem;
  }

  //on recupere les 2 noeuds avant et apres l'actuel
  next = gpThreadCourant->pSuivant;
  previous = gpThreadCourant->pPrecedant;

  //on bind les 2 ensemble
  next->pPrecedant = previous;
  previous->pSuivant = next;

  //on défonce le noeud actuel
  gpThreadCourant->pPrecedant = NULL;
  gpThreadCourant->pSuivant = NULL;

  gpThreadCourant->WakeupTime = time(NULL) + secondes;

  //changer le threaCourant au suivant.
}

