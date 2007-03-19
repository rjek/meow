/*
 * msim_debug.h
 * This file is part of MSIM, a MEOW Simulator
 *
 * Copyright (C) 2007 - Rob Kendrick <rjek@rjek.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to
 * do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */
 
#ifndef __MSIM_DEBUG_H__
#define __MSIM_DEBUG_H__

#include <stdbool.h>
#include <stdint.h>

#include "config.h"

#ifdef MSIM_WITH_LUA
#include MSIM_LUA_INCLUDE(lua.h)
#include MSIM_LUA_INCLUDE(lauxlib.h)
#endif

#define MSIM_DEBUG_BREAKPOINTS 10		/* max number of breakpoints */
#define MSIM_DEBUG_WATCHPOINTS 10		/* max number of watchpoints */

/* start a debugging session */
void msim_debugger(struct msim_ctx *ctx);

/* initialise debugging routines */
void msim_debug_init(struct msim_ctx *ctx);

/* finalise the debugging routines */
void msim_debug_fin(struct msim_ctx *ctx);

/* add a breakpoint at a specific address.  does nothing if there is already
 * a breakpoint at that location.  returns true on success.
 */
bool msim_breakpoint_add(struct msim_ctx *ctx, u_int32_t address);

/* returns true if there is a breakpoint at a specific location */
bool msim_breakpoint(struct msim_ctx *ctx, u_int32_t address);

/* removes a breakpoint from a specific location.  does nothing if there wasn't
 * one to start with.
 */
void msim_breakpoint_del(struct msim_ctx *ctx, u_int32_t address);

/* add a watchpoint.  returns true on success */
bool msim_watchpoint_add(struct msim_ctx *ctx, const char *expr);

/* check if a watchpoint has fired for the current state.  returns a pointer
 * to a string describing the watchpoint if so, or NULL if not.
 */
const char *msim_watchpoint(struct msim_ctx *ctx);

/* delete a numbered watchpoint */
void msim_watchpoint_del(struct msim_ctx *ctx, int watchpoint);

/* convience function that decodes and disassembles an instruction, doing
 * other steps for you.  Returned string points to a static buffer, copy it
 * before calling again.
 */
const char *msim_disassemble(u_int16_t instrword);

/* returns a string with the disassembled code pointed to by a decoded
 * instruction.
 */
char *msim_mnemonic(struct msim_ctx *ctx, char *buf, unsigned int bufl, 
			struct msim_instr *instr);
			
/* print out some useful debug information (contents of registers, etc) to
 * stdout.
 */
void msim_print_state(struct msim_ctx *ctx);

#endif
