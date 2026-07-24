/* testbus.cpp: the fake QUNIBUS the CPU cores are tested against

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

 This is the test suite counterpart of the unibone_*() implementations in
 10.02_devices/2_src/cpu.cpp - same contract, no hardware.
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "cpu_bus_adapter.h"
#include "testbus.hpp"

// The cores call free functions, so one instance is "installed" at a time,
// exactly like unibone_cpu in cpu.cpp.
static testbus_c *installed_bus = nullptr;

testbus_c::testbus_c(unsigned memory_words) :
    memory(memory_words, 0)
{
}

void testbus_c::install(void)
{
    installed_bus = this;
}

bool testbus_c::mem_deposit(uint16_t addr, uint16_t value)
{
    if ((unsigned)(addr >> 1) >= memory.size())
        return false;
    memory[addr >> 1] = value;
    return true;
}

void testbus_c::console_put(uint8_t c)
{
    if (c == '\a') {
        // MAINDEC: end of pass, no errors. This is the pass criterion.
        bell = true;
        return;
    }
    // Held back rather than printed: a passing diagnostic prints its banner and
    // nothing else of interest, and only a failing run's output is worth seeing.
    if (c != 0)
        console_output.push_back((char) c);
}

/*** the I/O page ***/

int testbus_c::io_read(unsigned addr, unsigned *data)
{
    switch (addr) {
    case KL11_RCSR:
        *data = kl11_rcsr;
        return 1;
    case KL11_RBUF:
        // no keyboard on this bus: always reads 0, and clears DONE
        kl11_rcsr &= ~0200;
        *data = 0;
        return 1;
    case KL11_XCSR:
        *data = kl11_xcsr;
        return 1;
    case KL11_XBUF:
        // write only, but answers
        *data = 0;
        return 1;
    case KW11_LKS:
        // line clock, never ticks: MONITOR stays 0
        *data = 0;
        return 1;
    }
    return 0;	// NXM
}

int testbus_c::io_write(unsigned addr, unsigned data)
{
    switch (addr) {
    case KL11_RCSR:
        // only the interrupt enable is writable, and nothing can interrupt here
        kl11_rcsr = (uint16_t)((kl11_rcsr & ~0100) | (data & 0100));
        return 1;
    case KL11_RBUF:
        return 1;	// read only, but answers
    case KL11_XCSR:
        kl11_xcsr = (uint16_t)((kl11_xcsr & ~0100) | (data & 0100) | 0200);
        return 1;
    case KL11_XBUF:
        console_put((uint8_t)(data & 0177));
        kl11_xcsr |= 0200;	// transmitter ready again immediately
        return 1;
    case KW11_LKS:
        return 1;
    }
    return 0;	// NXM
}

/*** the cpu_bus_adapter.h contract ***/

int testbus_c::dati(unsigned addr, unsigned *data)
{
    if ((addr >> 1) < memory.size()) {
        *data = memory[addr >> 1];
        return 1;
    }
    return io_read(addr & ~1u, data);
}

int testbus_c::dato(unsigned addr, unsigned data)
{
    if ((addr >> 1) < memory.size()) {
        memory[addr >> 1] = (uint16_t) data;
        return 1;
    }
    return io_write(addr & ~1u, data);
}

int testbus_c::datob(unsigned addr, unsigned data)
{
    /* The byte is passed in the half of "data" selected by the address: bits
     <7:0> for an even address, bits <15:8> for an odd one. Same convention as
     unibone_datob() in cpu.cpp, and as the "mask" in kd11ea.c dato(). Getting
     this backwards silently corrupts every odd byte write - and is caught by
     ZKACA0, ZKAEA0 and ZKAHA0. */
    if ((addr >> 1) < memory.size()) {
        uint16_t w = memory[addr >> 1];
        if (addr & 1)
            w = (uint16_t)((w & 0000377) | (data & 0177400));
        else
            w = (uint16_t)((w & 0177400) | (data & 0000377));
        memory[addr >> 1] = w;
        return 1;
    }
    // the register stubs here have no byte granularity worth modelling
    if (addr & 1)
        return io_write(addr & ~1u, (data >> 8) & 0377);
    return io_write(addr, data & 0377);
}

/*** free functions the emulation cores call, see cpu_bus_adapter.h ***/

extern "C" {

void unibone_log(unsigned msglevel, const char *srcfilename, unsigned srcline, const char *fmt,
                 ...)
{
    (void) msglevel;
    (void) srcfilename;
    (void) srcline;
    testbus_c *bus = installed_bus;
    if (bus == nullptr || !bus->tracing)
        return;	// the fast path: a passing diagnostic executes >100e6 of these
    va_list arg_ptr;
    va_start(arg_ptr, fmt);
    vfprintf(bus->trace_stream, fmt, arg_ptr);
    va_end(arg_ptr);
}

void unibone_logdump(void)
{
}

int unibone_dati(unsigned addr, unsigned *data)
{
    return installed_bus->dati(addr, data);
}

int unibone_dato(unsigned addr, unsigned data)
{
    return installed_bus->dato(addr, data);
}

int unibone_datob(unsigned addr, unsigned data)
{
    return installed_bus->datob(addr, data);
}

// Nothing on this bus requests an interrupt, so there is never a GRANT to give.
void unibone_grant_interrupts(void)
{
}

void unibone_prioritylevelchange(uint8_t level)
{
    (void) level;	// no arbitrator to tell
}

// RESET opcode. No devices to initialize.
void unibone_bus_init(void)
{
}

bool unibone_trace_enabled(void)
{
    return installed_bus != nullptr && installed_bus->tracing;
}

// No address filter: when tracing is on, everything is traced.
bool unibone_trace_addr(uint16_t a)
{
    (void) a;
    return installed_bus != nullptr && installed_bus->tracing;
}

}
