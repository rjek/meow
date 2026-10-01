-- Local memory, reference section 4: a RAM per CPU, reached at chip
-- select 30 as the accessing CPU's own and at chip select 29 as all of
-- them, 4 MB apart.  Here it is on the device bus behind the Chairman,
-- which says whose the transaction is; a system that wants a CPU's own
-- accesses off the shared bus decodes chip select 30 in the core
-- instead, as section 7.3 allows.
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.meow_pkg.all;

entity local_mem is
    generic (
        NCPU  : natural := 1;
        WORDS : natural := 1024             -- 4 KB each
    );
    port (
        clk    : in  std_logic;
        bus_i  : in  bus_m2s_t;             -- chip select 29 or 30, by addr(31 downto 27)
        master : in  natural range 0 to NCPU - 1;
        bus_o  : out bus_s2m_t
    );
end entity;

architecture rtl of local_mem is
    type lane_t is array (0 to NCPU * WORDS - 1) of std_logic_vector(7 downto 0);
    signal lane0, lane1, lane2, lane3 : lane_t := (others => (others => '0'));
    signal data : word_t := (others => '0');
    signal ack  : std_logic := '0';
begin
    process (clk)
        variable cpu : natural;
        variable a : natural;
    begin
        if rising_edge(clk) then
            ack <= '0';
            data <= (others => '0');
            if bus_i.req = '1' and ack = '0' then
                if bus_i.addr(27) = '0' then
                    cpu := to_integer(unsigned(bus_i.addr(26 downto 22)));   -- chip select 29: whose
                else
                    cpu := master;                                           -- 30: the asker's
                end if;
                a := to_integer(unsigned(bus_i.addr(21 downto 2))) mod WORDS;
                if cpu < NCPU then
                    a := cpu * WORDS + a;
                    if bus_i.wr = '1' then
                        if bus_i.be(0) = '1' then lane0(a) <= bus_i.wdata(7 downto 0); end if;
                        if bus_i.be(1) = '1' then lane1(a) <= bus_i.wdata(15 downto 8); end if;
                        if bus_i.be(2) = '1' then lane2(a) <= bus_i.wdata(23 downto 16); end if;
                        if bus_i.be(3) = '1' then lane3(a) <= bus_i.wdata(31 downto 24); end if;
                    end if;
                    data <= lane3(a) & lane2(a) & lane1(a) & lane0(a);
                end if;
                ack <= '1';
            end if;
        end if;
    end process;
    bus_o.ack <= ack;
    bus_o.rdata <= data;
end architecture;
