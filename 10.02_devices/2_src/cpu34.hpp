/* cpu34.hpp: PDP-11/34 CPU, emulated by the KD11-EA core

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


 24-jul-2026  JH      created

 Work in progress. The KD11-EA core in cpu34/ is a fork of the 11/20 KA11 core
 and still executes the 11/20 instruction set, with EIS, MFPS/MTPS and the
 11/34 SWAB behaviour switched on permanently.

 TODO 11/34: KT11-D memory management. It needs MMR0..MMR3 and the kernel/user
 PAR/PDR blocks. Unlike the 11/20, which publishes nothing, this CPU will then
 have to declare QUNIBUS registers: set register_count and fill registers[],
 as e.g. rl11.cpp does, and handle them in on_after_register_access().
 Until then register_count stays 0 (set by cpu_base_c).
 */
#ifndef _CPU34_HPP_
#define _CPU34_HPP_

#include "cpu.hpp"

// The KD11-EA core is plain C, see cpu34/kd11ea.h and cpu34/11.h.
// Only cpu34.cpp includes those headers, so that cpu20.hpp and cpu34.hpp
// can be used together in one compilation unit.
struct KD11EA;
struct Bus;

class cpu34_c: public cpu_base_c {
public:

    cpu34_c();
    ~cpu34_c();

    struct Bus *bus; // UNIBUS interface of CPU
    struct KD11EA *kd11ea; // CPU state

    // interface to the KD11-EA emulation core
    void core_condstep(void) override;
    void core_reset(void) override;
    void core_setintr(uint16_t vector) override;
    void core_pwrfail_trap(void) override;
    void core_pwrup_vector_fetch(void) override;
    void core_printstate(void) override;
    void core_tracestate(void) override;
    enum cpu_state_e core_get_state(void) override;
    void core_set_state(enum cpu_state_e state) override;
    uint16_t core_get_pc(void) override;
    void core_set_pc(uint16_t value) override;
    void core_set_switches(uint16_t value) override;
    // no core_apply_options(): the 11/34 has no CPU feature parameters.
    // EIS and MFPS/MTPS are always executed, SWAB always clears the V bit.
};

#endif
