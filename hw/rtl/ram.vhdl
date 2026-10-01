-- RAM: block RAM with byte lanes, read or written on the bus with the
-- answer the cycle after the request.
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.meow_pkg.all;

entity ram is
    generic (
        WORDS : natural := 16384
    );
    port (
        clk   : in  std_logic;
        bus_i : in  bus_m2s_t;
        bus_o : out bus_s2m_t
    );
end entity;

architecture rtl of ram is
    type lane_t is array (0 to WORDS - 1) of std_logic_vector(7 downto 0);
    signal lane0, lane1, lane2, lane3 : lane_t := (others => (others => '0'));
    signal data : word_t := (others => '0');
    signal ack  : std_logic := '0';
begin
    process (clk)
        variable a : natural;
    begin
        if rising_edge(clk) then
            ack <= '0';
            if bus_i.req = '1' and ack = '0' then
                a := to_integer(unsigned(bus_i.addr(26 downto 2))) mod WORDS;
                if bus_i.wr = '1' then
                    if bus_i.be(0) = '1' then lane0(a) <= bus_i.wdata(7 downto 0); end if;
                    if bus_i.be(1) = '1' then lane1(a) <= bus_i.wdata(15 downto 8); end if;
                    if bus_i.be(2) = '1' then lane2(a) <= bus_i.wdata(23 downto 16); end if;
                    if bus_i.be(3) = '1' then lane3(a) <= bus_i.wdata(31 downto 24); end if;
                end if;
                data <= lane3(a) & lane2(a) & lane1(a) & lane0(a);
                ack <= '1';
            end if;
        end if;
    end process;
    bus_o.ack <= ack;
    bus_o.rdata <= data;
end architecture;
