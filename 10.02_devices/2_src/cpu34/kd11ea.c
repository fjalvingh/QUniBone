/* kd11ea.c: PDP-11/34 (KD11-EA) CPU emulation core

 Forked from cpu20/ka11.c, Angelo Papenhoff's KA11 (PDP-11/20), then extended
 with the KT11-D memory management (cpu34/kt11d.c) and the kernel/user
 processor modes which come with it.

 All symbols are either static or prefixed kd11ea_/KD11EA: this core is linked
 into the same binary as the KA11 core of the 11/20.
 */

#include "11.h"
#include "kt11d.h"
#include "kd11ea.h"
#include <stdlib.h>

#include "cpu_debug_pins.h" // ARM_DEBUG_PIN*, no-ops off the BBB

// unibone_*() declared in cpu_bus_adapter.h, included via 11.h

/* EIS (MUL, DIV, ASH, ASHC, XOR, SOB) and MFPS/MTPS are native 11/34
   instructions and always executed. On the 11/20 they do not exist.
   SWAB is a CPU feature which is still a runtime parameter of cpu20_c. */
#define KD11EA_SWAB_VBIT	1	// SWAB clears the V bit

static int
dati_bus(Bus *bus)
{
	unsigned int data;
	if(!unibone_dati(bus->addr, &data))
		return 1;
	bus->data = data;
	return 0;
}

static int
dato_bus(Bus *bus)
{
	return !unibone_dato(bus->addr, bus->data);
}

static int
datob_bus(Bus *bus)
{
	return !unibone_datob(bus->addr, bus->data);
}

enum {
	PSW_PR = 0340,
	PSW_T = 020,
	PSW_N = 010,
	PSW_Z = 004,
	PSW_V = 002,
	PSW_C = 001,
	// bits actually implemented: <15:14> current mode, <13:12> previous mode,
	// <7:5> priority, <4> T, <3:0> NZVC. The KD11-EA has no second register
	// set, so PSW<11> and the unused <10:8> do not exist and always read zero.
	PSW_MASK = 0170377,
};

/* Assign the PSW.
 Besides the value itself this switches the stack pointer if the processor
 mode changed, tells the MMU which address space is current now, and tells the
 QUNIBUS arbitrator the new priority level.
 Instructions which only touch the condition codes assign cpu->psw directly:
 going through here on every opcode would call into the arbitrator each time.
 */
void
kd11ea_set_psw(KD11EA *cpu, word newpsw)
{
	unsigned oldsp = ((cpu->psw>>14)&3) == KT11D_MODE_KERNEL ?
			KD11EA_SP_KERNEL : KD11EA_SP_USER;
	unsigned newsp;

	// the bits which have no flipflop can never be loaded, whatever the
	// source: a write to 777776, MTPS, RTI/RTT or a trap vector.
	newpsw &= PSW_MASK;
	newsp = ((newpsw>>14)&3) == KT11D_MODE_KERNEL ?
			KD11EA_SP_KERNEL : KD11EA_SP_USER;

	if(oldsp != newsp){
		cpu->stackpointer[oldsp] = cpu->r[6];
		cpu->r[6] = cpu->stackpointer[newsp];
	}
	cpu->psw = newpsw;
	kt11d_set_modes(&cpu->mmu, newpsw);
	unibone_prioritylevelchange((newpsw>>5) & 7);
}

// is the CPU in kernel mode? Stack limit and the RTI/RTT restrictions need it.
#define IS_KERNEL(cpu)	((((cpu)->psw>>14)&3) == KT11D_MODE_KERNEL)




enum {
	TRAP_STACK = 1,
	TRAP_PWR = 2,
	TRAP_BR7 = 4,
	TRAP_BR6 = 010,
	TRAP_BR5 = 040,
	TRAP_BR4 = 0100,
	TRAP_CSTOP = 01000	// can't happen?
};

#define ISSET(f) ((cpu->psw&(f)) != 0)


static word
sgn(word w)
{
	return (w>>15)&1;
}

static word
sxt(byte b)
{
	return (word)(int8_t)b;
}

/* The 16 -> 18 bit address mapping lives in kt11d.h: kt11d_relocate() either
 relocates through the KT11-D page table, or - when MMR0<0> is clear - maps
 the top 8K of the virtual space to the IO page, exactly as the 11/20 does.
 */

// the KT11-D registers are internal to the CPU and not readable over the bus,
// so these dumps are the only way to see the state of the memory management.
#define KD11EA_STATE_FMT \
	" R0 %06o R1 %06o R2 %06o R3 %06o R4 %06o R5 %06o R6 %06o R7 %06o\n" \
	" 10 %06o 11 %06o 12 %06o 13 %06o 14 %06o 15 %06o 16 %06o 17 %06o\n" \
	" BA %06o IR %06o PSW %06o (%s mode, KSP %06o USP %06o)\n%s"
#define KD11EA_STATE_ARGS \
	cpu->r[0], cpu->r[1], cpu->r[2], cpu->r[3], \
	cpu->r[4], cpu->r[5], cpu->r[6], cpu->r[7], \
	cpu->r[8], cpu->r[9], cpu->r[10], cpu->r[11], \
	cpu->r[12], cpu->r[13], cpu->r[14], cpu->r[15], \
	cpu->ba, cpu->ir, cpu->psw, IS_KERNEL(cpu) ? "kernel" : "user", \
	IS_KERNEL(cpu) ? cpu->r[6] : cpu->stackpointer[KD11EA_SP_KERNEL], \
	IS_KERNEL(cpu) ? cpu->stackpointer[KD11EA_SP_USER] : cpu->r[6], \
	mmubuf

