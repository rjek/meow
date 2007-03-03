-- MEOW Assembler opcodes

register_stat("align_wastage", "Number of bytes wasted in alignments")

local function __queue_intermediate_label(info, streampos, labelname)
   _queue_bytes(info, find_label(nil, labelname))
   return true
end

local function __do_align(info, streampos, amount, barrier, alignval)
   local extra = math.mod(streampos, barrier)
   if extra > 0 then 
      _queue_bytes(info, string.rep(string.char(alignval), barrier-extra))
      stat_increment("align_wastage", barrier - extra)
   end
   return true -- Says that if I shrink, I meant it damnit, don't pad me.
end

function _do_align(info, streampos, barrier, alignval)
   return "defer", barrier - math.mod(streampos, barrier), __do_align, barrier, alignval
end

function meow_op_align(info, barrier, alignval)
   barrier = tonumber(barrier)
   barrier = barrier or 2
   alignval = tonumber(alignval) or 0
   if do_intermediate() then
      _queue_bytes(info, "\talign\t", tostring(barrier), "\t", tostring(alignval), "\n")
   else
      _queue_callback(info, _do_align, barrier, alignval)
   end
end

local function _resolve_address_dcd(info, streampos, amount, labelname)
   -- This time, the output stream is completed
   assert(amount == 4, "Amount is wrong in _resolve_address_dcd")
   local labelpos = find_label(streampos, labelname)
   condwhinge(labelpos == nil, info, "Label %s cannot be found", labelname)
   for i=1,4 do
      _queue_bytes(info, math.mod(labelpos,256))
      labelpos = math.floor(labelpos/256)
      stat_increment("data")
   end
end

local function _dcd_addrof_label(info, streampos, labelname)
   -- Assume that we're going to need four bytes to store
   -- the address and defer our synthesis until the stream is made
   return "defer", 4, _resolve_address_dcd, labelname
end

register_stat("data", "Bytes of data")

function meow_op_dcd(info, ...)
   local args = {...}
   local nargs = table.getn(args)
   condwhinge(nargs == 0, info, "DCD takes at minimum one argument.")
   if do_intermediate() then _queue_bytes(info, "\tdcd") end
   for i=1,nargs do
      local dec = parse_positional(info, args[i])
      condwhinge(dec.type ~= "constant" and dec.type ~= "label", info,
		 "DCD only supports immediate constants or labels.")
      if dec.type == "constant" then
	 if do_intermediate() then
	    _queue_bytes(info, "\t", args[i])
	 else
	    local v = dec.value
	    for i=1,4 do
	       _queue_bytes(info, math.mod(v,256))
	       v = math.floor(v/256)
	       stat_increment("data")
	    end
	 end
      else
	 if do_intermediate() then
	    _queue_bytes(info, "\t")
	    _queue_callback(info, __queue_intermediate_label, dec.value)
	 else
	    _queue_callback(info, _dcd_addrof_label, dec.value)
	 end
      end
   end
   if do_intermediate() then _queue_bytes(info, "\n") end
end

function meow_op_dcw(info, ...)
   local args = {...}
   local nargs = table.getn(args)
   condwhinge(nargs == 0, info, "DCW takes at minimum one argument.")
   if do_intermediate() then _queue_bytes(info, "\tdcw") end
   for i=1,nargs do
      local dec = parse_positional(info, args[i])
      condwhinge(dec.type ~= "constant", info, "DCW only supports immediate constants.")
      if dec.value < -32768 or dec.value > 65535 then
	 whinge(info, "DCW only supports constants in the range -32768 to 65535.")
      end
      if do_intermediate() then
	 _queue_bytes(info, "\t", args[i])
      else
	 if dec.value < 0 then dec.value = 65536 + dec.value end
	 local bottom = math.mod(dec.value, 256)
	 local top = math.floor(dec.value / 256)
	 stat_increment("data", 2)
	 _queue_bytes(info, bottom, top)
      end
   end
   if do_intermediate() then _queue_bytes(info, "\n") end
end

function meow_op_dcb(info, ...)
   local args = {...}
   local nargs = table.getn(args)
   condwhinge(nargs == 0, info, "DCB takes at minimum one argument.")
   if do_intermediate() then _queue_bytes(info, "\tdcb") end
   for i=1,nargs do
      local dec = parse_positional(info, args[i])
      condwhinge(dec.type ~= "constant" and dec.type ~= "string", info,
		 "DCB only supports immedate constants or literal strings.")
      if do_intermediate() then
	 _queue_bytes(info, "\t", args[i])
      else
	 if dec.type == "constant" then
	    if dec.value < -128 or dec.value > 255 then
	       whinge(info, "DCB only supports constants in the range -127 to 255.")
	    end
	    if dec.value < 0 then dec.value = 256 - dec.value end
	    _queue_bytes(info, dec.value)
	    stat_increment("data")
	 else
	    _queue_bytes(info, dec.value)
	    stat_increment("data", string.len(dec.value))
	 end
      end
   end
   if do_intermediate() then _queue_bytes(info, "\n") end
