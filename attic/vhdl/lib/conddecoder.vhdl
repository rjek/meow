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
      when "0000" => condpassed <= flag_z;
      when "0001" => condpassed <= not flag_z;
      when "0010" => condpassed <= flag_c;
      when "0011" => condpassed <= not flag_c;
      when "0100" => condpassed <= flag_n;
      when "0101" => condpassed <= not flag_n;
      when "0110" => condpassed <= flag_v;
      when "0111" => condpassed <= not flag_v;
      when "1000" => condpassed <= flag_c and (not flag_z);
      when "1001" => condpassed <= (not flag_c) or flag_z;
      when "1010" =>
        if flag_n = flag_v then
          condpassed <= '1';
        else
          condpassed <= '0';
        end if;
      when "1011" =>
        if flag_n /= flag_v then
          condpassed <= '1';
        else
          condpassed <= '0';
        end if;
      when "1100" =>
        if (flag_n = flag_v) and (flag_z = '0') then
          condpassed <= '1';
        else
          condpassed <= '0';
        end if;
      when "1101" =>
        if (flag_n /= flag_v) or (flag_z = '1') then
          condpassed <= '1';
        else
          condpassed <= '0';
        end if;
      when "1110" => condpassed <= '1';
      when "1111" => condpassed <= '0';
      when others => null;
    end case;
  end process decoder;

end Behavioural;
