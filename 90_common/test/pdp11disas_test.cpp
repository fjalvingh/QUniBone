/* pdp11disas_test.cpp: tests for 90_common/src/pdp11disas.cpp

 Copyright (c) 2026, Joerg Hoppe
 j_hoppe@t-online.de, www.retrocmp.com

 Permission is hereby granted, free of charge, to any person obtaining a
 copy of this software and associated documentation files (the "Software"),
 to deal in the Software without restriction, including without limitation
 the rights to use, copy, modify, merge, publish, distribute, sublicense,
 and/or sell copies of the Software, and to permit persons to whom the
 Software is furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 JOERG HOPPE BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


 A disassembler is a table plus a lot of small rules, and a wrong table entry
 produces text which looks perfectly plausible. So the cases below are written
 as "these words are this text", against the DEC instruction set documentation,
 and they cover every addressing mode, every operand class and every
 instruction set option - not just a sample.

 Three things get their own attention, because they are what a disassembler
 usually gets wrong:

 - the *length* of an instruction. An index or immediate word belongs to the
   instruction and must not be disassembled again as an opcode. A wrong length
   does not produce one wrong line, it derails everything that follows.
 - the pc relative modes 6 and 7 and the branch targets, which are computed
   against the PC *after* the word which holds the offset.
 - the availability flagging: the same word is a valid instruction on one
   machine and a trap on another.

 Exit code 0 = all passed, 1 = a case failed, 2 = usage error.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

#include "pdp11disas.hpp"

static unsigned cases_run = 0, cases_failed = 0;
static bool verbose = false;

static void check(const char *what, const std::string& expect, const std::string& actual)
{
	cases_run++;
	if (expect == actual) {
		if (verbose)
			printf("ok    %-40s -> \"%s\"\n", what, actual.c_str());
		return;
	}
	cases_failed++;
	printf("FAIL  %s\n", what);
	printf("        expected: \"%s\"\n", expect.c_str());
	printf("        actual:   \"%s\"\n", actual.c_str());
}

static void check_unsigned(const char *what, unsigned expect, unsigned actual)
{
	char buffer[32];
	sprintf(buffer, "%u", expect);
	std::string e(buffer);
	sprintf(buffer, "%u", actual);
	check(what, e, std::string(buffer));
}

// One instruction, given as up to 3 words at address 001000.
// <expect_text> is what text() must produce, <expect_words> the length.
static void one(const pdp11disas_options_c& options, const char *title, uint16_t w0, uint16_t w1,
		uint16_t w2, unsigned expect_words, const char *expect_text)
{
	uint16_t words[3] = { w0, w1, w2 };
	pdp11disas_buffermemory_c memory(words, 3, 001000);
	pdp11disas_instruction_c instr;
	char what[160];

	uint32_t next = pdp11disas_instruction(options, memory, 001000, &instr);

	sprintf(what, "%s [%06o]", title, w0);
	check(what, std::string(expect_text), instr.text());

	sprintf(what, "%s [%06o] wordcount", title, w0);
	check_unsigned(what, expect_words, instr.wordcount);

	sprintf(what, "%s [%06o] next address", title, w0);
	check_unsigned(what, 001000 + 2 * expect_words, next);
}

// the same, for an instruction which needs no extra words
static void one1(const pdp11disas_options_c& options, const char *title, uint16_t w0,
		const char *expect_text)
{
	one(options, title, w0, 0, 0, 1, expect_text);
}

int main(int argc, char **argv)
{
	if (argc > 2) {
		fprintf(stderr, "usage: %s [-v]\n", argv[0]);
		return 2;
	}
	if (argc == 2) {
		if (strcmp(argv[1], "-v")) {
			fprintf(stderr, "usage: %s [-v]\n", argv[0]);
			return 2;
		}
		verbose = true;
	}

	printf("pdp11disas_test: PDP-11 disassembler\n");

	// a machine with everything, so the instruction text is tested without
	// availability comments getting in the way
	pdp11disas_options_c all;
	all.set_cpu_model("11/94");
	all.set_option("cis", true);
	all.set_option("fis", true);

	pdp11disas_options_c cpu20;	// the default: PDP-11/20, no options

	/**********************************************************
	 * the CPU model and option table
	 **********************************************************/
	{
		pdp11disas_options_c o;
		check("default model", "11/20", o.cpu_model);
		check("default options", "none", pdp11disas_options_c::options_as_text(o.options));

		check("model 11/34 accepted", "1", std::string(o.set_cpu_model("11/34") ? "1" : "0"));
		check("model 11/34", "11/34", o.cpu_model);
		check("model 11/34 options", "eis mmu mfps sxs mark rtt",
				pdp11disas_options_c::options_as_text(o.options));
		// the FP11 was an option on the 11/34, not part of the model
		check("11/34 has no fp11", "0", std::string(o.has(pdp11disas_opt_fp11) ? "1" : "0"));

		check("model \"34\" accepted", "1", std::string(o.set_cpu_model("34") ? "1" : "0"));
		check("model \"1134\" accepted", "1", std::string(o.set_cpu_model("1134") ? "1" : "0"));
		check("model \"PDP-11/34\" accepted", "1",
				std::string(o.set_cpu_model("PDP-11/34") ? "1" : "0"));
		check("model \"11/99\" rejected", "0", std::string(o.set_cpu_model("11/99") ? "1" : "0"));
		check("model unchanged after reject", "11/34", o.cpu_model);

		check("option fp11 accepted", "1", std::string(o.set_option("fp11", true) ? "1" : "0"));
		check("option fp11 on", "1", std::string(o.has(pdp11disas_opt_fp11) ? "1" : "0"));
		check("option fp11 off", "1", std::string(o.set_option("fp11", false) ? "1" : "0"));
		check("option fp11 is off", "0", std::string(o.has(pdp11disas_opt_fp11) ? "1" : "0"));
		check("option \"nosuch\" rejected", "0",
				std::string(o.set_option("nosuch", true) ? "1" : "0"));
		// setting the model resets the options to the model's own set
		o.set_option("cis", true);
		o.set_cpu_model("11/20");
		check("model reset clears options", "none",
				pdp11disas_options_c::options_as_text(o.options));

		check("11/20 model text", "cpu pdp-11/20, options: none", o.as_text());
		check("11/70 options", "eis fp11 mmu spl sxs mark rtt",
				pdp11disas_options_c::options_as_text(
						(o.set_cpu_model("11/70"), o.options)));
	}

	/**********************************************************
	 * addressing modes, source and destination
	 **********************************************************/
	// mov <mode>,r0 : source operand of a double operand instruction
	one1(all, "mode 0 src", 0010100, "mov     r1,r0");
	one1(all, "mode 1 src", 0011100, "mov     (r1),r0");
	one1(all, "mode 2 src", 0012100, "mov     (r1)+,r0");
	one1(all, "mode 3 src", 0013100, "mov     @(r1)+,r0");
	one1(all, "mode 4 src", 0014100, "mov     -(r1),r0");
	one1(all, "mode 5 src", 0015100, "mov     @-(r1),r0");
	one(all, "mode 6 src", 0016100, 0000004, 0, 2, "mov     000004(r1),r0");
	one(all, "mode 7 src", 0017100, 0000004, 0, 2, "mov     @000004(r1),r0");
	// register names
	one1(all, "sp", 0010600, "mov     sp,r0");
	one1(all, "pc as plain register", 0010700, "mov     pc,r0");
	// the pc modes
	one(all, "immediate", 0012700, 0000377, 0, 2, "mov     #000377,r0");
	one(all, "absolute", 0013700, 0177570, 0, 2, "mov     @#177570,r0");
	// relative: target = address of the word after the offset word + offset
	one(all, "relative", 0016700, 0000004, 0, 2, "mov     001010,r0");
	one(all, "relative deferred", 0017700, 0000004, 0, 2, "mov     @001010,r0");
	one(all, "relative backwards", 0016700, 0177772, 0, 2, "mov     000776,r0");
	// destination operand, and both at once: two extra words
	one(all, "mode 6 dst", 0010160, 0000004, 0, 2, "mov     r1,000004(r0)");
	one(all, "two index words", 0016564, 0000004, 0000010, 3, "mov     000004(r5),000010(r4)");
	one(all, "immediate and absolute", 0012737, 0000100, 0177566, 3, "mov     #000100,@#177566");
	// the classic: mov #x,sp
	one(all, "mov #1776,sp", 0012706, 0001776, 0, 2, "mov     #001776,sp");

	/**********************************************************
	 * double and single operand instructions, byte forms
	 **********************************************************/
	one1(all, "mov", 0010001, "mov     r0,r1");
	one1(all, "cmp", 0020001, "cmp     r0,r1");
	one1(all, "bit", 0030001, "bit     r0,r1");
	one1(all, "bic", 0040001, "bic     r0,r1");
	one1(all, "bis", 0050001, "bis     r0,r1");
	one1(all, "add", 0060001, "add     r0,r1");
	one1(all, "sub", 0160001, "sub     r0,r1");
	one1(all, "movb", 0110001, "movb    r0,r1");
	one1(all, "cmpb", 0120001, "cmpb    r0,r1");
	one1(all, "bitb", 0130001, "bitb    r0,r1");
	one1(all, "bicb", 0140001, "bicb    r0,r1");
	one1(all, "bisb", 0150001, "bisb    r0,r1");
	one1(all, "clr", 0005000, "clr     r0");
	one1(all, "com", 0005100, "com     r0");
	one1(all, "inc", 0005200, "inc     r0");
	one1(all, "dec", 0005300, "dec     r0");
	one1(all, "neg", 0005400, "neg     r0");
	one1(all, "adc", 0005500, "adc     r0");
	one1(all, "sbc", 0005600, "sbc     r0");
	one1(all, "tst", 0005700, "tst     r0");
	one1(all, "ror", 0006000, "ror     r0");
	one1(all, "rol", 0006100, "rol     r0");
	one1(all, "asr", 0006200, "asr     r0");
	one1(all, "asl", 0006300, "asl     r0");
	one1(all, "clrb", 0105000, "clrb    r0");
	one1(all, "tstb", 0105737 - 037, "tstb    r0");
	one(all, "tstb @#", 0105737, 0177560, 0, 2, "tstb    @#177560");
	one1(all, "aslb", 0106300, "aslb    r0");
	one1(all, "swab", 0000300, "swab    r0");
	one(all, "jmp absolute", 0000137, 0001234, 0, 2, "jmp     @#001234");
	one1(all, "jmp (r0)", 0000110, "jmp     (r0)");

	/**********************************************************
	 * branches, jsr/rts, traps, condition codes
	 **********************************************************/
	one1(all, "br forward", 0000402, "br      001006");
	one1(all, "br backwards", 0000776, "br      000776");
	one1(all, "br to itself", 0000777, "br      001000");
	one1(all, "bne", 0001002, "bne     001006");
	one1(all, "beq", 0001402, "beq     001006");
	one1(all, "bge", 0002002, "bge     001006");
	one1(all, "blt", 0002402, "blt     001006");
	one1(all, "bgt", 0003002, "bgt     001006");
	one1(all, "ble", 0003402, "ble     001006");
	one1(all, "bpl", 0100002, "bpl     001006");
	one1(all, "bmi", 0100402, "bmi     001006");
	one1(all, "bhi", 0101002, "bhi     001006");
	one1(all, "blos", 0101402, "blos    001006");
	one1(all, "bvc", 0102002, "bvc     001006");
	one1(all, "bvs", 0102402, "bvs     001006");
	one1(all, "bcc", 0103002, "bcc     001006");
	one1(all, "bcs", 0103402, "bcs     001006");
	one1(all, "jsr pc,dst", 0004767 - 067 + 010, "jsr     pc,(r0)");
	one(all, "jsr pc,relative", 0004767, 0000004, 0, 2, "jsr     pc,001010");
	one1(all, "rts pc", 0000207, "rts     pc");
	one1(all, "rts r5", 0000205, "rts     r5");
	one1(all, "emt", 0104000, "emt     0");
	one1(all, "emt 377", 0104377, "emt     377");
	one1(all, "trap 0", 0104400, "trap    0");
	one1(all, "trap 77", 0104477, "trap    77");
	one1(all, "halt", 0000000, "halt");
	one1(all, "wait", 0000001, "wait");
	one1(all, "rti", 0000002, "rti");
	one1(all, "bpt", 0000003, "bpt");
	one1(all, "iot", 0000004, "iot");
	one1(all, "reset", 0000005, "reset");
	one1(all, "rtt", 0000006, "rtt");
	one1(all, "mfpt", 0000007, "mfpt");
	one1(all, "nop", 0000240, "nop");
	one1(all, "clc", 0000241, "clc");
	one1(all, "clv", 0000242, "clv");
	one1(all, "clz", 0000244, "clz");
	one1(all, "cln", 0000250, "cln");
	one1(all, "ccc", 0000257, "ccc");
	one1(all, "sec", 0000261, "sec");
	one1(all, "sev", 0000262, "sev");
	one1(all, "sez", 0000264, "sez");
	one1(all, "sen", 0000270, "sen");
	one1(all, "scc", 0000277, "scc");
	// the combinations, incl. the two SimH has wrong
	one1(all, "clz clv", 0000246, "clz clv");
	one1(all, "cln clz clv", 0000256, "cln clz clv");
	one1(all, "sen sez sev", 0000276, "sen sez sev");
	one1(all, "spl 7", 0000237, "spl     7");
	one1(all, "spl 0", 0000230, "spl     0");
	one1(all, "spl 5", 0000235, "spl     5");

	/**********************************************************
	 * EIS, SXS, MMU, MFPS, MARK
	 **********************************************************/
	one1(all, "mul", 0070001, "mul     r1,r0");
	one1(all, "div", 0071001, "div     r1,r0");
	one1(all, "ash", 0072001, "ash     r1,r0");
	one1(all, "ashc", 0073001, "ashc    r1,r0");
	one(all, "mul #", 0070027, 0000012, 0, 2, "mul     #000012,r0");
	one1(all, "xor", 0074001, "xor     r0,r1");
	one1(all, "sob", 0077201, "sob     r2,001000");	// (001000+2) - 2*1
	one1(all, "sob far", 0077277, "sob     r2,000604");	// (001000+2) - 2*63
	one1(all, "mark", 0006400, "mark    0");
	one1(all, "mark 4", 0006404, "mark    4");
	one1(all, "mfpi", 0006516, "mfpi    (sp)");
	one1(all, "mtpi", 0006616, "mtpi    (sp)");
	one1(all, "sxt", 0006700, "sxt     r0");
	one1(all, "mfpd", 0106516, "mfpd    (sp)");
	one1(all, "mtpd", 0106616, "mtpd    (sp)");
	one1(all, "mfps", 0106700, "mfps    r0");
	one1(all, "mtps", 0106400, "mtps    r0");
	one1(all, "csm", 0007000, "csm     r0");
	one1(all, "tstset", 0007200, "tstset  r0");
	one1(all, "wrtlck", 0007300, "wrtlck  r0");

	/**********************************************************
	 * FIS
	 **********************************************************/
	one1(all, "fadd", 0075000, "fadd    r0");
	one1(all, "fsub", 0075011, "fsub    r1");
	one1(all, "fmul", 0075022, "fmul    r2");
	one1(all, "fdiv", 0075033, "fdiv    r3");

	/**********************************************************
	 * FP11
	 **********************************************************/
	one1(all, "cfcc", 0170000, "cfcc");
	one1(all, "setf", 0170001, "setf");
	one1(all, "seti", 0170002, "seti");
	one1(all, "setd", 0170011, "setd");
	one1(all, "setl", 0170012, "setl");
	one1(all, "ldfps", 0170100, "ldfps   r0");
	one1(all, "stfps", 0170200, "stfps   r0");
	one1(all, "stst", 0170300, "stst    r0");
	// float operands: register mode names an accumulator, not a register
	one1(all, "clrf ac0", 0170400, "clrf    ac0");
	one1(all, "clrf (r1)", 0170411, "clrf    (r1)");
	one1(all, "tstf", 0170500, "tstf    ac0");
	one1(all, "absf", 0170600, "absf    ac0");
	one1(all, "negf", 0170700, "negf    ac0");
	one1(all, "mulf ac0", 0171000, "mulf    ac0,ac0");
	one1(all, "mulf ac3", 0171300, "mulf    ac0,ac3");
	one1(all, "modf", 0171400, "modf    ac0,ac0");
	one1(all, "addf", 0172000, "addf    ac0,ac0");
	one1(all, "ldf", 0172400, "ldf     ac0,ac0");
	one1(all, "ldf (r2),ac1", 0172512, "ldf     (r2),ac1");
	one1(all, "subf", 0173000, "subf    ac0,ac0");
	one1(all, "cmpf", 0173400, "cmpf    ac0,ac0");
	one1(all, "stf", 0174000, "stf     ac0,ac0");
	one1(all, "stf ac2,(r3)", 0174213, "stf     ac2,(r3)");
	one1(all, "divf", 0174400, "divf    ac0,ac0");
	one1(all, "stexp", 0175000, "stexp   ac0,r0");
	one1(all, "stexp ac1,r2", 0175102, "stexp   ac1,r2");
	one1(all, "stcfi", 0175400, "stcfi   ac0,r0");
	one1(all, "stcfd", 0176000, "stcfd   ac0,ac0");
	one1(all, "ldexp", 0176400, "ldexp   r0,ac0");
	one1(all, "ldcif", 0177000, "ldcif   r0,ac0");
	one1(all, "ldcfd", 0177400, "ldcfd   ac0,ac0");
	// the f/d and i/l spellings, which only the live FPS register decides
	{
		pdp11disas_options_c fpd = all;
		fpd.set_option("fpd", true);
		one1(fpd, "clrd", 0170400, "clrd    ac0");
		one1(fpd, "muld", 0171000, "muld    ac0,ac0");
		one1(fpd, "ldd", 0172400, "ldd     ac0,ac0");
		one1(fpd, "std", 0174000, "std     ac0,ac0");
		one1(fpd, "stcdi", 0175400, "stcdi   ac0,r0");
		one1(fpd, "stcdf", 0176000, "stcdf   ac0,ac0");
		one1(fpd, "ldcid", 0177000, "ldcid   r0,ac0");
		one1(fpd, "ldcdf", 0177400, "ldcdf   ac0,ac0");
		// "fpd" does not touch the instructions without an f/d form
		one1(fpd, "stexp with fpd", 0175000, "stexp   ac0,r0");

		pdp11disas_options_c fpl = all;
		fpl.set_option("fpl", true);
		one1(fpl, "stcfl", 0175400, "stcfl   ac0,r0");
		one1(fpl, "ldclf", 0177000, "ldclf   r0,ac0");
		one1(fpl, "clrf with fpl", 0170400, "clrf    ac0");

		pdp11disas_options_c fpdl = all;
		fpdl.set_option("fpd", true);
		fpdl.set_option("fpl", true);
		one1(fpdl, "stcdl", 0175400, "stcdl   ac0,r0");
		one1(fpdl, "ldcld", 0177000, "ldcld   r0,ac0");
	}

	/**********************************************************
	 * CIS
	 **********************************************************/
	one1(all, "l2dr", 0076020, "l2dr    r0");
	one1(all, "l3dr", 0076063, "l3dr    r3");
	one1(all, "movc", 0076030, "movc");
	one1(all, "movrc", 0076031, "movrc");
	one1(all, "movtc", 0076032, "movtc");
	one1(all, "locc", 0076040, "locc");
	one1(all, "matc", 0076045, "matc");
	one1(all, "addn", 0076050, "addn");
	one1(all, "cvtln", 0076057, "cvtln");
	one1(all, "addp", 0076070, "addp");
	one1(all, "cvtlp", 0076077, "cvtlp");
	// the in-line forms: only the opcode word belongs to the instruction,
	// the descriptors behind it are data
	one(all, "movci", 0076130, 0000010, 0001234, 1, "movci");
	one1(all, "cvtlpi", 0076177, "cvtlpi");
	one1(all, "ashni", 0076156, "ashni");

	/**********************************************************
	 * availability flagging: the same word on different machines
	 **********************************************************/
	one1(cpu20, "mul on 11/20", 0070001, "mul     r1,r0           ; eis not on pdp-11/20");
	one1(cpu20, "sxt on 11/20", 0006700, "sxt     r0              ; sxs not on pdp-11/20");
	one1(cpu20, "sob on 11/20", 0077201, "sob     r2,001000       ; sxs not on pdp-11/20");
	one1(cpu20, "mfpi on 11/20", 0006516, "mfpi    (sp)            ; mmu not on pdp-11/20");
	one1(cpu20, "mfps on 11/20", 0106700, "mfps    r0              ; mfps not on pdp-11/20");
	one1(cpu20, "spl on 11/20", 0000237, "spl     7               ; spl not on pdp-11/20");
	one1(cpu20, "rtt on 11/20", 0000006, "rtt                     ; rtt not on pdp-11/20");
	one1(cpu20, "mark on 11/20", 0006400, "mark    0               ; mark not on pdp-11/20");
	one1(cpu20, "clrf on 11/20", 0170400, "clrf    ac0             ; fp11 not on pdp-11/20");
	one1(cpu20, "movc on 11/20", 0076030, "movc                    ; cis not on pdp-11/20");
	one1(cpu20, "fadd on 11/20", 0075000, "fadd    r0              ; fis not on pdp-11/20");
	// the base set is never flagged
	one1(cpu20, "mov on 11/20", 0010001, "mov     r0,r1");
	one1(cpu20, "swab on 11/20", 0000300, "swab    r0");
	{
		pdp11disas_options_c cpu34;
		cpu34.set_cpu_model("11/34");
		one1(cpu34, "mul on 11/34", 0070001, "mul     r1,r0");
		one1(cpu34, "sxt on 11/34", 0006700, "sxt     r0");
		one1(cpu34, "mfps on 11/34", 0106700, "mfps    r0");
		one1(cpu34, "spl on 11/34", 0000237, "spl     7               ; spl not on pdp-11/34");
		one1(cpu34, "clrf on 11/34", 0170400, "clrf    ac0             ; fp11 not on pdp-11/34");
		// the FP11 was an option: switching it on stops the flagging
		cpu34.set_option("fp11", true);
		one1(cpu34, "clrf on 11/34 with fp11", 0170400, "clrf    ac0");

		pdp11disas_options_c cpu70;
		cpu70.set_cpu_model("11/70");
		one1(cpu70, "spl on 11/70", 0000237, "spl     7");
		one1(cpu70, "clrf on 11/70", 0170400, "clrf    ac0");
		one1(cpu70, "mfps on 11/70", 0106700, "mfps    r0              ; mfps not on pdp-11/70");
		one1(cpu70, "csm on 11/70", 0007000, "csm     r0              ; csm not on pdp-11/70");
	}

	/**********************************************************
	 * words which are no instruction, and unreadable memory
	 **********************************************************/
	one1(all, "reserved 000010", 0000010, ".word   000010");
	one1(all, "reserved 000077", 0000077, ".word   000077");
	one1(all, "reserved 007100", 0007100, ".word   007100");
	one1(all, "reserved 075040", 0075040, ".word   075040");
	one1(all, "reserved 076000", 0076000, ".word   076000");
	one1(all, "reserved 107000", 0107000, ".word   107000");
	one1(all, "reserved 170003", 0170003, ".word   170003");
	{
		// an instruction whose index word is not in memory any more
		uint16_t words[1] = { 0016700 };
		pdp11disas_buffermemory_c memory(words, 1, 001000);
		pdp11disas_instruction_c instr;
		uint32_t next = pdp11disas_instruction(all, memory, 001000, &instr);
		check("truncated instruction", ".word   016700          ; instruction incomplete, "
				"memory not readable", instr.text());
		check_unsigned("truncated wordcount", 1, instr.wordcount);
		check_unsigned("truncated next address", 001002, next);

		// nothing readable at all
		pdp11disas_buffermemory_c empty(words, 0, 001000);
		pdp11disas_instruction(all, empty, 001000, &instr);
		check_unsigned("unreadable wordcount", 0, instr.wordcount);
		check("unreadable comment", "memory not readable", instr.comment());
	}

	/**********************************************************
	 * the listing line, and a whole region
	 **********************************************************/
	{
		// a small boot loader style program
		static const uint16_t program[] = { //
				0012706, 0001776,	// mov #1776,sp
						0012701, 0177560,	// mov #177560,r1
						0105711,		// tstb (r1)
						0100376,		// bpl .-2
						0116100, 0000002,	// movb 2(r1),r0
						0000167, 0177764,	// jmp .-20
						0000000			// halt
				};
		pdp11disas_buffermemory_c memory(program, sizeof(program) / sizeof(program[0]), 001000);
		std::vector<pdp11disas_instruction_c> instructions;
		uint32_t next = pdp11disas_region(all, memory, 001000, 20, &instructions);

		check_unsigned("region instruction count", 7, (unsigned) instructions.size());
		check_unsigned("region next address", 001026, next);
		check("region line 1", "001000  012706 001776         mov     #001776,sp",
				instructions[0].listing_line());
		check("region line 2", "001004  012701 177560         mov     #177560,r1",
				instructions[1].listing_line());
		check("region line 3", "001010  105711                tstb    (r1)",
				instructions[2].listing_line());
		check("region line 4", "001012  100376                bpl     001010",
				instructions[3].listing_line());
		check("region line 5", "001014  116100 000002         movb    000002(r1),r0",
				instructions[4].listing_line());
		check("region line 6", "001020  000167 177764         jmp     001010",
				instructions[5].listing_line());
		check("region line 7", "001024  000000                halt", instructions[6].listing_line());

		// a region which runs out of memory stops, it does not loop
		std::vector<pdp11disas_instruction_c> tail;
		pdp11disas_region(all, memory, 001024, 10, &tail);
		check_unsigned("region stops at end of memory", 1, (unsigned) tail.size());

		// 22 bit bus addresses get 8 octal digits
		pdp11disas_buffermemory_c highmemory(program, 2, 017000000);
		std::vector<pdp11disas_instruction_c> high;
		pdp11disas_region(all, highmemory, 017000000, 1, &high);
		check("22 bit address", "17000000  012706 001776         mov     #001776,sp",
				high[0].listing_line());
	}

	printf("\n%u cases, %u failed\n", cases_run, cases_failed);
	return cases_failed ? 1 : 0;
}
