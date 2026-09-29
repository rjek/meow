-- something to make the VM work: the sieve, a string builder and a sort
local N = 2000
local flags = {}
for i = 2, N do flags[i] = true end
local count = 0
for i = 2, N do
    if flags[i] then
        count = count + 1
        for j = i * i, N, i do flags[j] = false end
    end
end
print("primes below", N, count)
local parts = {}
for i = 1, 200 do parts[#parts + 1] = string.format("%03d", (i * 7919) % 1000) end
local s = table.concat(parts)
print(#s, s:sub(1, 30), s:sub(-12))
local nums = {}
local seed = 12345
for i = 1, 500 do
    seed = (seed * 1103515245 + 12345) % 2147483648
    nums[i] = seed % 10000
end
table.sort(nums)
print(nums[1], nums[250], nums[500])
local sum, fsum = 0, 0.0
for i = 1, 500 do sum = sum + nums[i]; fsum = fsum + nums[i] / 3 end
print(sum, string.format("%.3f", fsum))
local words = {}
for w in ("the quick brown fox jumps over the lazy dog the end"):gmatch("%a+") do
    words[w] = (words[w] or 0) + 1
end
local list = {}
for w, n in pairs(words) do list[#list + 1] = w .. "=" .. n end
table.sort(list)
print(table.concat(list, " "))
