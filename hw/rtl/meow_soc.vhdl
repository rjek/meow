-- A MEOW system: NCPU cores, the Chairman, ROM at chip select 0 and RAM
-- at 1, with the IOC still to come.  CPU 0's registers, bus and
-- coprocessor port are brought out for the testbench; a board's top
-- level ties the port off.
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
        TICK_FROM_CORE : boolean := false   -- timers count instructions, as msim does, for the trace tests
    );
    port (
        clk      : in  std_logic;
        rst_n    : in  std_logic;

        cop_req  : out std_logic;
        cop_op   : out std_logic_vector(8 downto 0);
        cop_ack  : in  std_logic;
        cop_wr   : in  std_logic;
        cop_data : in  word_t;

        dbg_retire : out std_logic;
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
        t(31) := DEV_CHAIRMAN;
        return t;
    end function;

    function table_size return reg_file_t is
        variable t : reg_file_t := (others => (others => '0'));
    begin
        t(0) := std_logic_vector(to_unsigned(ROM_WORDS * 4, 32));
        t(1) := std_logic_vector(to_unsigned(RAM_WORDS * 4, 32));
        return t;
    end function;

    signal m_i : bus_m2s_array_t(0 to NCPU - 1);
    signal m_o : bus_s2m_array_t(0 to NCPU - 1);
    signal d_o : bus_m2s_t;
    signal d_i : bus_s2m_t;
    signal rom_i, ram_i : bus_m2s_t;
    signal rom_o, ram_o : bus_s2m_t;
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
begin
    core0 : entity work.meow_core
        generic map (CPU_ID => 0, MODEL => 1)
        port map (
            clk => clk, rst_n => rst_n, run => cpu_run(0), start_pc => cpu_start(0),
            bus_o => m_i(0), bus_i => m_o(0), irq => irq(0),
            cop_req => cop_req_v(0), cop_op => cop_op_v(8 downto 0),
            cop_ack => cop_ack, cop_wr => cop_wr, cop_data => cop_data,
            retire => retire(0), waiting => waiting(0),
            dbg_regs => dbg_regs, dbg_bank => dbg_bank, dbg_pc => dbg_pc, dbg_word => dbg_word);

    cores : for k in 1 to NCPU - 1 generate
        core : entity work.meow_core
            generic map (CPU_ID => k, MODEL => 1)
            port map (
                clk => clk, rst_n => rst_n, run => cpu_run(k), start_pc => cpu_start(k),
                bus_o => m_i(k), bus_i => m_o(k), irq => irq(k),
                cop_req => cop_req_v(k), cop_op => cop_op_v(9 * k + 8 downto 9 * k),
                cop_ack => '1', cop_wr => '0', cop_data => (others => '0'),
                retire => retire(k), waiting => waiting(k),
                dbg_regs => open, dbg_bank => open, dbg_pc => open, dbg_word => open);
    end generate;

    cop_req <= cop_req_v(0);
    cop_op  <= cop_op_v(8 downto 0);
    dbg_retire <= retire(0);

    tick <= (retire(0) or waiting(0)) when TICK_FROM_CORE else '1';

    ch : entity work.chairman
        generic map (NCPU => NCPU, CLK_HZ => CLK_HZ, CS_DEV => table_dev, CS_SIZE => table_size)
        port map (
            clk => clk, rst_n => rst_n, tick => tick,
            m_i => m_i, m_o => m_o, d_o => d_o, d_i => d_i, d_master => d_master,
            src => (others => '0'), irq => irq, cpu_run => cpu_run, cpu_start => cpu_start);

    -- the devices, by chip select
    cs <= d_o.addr(31 downto 27);
    rom_i <= d_o when cs = "00000" else BUS_M2S_IDLE;
    ram_i <= d_o when cs = "00001" else BUS_M2S_IDLE;
    d_i <= rom_o when cs = "00000" else
           ram_o when cs = "00001" else
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
end architecture;
