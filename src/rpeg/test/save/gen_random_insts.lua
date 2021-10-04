-- -*- Mode: Lua; -*-                                                               
--
-- gen_random_insts.lua
--
-- © Copyright Jamie A. Jennings 2018.
-- LICENSE: MIT License (https://opensource.org/licenses/mit-license.html)
-- AUTHOR: Jamie A. Jennings


-- For testing instruction decoding (which needs to be fast, because it's pure
-- overhead), we have generated two kinds of files containing signed 32-bit
-- numbers:
--
-- (1) A random mix of n instructions was generated with `jot`:
--     jot -w %i -r n -2147483648 2147483647
-- (2) The random mix is silly, because (a) the space of opcodes is large, but
--     very few are used, and (b) around 98% of the instructions executed in a
--     large Rosie pattern are ITestSet, IAny, IPartialCommit (in that order),
--     according to Mark S. who wrote a jit for an early version of Rosie.
--     This file generates an instruction mix in which 98% are those
--     instructions, while the remaining 2% are equally divided between the
--     other two instruction groups.

--    ITestSet is in the OffsetGroup: offset(22) opcode(7) 010
--    IAny is in the BareGroup: 0 opcode(21) unused(6) 110
--    IPartialCommit is in the OffsetGroup: offset(22) opcode(7) 010

--    Want to generate a file of 32-bit values with this distribution:
--    (Guessing at proportions within that 98%, since I don't have the data...)
--
--    1. ITestSet       50% -> encode ITestSet with random offset
--    2. IAny           40% -> encode IAny
--    3. IPartialCommit  8% -> encode IPartialCommit with random offset
--    4. all others      2%
--       a.        IChar 1% -> encode IChar with 3 random chars
--       b.    ITestChar 1% -> encode ITestChar with random char, random offset
   
--    The "all others" category is not based on any data.  I just want to
--    get instructions from those other two groups (MultiCharGroup and
--    TestChar) into the mix.


function coin100()
   return math.random(0, 99)
end

MAXoffset = math.tointeger(math.pow(2, 21) - 1)
MINoffset = math.tointeger(-math.pow(2, 21))
--print("22-bit range: ", MINoffset, MAXoffset)

function genITestSet()
   local offset = math.random(MINoffset, MAXoffset)
   local w = (offset << 10) | (1 << 3) | 2
   return w
end

function genIAny()
   return 6
end

function genIPartialCommit()
   local offset = math.random(MINoffset, MAXoffset)
   local w = (offset << 10) | (2 << 3) | 2
   return w
end

function genIChar()
   local c = math.random(0, 255)
   local w = (c << 24) | 4
   return w
end

function genITestChar()
   local offset = math.random(MINoffset, MAXoffset)
   local c = math.random(0, 255)
   local w = (offset << 10) | ((c & 0xFF) << 2) | 1
   return w
end

--  ITestChar opcode = 262306

if not(arg[1]) or not(tonumber(arg[1])) then
   print(string.format("Usage: %s <n>", arg[0]))
end

n = tonumber(arg[1])

for i = 1, n do
   local p = coin100()
   if (p < 50) then
      print(genITestSet())
   elseif (p < 90) then
      print(genIAny())
   elseif (p < 98) then
      print(genIPartialCommit())
   elseif (p < 99) then
      print(genIChar())
   else
      print(genITestChar())
   end
end

   