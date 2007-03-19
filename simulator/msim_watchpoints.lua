-- msim_watchpoints.lua
-- This file is part of MSIM, a MEOW Simulator
--
-- Copyright (C) 2007 - Rob Kendrick <rjek@rjek.com>
--
-- Permission is hereby granted, free of charge, to any person obtaining a copy
-- of this software and associated documentation files (the "Software"), to
-- deal in the Software without restriction, including without limitation the
-- rights to use, copy, modify, merge, publish, distribute, and/or sell copies
-- of the Software, and to permit persons to whom the Software is furnished to
-- do so, subject to the following conditions:
-- 
-- The above copyright notice and this permission notice shall be included in
-- all copies or substantial portions of the Software.
-- 
-- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
-- IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
-- FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
-- AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
-- LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
-- FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
-- IN THE SOFTWARE.

-- Lua-side of the watchpoint system
	
sp = 11		-- stack pointer alias
lr = 12		-- link register alias
ir = 13		-- immediate register alias
sr = 14		-- status register alias
pc = 15		-- program counter alias

SP, LR, IR, SR, PC = sp, lr, ir, sr, pc

do
	-- keep functions from baselib that we need
	local error = error
	local type = type
	
	-- keep things from msim_debug.c that we need
	local ctx = ctx
	local getr = getr
	local getar = getar
	local getword = getword
	local gethalf = gethalf
	local getbyte = getbyte

	r = setmetatable({ }, {
		__newindex = function(table, key, value)
			-- do nothing - just ignore the set
		end,
	
		__index = function(table, key)
			if type(key) ~= "number" then
				error "r[] must take a register number"
			end
		
			if key < 0 or key > 15 then
				error "register number out of range for r[]"
			end
		
			return getr(key, ctx);
		end
	})

	ar = setmetatable({ }, {
		__newindex = function(table, key, value)
			-- do nothing - just ignore the set
		end,
		
		__index = function(table, key)
			if type(key) ~= "number" then
				error "ar[] must take a register number"
			end
			
			if key < 0 or key > 15 then
				error "register number out of range for ar[]"
			end
			
			return getar(key, ctx);
		end
	})
	
	word = setmetatable({ }, {
		__newindex = function(table, key, value)
			-- do nothing - just ignore the set
		end,
		
		__index = function(table, key)
			if type(key) ~= "number" then
				error "word[] must take an address"
			end
			
			return getword(key, ctx);
		end
	})
	
	half = setmetatable({ }, {
		__newindex = function(table, key, value)
			-- do nothing - just ignore the set
		end,
		
		__index = function(table, key)
			if type(key) ~= "number" then
				error "half[] must take an address"
			end
			
			return gethalf(key, ctx);
		end
	})
	
	byte = setmetatable({ }, {
		__newindex = function(table, key, value)
			-- do nothing - just ignore the set
		end,
		
		__index = function(table, key)
			if type(key) ~= "number" then
				error "byte[] must take an address"
			end
			
			return getbyte(key, ctx);
		end
	})
end

R, AR, WORD, HALF, BYTE = r, ar, word, half, byte

-- Now remove the functions and userdata the C side put here for us to use,
-- such that the user can't fidget with them.

ctx, getr, getar, getword, gethalf, getbyte = nil

-- other parts of the Lua environment that we don't want (imported by baselib)
_G, _VERSION, ipairs, pairs, newproxy, assert, collectgarbage, dofile, 
 	gcinfo, getfenv, getmetatable, loadfile, load, loadstring, next, pcall,
 	print, rawequal, rawget, rawset, error, select, setfenv, setmetatable,
 	tonumber, tostring, type, unpack, xpcall = nil
