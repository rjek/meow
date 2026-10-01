-- The IOC, reference section 6: identification and clock, two UARTs,
-- the SPI master, GPIO, the real-time clock and counter, and system
-- control, each part 256 bytes apart.  Word access only.  Time is in
-- ticks, as the UART and SPI say.
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.meow_pkg.all;

entity ioc is
    generic (
        CLK_HZ     : natural := 1000000;
        GPIO_LINES : natural := 32;
        UART1_LOOPBACK : boolean := false
    );
    port (
        clk   : in  std_logic;
        rst_n : in  std_logic;
        tick  : in  std_logic;

        bus_i : in  bus_m2s_t;
        bus_o : out bus_s2m_t;
        src   : out std_logic_vector(7 downto 0);   -- the Chairman's sources 0 to 7

        uart0_rxd : in  std_logic;
        uart0_txd : out std_logic;
        uart1_rxd : in  std_logic;
        uart1_txd : out std_logic;
        spi_sclk  : out std_logic;
        spi_mosi  : out std_logic;
        spi_miso  : in  std_logic;
        spi_cs_n  : out std_logic;
        gpio_in   : in  std_logic_vector(31 downto 0);
        gpio_out  : out std_logic_vector(31 downto 0);
        gpio_oe   : out std_logic_vector(31 downto 0);
        leds      : out word_t;
        halted    : out std_logic;          -- halt 2: the end
        sys_reset : out std_logic;          -- a pulse: reset the system

        -- the testbench's backdoor to UART 0
        tb_rx_n     : in  natural range 0 to 16;
        tb_rx_bytes : in  byte_array_t(0 to 15);
        tb_break    : in  std_logic;
        tb_poll     : out std_logic;
        tb_room     : out natural range 0 to 16;
        tb_used     : out std_logic;
        tb_tx_valid : out std_logic;
        tb_tx_data  : out std_logic_vector(7 downto 0)
    );
end entity;

architecture rtl of ioc is
    signal part : unsigned(3 downto 0);
    signal off  : unsigned(7 downto 0);
    signal ack  : std_logic := '0';
    signal rd_now, rd_r : word_t;           -- the read's data as asked, and as answered a cycle on
    signal acc  : std_logic;                -- this cycle's access, answered at once
    signal sel_u0, sel_u1, sel_spi : std_logic;
    signal rd_u0, rd_u1, rd_spi : word_t;
    signal irq_u0, irq_u1, irq_spi : std_logic;

    signal gpio_dir, gpio_val, gpio_rise, gpio_fall, gpio_pending : word_t := (others => '0');
    signal gpio_level, gpio_last : word_t := (others => '0');
    signal seconds, alarm, counter : word_t := (others => '0');
    signal rtc_status : word_t := x"00000002";
    signal second_sub : unsigned(31 downto 0) := (others => '0');
    signal leds_r : word_t := (others => '0');
    signal halted_r : std_logic := '0';
