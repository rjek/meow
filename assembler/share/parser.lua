-- -*- Lua -*-

-- The parser for the MEOW assembler

-- The syntax for the MEOW assembler input is very simply:

-- Anything in col 1 is a label
-- Anything indented is a command
-- Anything following a semicolon is a comment
-- All commands have their arguments whitespace separated
-- Quoted strings are allowed using ""s
-- Registers are plain numbers
-- Numbers are prefixed by a #
-- Hex numbers are 0xblah
-- commands are not case sensitive (indeed everything including labels will be
-- 				 lowercased, the only thing escaping this is quoted strings)
-- The comma character is considered to be whitespace while normalising the input

local strtab = {}
local ustrctr = 0

function resolve_string(tag)
   local ret = strtab[tag]
   if not ret then return end
   if not strtab[ret] then return end
   return ret
end

function is_memozied_string(tag)
   return resolve_string(tag) ~= nil
end

local function memoize_string(str)
   if resolve_string(str) then return strtab[str] end
   local unique_name = '\127uniquestring' .. tostring(ustrctr)..''
   ustrctr = ustrctr + 1
   strtab[str] = unique_name
   strtab[unique_name] = str
   return unique_name
end

local function dispatch(info, op, args)
   op = resolve_defines(op)
   if is_macro(op) then
      run_macro(info, op, unpack(args))
      return true
   else
      local f = _G["meow_op_"..op]
      if f then 
	 local newparts = {}
	 for i, v in ipairs(args) do
	    newparts[i] = resolve_defines(v)
	 end
	 f(info, unpack(newparts))
	 return true
      end
   end
   return false
end

local function deal_with_line(parts, orig_line, linenumber, filename)
   local info = {line=orig_line,num=linenumber,file=filename}
   if parts[1] ~= "" then
      define_label(info, parts[1])
   end
   table.remove(parts, 1)
   if table.getn(parts) > 0 then
      local op = parts[1]
      table.remove(parts, 1)
      if not dispatch(info, op, parts) then
	 whinge(info, "Unable to find op '%s'.", op)
      end
   end
end

local parser_files = {}
local parser_lines = {}
function parser_steal_line()
   parser_lines[1] = parser_lines[1] + 1
   return parser_files[1]:read("*l"), parser_lines[1]
end

local parse_level = 0
function parse(f, filename)
   -- parse the file-like object f which comes from file 'filename'
   -- into the assembler's internal state.
   verbose(3, "  read %s", filename)
   parse_level = parse_level + 1
   if do_intermediate() then
      _queue_missing_newline() 
      _queue_bytes(nil, ";;;", string.rep(" ", parse_level), "> ", filename, "\n")
   end
   table.insert(parser_files, 1, f)
   table.insert(parser_lines, 1, 0)
   local line, linenumber = parser_steal_line();
   while line do
      local orig_line = line
      stat_increment "parsed_lines"
      line = string.gsub(line, '"([^"]+)"', memoize_string)
      line = string.gsub(line, ';.-$', "")
      line = string.gsub(line, '[ \t,]+', ' ')
      line = string.gsub(line, ' $', '')
      line = string.lower(line)
      if line ~= "" then
	 elements = strsplit(" ", line)
	 deal_with_line(elements, orig_line, linenumber, filename)
      end
      line, linenumber = parser_steal_line()
   end
   table.remove(parser_files, 1)
   if do_intermediate() then 
      _queue_missing_newline() 
      _queue_bytes(nil, ";;;", string.rep(" ", parse_level), "< ", filename, "\n")
   end
   parse_level = parse_level - 1
end

