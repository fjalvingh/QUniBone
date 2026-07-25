/* kt11d.h: KT11-D memory management unit of the PDP-11/34

 The KT11-D relocates the 16 bit virtual addresses of the KD11-EA to 18 bit
 physical QUNIBUS addresses. It knows two processor modes (kernel and user),
 8 pages of 8KB each per mode, and aborts through vector 0250.

 Differences to the KT11-C of the 11/45, which is the variant most emulators
 implement:
 - no supervisor mode, no MMR3, no separate I and D space
 - no memory management *traps*, only aborts. The PDR has no A bit, and the
   ACF is 2 bits (PDR<2:1>) instead of 3.

 The MMU registers are NOT published as QUNIBUS registers: on real hardware
 the KT11-D sits inside the KD11-EA and does not answer as a bus slave. They
 are decoded by kd11ea.c dati()/dato() through kt11d_read_reg()/write_reg().
 That also keeps all MMU state in cached ARM memory, which matters: the
 translation runs on every single CPU memory access.

 Everything is prefixed kt11d_/KT11D, like the rest of cpu34/, because this
 core is linked into the same binary as the KA11 core of the 11/20.

 This header needs cpu_core.h to be included first.
 */
#ifndef _KT11D_H_
#define _KT11D_H_

/* access kinds, used as bitmask against kt11d_page_t.deny */
enum {
	KT11D_READ = 1, KT11D_WRITE = 2
};

/* address space selectors: index of the first page of a mode in par[]/pdr[] */
enum {
	KT11D_SPACE_KERNEL = 0, KT11D_SPACE_USER = 8
};

/* PSW<15:14> / PSW<13:12> mode encoding. 01 and 10 do not exist on the KD11-EA
 - 01 is the supervisor mode of the 11/45, which the 11/34 does not have - and
 a memory reference made in one of them aborts whatever it addresses. */
enum {
	KT11D_MODE_KERNEL = 0, KT11D_MODE_USER = 3
};

/* MMR0 bits */
enum {
	KT11D_MMR0_ABORT_NONRESIDENT = 0100000,	// <15>
	KT11D_MMR0_ABORT_LENGTH = 0040000,		// <14>
	KT11D_MMR0_ABORT_READONLY = 0020000,	// <13>
	KT11D_MMR0_ABORTS = 0160000,			// <15:13>: any of them freezes MMR0..2
	KT11D_MMR0_MAINT = 0000400,				// <8> maintenance/destination mode
	KT11D_MMR0_MODE = 0000140,				// <6:5> mode at time of abort
	KT11D_MMR0_PAGE = 0000016,				// <3:1> page at time of abort
	KT11D_MMR0_ENABLE = 0000001,			// <0> enable relocation
	// <15:13>, <8> and <0> are writable, <6:1> are maintained by the hardware
	KT11D_MMR0_WRITABLE = 0160401
};

/* PDR bits. <15>, <7> (the KT11-C "A" bit), <5:4> and <0> do not exist */
enum {
	KT11D_PDR_WRITABLE = 0077516,	// PLF<14:8>, W<6>, ED<3>, ACF<2:1>
	KT11D_PDR_W = 0000100,			// <6> page has been written into
	KT11D_PDR_ED = 0000010			// <3> 0 = expand upward, 1 = expand downward
};

/* the PAF of an 18 bit machine is 12 bits wide */
#define KT11D_PAR_WRITABLE	0007777

/* trap vector of a memory management abort */
#define KT11D_ABORT_VECTOR	0250

/* Precomputed form of one PAR/PDR pair, rebuilt whenever the pair is written.
 The layout keeps sizeof() a power of 2, so indexing the array needs no
 multiplication - same reason as for pru_iopage_register_t in iopageregister.h.
 */
typedef struct {
	uint32 base;	// (PAF<<6) - (page<<13), so physical = (base + va) & 0777777
	uint8 blk_lo;	// lowest block number inside the page
	uint8 blk_span;	// highest legal block number - blk_lo
	uint8 deny;		// bit-OR of KT11D_READ/KT11D_WRITE: accesses which abort
	uint8 pdr_idx;	// index into pdr[], to set the W bit
} kt11d_page_t;

typedef struct KT11D KT11D;
struct KT11D
{
	/* register images. These are authoritative, page[] is derived from them */
	word mmr0, mmr1, mmr2;
	word par[16];	// index = space + page
	word pdr[16];

	kt11d_page_t page[16];	// derived from par[]/pdr[], see kt11d_rebuild()

	/* hot path state */
	unsigned enabled;		// mmr0 & KT11D_MMR0_ENABLE
	unsigned space;			// address space of the current processor mode
	unsigned prev_space;	// address space of PSW<13:12>, for MFPI/MTPI
	unsigned mode;			// PSW<15:14> itself, which unlike space keeps the
							// two modes the KD11-EA does not have apart
	unsigned prev_mode;		// PSW<13:12> itself
	unsigned access_space;	// space the next dati()/dato() relocates through.
							// normally == space, temporarily overridden for
							// trap vector fetches (always kernel) and MFPI/MTPI
	unsigned access_mode;	// and the mode it is made in
	unsigned access_illegal;// derived: access_mode is neither kernel nor user
	unsigned frozen;		// mmr0 & KT11D_MMR0_ABORTS: stop updating MMR0<6:1>,
							// MMR1 and MMR2 until software clears the abort bits
	unsigned maint;			// mmr0 & KT11D_MMR0_MAINT
	unsigned dest_ref;		// the reference being made belongs to the destination
							// operand. Only matters in maintenance mode
	uint8 mmr1_count;		// # of register changes logged in mmr1 so far
};