end

function meow_op_mov(info, ...)
   local args = {...}
   local args2 = {...}
   local nargs = table.getn(args)
   condwhinge(nargs < 2, info, "MOV takes at minimum two arguments.")
   local wordswap = false
   local byteswap = false
   local dest, source
   while nargs > 2 do
      if args[1] == "wordswap" then wordswap = true
      elseif args[1] == "byteswap" then byteswap = true
      else
	 whinge(info, "Unable to parse extra flag %s, expecting wordswap or byteswap.", args[1])
      end
      table.remove(args,1)
      nargs = nargs - 1
   end
   dest = parse_positional(info, args[1])
   source = parse_positional(info, args[2])
   condwhinge(dest.type ~= "register", info, "Source was a %s, expecting register.", dest.type)
   condwhinge(source.type ~= "register", info, "Destination was a %s, expecting register.", dest.type)
   -- We can always encode a mov at this point
   if do_intermediate() then
      _queue_bytes(info, "\tmov\t", table.concat(args2,"\t"), "\n")
   else
      _encode_mov(info, byteswap, wordswap, dest.alt, source.alt, dest.value, source.value)
   end
end

function _deferred_branch_callback(info, streampos, amount, condition, labelname)
   assert(amount==2,"Amount isn't 2 while performing deferred branch encoding")
   local labelpos = find_label(streampos, labelname)
   condwhinge(labelpos == nil, info, "Label %s cannot be found.", labelname)
   _encode_branch(info, condition, labelpos - streampos)
end

function _branch_callback(info, streampos, condition, target)
   -- Since a branch encodes to a single instruction always, let's encode it now
   if target.type == "constant" then
      _encode_branch(info, condition, target.value)
   else
      -- Since we have a label, defer with a 2 byte reservation
      return "defer", 2, _deferred_branch_callback, condition, target.value
   end
end

local branch_conditions = {
   eq = 0, ne = 1,
   cs = 2, cc = 3,
   mi = 4, pl = 5,
   vs = 6, vc = 7,
   hi = 8, ls = 9,
   ge = 10, lt = 11,
   gt = 12, le = 13,
   al = 14, nv = 15
}

function meow_op_branch(info, _condition, _target)
   target = parse_positional(info, _target)
   condwhinge(target.type ~= "label" and target.type ~= "constant", info, "B expects either a constant or a label.")

   condwhinge(branch_conditions[_condition] == nil, info, "Unknown branch condition code %s.", condition)
   condition = branch_conditions[_condition]

   if do_intermediate() then
      _queue_bytes(info, "\tbranch\t", _condition, "\t")
      if target.type == "label" then
	 _queue_callback(info, __queue_intermediate_label, target.value)
      else
	 _queue_bytes(info, _target)
      end
      _queue_bytes(info, "\n")
   else
      -- Nothing we can do for now except defer...
      _queue_callback(info, _branch_callback, condition, target)
   end
end

function meow_op_ldi(info, imm)
   local target = parse_positional(info, imm)
   condwhinge(target.type ~= "constant", info, "LDI expects a constant.")
   -- Simple (yay)
   if do_intermediate() then
      _queue_bytes(info, "\tldi\t", imm, "\n")
   else
      _encode_ldi(info, target.value)
   end
end

local function _mathop(info, is_add, _dest, _source, _value)
   dest = parse_positional(info, _dest)
   source = parse_positional(info, _source)
   condwhinge(dest.type ~= "register", info, "ADD/SUB require a register as their first argument.")
   condwhinge(dest.alt == true, info, "ADD/SUB cannot use the alternate register bank.")
   if not _value and source.type == "register" then _value = "#0" end
   if _value then
      condwhinge(source.type ~= "register", info, "ADD/SUB require a register as their source argument.")
      condwhinge(source.alt == true, info, "ADD/SUB cannot use the alternate register bank.")
      value = parse_positional(info, _value)
      condwhinge(value.type ~= "constant", info, "ADD/SUB require a constant as their third argument.")
   else
      condwhinge(source.type ~= "constant", info, "ADD/SUB requires a register or constant as the second argument.")
   end
   -- Simple again, encode it
   local v = value and value.value or false
   if do_intermediate() then
      _queue_bytes(info, "\t", is_add and "add" or "sub", "\t", _dest, "\t", _source)
      if source.type == "register" then
	 _queue_bytes(info, "\t", _value)
      end
      _queue_bytes(info, "\n")
   else
      _encode_mathop(info, is_add, dest.value, source.value, v)
   end
