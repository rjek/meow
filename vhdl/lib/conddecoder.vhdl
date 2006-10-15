-------------------------------------------------------------------------------
-- Title      : MEOW -- Condition Decoder
-- Project    : 
-------------------------------------------------------------------------------
-- File       : conddecoder.vhdl
-- Author     : Daniel Silverstone  <dsilvers@digital-scurf.org>
-- Company    : 
-- Last update: 2006/10/15
-- Platform   : 
-------------------------------------------------------------------------------
-- Description: Take the current flags, a condition code, and present pass/fail
-------------------------------------------------------------------------------
-- Revisions  :
-- Date        Version  Author  Description
-- 2006/10/15  1.0      dsilvers	Created
-------------------------------------------------------------------------------

library IEEE;
use IEEE.STD_LOGIC_1164.all;
use IEEE.STD_LOGIC_ARITH.all;
use IEEE.STD_LOGIC_UNSIGNED.all;

entity conddecoder is
  
  port (
    flag_n     : in  STD_LOGIC;         -- The N flag from the currently active PC
    flag_z     : in  STD_LOGIC;         -- The Z flag from the currently active PC
    flag_c     : in  STD_LOGIC;         -- The C flag from the currently active PC
    flag_v     : in  STD_LOGIC;         -- The V flag from the currently active PC
    condition  : in  STD_LOGIC_VECTOR (3 downto 0);
                                        -- The condition code from the instruction
    condpassed : out STD_LOGIC);        -- Success if high

end conddecoder;

architecture Behavioural of conddecoder is

begin  -- Behavioural

  -- purpose: Decodes the flags and condition and asserts condpassed as appropriate
  -- type   : combinational
  -- inputs : flag_n, flag_z, flag_c, flag_v, condition
  -- outputs: condpassed
  decoder: process (flag_n, flag_z, flag_c, flag_v, condition)
  begin  -- process decoder
    case condition is
      when "0000" => condpassed <= flag_z;      -- EQ
      when "0001" => condpassed <= not flag_z;  -- NE
      when "0010" => condpassed <= flag_c;      -- CS/HS
      when "0011" => condpassed <= not flag_c;  -- CC/LO
      when "0100" => condpassed <= flag_n;  -- MI
      when "0101" => condpassed <= not flag_n;  -- PL
      when "0110" => condpassed <= flag_v;  -- VS
      when "0111" => condpassed <= not flag_v;  -- VC
      when "1000" => condpassed <= flag_c and (not flag_z);  -- HI
      when "1001" => condpassed <= (not flag_c) or flag_z;  -- LS
      when "1010" => condpassed <= flag_n == flag_v;  -- GE
      when "1011" => condpassed <= flag_n /= flag_v;  -- LT
      when "1100" => condpassed <= (flag_n == flag_v) and (not flag_z);  -- GT
      when "1101" => condpassed <= (flag_n /= flag_v) or flag_z;  -- LE
      when "1110" => condpassed <= '1';  -- AL
      when "1111" => condpassed <= '0';  -- NV
      when others => null;
    end case;
  end process decoder;

end Behavioural;
