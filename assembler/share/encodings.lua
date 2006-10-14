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
   
   local targ = jump_by / 2

   -- Jump is 000c ccca aaaa aaaa so the upper byte is
   -- condition << 1 (*2) and 1 if targ is negative
   local upper_byte = condition * 2 + ((targ < 0) and 1 or 0)
   -- The lower byte is harder because it's the bottom eight bits of jump_by
   local lower_byte
   if targ >=0 and targ <= 255 then lower_byte = targ end
   if targ > 255 then lower_byte = targ - 256 end
   if targ < 0 then
      lower_byte = targ + 256
   end
   _queue_bytes(info, lower_byte, upper_byte)
   stat_increment "instructions"
end
