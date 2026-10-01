-- The testbench: a MEOW system running a ROM, checked against msim.
-- msim -T writes a line per instruction: the instruction's address and
-- word, then the sixteen registers of the active bank and the sixteen
-- of the other, all after the instruction.  This reads that file and,
-- each time core 0 retires an instruction, compares.
--
-- It plays the host's part too, as msim does it: the coprocessor port
-- (a negative BNV that msim answered with a value in ir gets that
-- value; the prints are written; BNV #-2 is the end), standard input
-- into UART 0's receive FIFO when the status register is read and every
-- 4096 ticks once the UART has been touched, with a break at its end,
-- and what UART 0 sends written out.  Everything written, with the
-- "exit N" line, goes to the OUT file, to compare with msim's.
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use std.textio.all;
use std.env.all;
use work.meow_pkg.all;

entity tb_soc is
    generic (
        ROM       : string := "rom.hex";
        TRACE     : string := "trace.txt";
        IN_FILE   : string := "";           -- standard input, or none
        OUT_FILE  : string := "out.txt";
        SD_IMAGE  : string := "";
        HAVE_SD   : boolean := false;
        ROM_WORDS : natural := 32768;
        RAM_WORDS : natural := 16384;
        LOCAL_WORDS : natural := 1024;
        MAX_CYCLES : natural := 100000000;
        VERBOSE   : boolean := false        -- a line per instruction: the bank and both pcs
    );
end entity;

architecture sim of tb_soc is
    signal clk : std_logic := '0';
    signal rst_n : std_logic := '0';
    signal cop_req, cop_ack, cop_wr : std_logic;
    signal cop_op : std_logic_vector(8 downto 0);
    signal cop_data : word_t;
    signal retire, tick, bank : std_logic;
    signal regs : reg_file_t;
    signal pc : word_t;
    signal word : half_t;
    signal exp_ir : word_t := (others => '0');   -- what msim left in ir after the instruction under way
    signal finished : boolean := false;
    signal halted : boolean := false;
    signal exit_status : word_t := (others => '0');

    signal uart0_txd, uart1_txd, spi_sclk, spi_mosi, spi_miso, spi_cs_n : std_logic;
    signal gpio_out, gpio_oe : std_logic_vector(31 downto 0);
    signal tb_rx_n : natural range 0 to 16 := 0;
    signal tb_rx_bytes : byte_array_t(0 to 15) := (others => x"00");
    signal tb_break, tb_poll, tb_used, tb_tx_valid : std_logic;
    signal tb_room : natural range 0 to 16;
    signal tb_tx_data : std_logic_vector(7 downto 0);

    type char_file is file of character;

    function name_of(i : natural) return string is
    begin
        case i is
            when 11 => return "sp";
            when 12 => return "lr";
            when 13 => return "ir";
            when 14 => return "sr";
            when 15 => return "pc";
            when others => return "r" & integer'image(i);
        end case;
    end function;

    function hex_of(v : word_t) return string is     -- as printf's %x
        variable s : string(1 to 8);
        variable n : natural := 0;
        constant digits : string := "0123456789abcdef";
    begin
        for i in 7 downto 0 loop
            s(8 - i) := digits(to_integer(unsigned(v(4 * i + 3 downto 4 * i))) + 1);
        end loop;
        for i in 1 to 8 loop
            if s(i) /= '0' or i = 8 then
                return s(i to 8);
            end if;
        end loop;
        return "0";
    end function;
begin
    clk <= not clk after 5 ns when not finished;
    rst_n <= '1' after 25 ns;

    dut : entity work.meow_soc
        generic map (NCPU => 1, MODEL => 0, ROM_FILE => ROM, ROM_WORDS => ROM_WORDS, RAM_WORDS => RAM_WORDS,
                     LOCAL_WORDS => LOCAL_WORDS, TICK_FROM_CORE => true, UART1_LOOPBACK => true, DEBUG => true)
        port map (clk => clk, rst_n => rst_n,
                  uart0_rxd => '1', uart0_txd => uart0_txd, uart1_rxd => '1', uart1_txd => uart1_txd,
                  spi_sclk => spi_sclk, spi_mosi => spi_mosi, spi_miso => spi_miso, spi_cs_n => spi_cs_n,
                  gpio_in => (others => '0'), gpio_out => gpio_out, gpio_oe => gpio_oe, leds => open, halted => open,
                  cop_req => cop_req, cop_op => cop_op, cop_ack => cop_ack, cop_wr => cop_wr, cop_data => cop_data,
                  tb_rx_n => tb_rx_n, tb_rx_bytes => tb_rx_bytes, tb_break => tb_break, tb_poll => tb_poll,
                  tb_room => tb_room, tb_used => tb_used, tb_tx_valid => tb_tx_valid, tb_tx_data => tb_tx_data,
                  dbg_retire => retire, dbg_tick => tick, dbg_regs => regs, dbg_bank => bank, dbg_pc => pc,
                  dbg_word => word);

    with_card : if HAVE_SD generate
        card : entity work.sd_model
            generic map (IMAGE => SD_IMAGE)
            port map (sclk => spi_sclk, mosi => spi_mosi, miso => spi_miso, cs_n => spi_cs_n);
    end generate;
    without_card : if not HAVE_SD generate
        spi_miso <= '1';
    end generate;

    -- the host's part: the operand is signed halfwords, so -2 is "111111111"
    cop_ack  <= cop_req;
    cop_wr   <= '1' when cop_req = '1' and (cop_op = 9x"1fa" or cop_op = 9x"1f9" or cop_op = 9x"1f8" or cop_op = 9x"1f7")
                else '0';                      -- getc, time, cycles, hostfs: a value in ir
    cop_data <= exp_ir;

    -- standard input into UART 0, when msim would look at it: on a read
    -- of the status register with room in the FIFO, in that same cycle,
    -- and every 4096 ticks once the UART has been touched; everything
    -- there is, to the room there is, and the break at the end.
    process
        file f : char_file;
        variable status : file_open_status := name_error;
        variable opened, eof, used : boolean := false;
        variable c : character;
        variable n : natural;
        variable ticks : natural := 0;
        variable poll : boolean;
    begin
        wait on clk, tb_poll;
        if not opened then
            opened := true;
            if IN_FILE'length /= 0 then
                file_open(status, f, IN_FILE, read_mode);
            end if;
        end if;
        poll := false;
        if rising_edge(clk) then
            tb_rx_n <= 0;
            tb_break <= '0';
            if tb_used = '1' then
                used := true;
            end if;
            if tick = '1' then
                ticks := ticks + 1;
                if used and ticks mod 4096 = 0 then
                    poll := true;
                end if;
            end if;
        elsif tb_poll'event and tb_poll = '1' then
            poll := true;
        end if;
        if poll and not eof and rst_n = '1' then
            n := 0;
            while n < tb_room loop
                if status /= open_ok or endfile(f) then
                    eof := true;
                    tb_break <= '1';
                    exit;
                end if;
                read(f, c);
                tb_rx_bytes(n) <= std_logic_vector(to_unsigned(character'pos(c), 8));
                n := n + 1;
            end loop;
            tb_rx_n <= n;
            if VERBOSE and (n /= 0 or eof) then
                report "poll: " & integer'image(n) & " bytes at tick " & integer'image(ticks) &
                       " room " & integer'image(tb_room);
            end if;
        end if;
    end process;

    -- everything the program writes: UART 0, the prints, and the end
    process (clk)
        file out_f : char_file;
        variable opened : boolean := false;
        variable v : word_t;
        variable s : string(1 to 12);
        variable l : line;
        procedure put(str : string) is
        begin
            for i in str'range loop
                write(out_f, str(i));
            end loop;
        end procedure;
    begin
        if rising_edge(clk) then
            if not opened then
                file_open(out_f, OUT_FILE, write_mode);
                opened := true;
            end if;
            if tb_tx_valid = '1' then
                write(out_f, character'val(to_integer(unsigned(tb_tx_data))));
            end if;
            if cop_req = '1' then
                if bank = '0' then v := regs(R_IR); else v := regs(16 + R_IR); end if;
                case cop_op is
                when 9x"1fd" =>             -- BNV #-6: a character
                    write(out_f, character'val(to_integer(unsigned(v(7 downto 0)))));
                when 9x"1fc" =>             -- BNV #-8: signed decimal
                    put(integer'image(to_integer(signed(v))));
                when 9x"1fb" =>             -- BNV #-10: hex
                    put(hex_of(v));
                when 9x"1ff" =>             -- BNV #-2: the end, with the status
                    if not halted then
                        put("exit " & integer'image(to_integer(unsigned(v(7 downto 0)))) & LF);
                        file_close(out_f);
                    end if;
                when others => null;
                end case;
            end if;
        end if;
    end process;

    process
        file f : text;
        variable l : line;
        variable exp_pc : word_t;
        variable exp_word : half_t;
        variable exp : reg_file_t;
        variable good : boolean;
        variable n : natural := 0;
        variable cycles : natural := 0;
        variable act : natural;
        variable failed : boolean := false;
    begin
        file_open(f, TRACE, read_mode);
        wait until rst_n = '1';
        while not endfile(f) loop
            readline(f, l);
            hread(l, exp_pc, good);
            next when not good;
            hread(l, exp_word, good);
            for i in 0 to 31 loop
                hread(l, exp(i), good);
            end loop;
            exp_ir <= exp(R_IR);
            n := n + 1;
            -- the instruction's address and word are checked as it ends,
            -- the registers just after
            loop
                wait until rising_edge(clk);
                cycles := cycles + 1;
                if cycles > MAX_CYCLES then
                    report "instruction " & integer'image(n) & ": no progress after " &
                           integer'image(cycles) & " cycles" severity failure;
                end if;
                if halted then
                    report "halted at instruction " & integer'image(n) & " with " &
                           "trace left" severity failure;
                end if;
                exit when retire = '1';
            end loop;
            if pc /= exp_pc or word /= exp_word then
                report "instruction " & integer'image(n) & ": at " & to_hstring(pc) & " word " &
                       to_hstring(word) & ", msim at " & to_hstring(exp_pc) & " word " &
                       to_hstring(exp_word) severity failure;
            end if;
            wait for 1 ns;
            if VERBOSE then
                report integer'image(n) & ": " & to_hstring(exp_pc) & " " & to_hstring(exp_word) &
                       " bank " & std_logic'image(bank) & " pc0 " & to_hstring(regs(15)) &
                       " pc1 " & to_hstring(regs(31)) & " sr0 " & to_hstring(regs(14)) &
                       " sr1 " & to_hstring(regs(30));
            end if;
            for i in 0 to 31 loop
                if bank = '0' then act := i; else act := (i + 16) mod 32; end if;
                if regs(act) /= exp(i) then
                    report "instruction " & integer'image(n) & " at " & to_hstring(exp_pc) & " word " &
                           to_hstring(exp_word) & ": " & name_of(i mod 16) &
                           (" " & integer'image(i / 16)) & " is " & to_hstring(regs(act)) & ", msim has " &
                           to_hstring(exp(i)) severity error;
                    failed := true;
                end if;
            end loop;
            if failed then
                report "mismatch after instruction " & integer'image(n) severity failure;
            end if;
        end loop;
        file_close(f);
        -- everything matched: the next thing the core does should be the halt
        loop
            wait until rising_edge(clk);
            cycles := cycles + 1;
            exit when halted;
            if cycles > MAX_CYCLES then
                report "trace ended but the core ran on" severity failure;
            end if;
        end loop;
        report integer'image(n) & " instructions matched in " & integer'image(cycles) & " cycles";
        finished <= true;
        wait;
    end process;

    process (clk)
    begin
        if rising_edge(clk) then
            if cop_req = '1' and cop_op = 9x"1ff" then
                halted <= true;
            end if;
        end if;
    end process;
end architecture;
