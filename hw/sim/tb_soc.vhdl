-- The testbench: a MEOW system running a ROM, checked against msim.
-- msim -T writes a line per instruction: the instruction's address and
-- word, then the sixteen registers of the active bank and the sixteen
-- of the other, all after the instruction.  This reads that file and,
-- each time core 0 retires an instruction, compares.  It also plays the
-- host's part on the coprocessor port: a negative BNV that msim
-- answered with a value in ir gets that value, and BNV #-2 is the end.
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
        ROM_WORDS : natural := 32768;
        RAM_WORDS : natural := 16384;
        MAX_CYCLES : natural := 10000000;
        VERBOSE   : boolean := false        -- a line per instruction: the bank and both pcs
    );
end entity;

architecture sim of tb_soc is
    signal clk : std_logic := '0';
    signal rst_n : std_logic := '0';
    signal cop_req, cop_ack, cop_wr : std_logic;
    signal cop_op : std_logic_vector(8 downto 0);
    signal cop_data : word_t;
    signal retire, bank : std_logic;
    signal regs : reg_file_t;
    signal pc : word_t;
    signal word : half_t;
    signal exp_ir : word_t := (others => '0');   -- what msim left in ir after the instruction under way
    signal finished : boolean := false;
    signal halted : boolean := false;

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
begin
    clk <= not clk after 5 ns when not finished;
    rst_n <= '1' after 25 ns;

    dut : entity work.meow_soc
        generic map (NCPU => 1, ROM_FILE => ROM, ROM_WORDS => ROM_WORDS, RAM_WORDS => RAM_WORDS,
                     TICK_FROM_CORE => true)
        port map (clk => clk, rst_n => rst_n,
                  cop_req => cop_req, cop_op => cop_op, cop_ack => cop_ack, cop_wr => cop_wr, cop_data => cop_data,
                  dbg_retire => retire, dbg_regs => regs, dbg_bank => bank, dbg_pc => pc, dbg_word => word);

    -- the host's part: the operand is signed halfwords, so -2 is "111111111"
    cop_ack  <= cop_req;
    cop_wr   <= '1' when cop_req = '1' and (cop_op = 9x"1fa" or cop_op = 9x"1f9" or cop_op = 9x"1f8" or cop_op = 9x"1f7")
                else '0';                      -- getc, time, cycles, hostfs: a value in ir
    cop_data <= exp_ir;

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
