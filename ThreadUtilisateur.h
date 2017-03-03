#ifndef THREAD_UTILISATEUR_H
#define THREAD_UTILISATEUR_H

/**************************************************************************
    Travail pratique No 2 : Thread utilisateurs
    
    Ce fichier représente l'interface exposée de votre libraire de thread
    utilisateurs. ATTENTION! Vous ne pouvez pas modifier ce fichier!
         
	Systemes d'explotation GLO-2001
	Universite Laval, Quebec, Qc, Canada.
	(c) 2016 Philippe Giguere
 **************************************************************************/

/* Les threads seront identifiés par un Thread ID. typedef permet de créer un nouveau
   type, qui sera enforcé par le compilateur. Donc moins de chance d'avoir des bugs */	
typedef unsigned int tid;
#define MAX_THREADS 100

/* ----------------------------------------------------------------------------
   Fonction int ThreadInit(void) 
   Cette fonction est utilisée pour pour initialiser la librairie. Elle doit être
   appelée avant la toute première utilisation de vos threads.
   La fonction retourne:
	    1 si tout s'est bien passé
	   -1 si vous avez une erreur lors de l'initialisation. */
int ThreadInit(void);


/* ----------------------------------------------------------------------------
   Fonction tid ThreadCreer(void (*fn)(void *), void *arg)
   Cette fonction va créer un thread et le placer dans la file d'ordonnancement.
   Les entrées sont :
   	  void (*fn)(void *) : une fonction fn qui sera le code exécuté par un thread.
   	  void *arg          : un pointeur sur les données passées au thread lors du démarrage.
   La fonction retourne :
	    le tid du thread si tout s'est bien passé
	   -1 si il y a une erreur. */
tid ThreadCreer(void (*fn)(void *), void *arg);


/* ----------------------------------------------------------------------------
   Fonction void ThreadCeder(void)
   Cette fonction va faire passer le fil d'exécution au prochain thread (dans la file
   d'ordonnacement) qui est prêt à être exécuté. L'ordonnancement se fait selon
   l'algorithme du tourniquet. Il faut que le prochain thread à exécuter soit dans 
   l'état THREAD_PRET. Il faut donc sauter tous les threads qui sont dans les 
   états THREAD_BLOQUE ou THREAD_TERMINE. */
void ThreadCeder(void);


/* ----------------------------------------------------------------------------
   Fonction int ThreadJoindre(tid ThreadAJoindre)
   Cette fonction va joindre le thread en cours à l'exécution du thread ThreadAJoindre.
   Cela signifie que a) le thread qui appelle cette fonction bloquera en passant à
   l'état THREAD_BLOQUE b) lorsque le thread ThreadAJoindre quitte, il devra "réveiller"
   le thread appelant. Cette fonction devra invoquer ultimement la fonction ThreadCeder()
   pour faire exécuter un autre thread. La fonction retourne 1 si tout se passe bien,
   et -1 en cas d'erreur. */
int ThreadJoindre(tid ThreadAJoindre);


/* ----------------------------------------------------------------------------
   Fonction void ThreadQuitter(void)
   Cette fonction sert à indiquer à la libraire que le thread appelant a terminé son travail. Il
   faut donc l'enlever de la file d'ordonnancement. Attention! Si un ou des threads sont joints
   (attend) sur le thread qui appelle cette fonction, il faudra les réveiller (voir 
   ThreadJoindre()). Il faudra marquer le thread comme étant dans un état terminé, par
   l'état THREAD_TERMINE. Cette fonction devra invoquer ultimement la fonction ThreadCeder()
   pour faire exécuter un autre thread. */
void ThreadQuitter(void);

/* ----------------------------------------------------------------------------
   Fonction void ThreadDormir(int TempsDormirSecondes)
   Cette fonction sert à indiquer à l'ordonnanceur qu'un thread doit demeurer inactif pendant
   au moins TempsDormirSecondes. Le thread sera dans l'état THREAD_BLOQUE. À chaque fois que 
   l'ordonnanceur est appelé via ThreadCeder, vous devez vérifier s'il y a un thread qui doit
   être réveillé, en consultant la liste des processus qui dorment. S'il y en a un, veuillez 
   l'ordonnancer immédiatement. S'il y en a plus d'un à réveiller, l'ordre importe peu, tant
   et aussi longtemps qu'ils seront tous les prochains à être exécuté. */
void ThreadDormir(int TempsDormirSeconde);

/* ----------------------------------------------------------------------------
   Fonction tid ThreadId(void)
   Retourne l'identifiant du thread en cours */
tid ThreadId(void);

#endif  // --- ifndef THREAD_UTILISATEUR_H
