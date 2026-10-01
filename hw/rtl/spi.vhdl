-- The IOC's SPI master, reference section 6.3: a byte at a time, the
-- clock the IOC clock over 2 over the divisor plus one, the chip
-- select a bit software drives.  Busy for 16 (divisor + 1) ticks from
-- the write, counting the write's own tick, which is what msim counts.
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity spi_master is
    port (
        clk   : in  std_logic;
        rst_n : in  std_logic;
        tick  : in  std_logic;

        sel   : in  std_logic;
        wr    : in  std_logic;
        off   : in  std_logic_vector(3 downto 2);
        wdata : in  std_logic_vector(31 downto 0);
        rdata : out std_logic_vector(31 downto 0);
        irq   : out std_logic;              -- a pulse at completion when asked for

        sclk  : out std_logic;
        mosi  : out std_logic;
        miso  : in  std_logic;
        cs_n  : out std_logic
    );
end entity;

architecture rtl of spi_master is
    signal control : std_logic_vector(31 downto 0) := (others => '0');
    signal shift_out, shift_in, last_in : std_logic_vector(7 downto 0) := (others => '0');
    signal busy, complete : std_logic := '0';
    signal half : natural range 0 to 16 := 0;    -- half periods done
    signal sub : unsigned(31 downto 0) := (others => '0');
    signal sclk_i : std_logic := '0';
    signal cpol, cpha : std_logic;
begin
    cpol <= control(1);
    cpha <= control(2);
    cs_n <= not control(3);
    sclk <= sclk_i;
    mosi <= shift_out(7);

    rdata <= (others => '0') when sel = '0' else
             control when off = "00" else
             x"000000" & last_in when off = "01" else
             x"0000000" & "00" & complete & busy when off = "10" else
             (others => '0');

    process (clk)
        variable b : std_logic;
        variable h : natural range 0 to 16;
        variable sb : unsigned(31 downto 0);
        variable si : std_logic_vector(7 downto 0);
        variable period : unsigned(31 downto 0);
    begin
        if rising_edge(clk) then
            irq <= '0';
            if rst_n = '0' then
                control <= (others => '0');
                busy <= '0';
                complete <= '0';
                sclk_i <= '0';
                half <= 0;
                sub <= (others => '0');
            else
                period := (x"000000" & unsigned(control(15 downto 8))) + 1;   -- ticks a half period
                b := busy;
                h := half;
                sb := sub;
                si := shift_in;
                if sel = '1' then
                    if wr = '1' then
                        if off = "00" then
                            control <= wdata;
                            if busy = '0' then
                                sclk_i <= wdata(1);                     -- idle at CPOL
                            end if;
                        elsif off = "01" and busy = '0' and control(0) = '1' then
                            shift_out <= wdata(7 downto 0);
                            b := '1';
                            h := 0;
                            sb := (others => '0');
                        end if;
                    elsif off = "10" then
                        complete <= '0';
                    end if;
                end if;
                -- this tick counts, the write's included: busy for 16 half periods
                if tick = '1' and b = '1' then
                    if sb + 1 = period then
                        sb := (others => '0');
                        -- an edge: with CPHA 0 the leading edge samples and
                        -- the trailing shifts; with CPHA 1 the other way
                        sclk_i <= not sclk_i;
                        if (h mod 2 = 0) = (cpha = '0') then
                            si := si(6 downto 0) & miso;
                        else
                            shift_out <= shift_out(6 downto 0) & '0';
                        end if;
                        h := h + 1;
                        if h = 16 then
                            b := '0';
                            complete <= '1';
                            last_in <= si;
                            sclk_i <= cpol;
                            if control(16) = '1' then
                                irq <= '1';
                            end if;
                        end if;
                    else
                        sb := sb + 1;
                    end if;
                end if;
                busy <= b;
                half <= h;
                sub <= sb;
                shift_in <= si;
            end if;
        end if;
    end process;
end architecture;
