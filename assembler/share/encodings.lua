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
