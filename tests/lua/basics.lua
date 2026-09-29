-- arithmetic, strings, tables, closures, errors
print(1 + 2, 7 // 2, 7 % 3, 2 ^ 10, 7 / 2, -7 // 2, 7 % -3)
print(1 << 40, 0xff >> 4, 5 & 3, 5 | 3, 5 ~ 3, ~0)
print(math.maxinteger, math.mininteger, math.maxinteger + 1 == math.mininteger)
print(math.sqrt(2), math.sin(1), math.floor(-3.5), math.ceil(3.2), math.abs(-4))
print(math.type(1), math.type(1.0), math.type("1"), 3 == 3.0, 1e15, 2^53)
print(tostring(nil), tostring(true), tonumber("0x10"), tonumber("  12  "), tonumber("1e2"))
print(#"hello", ("abc"):upper(), ("x"):rep(5, "-"), ("hello world"):find("wor"))
print(string.format("%5.2f|%-5d|%05d|%x|%s|%q", math.pi, 42, 42, 255, "str", "a\nb"))
print(("one two three"):gsub("%w+", string.upper))
for w in ("a,b,,c"):gmatch("([^,]*)") do io.write("[", w, "]") end print()
local t = {}
for i = 1, 10 do t[i] = i * i end
print(#t, table.concat(t, ","), table.unpack(t, 3, 5))
table.sort(t, function(a, b) return a > b end)
print(t[1], t[10], table.remove(t), #t)
local u = { x = 1, y = 2, [10] = "ten", "first" }
local keys = {}
for k in pairs(u) do keys[#keys + 1] = tostring(k) end
table.sort(keys)
print(table.concat(keys, " "), u[1], u.x, u[10])
local function counter()
    local n = 0
    return function() n = n + 1 return n end
end
local c = counter()
c() c()
print(c(), select("#", 1, 2, 3), select(2, "a", "b", "c"))
local function fib(n) if n < 2 then return n end return fib(n - 1) + fib(n - 2) end
print(fib(20))
print(pcall(error, "caught"))
print(select(2, pcall(error, { code = 7 })).code)
print(select(2, pcall(function() local x = nil; return x.y end)))
local co = coroutine.create(function(a)
    local b = coroutine.yield(a + 1)
    return b * 2
end)
print(coroutine.resume(co, 1))
print(coroutine.resume(co, 10))
print(coroutine.status(co))
print(string.byte("A"), string.char(72, 105), ("%d"):format(3.0), 10 // 3.0)
print(utf8.char(228, 8364), utf8.len("h\xc3\xa4"), #utf8.char(8364))
goto done
print("skipped")
::done::
print(os.time() > 0, os.clock() >= 0, type(os.date()))
