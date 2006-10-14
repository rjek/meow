-- MEOW Assembler opcodes

function meow_op_mov(info, ...)
   local args = {...}
   local nargs = table.getn(args)
   condwhinge(nargs < 2, info, "mov takes at minimum two arguments")
   
end