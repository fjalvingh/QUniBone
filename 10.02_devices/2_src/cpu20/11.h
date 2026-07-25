/* 11.h: basic types and macros of the KA11 (PDP-11/20) emulation core

 From Angelo Papenhoff's standalone aap emulator, trimmed to what ka11.c
 uses. The declarations of the standalone emulator's other components
 (terminal server, Memory, KE11 arithmetic element) are gone; the vendored
 originals remain under pdp11-master/. The interface to the ARM side is
 shared with the 11/34 core and lives in cpu_bus_adapter.h.

 This header is private to cpu20/: it must never be included together with
 cpu34/11.h in the same compilation unit.
 */
#ifndef _CPU20_11_H_
#define _CPU20_11_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

// unibone_*() interface to the QUNIBUS adapter, plus trace()
#include "cpu_bus_adapter.h"

typedef uint8_t uint8, byte;
typedef uint16_t uint16, word;
typedef uint32_t uint32;

#define WD(hi, lo) W((hi)<<8 | (lo))
#define W(w) ((word)(w))
#define M8  0377
#define M16 0177777
#define B7  0000200
#define B15 0100000
#define B31 0x80000000L
#define nil NULL

#define SETMASK(l, r, m) l = (((l)&~(m)) | ((r)&(m)))

typedef struct Bus Bus;
typedef struct Busdev Busdev;

struct Busdev
{
	Busdev *next;
	void *dev;
	int (*dati)(Bus *bus, void *dev);
	int (*dato)(Bus *bus, void *dev);
	int (*datob)(Bus *bus, void *dev);
	int (*svc)(Bus *bus, void *dev);
	int (*bg)(void *dev);
	void (*reset)(void *dev);
};

struct Bus
{
	Busdev *devs;
	uint32 addr;
	word data;
};

#endif
