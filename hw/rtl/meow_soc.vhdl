-- A MEOW system: NCPU cores, the Chairman, ROM at chip select 0, RAM at
-- 1 and the IOC at 2.  CPU 0's registers, coprocessor port and the
-- IOC's backdoor are brought out for the testbench; a board's top
-- level ties them off.
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.meow_pkg.all;

entity meow_soc is
    generic (
        NCPU      : natural := 1;
        ROM_FILE  : string  := "rom.hex";
        ROM_WORDS : natural := 32768;       -- 128 KB
        RAM_WORDS : natural := 16384;       -- 64 KB
        CLK_HZ    : natural := 1000000;
        TICK_FROM_CORE : boolean := false;  -- time counts instructions, as msim does, for the trace tests
        UART1_LOOPBACK : boolean := false;  -- as msim's UART 1
        DEBUG     : boolean := false        -- CPU 0's registers brought out
    );
    port (
        clk      : in  std_logic;
        rst_n    : in  std_logic;

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
        halted    : out std_logic;

        cop_req  : out std_logic;
        cop_op   : out std_logic_vector(8 downto 0);
        cop_ack  : in  std_logic;
        cop_wr   : in  std_logic;
        cop_data : in  word_t;

        tb_rx_n     : in  natural range 0 to 16;
        tb_rx_bytes : in  byte_array_t(0 to 15);
        tb_break    : in  std_logic;
        tb_poll     : out std_logic;
        tb_room     : out natural range 0 to 16;
        tb_used     : out std_logic;
        tb_tx_valid : out std_logic;
        tb_tx_data  : out std_logic_vector(7 downto 0);

        dbg_retire : out std_logic;
        dbg_tick   : out std_logic;
        dbg_regs   : out reg_file_t;
        dbg_bank   : out std_logic;
        dbg_pc     : out word_t;
        dbg_word   : out half_t
    );
end entity;

architecture rtl of meow_soc is
    function table_dev return reg_file_t is
        variable t : reg_file_t := (others => DEV_NONE);
    begin
        t(0) := DEV_ROM;
        t(1) := DEV_RAM;
        t(2) := DEV_IOC;
        t(31) := DEV_CHAIRMAN;
        return t;
    end function;

    function table_size return reg_file_t is
        variable t : reg_file_t := (others => (others => '0'));
    begin
        t(0) := std_logic_vector(to_unsigned(ROM_WORDS * 4, 32));
        t(1) := std_logic_vector(to_unsigned(RAM_WORDS * 4, 32));
        t(2) := x"00001000";
        return t;
    end function;

    signal m_i : bus_m2s_array_t(0 to NCPU - 1);
    signal m_o : bus_s2m_array_t(0 to NCPU - 1);
    signal d_o : bus_m2s_t;
    signal d_i : bus_s2m_t;
    signal rom_i, ram_i, ioc_i : bus_m2s_t;
    signal rom_o, ram_o, ioc_o : bus_s2m_t;
    signal cs : std_logic_vector(4 downto 0);
    signal none_ack : std_logic := '0';
    signal irq : std_logic_vector(NCPU - 1 downto 0);
    signal cpu_run : std_logic_vector(NCPU - 1 downto 0);
    signal cpu_start : reg_file_t;
    signal retire, waiting : std_logic_vector(NCPU - 1 downto 0);
    signal tick : std_logic;
    signal cop_req_v : std_logic_vector(NCPU - 1 downto 0);
    signal cop_op_v : std_logic_vector(9 * NCPU - 1 downto 0);
    signal d_master : natural range 0 to NCPU - 1;
    signal src : std_logic_vector(29 downto 0) := (others => '0');
    signal ioc_src : std_logic_vector(7 downto 0);
    signal sys_reset : std_logic;
    signal rst_all : std_logic;
