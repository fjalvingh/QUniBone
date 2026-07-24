/* cputest.cpp: run one MAINDEC diagnostic against one CPU emulation core

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


 A run: load the paper tape at 0, start at 0200, step until

	- the diagnostic prints a BEL		-> it completed a pass with no errors: PASS
	- the CPU halts				-> a MAINDEC reports errors by halting: FAIL
	- the instruction limit is reached	-> hung, or looping without ever
						   finishing a pass: FAIL

 On failure the identical run is replayed with tracing armed shortly before the
 end, and the trace printed. The fake bus has no threads, no clock and no
 randomness, so the replay is exact. Tracing is not on in the first place
 because formatting a line per instruction costs several times the emulation
 itself, and a passing diagnostic executes 15 to 150 million of them.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#include "papertape.hpp"
#include "testbus.hpp"
#include "testcore.hpp"

// Where a diagnostic is started from, by DEC convention.
static const uint16_t start_pc = 0200;

struct options_t {
    const char *corename = nullptr;
    const char *tapepath = nullptr;
    // 400e6 is generous: the slowest of the ZKA* tapes, ZKADA0, rings the bell
    // after 147e6 instructions.
    uint64_t maxsteps = 400000000ULL;
    unsigned ram_words = 28 * 1024;	// 28K words, the maximum without memory management
    uint16_t switches = 0;
    // instructions to trace before the point of failure
    uint64_t tracelines = 500;
};

static void usage(const char *argv0)
{
    fprintf(stderr, "usage: %s --core <name> --tape <file.BIN> [options]\n", argv0);
    fprintf(stderr, "  --core <name>       CPU emulation core to test: %s\n",
            testcore_c::known_names());
    fprintf(stderr, "  --tape <file.BIN>   MAINDEC diagnostic, absolute loader format\n");
    fprintf(stderr, "  --maxsteps <n>      give up after n instructions\n");
    fprintf(stderr, "  --ram-words <n>     words of memory below the I/O page\n");
    fprintf(stderr, "  --sw <octal>        console switch register\n");
    fprintf(stderr, "  --tracelines <n>    instructions to trace before a failure (0 = none)\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Settings may also be put in a \"<file.BIN>.opt\" next to the tape, as\n");
    fprintf(stderr, "\"key = value\" lines using the option names without the leading dashes.\n");
    fprintf(stderr, "Exit code 0 = passed, 1 = failed, 2 = usage or file error.\n");
}

// Read the optional "<tape>.opt" sidecar. Result: error message, or empty.
static std::string read_tape_options(const char *tapepath, options_t &opt)
{
    std::string path = std::string(tapepath) + ".opt";
    FILE *f = fopen(path.c_str(), "r");
    if (f == nullptr)
        return "";	// no sidecar is the normal case

    char line[256];
    unsigned linenr = 0;
    std::string error;
    while (fgets(line, sizeof(line), f)) {
        linenr++;
        char *s = line;
        while (*s == ' ' || *s == '\t')
            s++;
        if (*s == '#' || *s == '\n' || *s == '\r' || *s == 0)
            continue;
        char key[64], value[128];
        if (sscanf(s, "%63[^= \t] %*[= \t] %127s", key, value) != 2) {
            char buff[256];
            sprintf(buff, "%s(%u): expected \"key = value\"", path.c_str(), linenr);
            error = buff;
            break;
        }
        if (!strcmp(key, "maxsteps"))
            opt.maxsteps = strtoull(value, nullptr, 0);
        else if (!strcmp(key, "ram-words"))
            opt.ram_words = (unsigned) strtoul(value, nullptr, 0);
        else if (!strcmp(key, "sw"))
            opt.switches = (uint16_t) strtoul(value, nullptr, 8);
        else if (!strcmp(key, "tracelines"))
            opt.tracelines = strtoull(value, nullptr, 0);
        else {
            char buff[256];
            sprintf(buff, "%s(%u): unknown option \"%s\"", path.c_str(), linenr, key);
            error = buff;
            break;
        }
    }
    fclose(f);
    return error;
}

enum result_e {
    result_passed,	// BEL: end of pass, no errors
    result_halted,	// MAINDECs report an error by halting
    result_hung		// instruction limit reached
};

// One complete run. "trace_from" is the instruction count at which tracing is
// armed; pass "steps" >= maxsteps to trace nothing.
// Returns the outcome, and in "steps" the number of instructions executed.
static result_e run(testcore_c *core, testbus_c &bus, const options_t &opt, uint64_t trace_from,
                    uint64_t *steps)
{
    bus.install();
    core->power_reset();
    core->set_switches(opt.switches);
    core->set_pc(start_pc);
    core->set_state(testcore_c::state_running);

    uint64_t n = 0;
    while (core->get_state() != testcore_c::state_halted && !bus.bell && n < opt.maxsteps) {
        if (n == trace_from)
            bus.tracing = true;
        core->condstep();
        n++;
    }
    bus.tracing = false;
    *steps = n;

    if (bus.bell)
        return result_passed;
    if (core->get_state() == testcore_c::state_halted)
        return result_halted;
    return result_hung;
}

int main(int argc, char **argv)
{
    options_t opt;

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        const char *v = (i + 1 < argc) ? argv[i + 1] : nullptr;
        if (!strcmp(a, "--help") || !strcmp(a, "-h")) {
            usage(argv[0]);
            return 2;
        } else if (v == nullptr) {
            fprintf(stderr, "%s: option \"%s\" needs a value\n", argv[0], a);
            return 2;
        } else if (!strcmp(a, "--core")) {
            opt.corename = v, i++;
        } else if (!strcmp(a, "--tape")) {
            opt.tapepath = v, i++;
        } else if (!strcmp(a, "--maxsteps")) {
            opt.maxsteps = strtoull(v, nullptr, 0), i++;
        } else if (!strcmp(a, "--ram-words")) {
            opt.ram_words = (unsigned) strtoul(v, nullptr, 0), i++;
        } else if (!strcmp(a, "--sw")) {
            opt.switches = (uint16_t) strtoul(v, nullptr, 8), i++;
        } else if (!strcmp(a, "--tracelines")) {
            opt.tracelines = strtoull(v, nullptr, 0), i++;
        } else {
            fprintf(stderr, "%s: unknown option \"%s\"\n", argv[0], a);
            usage(argv[0]);
            return 2;
        }
    }
    if (opt.corename == nullptr || opt.tapepath == nullptr) {
        usage(argv[0]);
        return 2;
    }

    std::string error = read_tape_options(opt.tapepath, opt);
    if (!error.empty()) {
        fprintf(stderr, "%s\n", error.c_str());
        return 2;
    }

    // the tape name without directory, for the one line result
    const char *tapename = strrchr(opt.tapepath, '/');
    tapename = tapename ? tapename + 1 : opt.tapepath;

    testcore_c *core = testcore_c::create(opt.corename);
    if (core == nullptr) {
        fprintf(stderr, "%s: unknown core \"%s\", known are: %s\n", argv[0], opt.corename,
                testcore_c::known_names());
        return 2;
    }

    testbus_c bus(opt.ram_words);
    bus.install();	// the loader reports memory overflows against it
    error = papertape_load(opt.tapepath, bus);
    if (!error.empty()) {
        fprintf(stderr, "%s\n", error.c_str());
        delete core;
        return 2;
    }

    uint64_t steps = 0;
    result_e result = run(core, bus, opt, opt.maxsteps, &steps);

    if (result == result_passed) {
        printf("PASS  %-6s %-12s (%llu instructions)\n", opt.corename, tapename,
               (unsigned long long) steps);
        delete core;
        return 0;
    }

    /* Failed. Say what happened, then replay to show how it got there. */
    printf("FAIL  %-6s %-12s %s at PC %06o, after %llu instructions\n", opt.corename, tapename,
           result == result_halted ? "CPU halted" : "instruction limit reached", core->get_pc(),
           (unsigned long long) steps);
    if (!bus.console_output.empty())
        printf("--- diagnostic printed ---\n%s\n", bus.console_output.c_str());
    printf("--- CPU state ---\n");
    core->printstate();

    if (opt.tracelines > 0) {
        uint64_t trace_from = steps > opt.tracelines ? steps - opt.tracelines : 0;
        printf("--- last %llu instructions before the failure ---\n",
               (unsigned long long)(steps - trace_from));
        delete core;
        core = testcore_c::create(opt.corename);
        testbus_c replay_bus(opt.ram_words);
        replay_bus.install();
        error = papertape_load(opt.tapepath, replay_bus);
        if (error.empty()) {
            replay_bus.trace_stream = stdout;
            uint64_t replay_steps = 0;
            run(core, replay_bus, opt, trace_from, &replay_steps);
            if (replay_steps != steps)
                printf("(replay diverged: %llu instructions instead of %llu - the run is not "
                       "deterministic, which is a bug in the test harness)\n",
                       (unsigned long long) replay_steps, (unsigned long long) steps);
        }
    }

    delete core;
    return 1;
}
