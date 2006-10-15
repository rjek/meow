-------------------------------------------------------------------------------
-- Title      : MEOW Condition Decoder -- Behavioural test
-- Project    : 
-------------------------------------------------------------------------------
-- File       : conddecoder.vhdl<2>
-- Author     : Daniel Silverstone  <dsilvers@digital-scurf.org>
-- Company    : 
-- Last update: 2006/10/15
-- Platform   : 
-------------------------------------------------------------------------------
-- Description: Test the condition decoder
-------------------------------------------------------------------------------
-- Revisions  :
-- Date        Version  Author  Description
-- 2006/10/15  1.0      dsilvers	Created
-------------------------------------------------------------------------------

library IEEE;
use IEEE.STD_LOGIC_1164.all;
use IEEE.STD_LOGIC_ARITH.all;
use IEEE.STD_LOGIC_UNSIGNED.all;

entity test_conddecoder is
  -- Empty entity, don't need an interface for this
end test_conddecoder;

architecture Behavioural of test_conddecoder is

  component conddecoder
    port (
      flag_n     : in  STD_LOGIC;
      flag_c     : in  STD_LOGIC;
      flag_v     : in  STD_LOGIC;
      flag_z     : in  STD_LOGIC;
      condition  : in  STD_LOGIC_VECTOR (3 downto 0);
      condpassed : out STD_LOGIC);
  end component;

  signal test_flags : STD_LOGIC_VECTOR (3 downto 0);
  signal test_condition : STD_LOGIC_VECTOR (3 downto 0);
  signal test_result : STD_LOGIC;

begin  -- Behavioural

  DEC : conddecoder port map (
    flag_n     => test_flags(3),
    flag_z     => test_flags(2),
    flag_c     => test_flags(1),
    flag_v     => test_flags(0),
    condition  => test_condition,
    condpassed => test_result)  ;

  -- The actual test bench...
  process
    variable err_cnt : integer := 0;
  begin
    flags <= "0000";
    test_condition <= "1110";
    wait for 10ns;
    assert condpassed == '1' report "Failed";
    if condpassed /= '1' then
      err_cnt := err_cnt + 1;
    end if;
  end process;
  
end Behavioural;

