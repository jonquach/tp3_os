TestThread: ThreadUtilisateur.o TestThread.c
	gcc -g TestThread.c ThreadUtilisateur.o -o TestThread -lrt

ThreadUtilisateur.o: ThreadUtilisateur.c ThreadUtilisateur.h
	gcc -g -c ThreadUtilisateur.c -o ThreadUtilisateur.o 
	
clean: 
	rm -rf *.o *.*~ TestThread
