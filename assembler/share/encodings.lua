-- MEOW Assembler direct instruction encodings.

function _encode_mov(info, byteswap, wordswap, dest_alt, source_alt, dest_reg, source_reg)
   -- This form of mov cannot change size so we can encode it now
   local flags =
      (byteswap and 128 or 0) +
      (wordswap and 64 or 0) +
      (dest_alt and 32 or 0) +
      (source_alt and 16 or 0)
   -- backwards(ish) because the processor is littleendian remember :-)
   _queue_bytes(info, source_reg + flags, 128+dest_reg)
   stat_increment "instructions"
end

function _encode_branch(info, condition, jump_by)
   condwhinge(jump_by < -512 or jump_by > 510, info, "Branch can only jump forward or backward by by 256 instructions")
   condwhinge(math.mod(jump_by, 2) ~= 0, info, "Branch must jump to an instruction boundary, thus jumping by an odd number cannot happen.")
   
   local targ = to_bitfield(jump_by / 2, 9)

   -- Jump is 000c ccca aaaa aaaa so the upper byte is
   -- condition << 1 (*2) and 1 if targ is negative
   -- The lower byte is the bottom eight bits of jump_by
   local upper_bits = "000" .. to_bitfield(condition, 4) .. string.sub(targ,1,1)
   local lower_bits = string.sub(targ,2)
   _queue_bytes(info, from_bitfield(lower_bits), from_bitfield(upper_bits))
   stat_increment "instructions"
end

function _encode_ldi(info, value)
   condwhinge(value < -2048 or value > 2047, 
	      info, "ldi can only take constants between -2048 and 2047")
   local instr = "1001" .. to_bitfield(value, 12)
   local lower_bits = string.sub(instr,9)
   local upper_bits = string.sub(instr,1,8)
   _queue_bytes(info, from_bitfield(lower_bits), from_bitfield(upper_bits))
   stat_increment "instructions"
end

function _encode_mathop(info, is_add, destreg, source, value)
   local upper_bits = is_add and "001" or "010"
   local lower_bits
   if not value then
      -- encoding a two arg form, which is destreg +-= #source
      condwhinge(source < 0 or source > 255, info, "ADD/SUB can only use immediates in the range 0-255")
      upper_bits = upper_bits .. "1" .. to_bitfield(destreg, 4)
      lower_bits = to_bitfield(source, 8)
   else
      condwhinge(value < 0 or value > 15, info, "ADD/SUB can only use immediates in the range 0-15 when in three-argument form.")
      upper_bits = upper_bits .. "0"  .. to_bitfield(destreg, 4)
      lower_bits = to_bitfield(value, 4) .. to_bitfield(source, 4)
   end
   _queue_bytes(info, from_bitfield(lower_bits), from_bitfield(upper_bits))
   stat_increment "instructions"
end

function _encode_shift(info, is_left, is_roll, is_arith, destreg, amount_is_reg, amount)
   local upper_bits = "101" .. (is_arith and "1" or "0") .. to_bitfield(destreg, 4)
   local lower_bits = (is_left and "1" or "0") .. (is_roll and "1" or "0") .. (amount_is_reg and "10" or "0")
   if amount_is_reg == false then
      condwhinge(amount<0 or amount>31, info, "shifts can only be by 0-31 bits")
      lower_bits = lower_bits .. to_bitfield(amount,5)
   else
      lower_bits = lower_bits .. to_bitfield(amount,4)
   end
   _queue_bytes(info, from_bitfield(lower_bits), from_bitfield(upper_bits))
   stat_increment "instructions"
end

function _encode_bit(info, op, inverted, destreg, val_is_reg, val)
   local upper_bits = "110"..(inverted and "1" or "0")..to_bitfield(destreg, 4)
   local lower_bits = to_bitfield(op,2) .. (val_is_reg and "00" or "1")
   if val_is_reg then
      lower_bits = lower_bits .. to_bitfield(val, 4)
   else
      condwhinge(val<0 or val>31, info, "bit operations can only work on bits 0 to 31")
      lower_bits = lower_bits .. to_bitfield(val, 5)
   end
   _queue_bytes(info, from_bitfield(lower_bits), from_bitfield(upper_bits))
   stat_increment "instructions"
end


function _encode_mem(info, is_load, is_half, is_low, is_writeback, is_increase, valreg, addrreg)
   local upper_bits = "111" .. (is_load and "1" or "0") .. to_bitfield(valreg, 4)
   local lower_bits = (is_half and "1" or "0") .. (is_low and "1" or "0") .. (is_writeback and ("1" .. (is_increase and "1" or "0")) or "00") .. to_bitfield(addrreg, 4)
   _queue_bytes(info, from_bitfield(lower_bits), from_bitfield(upper_bits))
   stat_increment "instructions"
end

function _encode_tst(info, alt, reg, bit)
   local upper_bits = "0111" .. to_bitfield(reg,4)
   local lower_bits = "10"..(alt and "1" or "0") .. to_bitfield(bit,5)
   _queue_bytes(info, from_bitfield(lower_bits), from_bitfield(upper_bits))
   stat_increment "instructions"
end

