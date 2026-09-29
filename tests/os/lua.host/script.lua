-- run from /host under Catflap
local t = {}
for i = 1, 10 do t[i] = i * i end
print("script: " .. table.concat(t, " "))
print(string.format("pi %.5f, 2^40 %d, 7//2 %d", math.pi, 1 << 40, 7 // 2))
local co = coroutine.wrap(function(a) local b = coroutine.yield(a * 2) return b + 1 end)
print("coroutine", co(21), co(41))
print("pcall", pcall(error, "caught"))
local f = assert(io.open("/host/data.txt"))
local n, sum = 0, 0
for line in f:lines() do n = n + 1; sum = sum + tonumber(line) end
f:close()
print("data.txt", n, "lines, sum", sum)
local w = assert(io.open("/tmp/lua.out", "w"))
for i = 1, 3 do w:write("line ", i, "\n") end
w:close()
for line in io.lines("/tmp/lua.out") do io.write("[", line, "]") end
print()
local big = {}
for i = 1, 3000 do big[i] = tostring(i) end
print("heap grew to", #big, "strings", collectgarbage("count") > 100)
print("args", ...)
os.exit(7)
