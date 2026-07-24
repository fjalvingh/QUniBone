/* kd11ea.h: interface of the PDP-11/34 (KD11-EA) CPU emulator to QUniBone

 Forked from cpu20/ka11.h (Angelo Papenhoff's KA11).
 The core still executes the 11/20 instruction set: see the "TODO 11/34"
 markers in kd11ea.c for what has to be added to make this a real 11/34.

 Every externally visible symbol is prefixed kd11ea_/KD11EA, because this core
 is linked into the same binary as the KA11 core of the 11/20.

 This header needs cpu34/11.h to be included first.
 */
#ifndef _KD11EA_H_
#define _KD11EA_H_

enum {
	KD11EA_STATE_HALTED = 0,
	KD11EA_STATE_RUNNING = 1,
	KD11EA_STATE_WAITING = 2
};


typedef struct KD11EA KD11EA;
struct KD11EA
{
	word r[16];
	word b;		// B register before BUT JSRJMP
	word ba;
	word ir;
	Bus *bus;
	byte psw;
	int traps;
	int be;
	int state;

	struct {
		int (*bg)(void *dev);
		void *dev;
	} br[4];

	// UniBone
	pthread_mutex_t mutex ;
	volatile bool external_intr ; // INTR by parallel thread pending
	volatile word external_intrvec;	// associated vector

	word sw;

	// TODO 11/34: the KT11-D memory management unit.
	// Needs MMR0..MMR3 and the kernel/user PAR/PDR blocks as QUNIBUS
	// registers of cpu34_c, plus 16 -> 18 bit translation in dati()/dato().
	// TODO 11/34: kernel/user PSW modes (psw is only a byte here).
};


void kd11ea_tracestate(KD11EA *cpu);
void kd11ea_printstate(KD11EA *cpu);
void kd11ea_reset(KD11EA *cpu);
void kd11ea_setintr(KD11EA *cpu, unsigned vec);
void kd11ea_pwrfail_trap(KD11EA *cpu);
void kd11ea_pwrup_vector_fetch(KD11EA *cpu);
void kd11ea_condstep(KD11EA *cpu);

#endif