void
kd11ea_tracestate(KD11EA *cpu)
{
	char mmubuf[512];
	kt11d_format(&cpu->mmu, mmubuf, sizeof(mmubuf));
	trace(KD11EA_STATE_FMT, KD11EA_STATE_ARGS);
}

void
kd11ea_printstate(KD11EA *cpu)
{
	char mmubuf[512];
	kt11d_format(&cpu->mmu, mmubuf, sizeof(mmubuf));
	printf(KD11EA_STATE_FMT, KD11EA_STATE_ARGS);
}

// only to be called from kd11ea_condstep() thread
void
kd11ea_reset(KD11EA *cpu)
{
	Busdev *bd;

	cpu->traps = 0;
	cpu->external_intr = 0;
	cpu->mutex = PTHREAD_MUTEX_INITIALIZER ;
	// The KT11-D is deliberately NOT touched here: on real hardware the RESET
	// opcode pulses bus INIT, which does not reach the memory management.
	// An OS executing RESET must keep its address map - see kd11ea_power_reset()
	// for the console START / power-up case.

	for(bd = cpu->bus->devs; bd; bd = bd->next)
		bd->reset(bd->dev);
}

// console START and power-up: everything kd11ea_reset() does, plus the state
// which survives a RESET opcode - memory management, PSW and stack pointers.
// only to be called from kd11ea_condstep() thread
void
kd11ea_power_reset(KD11EA *cpu)
{
	kd11ea_reset(cpu);
	kt11d_reset(&cpu->mmu);	// clears MMR0, disabling relocation
	cpu->stackpointer[KD11EA_SP_KERNEL] = 0;
	cpu->stackpointer[KD11EA_SP_USER] = 0;
	cpu->trap_vector = 4;
	kd11ea_set_psw(cpu, 0);	// kernel mode, priority 0
}

/* The CPU internal registers are decoded on the *physical* address: once
 relocation is on, the IO page is reached through a PAR and the virtual
 address says nothing about which register is meant.
 Decoded here: the PSW at 777776 (the 11/34 has MFPS/MTPS *as well as* the PSW
 address, not instead of it) and the KT11-D registers, which are internal to
 the KD11-EA and are therefore not published as QUNIBUS registers of cpu34_c. */
static int
dati(KD11EA *cpu, int b)
{
	uint32 pa;
	word w;

	if(!b && cpu->ba&1)
		goto be;

	if(kt11d_relocate(&cpu->mmu, cpu->ba, cpu->mmu.access_space, KT11D_READ, &pa))
		goto abort;

	/* internal registers */
	if(pa >= 0760000){
		if(kt11d_read_reg(&cpu->mmu, pa & ~1, &w)){
			cpu->bus->data = w;
			goto ok;
		}
		switch(pa){
		case 0777570: case 0777571:
			cpu->bus->data = cpu->sw;
			goto ok;
		case 0777776: case 0777777:
			// the odd byte reads PSW<15:8>: the caller shifts it down
			cpu->bus->data = cpu->psw & PSW_MASK;
			goto ok;

		/* respond but don't return real data */
		case 0777547:
			cpu->bus->data = 0;
			goto ok;
		}
	}

	cpu->bus->addr = pa&~1;
	if(dati_bus(cpu->bus))
		goto be;
ok:
if (unibone_trace_addr(cpu->ba))
 	trace("DATI [%06o] => %06o\n", cpu->ba, cpu->bus->data);
	cpu->be = 0;
	return 0;
be:
	trace("DATI [%06o]: NXM\n", cpu->ba);
	cpu->trap_vector = 4;
	cpu->be++;
	return 1;
abort:
	cpu->trap_vector = KT11D_ABORT_VECTOR;
	cpu->be++;
	return 1;
}

static int
dato(KD11EA *cpu, int b)
{
	uint32 pa;
	word mask;

if (unibone_trace_addr(cpu->ba)) // default: all
trace("%s [%06o] <= %06o\n", b? "DATOB":"DATO", cpu->ba, cpu->bus->data);
	if(!b && cpu->ba&1)
		goto be;

	if(kt11d_relocate(&cpu->mmu, cpu->ba, cpu->mmu.access_space, KT11D_WRITE, &pa))
		goto abort;

	/* internal registers */
	if(pa >= 0760000){
		// bus->data already holds the byte in the half selected by the address
		mask = b ? (pa&1 ? 0177400 : 0377) : 0177777;
		if(kt11d_write_reg(&cpu->mmu, pa & ~1, cpu->bus->data, mask))
			goto ok;
		switch(pa){
		case 0777570: case 0777571:
			/* can't write switches */
			goto ok;
		case 0777776: case 0777777:
			// Goes through kd11ea_set_psw(): a write may change the mode,
			// which switches the stack pointer and the MMU address space,
			// and PSW<7:5>, which the arbitrator has to be told about.
			// DATOB writes only the addressed half; mask has it already.
			// The non-existent bits are dropped by kd11ea_set_psw().
			kd11ea_set_psw(cpu, (cpu->psw & ~mask) |
					(cpu->bus->data & mask));
			goto ok;
		}
	}

	if(b){
		cpu->bus->addr = pa;
		if(datob_bus(cpu->bus))
			goto be;
	}else{
		cpu->bus->addr = pa&~1;
		if(dato_bus(cpu->bus))
			goto be;
	}
ok:
	cpu->be = 0;
	return 0;
be:
	cpu->trap_vector = 4;
	cpu->be++;
	return 1;
abort:
	cpu->trap_vector = KT11D_ABORT_VECTOR;
	cpu->be++;
	return 1;
}

