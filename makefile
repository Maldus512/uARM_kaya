uarm = p2test.core.uarm p2test.stab.uarm asl.o pcb.o initial.o scheduler.o p2test.o exceptions.o system.o interrupt.o p1test.0.1.o p1test.0.1.core.uarm p1test.0.1.stab.uarm

final: p1test.0.1.core.uarm p2test.core.uarm

p2test.core.uarm : p2test 
	 elf2uarm -k p2test

p2test: initial.o scheduler.o pcb.o asl.o interrupt.o system.o p2test.o exceptions.o
	arm-none-eabi-ld -T /usr/include/uarm/ldscripts/elf32ltsarm.h.uarmcore.x -o p2test /usr/include/uarm/crtso.o /usr/include/uarm/libuarm.o initial.o scheduler.o system.o interrupt.o pcb.o asl.o p2test.o exceptions.o

p1test.0.1.core.uarm : p1test.0.1
	elf2uarm -k p1test.0.1

p1test.0.1: pcb.o asl.o p1test.0.1.o
	arm-none-eabi-ld -T /usr/include/uarm/ldscripts/elf32ltsarm.h.uarmcore.x -o p1test.0.1 /usr/include/uarm/crtso.o /usr/include/uarm/libuarm.o p1test.0.1.o pcb.o asl.o

p1test.0.1.o: p1test.0.1.c 
	arm-none-eabi-gcc -mcpu=arm7tdmi -c -o p1test.0.1.o p1test.0.1.c 

initial.o: initial.c 
	arm-none-eabi-gcc -mcpu=arm7tdmi -c -o initial.o initial.c

scheduler.o: scheduler.c
	arm-none-eabi-gcc -mcpu=arm7tdmi -c -o scheduler.o scheduler.c

system.o: system.c
	arm-none-eabi-gcc -mcpu=arm7tdmi -c -o system.o system.c

interrupt.o: interrupt.c
	arm-none-eabi-gcc -mcpu=arm7tdmi -c -o interrupt.o interrupt.c

exceptions.o: exceptions.c
	arm-none-eabi-gcc -mcpu=arm7tdmi -c -o exceptions.o exceptions.c

pcb.o: pcb.c ./includes/pcb.h
	arm-none-eabi-gcc -mcpu=arm7tdmi -c -o pcb.o pcb.c

asl.o: asl.c ./includes/asl.h ./includes/pcb.h
	arm-none-eabi-gcc -mcpu=arm7tdmi -c -o asl.o asl.c

p2test.o: p2test.0.1.c
	arm-none-eabi-gcc -w -mcpu=arm7tdmi -c -o p2test.o p2test.0.1.c

clean:
	rm p2test p1test.0.1 $(uarm)

