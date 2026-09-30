-- numerals and tonumber give the nearest double, in decimal and in
-- hexadecimal, and %a and %.17g print it back exactly
local t = { "0.1", "0.3", "123.456", "1e22", "1e23", "5e-324", "2.2250738585072014e-308", "1.7976931348623157e308",
  "3.141592653589793", "1e-5", "9007199254740993", "4.35", "2.675", "1e100", "6.02214076e23", "1.0000000000000002",
  "0.30000000000000004", "8.41e21", "1e400", "1e-400", "123456789012345678901234567890", ".5", "5.", "1e+3", "0e0",
  "0x10", "0x1p4", "0x.8", "0x1.8p1", "-0xA.8p-2", "0x1.fffffffffffffp1023", "0x1p-1074", "0x1.00000000000011p0",
  "1e", "0x", "0x.", "0x1p", "1e+", ".", "0xg", "1 2", " 12 ", "" }
for _, s in ipairs(t) do
  local n = tonumber(s)
  print(s, n and string.format("%.17g %a", n, n) or n, math.type(n))
end
print(0x1p4, 0x.8, 0xA.8p0, 0x10, 0xffffffffffffffff, 1e2, .5e1, 3 // 0.0, -(0/0) ~= -(0/0))
print(0.1 + 0.2 == 0.3, 0.1 + 0.2, 1/3, 100 / 3, 2^53, 1e15, 1e16, -0.0, 1e100)
for _, v in ipairs { 1.5, 1.0, 0.1, 255.0, 1e-310, 0.0, -2.75, 1/0, -1/0 } do
  print(string.format("%a %.3a %A %.0a %20.5a|", v, v, v, v, v))
end
print(string.format("%.14g %g %e %f %.0f %5.1f|%-8.2e|", 123.456, 1e-5, 12345.678, 2.5, 2.5, 3.14159, 0.000123))
print(string.format("%q %q %q", 0.1, 1/3, 2^63), load("return " .. string.format("%q", 0.1))() == 0.1)
print(load("return 0x1p4 + 0x.8")(), load("return 1e"), load("return 0x"))
print(math.tointeger("0x10"), math.tointeger(3.0), "10" + 0, "0x10" + 0, "1e1" + 0, "0x1p1" * 2, 10 // "3")
