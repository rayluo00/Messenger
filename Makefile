CC = cc
CFLAGS = -g -c
OFLAGS = -g -o
CRSFLAGS = -lcurses

make: piggy4.c head.c tail.c utilities.c network.h
	$(CC) $(CFLAGS) head.c
	$(CC) $(CFLAGS) tail.c
	$(CC) $(CFLAGS) piggy4.c
	$(CC) $(CFLAGS) utilities.c
	$(CC) $(CFLAGS) network.h 
	$(CC) $(OFLAGS) piggy4 piggy4.o head.o tail.o utilities.o $(CRSFLAGS)

clean:
	$(RM) piggy4 , *~ , *.o , *.gch , *# , core

turnt:
	tar -cvf piggy4.tar piggy4.c head.c tail.c utilities.c network.h Makefile
