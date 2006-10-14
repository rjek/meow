-- MEOW Assembler - output accumulator

local output_stream = {}
local output_count = 0
local output_labels = {}
local output_info = {}
local queue_routines

local function normal_queue_bytes(info, ...)
   -- A sequence of bytes (or strings if wanted) are appended to the output stream
   for i, v in ipairs({...}) do
      table.insert(output_stream, v)
      table.insert(output_info, info)
      if type(v) == "string" then 
	 output_count = output_count + #v 
      else 
	 output_count = output_count + 1 
      end
   end
end

local deferred_bytes = ""

local function deferred_queue_bytes(info, ...)
   -- A sequence of bytes (or strings if wanted) are appended to the output stream
   for i, v in ipairs({...}) do
      if type(v) == "string" then 
	 deferred_bytes = deferred_bytes .. v
      else 
	 deferred_bytes = deferred_bytes .. string.char(v)
      end
   end
end

local function normal_queue_callback(info, ...)
   if type(({...})[1]) ~= "function" then
      error("IAE: queue_callback called without a function")
   end
   table.insert(output_stream, {...})
   table.insert(output_info, info)
end

queue_routines = {
   bytes = normal_queue_bytes,
   callback = normal_queue_callback,
}

function _queue_bytes(...) queue_routines.bytes(...) end
function _queue_callback(...) queue_routines.callback(...) end

local function _define_label(info, stream_pos, str)
   -- Actually define the label at the current point in the output stream
   output_labels[stream_pos] = str
end

function define_label(info, str)
   local fc = string.sub(str,1,1)
   if fc == ">" or fc == "<" or fc == "#" or (fc >= "0" and fc <= "9") then
      whinge(info, "Bad label, cannot start with 0-9><#: %s", str)
   end
   _queue_callback(info, _define_label, str)
end

function is_label(str)
   -- Need to do something clever here, otherwise anything is a label...
   return true -- Yeah, it'll do
end

local function scan_for_label(streampos, label, incr)
   while output_labels[streampos] ~= label and streampos >= 0 and streampos <= output_count do
      streampos = streampos + incr
   end
   if output_labels[streampos] == label then return streampos end
end

function find_label(streampos, str)
   local fc = string.sub(str, 1, 1)
   local cand1, cand2
   if fc == "<" then
      cand1 = scan_for_label(streampos, string.sub(str, 2), -1)
   elseif fc == ">" then
      cand1 = scan_for_label(streampos, string.sub(str, 2), 1)
   else
      cand1 = scan_for_label(streampos, str, -1)
      cand2 = scan_for_label(streampos, str, 1)
   end
   if cand1 and not cand2 then return cand1 end
   if cand2 and not cand1 then return cand2 end
   if not cand1 and not cand2 then return end
   if streampos-cand1 < cand2-streampos then return cand1 else return cand2 end
end

function resolve_stream()
   -- We have a full stream, the process is as follows...
   -- Every byte/string is just transferred as-is
   -- tables indicate functions to call which are callbacks
   -- their job is to re-queue some content or else defer and
   -- in deferring tell us how many bytes they want to use.
   -- If deferring, we reserve that many bytes and add the result
   -- a list of fixups to deal with at the end
   -- When processing a fixup we sub in a special set of queue routines
   -- if the fixup sends a different number of bytes to the count they
   -- reserved then we update the reservation for that fixup and restart
   -- the entire process
   
   -- During resolution you may not allocate a callback
   queue_routines.callback = nil
   local raw_stream = output_stream
   local raw_info = output_info
   local finished = false
   local reservations = {}
   pin_stats()
   while not finished do
      clear_stats() -- resets them to the pinned value (except for passes)
      stat_increment "passes"
      -- Blank the stream (again)
      output_stream = {}
      output_info = {}
      output_labels = {}
      output_count = 0
      local early_exit = false
      for i,entry in ipairs(raw_stream) do
	 local info = raw_info[i]
	 if type(entry) == "number" or type(entry) == "string" then
	    normal_queue_bytes(info, entry)
	 else
	    if reservations[entry] == nil then
	       -- Need to prepare a reservation for this callback
	       local r = { tries = 0 } -- tries increments each time this causes a re-run
	       local bits = {} for ii,vv in ipairs(entry) do bits[ii]=vv end
	       local func = bits[1] table.remove(bits,1)
	       local before = output_count
	       local rets = { func(info, output_count, unpack(bits)) }
	       if rets[1] == "defer" then
		  -- We're being asked to defer, so let's fill us out
		  r.reservation = rets[2]
		  r.func = rets[3]
		  table.remove(rets,1)table.remove(rets,1)table.remove(rets,1)
		  r.func_args = rets
	       else
		  -- Not a defer, so I'm going to assume it output a bunch of bytes
		  r.reservation = output_count - before
		  r.func = func
		  r.func_args = bits
		  r.direct = true
	       end
	       reservations[entry] = r
	       early_exit = true
	       break
	    end
	    local res = reservations[entry]
	    if res.direct then
	       local before = output_count
	       queue_routines.bytes = normal_queue_bytes
	       res.func(info, output_count, unpack(res.func_args))
	       condwhinge((output_count-before)~=res.reservation, info,
			  "Non-deferred callback did not return a stable fill-in")
	    else
	       res.start_from = output_count
	       output_count = output_count + res.reservation
	       table.insert(output_stream, res)
	       table.insert(output_info, info)
	       res.replace = table.getn(output_stream)
	    end
	 end
      end
      -- if early_exit is set then all we want to do is go around the loop once more
      -- otherwise we can consider whether or not we succeeded
      if not early_exit then
	 for k, res in pairs(reservations) do
	    if not res.direct then
	       local cur_pos = res.start_from
	       local info = output_info[res.replace]
	       deferred_bytes = ""
	       queue_routines.bytes = deferred_queue_bytes
	       res.func(info, cur_pos, res.reservation, unpack(res.func_args))
	       if string.len(deferred_bytes) > res.reservation then
		  res.tries = res.tries + 1
		  res.reservation = string.len(deferred_bytes)
		  condwhinge(res.tries >= 10, info, "Assembler callback is not stable after 10 cycles")
		  early_exit = true
		  break
	       end
	       if string.len(deferred_bytes) < res.reservation then
		  if not res.warned then
		     printf("WARNING: Instability detected in instructions generated by line %d of %s",
			    info.num, info.file)
		     print(info.line)
		     print("Had to insert one or more NOOPs to stabilise the stream")
		     printf("Asked for %d bytes, but only used %d", 
			    res.reservation, string.len(deferred_bytes))
		     res.warned = true
		  end
		  while string.len(deferred_bytes) < res.reservation do
		     -- encode a noop (mov r0 r0)
		     _encode_mov(info, false, false, false, false, 0, 0)
		  end
	       end
	       output_stream[res.replace] = deferred_bytes
	    end
	 end
	 if not early_exit then
	    finished = true
	 end
      end
   end
end

function dump_stream(f)
   for i, v in ipairs(output_stream) do
      if type(v) == "number" then
	 f:write((string.char(v)))
      else
	 f:write(v)
      end
      f:flush()
   end
end