end

function meow_op_add(info, dest, source, value)
   _mathop(info, true, dest, source, value)
end

function meow_op_sub(info, dest, source, value)
   _mathop(info, false, dest, source, value)
end

function meow_op_shift(info, ...)
   local is_left, is_roll, is_arith, dest, value, _dest, _value = nil, false, false
   for i, v in ipairs({...}) do
      if v == "left" then is_left = true
      elseif v == "right" then is_left = false
      elseif v == "roll" then is_roll = true
      elseif v == "arithmetic" then is_arith = true
      else
	 if not dest then
	    _dest = v
	    dest = parse_positional(info, v)
	    condwhinge(dest.type ~= "register" or dest.alt == true, info,
		       "Shifts cannot use the alternate register bank.")
	 else
	    condwhinge(value ~= nil, info, "Shifts can only take two arguments.")
	    _value = value
	    value = parse_positional(info, v)
	    condwhinge(value.type ~= "constant" and value.type ~= "register", info,
		       "Shifts expect aregister or a constant as their second argument.")
	 end
      end
   end
   condwhinge(is_left == nil, info, "Shifts needs to be specifically left of right.")
   if do_intermediate() then
      _queue_bytes(info, "\tshift\t", is_left and "left\t" or "right\t", is_roll and "roll\t" or "", is_arith and "arithmetic\t" or "", _dest, "\t", _value, "\n")
   else
      _encode_shift(info, is_left, is_roll, is_arith, dest.value, value.type=="register", value.value)
   end
end

BITOP_NOT = 0
BITOP_AND = 1
BITOP_ORR = 2
BITOP_EOR = 3

function meow_op_bit(info, ...)
   local op, inverted, dest, value, _op, _dest, _value = nil, false
   for i, v in ipairs({...}) do
      if v == "not" then op, _op = BITOP_NOT, "not\t"
      elseif v == "and" then op, _op = BITOP_AND, "and\t" 
      elseif v == "orr" then op, _op = BITOP_ORR, "orr\t"
      elseif v == "eor" then op, _op = BITOP_EOR, "eor\t"
      elseif v == "inverted" then inverted = true
      else
	 if not dest then
	    _dest = v
	    dest = parse_positional(info, v)
	    condwhinge(dest.type ~= "register" or dest.alt == true, info,
		       "Bit operations cannot use the alternate register bank.")
	 else
	    _value = v
	    condwhinge(value ~= nil, info, "Bit operations can only have two arguments.")
	    value = parse_positional(info, v)
	    condwhinge(value.type ~= "constant" and value.type ~= "register", info,
		       "Bit operations expect a register or a constant as their second argument.")
	 end
      end
   end
   condwhinge(dest == nil, info, "No destination register supplied.")
   condwhinge(value == nil, info, "No value/reg supplied.")
   condwhinge(op == nil, info, "No operation supplied.")
   if do_intermediate() then
      _queue_bytes(info, "\tbit\t", _op, inverted and "inverted\t" or "", _dest, "\t", _value, "\n")
   else
      _encode_bit(info, op, inverted, dest.value, value.type=="register", value.value)
   end
end

function meow_op_mem(info, ...)
   local is_load, is_half, is_low, is_writeback, is_increase = nil, nil, true, false, nil
   local dest, src, _dest, _src
   for i, v in ipairs({...}) do
      if v == "load" then is_load = true
      elseif v == "store" then is_load = false
      elseif v == "half" then is_half = true
      elseif v == "byte" then is_half = false
      elseif v == "low" then is_low = true
      elseif v == "high" then is_low = false
      elseif v == "writeback" then is_writeback = true
      elseif v == "incrementing" then is_increase = true
      elseif v == "decrementing" then is_increase = false
      else
	 if not dest then
	    _dest = v
	    dest = parse_positional(info, v)
	    condwhinge(dest.type ~= "register" or dest.alt == true, info,
		       "Memory operations cannot use the alternate register bank.")
	 else
	    _src = v
	    condwhinge(src ~= nil, info, "mem ops take only two registers")
	    src = parse_positional(info, v)
	    condwhinge(src.type ~= "register" or src.alt == true, info,
		       "Memory operations cannot use the alternate register bank.")
	 end
      end
   end
   condwhinge(dest == nil, info, "No value register supplied.")
   condwhinge(src == nil, info, "No address register supplied.")
   condwhinge(is_load == nil, info, "Direction not supplied.")
   condwhinge(is_half == nil, info, "Size of transfer not supplied.")
   condwhinge(is_writeback and is_increase == nil, info, 
	      "Writeback requested but direction not provided.")
   if do_intermediate() then
      _queue_bytes(info, "\tmem\t", is_load and "load\t" or "store\t")
      _queue_bytes(info, is_half and "half\t" or "byte\t")
      _queue_bytes(info, is_low and "low\t" or "high\t")
      if is_writeback then
	 _queue_bytes(info, "writeback\t", is_increase and "incrementing\t" or "decrementing\t")
      end
      _queue_bytes(info, _dest, "\t", _src, "\n")
   else
      _encode_mem(info, is_load, is_half, is_low, is_writeback, is_increase, dest.value, src.value)
   end
