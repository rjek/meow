-- -*- Lua -*-

-- The MEOW assembler defines manager/resolver

local defines = {}
local define_info = {}

function resolve_defines(str)
   local ret = str
   local seen = { [ret] = true }
   while defines[ret] do 
      ret = defines[ret]
      if seen[ret] then
	 verbose(0, "Loop in resolving definition of `%s`, stopping.", str)
	 os.exit(1)
      end
      seen[ret] = true
   end
   return ret
end

function meow_op_define(info, what, is)
   if define_info[what] then
      whinge(info, "Redefinition of token `%s` previously defined in %s on line %d", what, define_info[what].file, define_info[what].num)
   end
   defines[what] = is
   define_info[what] = info
end