begin
    part <= unsigned(bus_i.addr(11 downto 8));
    off  <= unsigned(bus_i.addr(7 downto 0));
    acc  <= bus_i.req and not ack;         -- one cycle per transaction
    sel_u0  <= acc when part = 1 else '0';
    sel_u1  <= acc when part = 2 else '0';
    sel_spi <= acc when part = 3 else '0';

    u0 : entity work.uart
        generic map (LOOPBACK => false)
        port map (clk => clk, rst_n => rst_n, tick => tick,
                  sel => sel_u0, wr => bus_i.wr, off => bus_i.addr(4 downto 2), wdata => bus_i.wdata, rdata => rd_u0,
                  irq => irq_u0, rxd => uart0_rxd, txd => uart0_txd,
                  tb_rx_n => tb_rx_n, tb_rx_bytes => tb_rx_bytes, tb_break => tb_break, tb_poll => tb_poll,
                  tb_room => tb_room, tb_used => tb_used, tb_tx_valid => tb_tx_valid, tb_tx_data => tb_tx_data);

    u1 : entity work.uart
        generic map (LOOPBACK => UART1_LOOPBACK)
        port map (clk => clk, rst_n => rst_n, tick => tick,
                  sel => sel_u1, wr => bus_i.wr, off => bus_i.addr(4 downto 2), wdata => bus_i.wdata, rdata => rd_u1,
                  irq => irq_u1, rxd => uart1_rxd, txd => uart1_txd,
                  tb_rx_n => 0, tb_rx_bytes => (others => x"00"), tb_break => '0', tb_poll => open,
                  tb_room => open, tb_used => open, tb_tx_valid => open, tb_tx_data => open);

    s : entity work.spi_master
        port map (clk => clk, rst_n => rst_n, tick => tick,
                  sel => sel_spi, wr => bus_i.wr, off => bus_i.addr(3 downto 2), wdata => bus_i.wdata, rdata => rd_spi,
                  irq => irq_spi, sclk => spi_sclk, mosi => spi_mosi, miso => spi_miso, cs_n => spi_cs_n);

    gpio_out <= gpio_val;
    gpio_oe  <= gpio_dir;
    gpio_level <= (gpio_in and not gpio_dir) or (gpio_val and gpio_dir);
    leds <= leds_r;
    halted <= halted_r;

    src(0) <= irq_u0;
    src(1) <= irq_u1;
    src(2) <= irq_spi;
    src(3) <= '1' when gpio_pending /= x"00000000" else '0';
    src(4) <= rtc_status(0);
    src(7 downto 5) <= "000";

    -- the parts of our own: identification, GPIO, clock, system control
    process (all)
    begin
        rd_now <= (others => '0');
        case part is
        when x"0" =>
            if off = x"00" then
                rd_now <= "000000" & "11" & std_logic_vector(to_unsigned(GPIO_LINES, 8)) & x"02" & x"00";
            elsif off = x"04" then
                rd_now <= std_logic_vector(to_unsigned(CLK_HZ, 32));
            end if;
        when x"1" => rd_now <= rd_u0;
        when x"2" => rd_now <= rd_u1;
        when x"3" => rd_now <= rd_spi;
        when x"4" =>
            case off is
            when x"00" => rd_now <= gpio_dir;
            when x"04" => rd_now <= gpio_val;
            when x"08" => rd_now <= gpio_level;
            when x"14" => rd_now <= gpio_rise;
            when x"18" => rd_now <= gpio_fall;
            when x"1c" => rd_now <= gpio_pending;
            when others => null;
            end case;
        when x"5" =>
            case off is
            when x"00" => rd_now <= seconds;
            when x"04" => rd_now <= alarm;
            when x"08" => rd_now <= counter;
            when x"0c" => rd_now <= rtc_status;
            when others => null;
            end case;
        when x"f" =>
            if off = x"08" then
                rd_now <= leds_r;
            end if;
        when others => null;
        end case;
    end process;
    bus_o.ack <= ack;
    bus_o.rdata <= rd_r;

    process (clk)
        variable v, p : word_t;
    begin
        if rising_edge(clk) then
            ack <= acc;
            if acc = '1' then
                rd_r <= rd_now;
            end if;
            sys_reset <= '0';
            if rst_n = '0' then
                gpio_dir <= (others => '0'); gpio_val <= (others => '0');
                gpio_rise <= (others => '0'); gpio_fall <= (others => '0'); gpio_pending <= (others => '0');
                seconds <= (others => '0'); alarm <= (others => '0'); counter <= (others => '0');
                rtc_status <= x"00000002"; second_sub <= (others => '0');
                leds_r <= (others => '0');
                halted_r <= '0';
                gpio_last <= (others => '0');
            else
                v := gpio_val;
                p := gpio_pending;
                if acc = '1' and bus_i.wr = '1' then
                    case part is
                    when x"4" =>
                        case off is
                        when x"00" => gpio_dir <= bus_i.wdata;
                        when x"04" => v := bus_i.wdata;
                        when x"0c" => v := v or bus_i.wdata;
                        when x"10" => v := v and not bus_i.wdata;
                        when x"14" => gpio_rise <= bus_i.wdata;
                        when x"18" => gpio_fall <= bus_i.wdata;
                        when x"1c" => p := p and not bus_i.wdata;
                        when others => null;
                        end case;
                    when x"5" =>
                        case off is
                        when x"00" => seconds <= bus_i.wdata;
                        when x"04" => alarm <= bus_i.wdata;
                        when x"0c" => if bus_i.wdata(0) = '1' then rtc_status(0) <= '0'; end if;
                        when others => null;
                        end case;
                    when x"f" =>
                        case off is
                        when x"00" =>
                            if bus_i.wdata = x"00000002" then halted_r <= '1'; end if;
                        when x"04" =>
                            if bus_i.wdata = x"4d454f57" then sys_reset <= '1'; end if;
                        when x"08" => leds_r <= bus_i.wdata;
                        when others => null;
                        end case;
                    when others => null;
                    end case;
                end if;
                gpio_val <= v;
                -- edges on the lines as they are now driven or read
                gpio_last <= gpio_level;
                p := p or (gpio_level and not gpio_last and gpio_rise) or (gpio_last and not gpio_level and gpio_fall);
                gpio_pending <= p;
                -- the counter and the clock
                if tick = '1' then
                    counter <= std_logic_vector(unsigned(counter) + 1);
                    if second_sub + 1 = CLK_HZ then
                        second_sub <= (others => '0');
                        seconds <= std_logic_vector(unsigned(seconds) + 1);
                        if alarm /= x"00000000" and std_logic_vector(unsigned(seconds) + 1) = alarm then
                            rtc_status(0) <= '1';
                        end if;
                    else
                        second_sub <= second_sub + 1;
                    end if;
                end if;
            end if;
        end if;
    end process;
end architecture;
