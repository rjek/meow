-- -*- Lua -*-

-- The macro system for the MEOW Assembler

local macro_bodies = {}


function is_macro(str)
   return macro_bodies[str] ~= nil
end

register_stat("macros", "Number of macros defined")
register_stat("macrolines", "Number of lines of macro definitions")

function meow_op_macro(info, macroname, ...)
   -- Macros are a bit odd because we consume from underneath the parser...
   local macro = {args = {...}}
   macro.nargs = table.getn(macro.args)
   macro.info = info
   local line = parser_steal_line()
   while line do
      if string.find(string.lower(line), "^[^;]+endmacro") then
	 macro_bodies[macroname] = macro
	 stat_increment "macros"
	 return
      end
      table.insert(macro, line)
      stat_increment("macrolines")
      line = parser_steal_line()
   end
   whinge(info, "Macro `%s` does not terminate before the end of input", macroname)
end

local function macro_playback_read(f, arg)
   if arg ~= "*l" then
      error("macro_playback_read called with arg " .. tostring(arg))
   end
   local ret = f.macro[f.line]
   if not ret then return end
   for k, v in pairs(f.subst) do
      local fixed_k = string.gsub(k, "([^a-z0-9])", function(s)return "%"..s end)
      ret = string.gsub(ret, fixed_k, v)
   end
   f.line = f.line + 1
   return ret
end

register_stat("macrorun", "Number of macro invocations", true)

function run_macro(info, macroname, ...)
   local passed_args = {...}
   local npargs = table.getn(passed_args)
   if not is_macro(macroname) then
      whinge(info, "%s is not a macro yet run_macro was invoked", macroname)
   end
   if npargs ~= macro_bodies[macroname].nargs then
      whinge(info, "%s takes %d argument%s, yet %d provided", 
	     macroname, macro_bodies[macroname].nargs, 
	     (macro_bodies[macroname].nargs == 1) and "" or "s",
	     npargs)
   end
   -- Okay, we know we want to rerun this macro, we have been provided with a bunch of arguments
   -- so let's get on with it...
   local substtable = {}
   local macro = macro_bodies[macroname]
   for i, v in ipairs(passed_args) do
      substtable[macro.args[i]] = v
   end
   -- substtable now contains a bunch of substitutions...
   local filename = string.format("MACRO:%s (%s:%d)", macroname, macro.info.file, macro.info.num)
   filename = filename .. " invoked from " .. string.format("%s:%d", info.file, info.num)
   local file_like_obj = { macro = macro, subst = substtable, line = 1, read=macro_playback_read }
   stat_increment "macrorun"
   parse(file_like_obj, filename)
end
