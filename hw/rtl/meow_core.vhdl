-- The MEOW core: reference sections 1 to 3, as a multi-cycle machine.
--
-- An instruction is a fetch (one bus read of a halfword, as many cycles
-- as the bus takes), then one execute cycle, in which a memory
-- instruction also presents its bus transaction and waits for it, and
-- one more cycle when a memory instruction has a writeback as well as a
-- load.  So two clocks an instruction with memory that answers at once,
-- more when it does not.  There is no pipeline, which is what lets pc
-- read as its own address and a write to pc take effect at once.
--
-- The registers are two banks of sixteen.  r0 to r13 of both banks are
-- one small RAM with two read ports and one write port: the two
-- operands are read from it as the instruction arrives on the bus, and
-- a memory instruction's second write takes a cycle of its own.  sr and
-- pc of each bank are flops, since the fetch needs pc every cycle and
-- the flags change on their own.  The active bank is a bit, and
-- swapping banks is flipping it.
--
-- The interrupt is taken between instructions, at the start of a
-- fetch, when the active bank's I bit is clear.  BNV #6 waits in a
-- state of its own until irq.  Operands the reference leaves to the
-- implementation (negative BNVs) go out of the coprocessor port:
-- whatever is there answers with ack and may put a value in ir;
-- nothing there makes them no-ops, as on the real machine, and the
-- testbench plays msim's part.
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.meow_pkg.all;

entity meow_core is
    generic (
        CPU_ID : natural := 0;
        MODEL  : natural := 1;              -- BNV #0: 0 is msim, 1 is MEOW1
        DEBUG  : boolean := false           -- bring every register out, for a testbench
    );
    port (
        clk      : in  std_logic;
        rst_n    : in  std_logic;
        run      : in  std_logic;           -- 0 holds the core in reset, as the Chairman does CPUs other than 0
        start_pc : in  word_t;              -- where it begins when run rises

        bus_o    : out bus_m2s_t;
        bus_i    : in  bus_s2m_t;
        irq      : in  std_logic;

        cop_req  : out std_logic;
        cop_op   : out std_logic_vector(8 downto 0);   -- the BNV operand's field: halfwords, signed
        cop_ack  : in  std_logic;
        cop_wr   : in  std_logic;           -- with ack: cop_data goes to ir
        cop_data : in  word_t;

        retire   : out std_logic;           -- high in the cycle an instruction completes
        waiting  : out std_logic;           -- in WFI: the Chairman's timer still counts

        dbg_regs : out reg_file_t;          -- with DEBUG: both banks, bank 0 first
        dbg_bank : out std_logic;
        dbg_pc   : out word_t;
        dbg_word : out half_t
    );
end entity;

