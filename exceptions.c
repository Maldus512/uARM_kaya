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

//Modulo che implementa i gestori di trap e tlb tramite il vettore (eventualmente) specificato
//con una SYS5

#include "includes/libuarm.h"
#include "includes/arch.h"
#include "includes/uARMconst.h"
#include "includes/uARMtypes.h"
#include "includes/const.h"

#include "includes/asl.h"
#include "includes/pcb.h"

#include "includes/scheduler.h"
#include "includes/exceptions.h"
#include "includes/initial.h"


//utilizza il gestore specificato tramite SYS5; se il processo non ne ha specificato alcuno, termina
void specified_handler(int ex_type){
	if( current_process->TrapVec[ex_type*2] != NULL ){
		//Salvo la old area nel vettore specificato dal processo
		switch(ex_type){
			case SYSBK:
				copy_state( (state_t*) SYSBK_OLDAREA, current_process->TrapVec[ex_type*2] );
				break;
			case PGMTRAP:
				copy_state( (state_t*) PGMTRAP_OLDAREA, current_process->TrapVec[ex_type*2] );
				break;
			case TLB:
				copy_state( (state_t*) TLB_OLDAREA, current_process->TrapVec[ex_type*2] );
				break;
			default:
				terminateProcess(0);
				scheduler();
				break;
		}
		LDST( current_process->TrapVec[ex_type*2+1] );
	}
	else{
		terminateProcess(0);
		scheduler();
	}
}

//Gestore dei tlb
void tlb_handler(){
	if( current_process != NULL ){
		current_process->userTime += getTODLO() - userTimeStart;
		specified_handler(TLB);
	}
}

//Gestore dei Trap
void trap_handler(){
	if( current_process != NULL ){
		current_process->userTime += getTODLO() - userTimeStart;
		specified_handler(PGMTRAP);
	}
}

