/*  Copyright (C) 2015  Carlo Stomeo, Stefano Mazza, Alessandro Zini, Mattia Maldini

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.*/

//Modulo che implementa il gestore delle system call e tutte le possibili system call

#include "includes/libuarm.h"
#include "includes/arch.h"
#include "includes/uARMconst.h"
#include "includes/uARMtypes.h"
#include "includes/const.h"

#include "includes/asl.h"
#include "includes/pcb.h"

#include "includes/initial.h"
#include "includes/scheduler.h"
#include "includes/system.h"
#include "includes/exceptions.h"


inline int blockedOnSem(struct pcb_t *pcb){	//true se il processo è fermo su un semaforo
	return(pcb->p_cursem != NULL );
}

int blockedOnDev( struct pcb_t *pcb ){	//true se il processo è fermo sul semaforo di un device,
	if( blockedOnSem(pcb) ){	//false se è fermo su un semaforo generico o su nessun semaforo
		int* sem = pcb->p_cursem->s_semAdd;
		int* dev = devSem ;
		int i;
		for( i = 0; i < MAX_DEVICES ; i++ ){
			if( sem == dev ) return TRUE;
			dev++;
		}
		return FALSE;
	}
	else return FALSE;
}

int newpid(){	//restituisce un pid libero
	int i = 0;
	while( pid_map[i] && i<MAXPROC ) i++;
	if(i >= MAXPROC) PANIC();	//non ci possono essere più di MAXPROC processi
	pid_map[i]++;
	return i+1;
}


void bp(){int x;}
//gestore di system call
void sys_handler(){
	current_process->userTime += getTODLO() - userTimeStart;	//all'entrata nel sys_handler aggiorno il tempo utente del processo
	state_t *sysbp_old = (state_t*) SYSBK_OLDAREA;
	copy_state(sysbp_old, &current_process->p_s);
	unsigned int a1 = sysbp_old->a1;	//salvo i valori dei registri
	unsigned int a2 = sysbp_old->a2;
	unsigned int a3 = sysbp_old->a3;
	unsigned int a4 = sysbp_old->a4;
	int cause = getCAUSE();
	cause = CAUSE_EXCCODE_GET(cause);
	if(cause == EXC_SYSCALL){	
		if((current_process->p_s.cpsr & STATUS_SYS_MODE) == STATUS_SYS_MODE){	//se è una system call e ho il diritto di eseguirla
			switch(a1){							//procedo
				case CREATEPROCESS:
					createProcess( (state_t*) a2, (int) a3 );	
					break;
				case TERMINATEPROCESS:
					terminateProcess( (pid_t) a2 );
					break;
				case VERHOGEN:
					verhogen( (int*) a2, (int) a3 , 0 , 0 );	
					break;
				case PASSEREN:
					passeren( (int*) a2, (int) a3 );
					break;
				case SPECTRAPVEC:
					specTrapVec( (state_t**) a2 );
					break;
				case GETCPUTIME:
					getCpuTime( (cputime_t*) a2, (cputime_t*) a3 );
					break;
				case WAITCLOCK:
					waitClock();
					break;
				case WAITIO:
					waitIO( (int) a2, (int) a3, (int) a4 );
					break;
				case GETPID:
					getPid();
					break;
				case GETPPID:
					getPPid();
					break;
				default:
					specified_handler(SYSBK);
			}
			if( current_process == NULL){
				scheduler();	//chiamo lo scheduler; se il processo non è stato messo in attesa
						//ripartirà immediatamente
			}
			else{
				userTimeStart = getTODLO();
				LDST(&current_process->p_s);
			}
		}
		else if((current_process->p_s.cpsr & STATUS_USER_MODE) == STATUS_USER_MODE){	//se non sono in kernel mode lancio un trap
			if(a1 >= CREATEPROCESS && a1 <= GETPPID){
				copy_state( sysbp_old , (state_t*) PGMTRAP_OLDAREA );	//cambio la gestione da syscall a trap
				((state_t*) PGMTRAP_OLDAREA)->CP15_Cause = CAUSE_EXCCODE_SET(((state_t*) PGMTRAP_OLDAREA)->CP15_Cause, EXC_RESERVEDINSTR);	//setto il trap adeguato
				userTimeStart = getTODLO();
				trap_handler();
			}
			else{
				specified_handler(SYSBK);	//usa il vettore delle eccezioni specificato
			}
		}
	}
	else if( cause == EXC_BREAKPOINT ){
		specified_handler(SYSBK);	//usa il vettore delle eccezioni specificato
	}
	PANIC();		
}


//SYS1
//Crea un processo, con stato e priorità passati come parametri
void createProcess(state_t *state, int priority){
	int pid;
	if( !(( priority >= PRIO_LOW ) && ( priority <= PRIO_HIGH )) ){	//livello di priorità non valido
		current_process->p_s.a1 = -1;
		return;
	}
	struct pcb_t *newp = allocPcb();
	if( newp == NULL ){	//allocazione del nuovo processo fallita
		current_process->p_s.a1 = -1;
	}
	else{	//creo un nuovo processo usando lo stato preso dal registro a2
		copy_state( state, &newp->p_s );
		pid = newpid();	
		current_process->p_s.a1 = pid;
		activeProcesses[pid-1] = newp;
		newp->priority = priority;
		insertProcQ(priority_queue( priority ) , newp );
		insertChild( current_process, newp ) ;
		newp->pid = pid;
		process_count++;
	}
}