architecture rtl of meow_core is
    type state_t is (S_FETCH, S_EXEC, S_WB2, S_WAIT_IRQ);
    signal state : state_t := S_FETCH;

    -- the register file proper: r0 to r13 of each bank, bank in bit 4
    type pair_t is array (0 to 1) of word_t;
    signal rf    : reg_file_t := (others => (others => '0'));
    signal pcr   : pair_t := (others => (others => '0'));   -- pc of each bank
    signal srr   : pair_t := (others => (others => '0'));   -- sr of each bank
    signal bank  : std_logic := '0';
    signal ir    : half_t := (others => '0');     -- the instruction being executed
    signal ir_pc : word_t := (others => '0');     -- and where it came from
    signal opd   : word_t := (others => '0');     -- the register named by bits 11:8, as read
    signal ops   : word_t := (others => '0');     -- the one named by bits 3:0, or sp
    signal d_sel, s_sel : unsigned(4 downto 0) := (others => '0');       -- which they were

    -- the second write of a memory instruction
    signal wb2_sel : unsigned(4 downto 0) := (others => '0');
    signal wb2_val : word_t := (others => '0');

    function sel(b : std_logic; r : std_logic_vector(3 downto 0)) return unsigned is
    begin
        return unsigned(b & r);
    end function;

    function bank_of(b : std_logic) return natural is
    begin
        if b = '1' then return 1; else return 0; end if;
    end function;

    -- which registers an instruction reads, from its word
    procedure read_sel(w : half_t; b : std_logic; d, s : out unsigned(4 downto 0)) is
        variable db, sb : std_logic;
    begin
        db := b;
        sb := b;
        if w(15 downto 12) = "0111" or w(15 downto 12) = "1000" then
            db := b xor w(5);                               -- CMP, TST, MOV name a bank
            if w(15 downto 12) = "1000" or w(7 downto 6) = "00" then
                sb := b xor w(4);
            end if;
        end if;
        d := sel(db, w(11 downto 8));
        if w(15 downto 12) = "1011" and w(7 downto 6) = "01" then
            s := sel(b, std_logic_vector(to_unsigned(R_SP, 4)));   -- [sp, #n]
        else
            s := sel(sb, w(3 downto 0));
        end if;
    end procedure;

    function lanes_of(addr : word_t; size : natural) return std_logic_vector is
        variable be : std_logic_vector(3 downto 0);
    begin
        case size is
            when 4 => be := "1111";
            when 2 => if addr(1) = '0' then be := "0011"; else be := "1100"; end if;
            when others =>
                case addr(1 downto 0) is
                    when "00" => be := "0001";
                    when "01" => be := "0010";
                    when "10" => be := "0100";
                    when others => be := "1000";
                end case;
        end case;
        return be;
    end function;

    -- the data of a read, from its lanes to the low bits
    function lane_data(addr : word_t; size : natural; d : word_t) return word_t is
        variable r : word_t := (others => '0');
    begin
        case size is
            when 4 => r := d;
            when 2 => if addr(1) = '0' then r(15 downto 0) := d(15 downto 0); else r(15 downto 0) := d(31 downto 16); end if;
            when others =>
                case addr(1 downto 0) is
                    when "00" => r(7 downto 0) := d(7 downto 0);
                    when "01" => r(7 downto 0) := d(15 downto 8);
                    when "10" => r(7 downto 0) := d(23 downto 16);
                    when others => r(7 downto 0) := d(31 downto 24);
                end case;
        end case;
        return r;
    end function;

    -- and a value into its lanes for a write
    function lane_put(size : natural; v : word_t) return word_t is
    begin
        case size is
            when 4 => return v;
            when 2 => return v(15 downto 0) & v(15 downto 0);
            when others => return v(7 downto 0) & v(7 downto 0) & v(7 downto 0) & v(7 downto 0);
        end case;
    end function;

    -- decode of the instruction in ir, as signals the execute uses
    signal is_mem, is_spmem, is_cop : std_logic;
    signal mem_size : natural range 1 to 4;
    signal mem_addr : word_t;
    signal mem_wr   : std_logic;
    signal mem_data : word_t;
    signal exec_done : std_logic;
    signal pc_a, sr_a : word_t;         -- the active bank's
    -- what the execute decided, applied at the edge that completes it
    signal x_done, x_wen, x_pc_written, x_flags, x_other_pc_wr, x_wait_irq, x_wb2 : boolean;
    signal x_wsel : unsigned(4 downto 0);
    signal x_res, x_pc_val, x_new_sr, x_other_pc, x_wb2_val, x_next_pc : word_t;
    signal x_new_bank : std_logic;
    signal x_wb2_sel : unsigned(4 downto 0);
    signal opd_v, ops_v : word_t;       -- the operands, with sr and pc from their flops
    signal rf_wen : std_logic;
    signal rf_wsel : unsigned(4 downto 0);
    signal rf_wval : word_t;
begin
    pc_a <= pcr(bank_of(bank));
    sr_a <= srr(bank_of(bank));

    dbg_bank <= bank;
    dbg_pc   <= ir_pc;
    dbg_word <= ir;
    debug_view : if DEBUG generate
        process (all)
        begin
            for i in 0 to 31 loop
                if i mod 16 = R_PC then
                    dbg_regs(i) <= pcr(i / 16);
                elsif i mod 16 = R_SR then
                    dbg_regs(i) <= srr(i / 16);
                else
                    dbg_regs(i) <= rf(i);
                end if;
            end loop;
        end process;
    end generate;

    -- what the memory instructions present, from the operands read
    is_mem   <= '1' when state = S_EXEC and ir(15 downto 13) = "111" else '0';
    is_spmem <= '1' when state = S_EXEC and ir(15 downto 12) = "1011" and ir(7 downto 6) = "01" else '0';
    is_cop   <= '1' when state = S_EXEC and ir(15 downto 8) = "00011111" else '0';
    mem_size <= 4 when is_spmem = '1' else 2 when ir(7) = '1' else 4 when ir(6) = '1' else 1;
    mem_wr   <= ir(12) when is_mem = '1' else ir(5);
    process (all)
        variable v : word_t;
    begin
        if is_spmem = '1' then
            mem_addr <= std_logic_vector(unsigned(ops) + (unsigned(ir(4 downto 0)) & "00"));
            mem_data <= opd;
        else
            if ir(5) = '0' and ir(4) = '1' then
                mem_addr <= std_logic_vector(unsigned(ops) - mem_size);    -- decrease before
            else
                mem_addr <= ops;
            end if;
            v := opd;
            if ir(7) = '1' and ir(6) = '0' then
                v := x"0000" & opd(31 downto 16);                           -- the high half
            end if;
            mem_data <= lane_put(mem_size, v);
        end if;
    end process;

    -- the fetch and the memory instructions share the bus; a fetch is
    -- not started while an interrupt is to be taken instead
    bus_o.req   <= (is_mem or is_spmem) when state = S_EXEC
                   else '1' when state = S_FETCH and not (irq = '1' and sr_a(SR_I) = '0')
                   else '0';
    bus_o.wr    <= mem_wr when state = S_EXEC else '0';
    bus_o.addr  <= mem_addr when state = S_EXEC else pc_a;
    bus_o.be    <= lanes_of(mem_addr, mem_size) when state = S_EXEC else lanes_of(pc_a, 2);
    bus_o.wdata <= mem_data;
    cop_req     <= is_cop;
    cop_op      <= ir(8 downto 0);

    -- complete when its last write is: a memory instruction with a
    -- writeback has one more cycle
    retire  <= '1' when (state = S_EXEC and x_done and not x_wb2) or state = S_WB2 else '0';
    waiting <= '1' when state = S_WAIT_IRQ else '0';

    -- an operand that names sr or pc is the flop, not the RAM's word
    opd_v <= pcr(bank_of(d_sel(4))) when d_sel(3 downto 0) = R_PC else
             srr(bank_of(d_sel(4))) when d_sel(3 downto 0) = R_SR else opd;
    ops_v <= pcr(bank_of(s_sel(4))) when s_sel(3 downto 0) = R_PC else
             srr(bank_of(s_sel(4))) when s_sel(3 downto 0) = R_SR else ops;

    -- the register RAM: read as the instruction arrives, written as it
    -- ends, and once more for a memory instruction's writeback
    rf_wen  <= '1' when (state = S_EXEC and x_done and x_wen and x_wsel(3 downto 0) /= R_PC and x_wsel(3 downto 0) /= R_SR)
                     or state = S_WB2
               else '0';
    rf_wsel <= wb2_sel when state = S_WB2 else x_wsel;
    rf_wval <= wb2_val when state = S_WB2 else x_res;

    process (clk)
        variable d, s : unsigned(4 downto 0);
        variable w : half_t;
    begin
        if rising_edge(clk) then
            if rf_wen = '1' then
                rf(to_integer(rf_wsel)) <= rf_wval;
            end if;
            if state = S_FETCH and bus_i.ack = '1' then
                if pc_a(1) = '0' then w := bus_i.rdata(15 downto 0); else w := bus_i.rdata(31 downto 16); end if;
                read_sel(w, bank, d, s);
                d_sel <= d;
                s_sel <= s;
                opd <= rf(to_integer(d));
                ops <= rf(to_integer(s));
            end if;
        end if;
    end process;

    -- Execute: everything an instruction does, decided from ir and the
    -- operands, as signals the clocked process applies when x_done.
    process (all)
        variable pc, sr, a, b, res, d, v, operand : word_t;
        variable rs : unsigned(3 downto 0);
        variable wsel : unsigned(4 downto 0);
        variable wen : boolean;
        variable cond : std_logic_vector(3 downto 0);
        variable size : natural range 1 to 4;
        variable pc_written, flags, done, wait_irq, wb2, other_pc_wr : boolean;
        variable new_bank : std_logic;
        variable new_sr, pc_val, other_pc, wb2_v : word_t;
        variable wb2_s : unsigned(4 downto 0);
        variable diff : unsigned(32 downto 0);
        variable amount : unsigned(4 downto 0);
        variable sh_left, sh_rot, sh_arith : std_logic;
        variable shifted : word_t;
        variable n : natural;
    begin
        pc := pc_a;
        sr := sr_a;
        done := true;
        wait_irq := false;
        wb2 := false;
        pc_written := false;
        flags := false;
        wen := false;
        other_pc_wr := false;
        new_bank := bank;
        new_sr := sr;
        pc_val := pc;
        other_pc := pc;
        wb2_s := (others => '0');
        wb2_v := (others => '0');
        rs := unsigned(ir(3 downto 0));
        wsel := sel(bank, ir(11 downto 8));
        res := (others => '0');

        -- one shifter for the four shift forms
        if ir(15 downto 12) = "1011" then
            sh_left := '0'; sh_rot := '0'; sh_arith := '1';
        else
            sh_left := ir(7); sh_rot := ir(6); sh_arith := '0';
        end if;
        if ir(5) = '0' then
            amount := unsigned(ir(4 downto 0));
        else
            amount := unsigned(ops_v(4 downto 0));
        end if;
        n := to_integer(amount);
        if sh_rot = '1' then
            if sh_left = '1' then
                shifted := std_logic_vector(rotate_left(unsigned(opd_v), n));
            else
                shifted := std_logic_vector(rotate_right(unsigned(opd_v), n));
            end if;
        elsif sh_left = '1' then
            shifted := std_logic_vector(shift_left(unsigned(opd_v), n));
        elsif sh_arith = '1' then
            shifted := std_logic_vector(shift_right(signed(opd_v), n));
        else
            shifted := std_logic_vector(shift_right(unsigned(opd_v), n));
        end if;

        case ir(15 downto 12) is
        when "0000" | "0001" =>
            -- B: 000c ccca aaaa aaaa
            cond := ir(12 downto 9);
            if cond = "1111" then
                if ir(8) = '1' then
                    -- implementation-defined: the coprocessor port
                    if cop_ack = '1' then
                        if cop_wr = '1' then
                            wen := true;
                            wsel := sel(bank, std_logic_vector(to_unsigned(R_IR, 4)));
                            res := cop_data;
                        end if;
                    else
                        done := false;
                    end if;
                else
                    -- the field counts halfwords: operand 2n is field n
                    case ir(7 downto 0) is
                    when x"00" =>       -- BNV #0, model: byte 1, the MEOW project's
                        wen := true;
                        wsel := sel(bank, std_logic_vector(to_unsigned(R_IR, 4)));
                        res := x"0000" & std_logic_vector(to_unsigned(MODEL, 8)) & x"00";
                    when x"01" =>       -- BNV #2, bus ID
                        wen := true;
                        wsel := sel(bank, std_logic_vector(to_unsigned(R_IR, 4)));
                        res := std_logic_vector(to_unsigned(CPU_ID, 32));
                    when x"02" =>       -- BNV #4, IRQRTN
                        if sr(SR_I) = '1' then
                            new_bank := not bank;
                            pc_written := true;     -- neither bank's pc moves
                        end if;
                    when x"03" =>       -- BNV #6, WFI
                        wait_irq := true;
                    when others =>
                        null;           -- defined but not provided: a no-op
                    end case;
                end if;
            elsif cond_true(sr, cond) then
                pc_val := std_logic_vector(unsigned(pc) + unsigned(sext(ir(8 downto 0) & '0', 10)));
                pc_written := true;
            end if;

        when "0010" | "0100" =>
            -- ADD3, SUB3: 0x00 dddd iiii ssss
            if ir(7 downto 4) = "0000" then
                a := opd_v;
                operand := ops_v;
            else
                a := ops_v;
                operand := x"0000000" & ir(7 downto 4);
            end if;
            if ir(14) = '0' then
                res := std_logic_vector(unsigned(a) + unsigned(operand));
            else
                res := std_logic_vector(unsigned(a) - unsigned(operand));
            end if;
            wen := true;

        when "0011" =>
            res := std_logic_vector(unsigned(opd_v) + unsigned(ir(7 downto 0)));
            wen := true;

        when "0101" =>
            res := std_logic_vector(unsigned(opd_v) - unsigned(ir(7 downto 0)));
            wen := true;

        when "0110" | "0111" =>
            -- CMPI, CMPR, TST
            a := opd_v;
            if ir(12) = '0' then
                b := sext(ir(7 downto 0), 8);
            else
                b := ops_v;
            end if;
            if ir(12) = '0' or ir(7 downto 6) = "00" then
                diff := ('0' & unsigned(a)) - ('0' & unsigned(b));
                res := std_logic_vector(diff(31 downto 0));
                new_sr(SR_N) := res(31);
                if unsigned(res) = 0 then new_sr(SR_Z) := '1'; else new_sr(SR_Z) := '0'; end if;
                new_sr(SR_C) := not diff(32);
                new_sr(SR_V) := (a(31) xor b(31)) and (a(31) xor res(31));
                flags := true;
            elsif ir(7 downto 6) = "10" then
                res := a and std_logic_vector(shift_left(to_unsigned(1, 32), to_integer(unsigned(ir(4 downto 0)))));
                new_sr(SR_N) := res(31);
                if unsigned(res) = 0 then new_sr(SR_Z) := '1'; else new_sr(SR_Z) := '0'; end if;
                flags := true;
            end if;

        when "1000" =>
            -- MOV: 1000 dddd bhwx ssss
            v := ops_v;
            if ir(7) = '1' then
                v := v(23 downto 16) & v(31 downto 24) & v(7 downto 0) & v(15 downto 8);
            end if;
            if ir(6) = '1' then
                v := v(15 downto 0) & v(31 downto 16);
            end if;
            res := v;
            wen := true;
            wsel := sel(bank xor ir(5), ir(11 downto 8));

        when "1001" =>
            res := sext(ir(11 downto 0), 12);
            wen := true;
            wsel := sel(bank, std_logic_vector(to_unsigned(R_IR, 4)));

        when "1010" =>
            -- SHI: 1010 rrrr dR0i iiii; SHR: 1010 rrrr dR10 ssss
            if ir(5) = '0' or ir(4) = '0' then
                res := shifted;
                wen := true;
            end if;

        when "1011" =>
            -- ASRI: 1011 rrrr 000i iiii; ASRR: 1011 rrrr 0010 ssss; SPMEM: 1011 vvvv 01Lo oooo
            if ir(7 downto 5) = "000" or ir(7 downto 4) = "0010" then
                res := shifted;
                wen := true;
            elsif ir(7 downto 6) = "01" then
                if bus_i.ack = '1' then
                    if ir(5) = '0' then
                        res := bus_i.rdata;
                        wen := true;
                    end if;
                else
                    done := false;
                end if;
            end if;

        when "1100" | "1101" =>
            -- BITR: 110n dddd oo00 ssss; BITI: 110n dddd oo1b bbbb
            if ir(5) = '0' then
                operand := ops_v;
            else
                operand := std_logic_vector(shift_left(to_unsigned(1, 32), to_integer(unsigned(ir(4 downto 0)))));
            end if;
            if (ir(5) = '1' or ir(4) = '0') and not (ir(12) = '1' and ir(7 downto 6) = "00") then
                if ir(12) = '1' then
                    operand := not operand;
                end if;
                case ir(7 downto 6) is
                    when "00" => res := not operand;
                    when "01" => res := opd_v and operand;
                    when "10" => res := opd_v or operand;
                    when others => res := opd_v xor operand;
                end case;
                wen := true;
            end if;

        when others =>
            -- MEM: 111L vvvv SHWD aaaa, presented by the process above
            if bus_i.ack = '1' then
                size := mem_size;
                if ir(12) = '0' then
                    d := lane_data(mem_addr, size, bus_i.rdata);
                    if ir(7) = '1' and ir(6) = '0' then
                        res := d(15 downto 0) & opd_v(15 downto 0);
                    else
                        res := d;
                    end if;
                    wen := true;
                end if;
                if ir(5) = '1' or ir(4) = '1' then
                    -- the writeback of the address register: a second write
                    if ir(5) = '0' then
                        v := mem_addr;                                      -- decreased before
                    elsif ir(4) = '1' then
                        v := std_logic_vector(unsigned(ops_v) + size);
                    else
                        v := std_logic_vector(unsigned(ops_v) - size);
                    end if;
                    if rs = R_PC then
                        pc_val := v;
                        pc_written := true;
                    elsif rs = R_SR then
                        new_sr := v;
                        flags := true;
                    else
                        wb2 := true;
                        wb2_s := sel(bank, ir(3 downto 0));
                        wb2_v := v;
                    end if;
                end if;
            else
                done := false;
            end if;
        end case;

        -- a first write that names sr or pc goes to the flops
        if wen and wsel(3 downto 0) = R_PC then
            if wsel(4) = bank then
                pc_val := res;
                pc_written := true;
            else
                other_pc := res;
                other_pc_wr := true;
            end if;
        elsif wen and wsel(3 downto 0) = R_SR and wsel(4) = bank then
            new_sr := res;
            flags := true;
        end if;

        x_done <= done;
        x_wen <= wen;
        x_wsel <= wsel;
        x_res <= res;
        x_pc_written <= pc_written;
        x_pc_val <= pc_val;
        x_next_pc <= std_logic_vector(unsigned(pc) + 2);
        x_flags <= flags;
        x_new_sr <= new_sr;
        x_new_bank <= new_bank;
        x_other_pc_wr <= other_pc_wr;
        x_other_pc <= other_pc;
        x_wait_irq <= wait_irq;
        x_wb2 <= wb2;
        x_wb2_sel <= wb2_s;
        x_wb2_val <= wb2_v;
    end process;

    exec_done <= '1' when state = S_EXEC and x_done else '0';

    -- the state, the banks' pc and sr, and the instruction register
    process (clk)
    begin
        if rising_edge(clk) then
            if rst_n = '0' or run = '0' then
                pcr(0) <= start_pc;
                pcr(1) <= (others => '0');
                srr(0) <= (others => '0');
                srr(1) <= (0 => '1', others => '0');          -- the other bank is the interrupt bank
                bank <= '0';
                state <= S_FETCH;
            else
                case state is
                when S_FETCH =>
                    if irq = '1' and sr_a(SR_I) = '0' then
                        -- taken between instructions: the other bank runs from the vector
                        bank <= not bank;
                        pcr(bank_of(not bank)) <= IRQ_VECTOR;
                    elsif bus_i.ack = '1' then
                        if pc_a(1) = '0' then
                            ir <= bus_i.rdata(15 downto 0);
                        else
                            ir <= bus_i.rdata(31 downto 16);
                        end if;
                        ir_pc <= pc_a;
                        state <= S_EXEC;
                    end if;

                when S_WAIT_IRQ =>
                    if irq = '1' then
                        state <= S_FETCH;
                    end if;

                when S_WB2 =>
                    state <= S_FETCH;

                when S_EXEC =>
                    if x_done then
                        if x_wen and x_wsel(3 downto 0) = R_SR and x_wsel(4) /= bank then
                            srr(bank_of(not bank)) <= x_res;
                        end if;
                        if x_flags then
                            srr(bank_of(bank)) <= x_new_sr;
                        end if;
                        if x_other_pc_wr then
                            pcr(bank_of(not bank)) <= x_other_pc;
                        end if;
                        if x_pc_written then
                            if x_new_bank = bank then          -- IRQRTN moves neither
                                pcr(bank_of(bank)) <= x_pc_val;
                            end if;
                        else
                            pcr(bank_of(bank)) <= x_next_pc;
                        end if;
                        bank <= x_new_bank;
                        wb2_sel <= x_wb2_sel;
                        wb2_val <= x_wb2_val;
                        if x_wait_irq then
                            state <= S_WAIT_IRQ;
                        elsif x_wb2 then
                            state <= S_WB2;
                        else
                            state <= S_FETCH;
                        end if;
                    end if;
                end case;
            end if;
        end if;
    end process;
end architecture;
