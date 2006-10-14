-- MEOW Assembler opcodes

register_stat("align_wastage", "Number of bytes wasted in alignments")

local function _do_align(info, streampos, barrier, alignval)
   local extra = math.mod(streampos, barrier)
   if extra == 0 then return end
   _queue_bytes(info, string.rep(string.char(alignval), barrier-extra))
   stat_increment("align_wastage", barrier - extra)
end

function meow_op_align(info, barrier, alignval)
   barrier = tonumber(barrier)
   barrier = barrier or 2
   alignval = tonumber(alignval) or 0
   _queue_callback(info, _do_align, barrier, alignval)
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
   condwhinge(nargs == 0, info, "dcd takes at minimum one argument")
   for i=1,nargs do
      local dec = parse_positional(info, args[i])
      condwhinge(dec.type ~= "constant" and dec.type ~= "label", info,
		 "dcd only supports immediate constants or labels")
      if dec.type == "constant" then
	 local v = dec.value
	 for i=1,4 do
	    _queue_bytes(info, math.mod(v,256))
	    v = math.floor(v/256)
	    stat_increment("data")
	 end
      else
	 _queue_callback(info, _dcd_addrof_label, dec.value)
      end
   end
end

function meow_op_dcw(info, ...)
   local args = {...}
   local nargs = table.getn(args)
   condwhinge(nargs == 0, info, "dcw takes at minimum one argument")
   for i=1,nargs do
      local dec = parse_positional(info, args[i])
      condwhinge(dec.type ~= "constant", info, "dcw only supports immediate constants.")
      if dec.value < -32768 or dec.value > 65535 then
	 whinge(info, "dcw only supports constants in the range -32768 to 65535")
      end
      if dec.value < 0 then dec.value = 65536 + dec.value end
      local bottom = math.mod(dec.value, 256)
      local top = math.floor(dec.value / 256)
      stat_increment("data", 2)
      _queue_bytes(info, bottom, top)
   end
end

function meow_op_dcb(info, ...)
   local args = {...}
   local nargs = table.getn(args)
   condwhinge(nargs == 0, info, "dcb takes at minimum one argument")
   for i=1,nargs do
      local dec = parse_positional(info, args[i])
      condwhinge(dec.type ~= "constant" and dec.type ~= "string", info,
		 "dcb only supports immedate constants or literal strings.")
      if dec.type == "constant" then
	 if dec.value < -128 or dec.value > 255 then
	    whinge(info, "dcb only supports constants in the range -127 to 255")
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

function meow_op_mov(info, ...)
   local args = {...}
   local nargs = table.getn(args)
   condwhinge(nargs < 2, info, "mov takes at minimum two arguments")
   local wordswap = false
   local byteswap = false
   local dest, source
   while nargs > 2 do
      if args[1] == "wordswap" then wordswap = true
      elseif args[1] == "byteswap" then byteswap = true
      else
	 whinge(info, "Unable to parse extra flag %s, expecting wordswap or byteswap", args[1])
      end
      table.remove(args,1)
      nargs = nargs - 1
   end
   dest = parse_positional(info, args[1])
   source = parse_positional(info, args[2])
   condwhinge(dest.type ~= "register", info, "Source was a %s, expecting register", dest.type)
   condwhinge(source.type ~= "register", info, "Destination was a %s, expecting register", dest.type)
   -- We can always encode a mov at this point
   _encode_mov(info, byteswap, wordswap, dest.alt, source.alt, dest.value, source.value)
end

function _deferred_branch_callback(info, streampos, amount, condition, labelname)
   assert(amount==2,"Amount isn't 2 while performing deferred branch encoding")
   local labelpos = find_label(streampos, labelname)
   condwhinge(labelpos == nil, info, "Label %s cannot be found", labelname)
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

function meow_op_branch(info, condition, target)
   target = parse_positional(info, target)
   condwhinge(target.type ~= "label" and target.type ~= "constant", info, "Branch expects either a constant or a label")

   condwhinge(branch_conditions[condition] == nil, info, "Unknown branch condition code %s", condition)
   condition = branch_conditions[condition]


   -- Nothing we can do for now except defer...
   _queue_callback(info, _branch_callback, condition, target)
end

function meow_op_ldi(info, imm)
   local target = parse_positional(info, imm)
   condwhinge(target.type ~= "constant", info, "ldi expects a constant")
   -- Simple (yay)
   _encode_ldi(info, target.value)
