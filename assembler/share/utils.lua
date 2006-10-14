-- MEOW Assembler utility functions


local strfind, strsub, tinsert = string.find, string.sub, table.insert
-- Split on delimiter pattern, returns table(list)
function strsplit(delimiter, text)
  local list = {}
  local pos = 1
  if strfind("", delimiter, 1) then -- this would result in endless loops
    error("delimiter matches empty string!")
  end
  while 1 do
    local first, last = strfind(text, delimiter, pos)
    if first then -- found?
      tinsert(list, strsub(text, pos, first-1))
      pos = last+1
    else
      tinsert(list, strsub(text, pos))
      break
    end
  end
  return list
end

function whinge(info, ...)
   io.stderr:write(string.format("ERROR in %s on line %d: ", info.file, info.num))
   io.stderr:write(string.format(...))
   io.stderr:write("\n")
   io.stderr:write(string.format("%s\n", info.line))
   io.stderr:flush()
   os.exit(1)
end

function condwhinge(cond, ...)
   if cond then whinge(...) end
end

function parse_positional(info, str)
   -- Parse the positional argument str as one of:
   -- a register
   -- a number
   -- a label
   local ret = {}
   if tonumber(str) ~= nil then
      local reg = tonumber(str)
      local alt = reg > 15
      if alt then reg = reg - 16 end
      condwhinge(reg > 15 or reg < 0, info, "Unable to parse register number %s", str)
      return { type="register", value=reg, alt=alt }
   end
   if string.sub(str, 1, 1) == "#" then
      local val = tonumber(string.sub(str, 2))
      condwhinge(val == nil, info, "Unable to parse constant value %s", str)
      return { type="constant", value=val }
   end
   if is_memozied_string(str) then
      return { type="string", value=resolve_string(str), tag=str }
   end
   if is_label(str) then
      return { type="label", value=str }
   end
   whinge(info, 
	  "Unable to parse positional argument %s. Expecting register, number, string or label", str)
end

function __dump(t)
   if type(t) == "table" then
      io.stderr:write("{")
      local comma = ""
      for k, v in ipairs(t) do
	 io.stderr:write(comma)
	 __dump(v)
	 comma = ", "
      end
      for k, v in pairs(t) do
	 if type(k) ~= "number" then
	    io.stderr:write(comma)
	    io.stderr:write("[")
	    __dump(k)
	    io.stderr:write("]=")
	    __dump(v)
	    comma = ", "
	 end
      end
      io.stderr:write("}")
   elseif type(t) == "string" then
      io.stderr:write(string.format("%q",t))
   elseif type(t) == "function" then
      local fn = debug.getinfo(t).name
      if not fn then fn = "?" end
      io.stderr:write("<"..fn..">")
   else
      io.stderr:write(tostring(t))
   end
end

function _dump(name, t)
   io.stderr:write(name)
   io.stderr:write("=")
   __dump(t)
   io.stderr:write("\n")
   io.stderr:flush()
end

function printf(...)
   print(string.format(...))
end

local meow_stats = { passes = 0, parsed_lines = 0, instructions = 0 }

local stat_names = {
   passes = "Number of passes to resolve instruction stream",
   parsed_lines = "Number of lines processed by the parser",
   instructions = "Number of instructions"
}

local stat_accum = { passes = true }

function register_stat(s, name, accum)
   stat_names[s] = name
   meow_stats[s] = 0
   accum = accum and true or false
   stat_accum[s] = accum
end

local pinned
function pin_stats()
   pinned = {}
   for k, v in pairs(meow_stats) do pinned[k] = v end
end

function clear_stats()
   for k, v in pairs(pinned) do
      if not stat_accum[k] then
	 meow_stats[k] = v
      end
   end
end

function stat_increment(s, v)
   v = v or 1
   meow_stats[s] = meow_stats[s] + v
end

function dump_stats()
   verbose(2, "Assembly statistics:")
   for k, v in pairs(meow_stats) do
      verbose(2, "  %s: %d", stat_names[k], v)
   end
end
