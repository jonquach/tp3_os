TestThread: ThreadUtilisateur.o TestThread.c
	gcc -g3 TestThread.c ThreadUtilisateur.o -o TestThread -lrt

ThreadUtilisateur.o: ThreadUtilisateur.c ThreadUtilisateur.h
	gcc -g3 -c ThreadUtilisateur.c -o ThreadUtilisateur.o 
	
clean: 
	rm -rf *.o *.*~ TestThread
