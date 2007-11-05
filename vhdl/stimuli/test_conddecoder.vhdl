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
    type vector_lines is array (0 to 255) of std_logic;
    variable test_vector : vector_lines := (
   --  EQ   NE   CS   CC   MI   PL   VS   VC   HI   LS   GE   LT   GT   LE   AL   NV       N Z C V
       '0', '1', '0', '1', '0', '1', '0', '1', '0', '1', '1', '0', '1', '0', '1', '0' , -- 0 0 0 0
       '0', '1', '0', '1', '0', '1', '1', '0', '0', '1', '0', '1', '0', '1', '1', '0' , -- 0 0 0 1
       '0', '1', '1', '0', '0', '1', '0', '1', '1', '0', '1', '0', '1', '0', '1', '0' , -- 0 0 1 0
       '0', '1', '1', '0', '0', '1', '1', '0', '1', '0', '0', '1', '0', '1', '1', '0' , -- 0 0 1 1
       '1', '0', '0', '1', '0', '1', '0', '1', '0', '1', '1', '0', '0', '1', '1', '0' , -- 0 1 0 0
       '1', '0', '0', '1', '0', '1', '1', '0', '0', '1', '0', '1', '0', '1', '1', '0' , -- 0 1 0 1
       '1', '0', '1', '0', '0', '1', '0', '1', '0', '1', '1', '0', '0', '1', '1', '0' , -- 0 1 1 0
       '1', '0', '1', '0', '0', '1', '1', '0', '0', '1', '0', '1', '0', '1', '1', '0' , -- 0 1 1 1
       '0', '1', '0', '1', '1', '0', '0', '1', '0', '1', '0', '1', '0', '1', '1', '0' , -- 1 0 0 0
       '0', '1', '0', '1', '1', '0', '1', '0', '0', '1', '1', '0', '1', '0', '1', '0' , -- 1 0 0 1
       '0', '1', '1', '0', '1', '0', '0', '1', '1', '0', '0', '1', '0', '1', '1', '0' , -- 1 0 1 0
       '0', '1', '1', '0', '1', '0', '1', '0', '1', '0', '1', '0', '1', '0', '1', '0' , -- 1 0 1 1
       '1', '0', '0', '1', '1', '0', '0', '1', '0', '1', '0', '1', '0', '1', '1', '0' , -- 1 1 0 0
       '1', '0', '0', '1', '1', '0', '1', '0', '0', '1', '1', '0', '0', '1', '1', '0' , -- 1 1 0 1
       '1', '0', '1', '0', '1', '0', '0', '1', '0', '1', '0', '1', '0', '1', '1', '0' , -- 1 1 1 0
       '1', '0', '1', '0', '1', '0', '1', '0', '0', '1', '1', '0', '0', '1', '1', '0'   -- 1 1 1 1
      );
    variable test_counter : std_logic_vector(7 downto 0) := "00000000";
    variable test_counter_num : integer := 0;
  begin
    test_flags <= test_counter(7 downto 4);
    test_condition <= test_counter(3 downto 0);
    wait for 10 ns;
    assert test_result = test_vector(test_counter_num) report "Failed";
    wait for 10 ns;
    test_counter := test_counter + 1;
    test_counter_num := test_counter_num + 1;
    wait for 10 ns;
    if test_counter_num = 256 then
      test_counter_num := 0;
    end if;
    assert test_counter_num >= 0 and test_counter_num <= 255 report "Counter out of range?!";
  end process;

end Behavioural;