begin
    rst_all <= rst_n and not sys_reset;

    core0 : entity work.meow_core
        generic map (CPU_ID => 0, MODEL => 1, DEBUG => DEBUG)
        port map (
            clk => clk, rst_n => rst_all, run => cpu_run(0), start_pc => cpu_start(0),
            bus_o => m_i(0), bus_i => m_o(0), irq => irq(0),
            cop_req => cop_req_v(0), cop_op => cop_op_v(8 downto 0),
            cop_ack => cop_ack, cop_wr => cop_wr, cop_data => cop_data,
            retire => retire(0), waiting => waiting(0),
            dbg_regs => dbg_regs, dbg_bank => dbg_bank, dbg_pc => dbg_pc, dbg_word => dbg_word);

    cores : for k in 1 to NCPU - 1 generate
        core : entity work.meow_core
            generic map (CPU_ID => k, MODEL => 1)
            port map (
                clk => clk, rst_n => rst_all, run => cpu_run(k), start_pc => cpu_start(k),
                bus_o => m_i(k), bus_i => m_o(k), irq => irq(k),
                cop_req => cop_req_v(k), cop_op => cop_op_v(9 * k + 8 downto 9 * k),
                cop_ack => '1', cop_wr => '0', cop_data => (others => '0'),
                retire => retire(k), waiting => waiting(k),
                dbg_regs => open, dbg_bank => open, dbg_pc => open, dbg_word => open);
    end generate;

    cop_req <= cop_req_v(0);
    cop_op  <= cop_op_v(8 downto 0);
    dbg_retire <= retire(0);
    dbg_tick <= tick;

    tick <= (retire(0) or waiting(0)) when TICK_FROM_CORE else '1';

    ch : entity work.chairman
        generic map (NCPU => NCPU, CLK_HZ => CLK_HZ, CS_DEV => table_dev, CS_SIZE => table_size)
        port map (
            clk => clk, rst_n => rst_all, tick => tick,
            m_i => m_i, m_o => m_o, d_o => d_o, d_i => d_i, d_master => d_master,
            src => src, irq => irq, cpu_run => cpu_run, cpu_start => cpu_start);
    src(7 downto 0) <= ioc_src;

    -- the devices, by chip select
    cs <= d_o.addr(31 downto 27);
    rom_i <= d_o when cs = "00000" else BUS_M2S_IDLE;
    ram_i <= d_o when cs = "00001" else BUS_M2S_IDLE;
    ioc_i <= d_o when cs = "00010" else BUS_M2S_IDLE;
    d_i <= rom_o when cs = "00000" else
           ram_o when cs = "00001" else
           ioc_o when cs = "00010" else
           (none_ack, (others => '0'));

    -- nothing there: answered with zero, a cycle later
    process (clk)
    begin
        if rising_edge(clk) then
            none_ack <= d_o.req and not none_ack;
        end if;
    end process;

    r : entity work.rom
        generic map (FILE_NAME => ROM_FILE, WORDS => ROM_WORDS)
        port map (clk => clk, bus_i => rom_i, bus_o => rom_o);

    w : entity work.ram
        generic map (WORDS => RAM_WORDS)
        port map (clk => clk, bus_i => ram_i, bus_o => ram_o);

    io : entity work.ioc
        generic map (CLK_HZ => CLK_HZ, UART1_LOOPBACK => UART1_LOOPBACK)
        port map (
            clk => clk, rst_n => rst_all, tick => tick, bus_i => ioc_i, bus_o => ioc_o, src => ioc_src,
            uart0_rxd => uart0_rxd, uart0_txd => uart0_txd, uart1_rxd => uart1_rxd, uart1_txd => uart1_txd,
            spi_sclk => spi_sclk, spi_mosi => spi_mosi, spi_miso => spi_miso, spi_cs_n => spi_cs_n,
            gpio_in => gpio_in, gpio_out => gpio_out, gpio_oe => gpio_oe, leds => leds,
            halted => halted, sys_reset => sys_reset,
            tb_rx_n => tb_rx_n, tb_rx_bytes => tb_rx_bytes, tb_break => tb_break, tb_poll => tb_poll,
            tb_room => tb_room, tb_used => tb_used, tb_tx_valid => tb_tx_valid, tb_tx_data => tb_tx_data);
end architecture;
