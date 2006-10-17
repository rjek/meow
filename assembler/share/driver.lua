-- -*- Lua -*-

-- The MEOW assembler driver interface

function verbose(level, ...)
   if masm.verbose >= level then
      print(string.format(...))
   end
end

local function find_source_file(fn)
   local f = io.open(fn, "r")
   if f then return f, fn end
   if string.sub(fn, 1, 1) == "/" then return end
   for bit in string.gfind(masm.include_path, "([^:]+)") do
      f = io.open(bit .. "/" .. fn, "r")
      if f then return f, bit .. "/" .. fn end
   end
end

function meow_op_include(info, fname)
   local file, name = find_source_file(fname)
   if not file then
      whinge(info, "Unable to find file '%s' in include directive.", fname)
   end
   parse(file, name)
end

function masm_main(...)
   local argv = {...}
   local ignore = {}
   for i, v in ipairs(argv) do
      if not ignore[i] then
	 if v == "-v" then masm.verbose = masm.verbose + 1
	 elseif v == "-nostdinc" then masm.do_stdinc = false
	 elseif v == "-i" then 
	    ignore[i+1] = true
	    masm.extra_include =  masm.extra_include or {}
	    table.insert(masm.extra_include, argv[i+1])
	 elseif v == "-I" then
	    ignore[i+1] = true
	    masm.include_path = masm.include_path .. ":" .. argv[i+1]
	 elseif v == "-o" then
	    ignore[i+1] = true
	    if masm.output_file then
	       verbose(0, "More than one -o supplied; unable to output to more than one file.")
	       os.exit(1)
	    end
	    masm.output_file = argv[i+1]
	 elseif string.sub(v,1,1) == "-" then
	    verbose(0, "Unknown argument '`%s`' at position %d.", v, i)
	    os.exit(1)
	 else
	    table.insert(masm.source_files, v)
	 end
      end
   end
   if not masm.output_file then masm.output_file = "masm.out" end
   verbose(1, "MEOW assembler version %s", masm.version)
   if masm.do_stdinc then
      table.insert(masm.source_files, 1, masm.stdlib_path .. "/masm-stdlib.s")
   end
   verbose(3, "Include path = %s", masm.include_path)
   verbose(3, "Invoke parser...")
   for i,v in ipairs(masm.source_files) do
      parse(find_source_file(v))
   end
   -- All the input is now parsed, we have to resolve the instruction stream
   verbose(3, "Resolve instruction stream...")
   resolve_stream()
   -- The stream is resolved, dump it to disk (yay!)
   verbose(3, "Write stream to disk...")
   f = io.open(masm.output_file, "wb")
   if not f then
      verbose(0, "Unable to open output file '%s'.", masm.output_file)
      os.exit(1)
   end
   dump_stream(f)
   f:close()
   dump_stats()
   verbose(1, "MEOW assembler finished.")
   os.exit(0)
end