static void
svc(KD11EA *cpu, Bus *bus)
{
	int l;
	Busdev *bd;
	static int brtraps[4] = { TRAP_BR4, TRAP_BR5, TRAP_BR6, TRAP_BR7 };
	for(l = 0; l < 4; l++){
		cpu->br[l].bg = nil;
		cpu->br[l].dev = nil;
	}
	cpu->traps &= ~(TRAP_BR4|TRAP_BR5|TRAP_BR6|TRAP_BR7);
	for(bd = bus->devs; bd; bd = bd->next){
		l = bd->svc(bus, bd->dev);
		if(l >= 4 && l <= 7 && cpu->br[l-4].bg == nil){
			cpu->br[l-4].bg = bd->bg;
			cpu->br[l-4].dev = bd->dev;
			cpu->traps |= brtraps[l-4];
		}
	}
}

static int
addrop(KD11EA *cpu, int m, int b)
{
	int r;
	int ai;
	r = m&7;
	m >>= 3;
	ai = 1 + (!b || (r&6)==6 || m&1);
	if(m == 0){
		assert(0);
		return 0;
	}
	switch(m&6){
	case 0:		// REG
		cpu->b = cpu->ba = cpu->r[r];
		return 0;	// this already is mode 1
	case 2:		// INC
		cpu->ba = cpu->r[r];
		cpu->b = cpu->r[r] = cpu->r[r] + ai;
		// MMR1 records the change, so an abort handler can undo it
		kt11d_log_register(&cpu->mmu, r, ai);
		break;
	case 4:		// DEC
		cpu->b = cpu->ba = cpu->r[r]-ai;
		// stack limit applies to the kernel stack only
		if(r == 6 && IS_KERNEL(cpu) && (cpu->ba&~0377) == 0) cpu->traps |= TRAP_STACK;
		cpu->r[r] = cpu->ba;
		kt11d_log_register(&cpu->mmu, r, -ai);
		break;
	case 6:		// INDEX
		cpu->ba = cpu->r[7];
		cpu->r[7] += 2;
		if(dati(cpu, 0)) return 1;
		cpu->b = cpu->ba = cpu->bus->data + cpu->r[r];
		break;
	}
	if(m&1){
		if(dati(cpu, 0)) return 1;
		cpu->b = cpu->ba = cpu->bus->data;
	}
	return 0;

}

static int
fetchop(KD11EA *cpu, int t, int m, int b)
{
	int r;
	r = m&7;
	if((m&070) == 0)
		cpu->r[t] = cpu->r[r];
	else{
		if(dati(cpu, b)) return 1;
		cpu->r[t] = cpu->bus->data;
		if(b && cpu->ba&1) cpu->r[t] = cpu->r[t]>>8;
	}
	if(b) cpu->r[t] = sxt(cpu->r[t]);
	return 0;
}

static int
readop(KD11EA *cpu, int t, int m, int b)
{
	return !(addrop(cpu, m, b) == 0 && fetchop(cpu, t, m, b) == 0);
}

static int
writedest(KD11EA *cpu, word v, int b)
{
	int d;
	if((cpu->ir & 070) == 0){
		d = cpu->ir & 7;
		if(b) SETMASK(cpu->r[d], v, 0377);
		else cpu->r[d] = v;
	}else{
		if(cpu->ba&1) v <<= 8;
		cpu->bus->data = v;
		if(dato(cpu, b)) return 1;
	}
	return 0;
}

static void
setnz(KD11EA *cpu, word w)
{
	cpu->psw &= ~(PSW_N|PSW_Z);
	if(w & 0100000) cpu->psw |= PSW_N;
	if(w == 0) cpu->psw |= PSW_Z;
}

