/* testbus.hpp: the fake QUNIBUS the CPU cores are tested against

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


 A CPU emulation core reaches the world only through the unibone_*() functions
 of 10.02_devices/2_src/cpu_bus_adapter.h. On a BeagleBone cpu.cpp implements
 them on top of qunibusadapter, the PRUs and a real backplane. Here they are
 implemented on top of a word array and two register stubs, which is all the
 MAINDEC instruction diagnostics need and which needs no hardware at all.

 Like cpu.cpp, this works on one installed instance at a time: the core calls
 free functions, so there is a single "current" testbus_c, see install().

 Everything here is deterministic - no threads, no wall clock, no randomness.
 The runner relies on that: a failing run is replayed identically with tracing
 armed, to print the instructions leading up to the failure.
 */
#ifndef _TESTBUS_HPP_
#define _TESTBUS_HPP_

#include <stdint.h>
#include <stdio.h>
#include <string>
#include <vector>

class testbus_c {
public:
    // The KL11 console. The MAINDECs report "end of pass, no errors" by
    // printing a BEL, which is how a test run is recognized as passed.
    // See svc_kl11() in cpu20/pdp11-master/kl11.c, where the same trick is
    // used by the AUTODIAG build of the upstream emulator.
    static const unsigned KL11_RCSR = 0777560;
    static const unsigned KL11_RBUF = 0777562;
    static const unsigned KL11_XCSR = 0777564;
    static const unsigned KL11_XBUF = 0777566;
    // KW11-L line clock. Answers, but never requests an interrupt: nothing on
    // this bus can, see unibone_grant_interrupts().
    static const unsigned KW11_LKS = 0777546;

    explicit testbus_c(unsigned memory_words);

    // make this the instance the unibone_*() functions work on
    void install(void);

    unsigned memory_words(void) const { return (unsigned) memory.size(); }
    // Deposit by the tape loader, bypassing the bus.
    // "addr" is a 16 bit byte address. Result false: outside of memory.
    bool mem_deposit(uint16_t addr, uint16_t value);

    /*** state observed by the runner ***/

    // a BEL was printed: the diagnostic completed a pass without errors
    bool bell = false;
    // everything else the diagnostic printed, kept back and only shown on failure
    std::string console_output;

    /*** tracing ***/

    // While false, unibone_log() returns immediately: formatting a trace line
    // per instruction costs more than the emulation itself, and a passing run
    // executes over 100 million of them.
    bool tracing = false;
    // where trace output goes while tracing is on
    FILE *trace_stream = stdout;

    /*** implementation of the cpu_bus_adapter.h contract ***/

    int dati(unsigned addr, unsigned *data);
    int dato(unsigned addr, unsigned data);
    int datob(unsigned addr, unsigned data);

private:
    // 16 bit word addressed. Physical addresses at or above this are I/O page
    // or non existing memory.
    std::vector<uint16_t> memory;

    // KL11 register state. Only what a diagnostic can observe.
    uint16_t kl11_rcsr = 0;
    uint16_t kl11_xcsr = 0200;	// transmitter ready from the start

    // Result 0 = nobody answered (bus timeout / NXM), 1 = handled.
    int io_read(unsigned addr, unsigned *data);
    int io_write(unsigned addr, unsigned data);

    void console_put(uint8_t c);
};

#endif
