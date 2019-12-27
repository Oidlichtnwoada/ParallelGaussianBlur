CFLAGS= -Wall -O2 -DLOADBMP_IMPLEMENTATION
EXECS=p2
MPICC?=mpicc

all: ${EXECS}

p2: 	main_template.c kernels.c image.c
	${MPICC} ${CFLAGS} -o p2 main_template.c kernels.c image.c

p2sol: 	main.c kernels.c image.c
	${MPICC} ${CFLAGS} -o p2 main.c kernels.c image.c

clean:
	rm ${EXECS}