static void
step(KD11EA *cpu)
{
	uint by;
	uint br;
	uint b;
	uint c;
	uint src, dst, sf, df, sm, dm;
	word mask, sign;
	int inhov;
	word oldpsw;	// 16 bits since the KT11-D added the mode fields
	word trapped_pc, trapped_psw, newpc, newpsw;	// used by the trap sequence
	uint reg;
	int32_t prod;
	word sh;

//	printf("fetch from %06o\n", cpu->r[7]);
//	printstate(cpu);

#define SP	cpu->r[6]
#define PC	cpu->r[7]
#define SR	cpu->r[010]
#define DR	cpu->r[011]
#define TV	cpu->r[012]
#define BA	cpu->ba
#define PSW	cpu->psw

#define RD_B	if(sm != 0) if(readop(cpu, 010, src, by)) goto be;\
		if(dm != 0) if(readop(cpu, 011, dst, by)) goto be;\
		if(sm == 0) fetchop(cpu, 010, src, by);\
		if(dm == 0) fetchop(cpu, 011, dst, by)
#define RD_U	if(dm != 0) if(readop(cpu, 011, dst, by)) goto be;\
		if(dm == 0) fetchop(cpu, 011, dst, by);\
		SR = DR
#define WR	if(writedest(cpu, b, by)) goto be
#define NZ	setnz(cpu, b)
#define SVC	goto service
#define TRAP(v)	TV = v; goto trap
#define CLC	cpu->psw &= ~PSW_C
#define CLV	cpu->psw &= ~PSW_V
#define CLCV	cpu->psw &= ~(PSW_V|PSW_C)
#define SEV	cpu->psw |= PSW_V
#define SEC	cpu->psw |= PSW_C
#define C	if(b & 0200000) SEC
#define NC	if((b & 0200000) == 0) SEC
#define CLNZ	cpu->psw &= ~(PSW_N|PSW_Z)
#define SEN	cpu->psw |= PSW_N
#define SEZ	cpu->psw |= PSW_Z
#define BXT	if(by) b = sxt(b)
#define BR	PC += br
#define CBR(c)	if(((c)>>(cpu->psw&017)) & 1) BR
#define PUSH	SP -= 2; if(!inhov && IS_KERNEL(cpu) && (SP&~0377) == 0) cpu->traps |= TRAP_STACK
#define POP	SP += 2
// force the address space of the next dati()/dato(): trap vector fetches and
// the trap pushes are always kernel references, MFPI/MTPI use the previous mode
#define SPACE(s)	cpu->mmu.access_space = (s)
#define SPACE_RESTORE	cpu->mmu.access_space = cpu->mmu.space
#define OUT(a,d)	cpu->ba = (a); cpu->bus->data = (d); if(dato(cpu, 0)) goto be
#define IN(d)	if(dati(cpu, 0)) goto be; d = cpu->bus->data
#define INA(a,d)	cpu->ba = a; if(dati(cpu, 0)) goto be; d = cpu->bus->data
#define TR(m)	if (unibone_trace_addr(PC-2)) trace("EXEC [%06o] "#m"\n", PC-2)
#define TRB(m)	if (unibone_trace_addr(PC-2)) trace("EXEC [%06o] "#m"%s\n", PC-2, by ? "B" : "")
//#define TR(m)	trace("EXEC [%06o] "#m"\n", PC-2)
//#define TRB(m)	trace("EXEC [%06o] "#m"%s\n", PC-2, by ? "B" : "")

	inhov = 0;

	{
		// external interrupt from parallel threads?
		pthread_mutex_lock(&cpu->mutex) ;
		bool external_intr = cpu->external_intr ;
		word external_intrvec = cpu->external_intrvec ;
		cpu->external_intr = 0 ;
		pthread_mutex_unlock(&cpu->mutex) ;
		if (external_intr){
			//ARM_DEBUG_PIN1(0);	// INTR processed
			cpu->state = KD11EA_STATE_RUNNING ;
			TRAP(external_intrvec);
		}	
	}

//	if(cpu->r[7] == 016440) {
//	 	cpu->state = KD11EA_STATE_HALTED;
//	 	printf("\nUB BREAKPOINT\n");
//	 	printf("R0 %06o R1 %06o R2 %06o R3 %06o R4 %06o R5 %06o R6 %06o R7 %06o\n", cpu->r[0], cpu->r[1], cpu->r[2], cpu->r[3], cpu->r[4], cpu->r[5], cpu->r[6], cpu->r[7]);
//	 	printf("ba %06o ir %06o psw %06o\n", cpu->ba, cpu->ir, cpu->psw);
//	 	return;
//	}

	oldpsw = PSW;
	// MMR2 latches the address of this instruction, MMR1 starts empty.
	// Both stay frozen while MMR0 holds an abort.
	kt11d_instruction_start(&cpu->mmu, PC);
	INA(PC, cpu->ir);
	PC += 2;	/* don't increment on bus error! */
	by = !!(cpu->ir&B15);
	br = sxt(cpu->ir)<<1;
	src = cpu->ir>>6 & 077;
	sf = src & 7;
	sm = src>>3 & 7;
	dst = cpu->ir & 077;
	df = dst & 7;
	dm = dst>>3 & 7;
	if(by)	mask = M8, sign = B7;
	else	mask = M16, sign = B15;

	/* Binary */
	switch(cpu->ir & 0170000){
	case 0110000: case 0010000:	TRB(MOV);
		RD_B; CLV;
		b = SR; NZ;
		if(dm==0) cpu->r[df] = SR;
		else writedest(cpu, SR, by);
		SVC;
	case 0120000: case 0020000:	TRB(CMP);
		RD_B; CLCV;
		b = SR + W(~DR) + 1; NC; BXT;
//		if(cpu->ir == 021527)
//			printf("cmp (r5),xx -> %o vs %o\n", SR, DR);
		if(sgn((SR ^ DR) & ~(DR ^ b))) SEV;
		NZ; SVC;
	case 0130000: case 0030000:	TRB(BIT);
		RD_B; CLV;
		b = DR & SR;
		NZ; SVC;
	case 0140000: case 0040000:	TRB(BIC);
		RD_B; CLV;
		b = DR & ~SR;
		NZ; WR; SVC;
	case 0150000: case 0050000:	TRB(BIS);
		RD_B; CLV;
		b = DR | SR;
		NZ; WR; SVC;
	case 0060000:			TR(ADD);
		by = 0; RD_B; CLCV;
		b = SR + DR; C;
		if(sgn(~(SR ^ DR) & (DR ^ b))) SEV;
		NZ; WR; SVC;
	case 0160000:			TR(SUB);
		by = 0; RD_B; CLCV;
		b = DR + W(~SR) + 1; NC;
		if(sgn((SR ^ DR) & (DR ^ b))) SEV;
		NZ; WR; SVC;

	/* Reserved instructions */
	case 0170000:
        goto ri;

	case 0070000:
		reg = (cpu->ir >> 6) & 07;
        switch(cpu->ir & 0177000) {
              default:
	    		printf("-- ext: %o\n", cpu->ir);
                goto ri;

			case 0070000:		TR(MUL);
				RD_U;
              	cpu->psw &= ~(PSW_N|PSW_Z|PSW_V|PSW_C);
              	{
              		int32_t v1 = (int16_t) DR;
              		int32_t v2 = (int16_t) cpu->r[reg];
              		prod = v1 * v2;
					if(prod < -32768 || prod > 32767) {
						SEC;
					}
					if(prod == 0)
						SEZ;
					if(prod & B31)
						SEN;

              		if(reg & 0x1) {
              			//-- Odd register: store only lower 16 bits
						cpu->r[reg] = (word) prod;
              		} else {
              			cpu->r[reg + 1] = prod & 0xffff;
              			cpu->r[reg] = (word) (prod >> 16);
              		}
              	}
				SVC;

			case 0071000:		TR(DIV);
				RD_U;
              	cpu->psw &= ~(PSW_N|PSW_Z|PSW_V|PSW_C);
				if(reg & 0x1) goto ri;			// for div register must be even
				{
					int32_t dv = (int16_t) DR;
					prod = (uint32_t) cpu->r[reg + 1] | ((uint32_t) cpu->r[reg] << 16);
					if(DR == 0) {
						SEC;
						SEV;
					} else {
						ldiv_t d = ldiv(prod, dv);
						if(d.quot < -32768 || d.quot > 32767) {
							SEV;
						} else {
							cpu->r[reg] = (word) d.quot;
							cpu->r[reg + 1] = (word) (d.rem);
						}
						if(sgn(d.quot))
							SEN;
						if(0 == (d.quot & 0xffff))
							SEZ;
					}
				}
				SVC;

			case 0072000:		TR(ASH);
                // ASH
				RD_U;
              	cpu->psw &= ~(PSW_N|PSW_Z|PSW_V);
				b = cpu->r[reg];
				sh = (DR & 0x3f);				// Extract 6 bits
				if(sh & 0x20) {					// -ve?
					// we shift right
					sh = 0x40 - sh;					// +ve shift, 1..62
               		mask = sgn(b) ? 0xffff : 0x0;	// The previous sign gets shifted in
                    if(sh >= 17) {
                        // Really shifted out completely.
                		b = mask;
                		if(mask)
                			SEC;
                		else
                			CLC;
                		NZ;
                	} else {
						if(b & (1 << (sh - 1)))
							SEC;
						else
							CLC;
						b >>= sh;
						mask <<= (16 - sh);
						b |= mask;					// Sign extend
						b &= 0xffff;
						NZ;
						if(b & B15)
							SEN;
                	}
                } else {
                	// we shift left
                	if(sh == 0) {
                		//-- Nothing -> only set Z and N flags
                		NZ;
                	} else if(sh >= 17) {
                		if(sgn(b))
                			SEV;
						b = 0;
						CLC;
						SEZ;
					} else {
						//-- Loop, to handle overflow correctly: overflow is part of the last step!
						while(sh-- > 0) {
							if(b & B15) {
								SEC;
							} else {
								CLC;
							}
							uint ob = b;
							b <<= 1;
							ob ^= b;
							if(ob & B15) {					// Sign changed?
								SEV;
							}
						}
						NZ;
					}
                }
                b &= 0xffff;
				cpu->r[reg] = b;
				SVC;

              case 0073000:		TR(ASHC);
				RD_U;
              	cpu->psw &= ~(PSW_N|PSW_Z|PSW_V);
              	{
              		uint32_t val = ((uint32_t) cpu->r[reg] << 16) | cpu->r[reg | 1];	// The bitwise OR is intentional!

					sh = (DR & 0x3f);					// Extract 6 bits
					if(sh & 0x20) {						// -ve?
						// we shift right
						sh = 0x40 - sh;					// +ve shift, 1..62
                		uint32_t msk = val & B31 ? 0xffffffffL : 0x0;
						if(val & (1 << (sh - 1)))
							SEC;
						else
							CLC;
						val >>= sh;
						msk <<= (32 - sh);
						val |= msk;				// Sign extend
						if(val == 0)
							SEZ;
						if(val & B31)
							SEN;
                	} else {
						while(sh-- > 0) {
							if(val & B31) {
								SEC;
							} else {
								CLC;
							}
							uint32_t ob = val;
							val <<= 1;
							ob ^= val;
							if(ob & B31) {					// Sign changed?
								SEV;
							}
						}
						if(val == 0)
							SEZ;
						if(val & B31)
							SEN;
                	}
					if(reg & 0x1) {
						cpu->r[reg] = (word) val;		// Truncated result
					} else {
						cpu->r[reg] = (word) (val >> 16);
						cpu->r[reg + 1] = (word) val;
					}
				}
				SVC;
                break;

              case 0074000:		TR(XOR);
              	RD_U;
              	cpu->psw &= ~(PSW_N|PSW_Z|PSW_V);
				b = cpu->r[reg];
				b = DR ^ b;
				if(sgn(b)) {
					SEN;
				}
				NZ;
				WR; SVC;

			case 0077000:		TR(SOB);
				b = --(cpu->r[reg]);		// decrement reg
				if(b != 0) {
					//-- Jump
					mask = (cpu->ir & 077) << 1;			// Get jump offset (*2)
					cpu->r[7] -= mask;						// Decrement by offset
				}
				SVC;
		}
        // All else: not an EIS opcode
       	goto ri;
	}
	//-- remaining here is ir=x0xxxx

	/* Unary */
	switch(cpu->ir & 0007700){
	case 0005000:	TRB(CLR);
		RD_U; CLCV;
		b = 0;
		NZ; WR; SVC;
	case 0005100:	TRB(COM);
		RD_U; CLV; SEC;
		b = W(~SR);
		NZ; WR; SVC;
	case 0005200:	TRB(INC);
		RD_U; CLV;
		b = W(SR+1); BXT;
		if(sgn(~SR&b)) SEV;
		NZ; WR; SVC;
	case 0005300:	TRB(DEC);
		RD_U; CLV;
		b = W(SR+~0); BXT;
		if(sgn(SR&~b)) SEV;
		NZ; WR; SVC;
	case 0005400:	TRB(NEG);
		RD_U; CLCV;
		b = W(~SR+1); BXT; if(b) SEC;
		if(sgn(b&SR)) SEV;
		NZ; WR; SVC;
	case 0005500:	TRB(ADC);
		RD_U; c = ISSET(PSW_C); CLCV;
		b = SR + c; C; BXT;
		if(sgn(~SR&b)) SEV;
		NZ; WR; SVC;
	case 0005600:	TRB(SBC);
		RD_U; c = !ISSET(PSW_C)-1; CLCV;
		b = W(SR+c); if(c && SR == 0) SEC; BXT;
		if(sgn(SR&~b)) SEV;
		NZ; WR; SVC;
	case 0005700:	TRB(TST);
		RD_U; CLCV;
		b = SR;
		NZ; SVC;

	case 0006000:	TRB(ROR);
		RD_U; c = ISSET(PSW_C); CLCV;
		b = (SR&mask) >> 1; if(c) b |= sign; if(SR & 1) SEC; BXT;
		NZ; if((PSW>>3^PSW)&1) SEV;
		WR; SVC;
	case 0006100:	TRB(ROL);
		RD_U; c = ISSET(PSW_C); CLCV;
		b = (SR<<1) & mask; if(c) b |= 1; if(SR & B15) SEC; BXT;
		NZ; if((PSW>>3^PSW)&1) SEV;
		WR; SVC;
	case 0006200:	TRB(ASR);
		RD_U; c = ISSET(PSW_C); CLCV;
		b = W(SR>>1) | SR&B15; if(SR & 1) SEC; BXT;
		NZ; if((PSW>>3^PSW)&1) SEV;
		WR; SVC;
	case 0006300:	TRB(ASL);
		RD_U; CLCV;
		b = W(SR<<1); if(SR & B15) SEC; BXT;
		NZ; if((PSW>>3^PSW)&1) SEV;
		WR; SVC;

	case 0006400:
		// mtps. Only the byte form 106400 is MTPS, 006400 is MARK
		if(!by)
			goto ri;
		TR(MTPS);
		RD_U;
		// changes PSW<7:5>, so the arbitrator has to be told: go through
		// kd11ea_set_psw(). The mode bits are not affected.
		kd11ea_set_psw(cpu, (cpu->psw & 0xff00) | (DR & 0377));
		SVC;

	/* MFPI/MTPI address in the current mode but transfer in the previous
	   mode. The 11/34 has no separate I and D space, so MFPD/MTPD (the
	   1065xx/1066xx byte-bit variants) are the same instructions. */
	case 0006500:	TR(MFPI);
		by = 0;
		if(dm == 0){
			// register mode: R6 means the previous mode's stack pointer
			if(df == 6 && cpu->mmu.prev_space != cpu->mmu.space)
				b = cpu->stackpointer[cpu->mmu.prev_space == KT11D_SPACE_KERNEL ?
						KD11EA_SP_KERNEL : KD11EA_SP_USER];
			else
				b = cpu->r[df];
		}else{
			if(addrop(cpu, dst, 0)) goto be;
			SPACE(cpu->mmu.prev_space);
			if(dati(cpu, 0)){ SPACE_RESTORE; goto be; }
			SPACE_RESTORE;
			b = cpu->bus->data;
		}
		CLV; NZ;
		PUSH; OUT(SP, b);
		SVC;

	case 0006600:	TR(MTPI);
		by = 0;
		BA = SP; POP; IN(b);
		CLV; NZ;
		if(dm == 0){
			if(df == 6 && cpu->mmu.prev_space != cpu->mmu.space)
				cpu->stackpointer[cpu->mmu.prev_space == KT11D_SPACE_KERNEL ?
						KD11EA_SP_KERNEL : KD11EA_SP_USER] = b;
			else
				cpu->r[df] = b;
		}else{
			if(addrop(cpu, dst, 0)) goto be;
			cpu->bus->data = b;
			SPACE(cpu->mmu.prev_space);
			if(dato(cpu, 0)){ SPACE_RESTORE; goto be; }
			SPACE_RESTORE;
		}
		SVC;

	case 0006700:
		// mfps. Only the byte form 106700 is MFPS, 006700 is SXT
		if(!by)
			goto ri;
		TR(MFPS);
		by = 0;
		if(addrop(cpu, dst, 0)) goto be;
//		RD_U;
		b = cpu->psw & 0377;
//		printf("mfps: res=%o\n", b);
		WR; SVC;
	}

	switch(cpu->ir & 0107400){
	case 0004000:
	case 0004400:	TR(JSR);
		if(dm == 0) goto ill;
		if(addrop(cpu, dst, 0)) goto be;
		DR = cpu->b;
		PUSH; OUT(SP, cpu->r[sf]);
		cpu->r[sf] = PC; PC = DR;
		SVC;
	case 0104000:	TR(EMT); TRAP(030);
	case 0104400:	TR(TRAP); TRAP(034);
	}

	/* Branches */
    // ! 000 0!! !xx xxx xxx    (! = at least one is non-zero)
    if((cpu->ir & 074000) == 0 && (cpu->ir & 0103400) != 0) {
        switch(cpu->ir & 0103400){
        case 0000400:	TR(BR); BR; SVC;
        case 0001000:	TR(BNE); CBR(0x0F0F); SVC;
        case 0001400:	TR(BEQ); CBR(0xF0F0); SVC;
        case 0002000:	TR(BGE); CBR(0xCC33); SVC;
        case 0002400:	TR(BLT); CBR(0x33CC); SVC;
        case 0003000:	TR(BGT); CBR(0x0C03); SVC;
        case 0003400:	TR(BLE); CBR(0xF3FC); SVC;
        case 0100000:	TR(BPL); CBR(0x00FF); SVC;
        case 0100400:	TR(BMI); CBR(0xFF00); SVC;
        case 0101000:	TR(BHI); CBR(0x0505); SVC;
        case 0101400:	TR(BLOS); CBR(0xFAFA); SVC;
        case 0102000:	TR(BVC); CBR(0x3333); SVC;
        case 0102400:	TR(BVS); CBR(0xCCCC); SVC;
        case 0103000:	TR(BCC); CBR(0x5555); SVC;
        case 0103400:	TR(BCS); CBR(0xAAAA); SVC;
        }
    }

	/* Misc */
	switch(cpu->ir & 0777300){
	case 0100:	TR(JMP);
		if(dm == 0) goto ill;
		if(addrop(cpu, dst, 0)) goto be;
		PC = cpu->b;
		SVC;
	case 0200:
        switch(cpu->ir&070){
        case 000:	TR(RTS);
            BA = SP; POP;
            PC = cpu->r[df];
            IN(cpu->r[df]);
            SVC;
        case 010: case 020: case 030:
            goto ri;
        case 040: case 050:	TR(CCC); PSW &= ~(cpu->ir&017); SVC;
        case 060: case 070:	TR(SEC); PSW |= cpu->ir&017; SVC;
        }
	case 0300:	TR(SWAB);
		RD_U;
		if(KD11EA_SWAB_VBIT) {
		    CLCV;   // v-bit cleared, ZQKC compatible
		} else {
		    CLC;    // v-bit unchanged, actual 11/20 behavior
		}
		b = WD(DR & 0377, (DR>>8) & 0377);
		CLNZ; if(b & B7) SEN; if((b & M8) == 0) SEZ;
		WR; SVC;
	}

	/* Operate */
	switch(cpu->ir){
	case 0:	TR(HALT);
		// HALT is kernel-only on the KD11-EA: executed in user mode it is a
		// reserved instruction and traps through vector 10, leaving the
		// machine running. Only the 11/20 halts whatever the mode - it has
		// no modes to begin with.
		if(!IS_KERNEL(cpu))
			goto ri;
		cpu->state = KD11EA_STATE_HALTED; return;
	case 1:	TR(WAIT); cpu->state = KD11EA_STATE_WAITING; return ; // no traps
	case 2:
	case 6:
		if(cpu->ir == 2){ TR(RTI); }else{ TR(RTT); }
		BA = SP; POP; IN(PC);
		BA = SP; POP; IN(b);
		// In user mode neither RTI nor RTT may change the mode fields or the
		// priority: those bits keep their current value.
		if(!IS_KERNEL(cpu))
			b = (b & ~(0170000|PSW_PR)) | (PSW & (0170000|PSW_PR));
		// RTI takes a pending trace trap right away, RTT defers it by one
		// instruction. service: tests oldpsw, which gives the RTT behaviour.
		if(cpu->ir == 2 && (b & PSW_T))
			oldpsw |= PSW_T;
		kd11ea_set_psw(cpu, b);
		SVC;
	case 3:	TR(BPT); TRAP(014);
	case 4:	TR(IOT); TRAP(020);
	case 5:	TR(RESET);
		// Like HALT a kernel-only instruction, but this one does not trap:
		// outside kernel mode RESET is simply a no-op, so a user program
		// cannot INIT the bus out from under the devices.
		if(IS_KERNEL(cpu)){
			kd11ea_reset(cpu);
			unibone_bus_init();
		}
		SVC;
	}

	// All other instructions should be reserved now

ri:	TRAP(010);
ill:	TRAP(4);
be:	if(cpu->be > 1){
		printf("double bus error, HALT\n");
		trace("double bus error, HALT");
		cpu->state = KD11EA_STATE_HALTED;
		return;
	}
	trace("bus error at %06o\n", cpu->ba);
	// 4 for a bus timeout or an odd address, 0250 for an MMU abort
	TRAP(cpu->trap_vector);

trap:
	if (unibone_trace_addr(PC-2))
	trace("TRAP %o\n", TV);
	trapped_pc = PC;
	trapped_psw = PSW;
	/* The vector is read through kernel space and *before* the mode switch:
	   PSW<15:14> may still say "user" at this point. Then the new PSW is
	   installed - which switches the stack pointer - and only then are the
	   old PSW and PC pushed, onto the new mode's stack. */
	SPACE(KT11D_SPACE_KERNEL);
	BA = TV;	if(dati(cpu, 0)) goto be;	newpc = cpu->bus->data;
	BA = TV+2;	if(dati(cpu, 0)) goto be;	newpsw = cpu->bus->data;
	SPACE_RESTORE;
	// previous mode of the new PSW := the mode the trap came from
	newpsw = (newpsw & ~030000) | ((trapped_psw >> 2) & 030000);
	kd11ea_set_psw(cpu, newpsw);
	PUSH; OUT(SP, trapped_psw);
	PUSH; OUT(SP, trapped_pc);
	PC = newpc;
	/* no trace trap after a trap */
	oldpsw = PSW;

	if (unibone_trace_addr(PC-2))
	kd11ea_tracestate(cpu);
	return;		// TODO: is this correct?
//	SVC;

service:
	c = (PSW >> 5) & 7;	// PSW is 16 bits now, mask the priority out
	if(oldpsw & PSW_T){
		oldpsw &= ~PSW_T;
		TRAP(014);
	}else if(cpu->traps & TRAP_STACK){
		cpu->traps &= ~TRAP_STACK;
		inhov = 1;
		TRAP(4);
	}else if(cpu->traps & TRAP_PWR){
		cpu->traps &= ~TRAP_PWR;
		TRAP(024);
	}else if(c < 7 && cpu->traps & TRAP_BR7){
		cpu->traps &= ~TRAP_BR7;
		TRAP(cpu->br[3].bg(cpu->br[3].dev));
	}else if(c < 6 && cpu->traps & TRAP_BR6){
		cpu->traps &= ~TRAP_BR6;
		TRAP(cpu->br[2].bg(cpu->br[2].dev));
	}else if(c < 5 && cpu->traps & TRAP_BR5){
		cpu->traps &= ~TRAP_BR5;
		TRAP(cpu->br[1].bg(cpu->br[1].dev));
	}else if(c < 4 && cpu->traps & TRAP_BR4){
		cpu->traps &= ~TRAP_BR4;
		TRAP(cpu->br[0].bg(cpu->br[0].dev));
	}else
	// TODO? console stop
		/* fetch next instruction */
		return;
}