end

local function _mathop(info, is_add, dest, source, value)
   dest = parse_positional(info, dest)
   source = parse_positional(info, source)
   condwhinge(dest.type ~= "register", info, "ADD/SUB require a register as their first argument")
   condwhinge(dest.alt == true, info, "ADD/SUB cannot use the alternate register bank")
   if not value and source.type == "register" then value = "#0" end
   if value then
      condwhinge(source.type ~= "register", info, "ADD/SUB require a register as their source argument")
      condwhinge(source.alt == true, info, "ADD/SUB cannot use the alternate register bank")
      value = parse_positional(info, value)
      condwhinge(value.type ~= "constant", info, "ADD/SUB require a constant as their third argument")
   else
      condwhinge(source.type ~= "constant", info, "ADD/SUB requires a register or constant as the second argument")
   end
   -- Simple again, encode it
   local v = value and value.value or false
   _encode_mathop(info, is_add, dest.value, source.value, value and value.value or false)
end

function meow_op_add(info, dest, source, value)
   _mathop(info, true, dest, source, value)
end

function meow_op_sub(info, dest, source, value)
   _mathop(info, false, dest, source, value)
end

function meow_op_shift(info, ...)
   local is_left, is_roll, is_arith, dest, value = nil, false, false
   for i, v in ipairs({...}) do
      if v == "left" then is_left = true
      elseif v == "right" then is_left = false
      elseif v == "roll" then is_roll = true
      elseif v == "arithmetic" then is_arith = true
      else
	 if not dest then
	    dest = parse_positional(info, v)
	    condwhinge(dest.type ~= "register" or dest.alt == true, info,
		       "shift must be given a non-alternate register as its first argument")
	 else
	    condwhinge(value ~= nil, info, "shift can only take two arguments")
	    value = parse_positional(info, v)
	    condwhinge(value.type ~= "constant" and value.type ~= "register", info,
		       "shift's second argument must be either a register or a constant")
	 end
      end
   end
   condwhinge(is_left == nil, info, "shift needs to be told left vs. right")
   _encode_shift(info, is_left, is_roll, is_arith, dest.value, value.type=="register", value.value)
end

function meow_op_bit(info, ...)
   local op, inverted, dest, value = nil, false
   for i, v in ipairs({...}) do
      if v == "not" then op = 0
      elseif v == "and" then op = 1
      elseif v == "orr" then op = 2
      elseif v == "eor" then op = 3
      elseif v == "inverted" then inverted = true
      else
	 if not dest then
	    dest = parse_positional(info, v)
	    condwhinge(dest.type ~= "register" or dest.alt == true, info,
		       "bit ops can only be performed on the current register bank.")
	 else
	    condwhinge(value ~= nil, info, "bit ops can only have two arguments")
	    value = parse_positional(info, v)
	    condwhinge(value.type ~= "constant" and value.type ~= "register", info,
		       "bit ops second argument must be a register or a constant")
	 end
      end
   end
   condwhinge(dest == nil, info, "No destination register supplied")
   condwhinge(value == nil, info, "No value/reg supplied")
   condwhinge(op == nil, info, "No operation supplied")
   _encode_bit(info, op, inverted, dest.value, value.type=="register", value.value)
end

function meow_op_mem(info, ...)
   local is_load, is_half, is_low, is_writeback, is_increase = nil, nil, true, false, nil
   local dest, src
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
	    dest = parse_positional(info, v)
	    condwhinge(dest.type ~= "register" or dest.alt == true, info,
		       "mem ops can only be performed on the current register bank.")
	 else
	    condwhinge(src ~= nil, info, "mem ops take only two registers")
	    src = parse_positional(info, v)
	    condwhinge(src.type ~= "register" or src.alt == true, info,
		       "mem ops can only be performed from the current register bank.")
	 end
      end
   end
   condwhinge(dest == nil, info, "No value register supplied")
   condwhinge(src == nil, info, "No address register supplied")
   condwhinge(is_load == nil, info, "Direction not supplied")
   condwhinge(is_half == nil, info, "Size of transfer not supplied")
   condwhinge(is_writeback and is_increase == nil, info, 
	      "Writeback requested but direction not provided")
   _encode_mem(info, is_load, is_half, is_low, is_writeback, is_increase, dest.value, src.value)
end