void kt11d_reset(KT11D *mmu);
void kt11d_init(KT11D *mmu);
void kt11d_rebuild(KT11D *mmu, unsigned idx);
void kt11d_rebuild_all(KT11D *mmu);
void kt11d_set_modes(KT11D *mmu, word psw);
int kt11d_abort(KT11D *mmu, word va, unsigned space, unsigned access);
int kt11d_abort_mode(KT11D *mmu, word va);
int kt11d_read_reg(KT11D *mmu, uint32 pa, word *w);
int kt11d_write_reg(KT11D *mmu, uint32 pa, word w, word mask);
// render MMR0..MMR2 and the PAR/PDR blocks into a multi-line string.
// The registers are not visible on the bus, so this is the only way to see them.
void kt11d_format(KT11D *mmu, char *buf, size_t bufsize);

/* Select the address space and the processor mode the next reference is made
 in. Normally that is the current mode, but a trap vector fetch is always made
 in kernel mode and MFPI/MTPI make theirs in the previous mode. */
static inline void
kt11d_set_access(KT11D *mmu, unsigned space, unsigned mode)
{
	mmu->access_space = space;
	mmu->access_mode = mode;
	mmu->access_illegal = mode != KT11D_MODE_KERNEL && mode != KT11D_MODE_USER;
}

/* 16 -> 18 bit widening without relocation: the top 8K of the virtual space
 is the IO page. This is what the 11/20 does unconditionally. */
static inline uint32
kt11d_unrelocated(word va)
{
	return (va&0160000)==0160000 ? va|0600000 : va;
}

/* The translation itself, on the hot path of every CPU memory access.
 "access" is KT11D_READ or KT11D_WRITE.
 Result: 0 = ok and *pa valid, 1 = aborted, caller must trap through 0250.

 Maintenance mode (MMR0<8>) is the second way in: with relocation itself off it
 relocates the destination operand references of an instruction and nothing
 else, so that a diagnostic can compare a relocated address against the
 unrelocated one the same instruction formed for its source. It exists for
 maintenance only - no software uses it.
 */
static inline int
kt11d_relocate(KT11D *mmu, word va, unsigned space, unsigned access, uint32 *pa)
{
	kt11d_page_t *p;
	unsigned blk;

	if(!mmu->enabled && !(mmu->maint && mmu->dest_ref)){
		*pa = kt11d_unrelocated(va);
		return 0;
	}
	if(mmu->access_illegal)
		return kt11d_abort_mode(mmu, va);
	p = &mmu->page[space + (va >> 13)];
	blk = (va >> 6) & 0177;
	// One unsigned compare covers both expansion directions: for a page which
	// expands downward blk_lo is the PLF, for one expanding upward it is 0.
	if(p->deny & access)
		return kt11d_abort(mmu, va, space, access);
	if((unsigned)(blk - p->blk_lo) > p->blk_span)
		return kt11d_abort(mmu, va, space, access);
	if(access & KT11D_WRITE)
		mmu->pdr[p->pdr_idx] |= KT11D_PDR_W;	// only touches the raw PDR,
												// page[] does not depend on W
	*pa = (p->base + va) & 0777777;
	return 0;
}

/* called before every opcode fetch: MMR1 logs the register changes of the
 instruction about to be executed, so it starts empty. Frozen after an abort, so
 that the abort handler can undo the instruction.
 MMR2 is deliberately not touched here - see kt11d_instruction_fetched(). */
static inline void
kt11d_instruction_start(KT11D *mmu)
{
	if(mmu->frozen)
		return;
	mmu->mmr1 = 0;
	mmu->mmr1_count = 0;
}

/* called once the opcode fetch has succeeded, with the address it was made
 from: MMR2 holds the address of the instruction being executed.
 It is a separate step because MMR2 "is loaded with the 16-bit virtual address
 at the beginning of each instruction fetch, but is not updated if the
 instruction fetch is aborted" (PDP-11 Architecture Handbook). An aborted fetch
 therefore leaves MMR2 pointing at the previous instruction. MAINDEC DFKTF-A
 checks exactly that: it runs off the end of a page and expects MMR2 to address
 the last instruction inside it, not the one that could not be fetched. */
static inline void
kt11d_instruction_fetched(KT11D *mmu, word pc)
{
	if(mmu->frozen)
		return;
	mmu->mmr2 = pc;
}

/* log an autoincrement/autodecrement of a register into MMR1.
 Format per byte: <7:3> = amount added, 2's complement, <2:0> = register. */
static inline void
kt11d_log_register(KT11D *mmu, unsigned reg, int amount)
{
	word entry;
	if(mmu->frozen || mmu->mmr1_count >= 2)
		return;
	entry = ((amount & 037) << 3) | (reg & 7);
	if(mmu->mmr1_count == 0)
		mmu->mmr1 = entry;
	else
		mmu->mmr1 |= entry << 8;
	mmu->mmr1_count++;
}

#endif