// to be called from parallel threads to signal async intr
// (unibusadapter worker thread)
void
kd11ea_setintr(KD11EA *cpu, unsigned vec)
{
	pthread_mutex_lock(&cpu->mutex) ;
	cpu->external_intr = true;
	cpu->external_intrvec = vec;
	trace("INTR vec=%03o\n", vec) ;
//	if (cpu->state == KD11EA_STATE_WAITING) // atomically
//		cpu->state = KD11EA_STATE_RUNNING ;
	pthread_mutex_unlock(&cpu->mutex) ;
}

// only to be called from kd11ea_condstep() thread

void
kd11ea_pwrfail_trap(KD11EA *cpu)
{
	cpu->traps |= TRAP_PWR;
}

// only to be called from kd11ea_condstep() thread
// if locked, will lock DATI and unibus adapter()!
void
kd11ea_pwrup_vector_fetch(KD11EA *cpu)
{
	// caller must have issued reset()
	// cpu->traps &= ~TRAP_PWR; // no, would be a fix
	INA(024, PC);
	BA = 024+2; if(dati(cpu, 0)) goto be;
	// through kd11ea_set_psw(): the vector may select user mode, which picks
	// the other stack pointer and address space
	kd11ea_set_psw(cpu, cpu->bus->data);
	return ;
be:
	trace("BE\n");
	cpu->be++ ;
}

void
kd11ea_condstep(KD11EA *cpu)
{
	if(cpu->state == KD11EA_STATE_RUNNING || cpu->state == KD11EA_STATE_WAITING)
		// GRANT Interrupts before opcode fetch, or when CPU is on WAIT
	unibone_grant_interrupts() ;

	if((cpu->state == KD11EA_STATE_RUNNING) ||
	   (cpu->state == KD11EA_STATE_WAITING && cpu->traps)
	   || (cpu->state == KD11EA_STATE_WAITING && cpu->external_intr) ){
		cpu->state = KD11EA_STATE_RUNNING;
		// external_intr WAIT handled atomically in kd11ea_setintr() !

		svc(cpu, cpu->bus);
		step(cpu);
	}
}

// ka11.c has a run() here, the driver of the standalone emulator.
// QUniBone steps the CPU from cpu_base_c::worker() instead.