end

function meow_op_tst(info, _reg, _bit)
   reg = parse_positional(info, _reg)
   bit = parse_positional(info, _bit)
   condwhinge(reg.type ~= "register", info, "TST takes a register as its first argument.")
   condwhinge(bit.type ~= "constant", info, "TST takes a constant as its second argument.")
   condwhinge(bit.value<0 or bit.value>31, info, "TST can only test bits in the range 0-31.")
   if do_intermediate() then
      _queue_bytes(info, "\ttst\t", _reg, "\t", _bit, "\n")
   else
      _encode_tst(info, reg.alt, reg.value, bit.value)
   end
end

function meow_op_cmp(info, _reg, _val)
   reg = parse_positional(info, _reg)
   val = parse_positional(info, _val)
   condwhinge(reg.type ~= "register", info, "CMP takes a register as its first argument.")
   condwhinge(val.type ~= "constant" and val.type ~= "register", info, "CMP takes a constant or register as its second argument.")
   if val.type == "constant" then
      condwhinge(val.value < -128 or val.value > 127, info, "CMP takes a value in the range -128 to 127.")
   end
   local alt = nil
   if val.type=="register" then alt=val.alt end
   if alt == nil then
      condwhinge(reg.alt == true, info, "CMP cannot operate on the alternate bank if it is comparing against an immediate.")
   end
   if do_intermediate() then
      _queue_bytes(info, "\tcmp\t", _reg, "\t", _val)
   else
      _encode_cmp(info, reg.alt, reg.value, alt, val.value)
   end
end

local function __cb_adr(info, streampos, amount, register, label)
   -- First up, work out the delta to where we want to address
   local delta = find_label(streampos, label) - streampos
   if delta == 0 then
      -- If the delta is zero, it's just capture pc
      _encode_mov(info, false, false, false, false, register, 15)
   elseif delta >= -15 and delta <= 15 then
      -- The next simplest form is add/sub
      local is_add = (delta > 0) and true or false
      _encode_mathop(info, is_add, register, 15, math.abs(delta))
   elseif delta >= -255-15 and delta <= 255+15 then
      -- add/sub/mov and then add/sub
      if (delta >= -255 and delta <= 255) then
         _encode_mov(info, false, false, false, false, register, 15)
      else
         _encode_mathop(info, delta>0 and true or false, register, 15, 15)
         if delta < 0 then delta = delta + 15 else delta = delta - 15 end
      end
      _encode_mathop(info, delta>0 and true or false, register, math.abs(delta))
   elseif delta >= -2048-15 and delta <= (2047+15) then
      -- Next simplest form is a mov/add/sub, then an ldi and then an add/sub
      if (delta >= -2048 and delta <= 2047) then
	 _encode_mov(info, false, false, false, false, register, 15)
      else
	 _encode_mathop(info, delta>0 and true or false, register, 15, 15)
	 if delta < 0 then delta = delta + 15 else delta = delta - 15 end
      end
      _encode_ldi(info, delta)
      _encode_mathop(info, true, register, 12, 0)
   else
      -- Unfortunately the user is trying to encode an ADR which is too big.
      whinge(info, "ADR target is out of range. (target is %d away, maximal range %d to %d)", delta, -2048-25, 2047+15)
   end
end

local function _cb_adr(info, streampos, register, label)
   -- At minimum, adr takes one instruction, typically 2, sometimes more
   return "defer", 2, __cb_adr, register, label
end

function meow_op_adr(info, _reg, label)
   reg = parse_positional(info, _reg)
   label = parse_positional(info, label)
   condwhinge(reg.type ~= "register" or label.type ~= "label", info,
	      "ADR takes a register and a label.")
   condwhinge(reg.alt == true, info,
	      "ADR can only operate on registers in the current bank.")
   if do_intermediate() then
      _queue_bytes(info, "\tadr\t", _reg, "\t")
      _queue_callback(info, __queue_intermediate_label, label.value)
      _queue_bytes(info, "\n")
   else
      _queue_callback(info, _cb_adr, reg.value, label.value)
   end
end