//SYS2
//Termina il processo con pid passato come parametro ( il processo corrente se è 0 )
void terminateProcess(pid_t pid){
	if( pid == 0 ){	//termina il processo chiamante
		pid = current_process->pid;
	}
	if(pid <= MAXPROC){
		struct pcb_t *pcb = activeProcesses[pid-1];
		activeProcesses[pid-1] = NULL;
		pid_map[pid-1] = 0;
		if( blockedOnSem(pcb)){
			outBlocked(pcb);
			if(  !blockedOnDev(pcb) ){
				*(pcb->p_cursem->s_semAdd) += pcb->p_cursem->waitingFor;
			}
			else softblock_count--;	//se era bloccato su un device riduco i softblock
		}
		while(!emptyChild(pcb)){	//termino anche tutti i suoi figli
			terminateProcess( removeChild(pcb)->pid );
		}
		outChild(pcb);	//il processo da terminare non è più figlio di suo padre
		if(current_process == pcb){
			current_process->CPUTime = getTODLO() - CPUTimeStart;	//anche se termina aggiorno il suo cputime 
            		current_process = NULL;
      		}
        	outProcQ( priority_queue(pcb->priority), pcb);
        	freePcb(pcb);
        	process_count--;
	}
}

//SYS3
//Operazione di V pesata su di un semaforo
//il terzo parametro è il registro di stato di un device che il processo sta aspettando;
//il quarto è il tempo impiegato a gestire l'interrupt che provoca la verhogen sul processo
//(se di interrupt si è trattato); vengono utilizzati solo dal gestore degli interrupt
void verhogen(int* semaddr, int weight, unsigned int p_a1reg, cputime_t time){
	struct pcb_t* toFree;
	while( (toFree = headBlocked(semaddr)) && weight ){
		weight--;
		(*semaddr)++;
		toFree->p_cursem->waitingFor--;		//il processo bloccato potrebbe aver richiesto più di una istanza di una risorsa;
		if(toFree->p_cursem->waitingFor <= 0){	// in tal caso devo sbloccarlo solo se tutte le istanze richieste sono disponibili
			toFree = removeBlocked(semaddr);
			toFree->p_cursem = NULL;
			if( p_a1reg != 0 ){
				toFree->p_s.a1 = p_a1reg;
				toFree->CPUTime += time;
			}
			insertProcQ( priority_queue( toFree->priority) , toFree );
		}
	}
	*semaddr += weight;
}

//SYS4
//Operazione di P pesata su un semaforo
void passeren( int* semaddr, int weight ){
	int request ;
	if( (*semaddr) > 0 )request = weight - (*semaddr);	//numero di risorse richieste dal processo che però non sono disponibili
	else request = weight;
	(*semaddr) -= weight;
	if( (*semaddr) < 0 ){
		insertBlocked(semaddr, current_process);
		current_process->p_cursem->waitingFor = request;
		current_process->CPUTime += getTODLO() - CPUTimeStart;	//aggiorno solo il CPUTime, lo userTime è stato gestito in sys_handler
		current_process = NULL;
	}
}

//SYS5
//Specifica un vettore di NewArea - OldArea per la gestione di TLB, Trap e System call/Breakpoint
void specTrapVec( state_t **state_vector ){
	if(current_process->TrapVec[0] == NULL){
		int i;
		for( i=0; i < 6; i++){
			current_process->TrapVec[i] = state_vector[i];
		}
	}
	else{
		terminateProcess((pid_t) 0);
	}
}

//SYS6
//Restituisce il lo UserTime e CPUTime del processo corrente
void getCpuTime( cputime_t *global, cputime_t *user ){
	current_process->CPUTime += getTODLO() - CPUTimeStart;	//lo userTime viene gestito nell'handler,
	*global = current_process->CPUTime;			//per cui devo preoccuparmi solo del CPUTime
	*user = current_process->userTime;
	CPUTimeStart = getTODLO();
}

//SYS7
//Mette il processo in attesa sullo pseudo clock
void waitClock(){
	softblock_count++;
	passeren( &devSem[CLOCK_SEM], 1 );
}

//SYS8
//Mette il processo in attesa che un device termini una procedura di input/output,
//restituendo in a1 il valore del suo registro di stato e aggiornando eventualmente il tempo di CPU impiegato a gestire
//l'interrupt sul device
void waitIO( int intlNo, int dnum, int waitForTermRead ){
	if( intlNo == IL_TERMINAL && !waitForTermRead ) intlNo++;	//i subdevice per la scrittura stanno sulla linea 8
	int i = devSemIndex(intlNo, dnum);
	unsigned int result;
	if( devStatus[i] != 0 ){	//se l'interrupt che sto aspettando c'è già stato restituisco lo status register
					// e ne aggiungo il tempo di gestione al CPUTime
		current_process->p_s.a1 = devStatus[i];
		current_process->CPUTime += interruptTime[i];
		devStatus[i] = 0;
		interruptTime[i] = 0;
	}
	else{
		softblock_count++;
		passeren( &devSem[i] , 1 );
	}
}

//SYS9
//Restituisce il pid del processo corrente
void getPid(){
	current_process->p_s.a1 = (pid_t) current_process->pid;
}

//SYS10
//Restituisce il pid del padre del processo corrente
void getPPid(){
	//se il processo corrente non ha padre restituisco il suo stesso id
	if(current_process->p_parent == NULL) current_process->p_s.a1 = (pid_t) current_process->pid;
	else current_process->p_s.a1 = (pid_t) current_process->p_parent->pid;
}
