-- MEOW: what the core, the Chairman and the rest agree on.  The
-- encodings are those of isa/meow.isa; the bus is reference section 7.
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

package meow_pkg is
    subtype word_t is std_logic_vector(31 downto 0);
    subtype half_t is std_logic_vector(15 downto 0);
    subtype reg_t is unsigned(3 downto 0);

    -- both banks: index 0 to 15 is bank 0, 16 to 31 bank 1
    type reg_file_t is array (0 to 31) of word_t;

    constant R_SP : natural := 11;
    constant R_LR : natural := 12;
    constant R_IR : natural := 13;
    constant R_SR : natural := 14;
    constant R_PC : natural := 15;

    constant SR_N : natural := 31;
    constant SR_Z : natural := 30;
    constant SR_C : natural := 29;
    constant SR_V : natural := 28;
    constant SR_I : natural := 0;

    constant IRQ_VECTOR : word_t := x"00000020";

    -- the bus, from a master
    type bus_m2s_t is record
        req   : std_logic;
        wr    : std_logic;
        addr  : word_t;
        be    : std_logic_vector(3 downto 0);
        wdata : word_t;
    end record;

    -- and from the slave
    type bus_s2m_t is record
        ack   : std_logic;
        rdata : word_t;
    end record;

    constant BUS_M2S_IDLE : bus_m2s_t := ('0', '0', (others => '0'), (others => '0'), (others => '0'));
    constant BUS_S2M_IDLE : bus_s2m_t := ('0', (others => '0'));

    type bus_m2s_array_t is array (natural range <>) of bus_m2s_t;
    type byte_array_t is array (natural range <>) of std_logic_vector(7 downto 0);
    type bus_s2m_array_t is array (natural range <>) of bus_s2m_t;

    -- device numbers in the chip-select table, vendor 0
    constant DEV_NONE     : word_t := x"ffffffff";
    constant DEV_ROM      : word_t := x"00000000";
    constant DEV_RAM      : word_t := x"00000001";
    constant DEV_CHAIRMAN : word_t := x"00000002";
    constant DEV_IOC      : word_t := x"00000003";
    constant DEV_LOCAL    : word_t := x"00000004";

    function cond_true(sr : word_t; cond : std_logic_vector(3 downto 0)) return boolean;
    function sext(v : std_logic_vector; width : natural) return word_t;
    function bool_to_sl(b : boolean) return std_logic;
end package;

package body meow_pkg is
    function cond_true(sr : word_t; cond : std_logic_vector(3 downto 0)) return boolean is
        variable n, z, c, v : boolean;
    begin
        n := sr(SR_N) = '1';
        z := sr(SR_Z) = '1';
        c := sr(SR_C) = '1';
        v := sr(SR_V) = '1';
        case cond is
            when "0000" => return z;                 -- EQ
            when "0001" => return not z;             -- NE
            when "0010" => return c;                 -- CS
            when "0011" => return not c;             -- CC
            when "0100" => return n;                 -- MI
            when "0101" => return not n;             -- PL
            when "0110" => return v;                 -- VS
            when "0111" => return not v;             -- VC
            when "1000" => return c and not z;       -- HI
            when "1001" => return (not c) or z;      -- LS
            when "1010" => return n = v;             -- GE
            when "1011" => return n /= v;            -- LT
            when "1100" => return n = v and not z;   -- GT
            when "1101" => return n /= v or z;       -- LE
            when "1110" => return true;              -- AL
            when others => return false;             -- NV
        end case;
    end function;

    -- sign-extend the low width bits of v to a word
    function sext(v : std_logic_vector; width : natural) return word_t is
        variable t : std_logic_vector(v'length - 1 downto 0) := v;   -- whatever way v runs
        variable r : word_t;
    begin
        r := (others => t(width - 1));
        r(width - 1 downto 0) := t(width - 1 downto 0);
        return r;
    end function;

    function bool_to_sl(b : boolean) return std_logic is
    begin
        if b then return '1'; else return '0'; end if;
    end function;
end package body;
