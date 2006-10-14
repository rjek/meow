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
      condwhinge(reg > 15 or reg < 0, "Unable to parse register number %s", str)
      return { type="register", value=reg, alt=alt }
   end
   
end