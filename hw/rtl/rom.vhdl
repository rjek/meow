-- ROM: block RAM initialised from a file of hex words, one a line, read
-- on the bus with the answer the cycle after the request.  Reference
-- section 7 lets a slave take its time; one cycle is what a block RAM
-- with a registered output needs.
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use std.textio.all;
use work.meow_pkg.all;

entity rom is
    generic (
        FILE_NAME : string;
        WORDS     : natural := 32768
    );
    port (
        clk   : in  std_logic;
        bus_i : in  bus_m2s_t;
        bus_o : out bus_s2m_t
    );
end entity;

architecture rtl of rom is
    type mem_t is array (0 to WORDS - 1) of word_t;

    impure function load(name : string) return mem_t is
        file f : text;
        variable l : line;
        variable m : mem_t := (others => (others => '0'));
        variable w : word_t;
        variable i : natural := 0;
        variable good : boolean;
        variable status : file_open_status;
    begin
        file_open(status, f, name, read_mode);
        if status /= open_ok then
            return m;                       -- elaborated without an image: empty
        end if;
        while not endfile(f) and i < WORDS loop
            readline(f, l);
            hread(l, w, good);
            if good then
                m(i) := w;
                i := i + 1;
            end if;
        end loop;
        file_close(f);
        return m;
    end function;

    signal mem   : mem_t := load(FILE_NAME);
    signal data  : word_t := (others => '0');
    signal ack   : std_logic := '0';
begin
    process (clk)
    begin
        if rising_edge(clk) then
            ack <= '0';
            if bus_i.req = '1' and ack = '0' then
                data <= mem(to_integer(unsigned(bus_i.addr(26 downto 2))) mod WORDS);
                ack <= '1';
            end if;
        end if;
    end process;
    bus_o.ack <= ack;
    bus_o.rdata <= data;
end architecture;
