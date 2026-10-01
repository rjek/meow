-- The MEOW core: reference sections 1 to 3, as a multi-cycle machine.
--
-- An instruction is a fetch (one bus read of a halfword, as many cycles
-- as the bus takes), then one execute cycle, in which a memory
-- instruction also presents its bus transaction and waits for it.  So
-- two clocks an instruction with memory that answers at once, more
-- when it does not.  There is no pipeline, which is what lets pc read
-- as its own address and a write to pc take effect at once.
--
-- The registers are two banks of sixteen in one array; the active bank
-- is a bit, and swapping banks is flipping it.  The interrupt is taken
-- between instructions, at the start of a fetch, when the active bank's
-- I bit is clear.  BNV #6 waits in a state of its own until irq.
--
-- Operands that the reference leaves to the implementation (negative
-- BNVs) go out of the coprocessor port: whatever is there answers with
-- ack, and may put a value in ir.  Nothing there means they are no-ops,
-- which is what the real machine has; the testbench plays msim's part.
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.meow_pkg.all;

entity meow_core is
    generic (
        CPU_ID : natural := 0;
        MODEL  : natural := 1               -- BNV #0: 0 is msim, 1 is MEOW1
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

        -- for a testbench to watch
        dbg_regs   : out reg_file_t;
        dbg_bank   : out std_logic;
        dbg_pc     : out word_t;
        dbg_word   : out half_t
    );
end entity;

architecture rtl of meow_core is
    type state_t is (S_FETCH, S_EXEC, S_WAIT_IRQ);
    signal state : state_t := S_FETCH;
    signal regs  : reg_file_t := (others => (others => '0'));
    signal bank  : std_logic := '0';
    signal ir    : half_t := (others => '0');     -- the instruction being executed

    -- the active bank's registers as read
    function idx(b : std_logic; r : unsigned(3 downto 0)) return natural is
    begin
        return to_integer(unsigned'(b & r));
    end function;

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
    function lane_put(addr : word_t; size : natural; v : word_t) return word_t is
        variable r : word_t := (others => '0');
    begin
        case size is
            when 4 => r := v;
            when 2 => r := v(15 downto 0) & v(15 downto 0);
            when others => r := v(7 downto 0) & v(7 downto 0) & v(7 downto 0) & v(7 downto 0);
        end case;
        return r;
    end function;

    function bitop(op : std_logic_vector(1 downto 0); a, b : word_t) return word_t is
    begin
        case op is
            when "00" => return not b;
            when "01" => return a and b;
            when "10" => return a or b;
            when others => return a xor b;
        end case;
    end function;

    function shifter(v : word_t; left, rot, arith : std_logic; amount : unsigned(4 downto 0)) return word_t is
        variable n : natural := to_integer(amount);
    begin
        if rot = '1' then
            if left = '1' then
                return std_logic_vector(rotate_left(unsigned(v), n));
            else
                return std_logic_vector(rotate_right(unsigned(v), n));
            end if;
        elsif left = '1' then
            return std_logic_vector(shift_left(unsigned(v), n));
        elsif arith = '1' then
            return std_logic_vector(shift_right(signed(v), n));
        else
            return std_logic_vector(shift_right(unsigned(v), n));
        end if;
    end function;

    -- what the execute cycle decided, for the bus outputs
    signal mem_req  : std_logic;
    signal mem_wr   : std_logic;
    signal mem_addr : word_t;
    signal mem_size : natural range 1 to 4;
    signal mem_data : word_t;
    signal cop_req_i : std_logic;
    signal exec_done : std_logic;       -- the instruction completes at the coming edge
begin
    dbg_regs <= regs;
    dbg_bank <= bank;
    dbg_pc   <= regs(idx(bank, to_unsigned(R_PC, 4)));
    dbg_word <= ir;

    -- Decode and execute, combinationally from the registers and ir:
    -- the results are applied in the clocked process below.  Everything
    -- here is a function of state that does not change while a bus
    -- transaction waits, so the bus signals hold as section 7 asks.
    process (all)
        variable pc, sr, a, b, v, operand, res : word_t;
        variable ra, rd, rs : unsigned(3 downto 0);
        variable size : natural range 1 to 4;
        variable addr : word_t;
    begin
        pc := regs(idx(bank, to_unsigned(R_PC, 4)));
        mem_req  <= '0';
        mem_wr   <= '0';
        mem_addr <= (others => '0');
        mem_size <= 4;
        mem_data <= (others => '0');
        cop_req_i <= '0';
        if state = S_EXEC and ir(15 downto 13) = "111" then
            -- MEM: 111L vvvv SHWD aaaa
            ra := unsigned(ir(3 downto 0));
            rd := unsigned(ir(11 downto 8));
            if ir(7) = '1' then
                size := 2;
            elsif ir(6) = '1' then
                size := 4;
            else
                size := 1;
            end if;
            a := regs(idx(bank, ra));
            if ir(5) = '0' and ir(4) = '1' then
                addr := std_logic_vector(unsigned(a) - size);   -- decrease before
            else
                addr := a;
            end if;
            v := regs(idx(bank, rd));
            if ir(7) = '1' and ir(6) = '0' then
                v := x"0000" & v(31 downto 16);                 -- the high half
            end if;
            mem_req  <= '1';
            mem_wr   <= ir(12);
            mem_addr <= addr;
            mem_size <= size;
            mem_data <= lane_put(addr, size, v);
        elsif state = S_EXEC and ir(15 downto 12) = "1011" and ir(7 downto 6) = "01" then
            -- SPMEM: 1011 vvvv 01Lo oooo
            addr := std_logic_vector(unsigned(regs(idx(bank, to_unsigned(R_SP, 4)))) +
                                     (unsigned(ir(4 downto 0)) & "00"));
            mem_req  <= '1';
            mem_wr   <= ir(5);
            mem_addr <= addr;
            mem_size <= 4;
            mem_data <= regs(idx(bank, unsigned(ir(11 downto 8))));
        elsif state = S_EXEC and ir(15 downto 9) = "0001111" and ir(8) = '1' then
            cop_req_i <= '1';                                   -- a negative BNV
        end if;
    end process;

    -- the fetch and the memory instructions share the bus
    bus_o.req   <= '1' when state = S_FETCH and not (irq = '1' and regs(idx(bank, to_unsigned(R_SR, 4)))(SR_I) = '0')
                   else mem_req;
    bus_o.wr    <= mem_wr when state = S_EXEC else '0';
    bus_o.addr  <= mem_addr when state = S_EXEC else regs(idx(bank, to_unsigned(R_PC, 4)));
    bus_o.be    <= lanes_of(mem_addr, mem_size) when state = S_EXEC
                   else lanes_of(regs(idx(bank, to_unsigned(R_PC, 4))), 2);
    bus_o.wdata <= mem_data;
    cop_req     <= cop_req_i;
    cop_op      <= ir(8 downto 0);

    exec_done <= '1' when state = S_EXEC and ((mem_req = '1' and bus_i.ack = '1') or
                                               (cop_req_i = '1' and cop_ack = '1') or
                                               (mem_req = '0' and cop_req_i = '0'))
                 else '0';
    retire  <= exec_done;
    waiting <= '1' when state = S_WAIT_IRQ else '0';

    process (clk)
        variable pc, sr, a, b, v, operand, res, d : word_t;
        variable rd, rs, rn, rm, ra : unsigned(3 downto 0);
        variable cond : std_logic_vector(3 downto 0);
        variable off : word_t;
        variable size : natural range 1 to 4;
        variable addr : word_t;
        variable active_pc, active_sr : natural;
        variable next_pc : word_t;
        variable pc_written : boolean;
        variable done : boolean;        -- the instruction completes this cycle
        variable new_regs : reg_file_t;
        variable new_bank : std_logic;
        variable idx_d : natural;
        variable nz_sr : word_t;
        variable diff : unsigned(32 downto 0);
        variable wait_irq : boolean;
    begin
        if rising_edge(clk) then
            if rst_n = '0' or run = '0' then
                regs <= (others => (others => '0'));
                regs(16 + R_SR) <= (0 => '1', others => '0');   -- the other bank is the interrupt bank
                regs(R_PC) <= start_pc;
                bank <= '0';
                state <= S_FETCH;
            else
                new_regs := regs;
                new_bank := bank;
                active_pc := idx(bank, to_unsigned(R_PC, 4));
                active_sr := idx(bank, to_unsigned(R_SR, 4));
                pc := regs(active_pc);
                sr := regs(active_sr);
                case state is
                when S_FETCH =>
                    if irq = '1' and sr(SR_I) = '0' then
                        -- taken between instructions: the other bank runs from the vector
                        new_bank := not bank;
                        new_regs(idx(new_bank, to_unsigned(R_PC, 4))) := IRQ_VECTOR;
                        regs <= new_regs;
                        bank <= new_bank;
                    elsif bus_i.ack = '1' then
                        if pc(1) = '0' then
                            ir <= bus_i.rdata(15 downto 0);
                        else
                            ir <= bus_i.rdata(31 downto 16);
                        end if;
                        state <= S_EXEC;
                    end if;

                when S_WAIT_IRQ =>
                    if irq = '1' then
                        state <= S_FETCH;
                    end if;

                when S_EXEC =>
                    done := true;
                    wait_irq := false;
                    pc_written := false;
                    next_pc := std_logic_vector(unsigned(pc) + 2);
                    rd := unsigned(ir(11 downto 8));
                    idx_d := idx(bank, rd);
                    case ir(15 downto 12) is
                    when "0000" | "0001" =>
                        -- B: 000c ccca aaaa aaaa
                        cond := ir(12 downto 9);
                        off := sext(ir(8 downto 0) & '0', 10);
                        if cond = "1111" then
                            if ir(8) = '1' then
                                -- implementation-defined: the coprocessor port
                                if cop_ack = '1' then
                                    if cop_wr = '1' then
                                        new_regs(idx(bank, to_unsigned(R_IR, 4))) := cop_data;
                                    end if;
                                else
                                    done := false;
                                end if;
                            else
                                -- the field counts halfwords: operand 2n is field n
                                case ir(7 downto 0) is
                                when x"00" =>       -- BNV #0, model: byte 1, the MEOW project's
                                    new_regs(idx(bank, to_unsigned(R_IR, 4))) :=
                                        x"0000" & std_logic_vector(to_unsigned(MODEL, 8)) & x"00";
                                when x"01" =>       -- BNV #2, bus ID
                                    new_regs(idx(bank, to_unsigned(R_IR, 4))) := std_logic_vector(to_unsigned(CPU_ID, 32));
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
                            new_regs(active_pc) := std_logic_vector(unsigned(pc) + unsigned(off));
                            pc_written := true;
                        end if;

                    when "0010" | "0100" =>
                        -- ADD3, SUB3: 00x0 dddd iiii ssss
                        rs := unsigned(ir(3 downto 0));
                        if ir(7 downto 4) = "0000" then
                            a := regs(idx_d);
                            operand := regs(idx(bank, rs));
                        else
                            a := regs(idx(bank, rs));
                            operand := x"0000000" & ir(7 downto 4);
                        end if;
                        if ir(14) = '0' then
                            res := std_logic_vector(unsigned(a) + unsigned(operand));
                        else
                            res := std_logic_vector(unsigned(a) - unsigned(operand));
                        end if;
                        new_regs(idx_d) := res;
                        pc_written := rd = R_PC;

                    when "0011" =>
                        -- ADD8
                        new_regs(idx_d) := std_logic_vector(unsigned(regs(idx_d)) + unsigned(ir(7 downto 0)));
                        pc_written := rd = R_PC;

                    when "0101" =>
                        -- SUB8
                        new_regs(idx_d) := std_logic_vector(unsigned(regs(idx_d)) - unsigned(ir(7 downto 0)));
                        pc_written := rd = R_PC;

                    when "0110" | "0111" =>
                        -- CMPI, CMPR, TST
                        if ir(12) = '0' then
                            a := regs(idx_d);
                            b := sext(ir(7 downto 0), 8);
                        elsif ir(7 downto 6) = "00" then
                            a := regs(idx(bank xor ir(5), rd));
                            b := regs(idx(bank xor ir(4), unsigned(ir(3 downto 0))));
                        elsif ir(7 downto 6) = "10" then
                            a := regs(idx(bank xor ir(5), rd));
                            b := (others => '0');
                        else
                            a := (others => '0');       -- reserved: nothing happens
                            b := (others => '0');
                        end if;
                        if ir(12) = '0' or ir(7 downto 6) = "00" then
                            diff := ('0' & unsigned(a)) - ('0' & unsigned(b));
                            res := std_logic_vector(diff(31 downto 0));
                            nz_sr := sr;
                            nz_sr(SR_N) := res(31);
                            if unsigned(res) = 0 then nz_sr(SR_Z) := '1'; else nz_sr(SR_Z) := '0'; end if;
                            nz_sr(SR_C) := not diff(32);
                            nz_sr(SR_V) := (a(31) xor b(31)) and (a(31) xor res(31));
                            new_regs(active_sr) := nz_sr;
                        elsif ir(7 downto 6) = "10" then
                            res := a and std_logic_vector(shift_left(to_unsigned(1, 32), to_integer(unsigned(ir(4 downto 0)))));
                            nz_sr := sr;
                            nz_sr(SR_N) := res(31);
                            if unsigned(res) = 0 then nz_sr(SR_Z) := '1'; else nz_sr(SR_Z) := '0'; end if;
                            new_regs(active_sr) := nz_sr;
                        end if;

                    when "1000" =>
                        -- MOV: 1000 dddd bhwx ssss
                        v := regs(idx(bank xor ir(4), unsigned(ir(3 downto 0))));
                        if ir(7) = '1' then
                            v := v(23 downto 16) & v(31 downto 24) & v(7 downto 0) & v(15 downto 8);
                        end if;
                        if ir(6) = '1' then
                            v := v(15 downto 0) & v(31 downto 16);
                        end if;
                        new_regs(idx(bank xor ir(5), rd)) := v;
                        pc_written := rd = R_PC and ir(5) = '0';

                    when "1001" =>
                        -- LDI
                        new_regs(idx(bank, to_unsigned(R_IR, 4))) := sext(ir(11 downto 0), 12);

                    when "1010" =>
                        -- SHI: 1010 rrrr dR0i iiii; SHR: 1010 rrrr dR10 ssss
                        if ir(5) = '0' then
                            new_regs(idx_d) := shifter(regs(idx_d), ir(7), ir(6), '0', unsigned(ir(4 downto 0)));
                            pc_written := rd = R_PC;
                        elsif ir(4) = '0' then
                            new_regs(idx_d) := shifter(regs(idx_d), ir(7), ir(6), '0',
                                                       unsigned(regs(idx(bank, unsigned(ir(3 downto 0))))(4 downto 0)));
                            pc_written := rd = R_PC;
                        end if;

                    when "1011" =>
                        -- ASRI: 1011 rrrr 000i iiii; ASRR: 1011 rrrr 0010 ssss; SPMEM: 1011 vvvv 01Lo oooo
                        if ir(7 downto 5) = "000" then
                            new_regs(idx_d) := shifter(regs(idx_d), '0', '0', '1', unsigned(ir(4 downto 0)));
                            pc_written := rd = R_PC;
                        elsif ir(7 downto 4) = "0010" then
                            new_regs(idx_d) := shifter(regs(idx_d), '0', '0', '1',
                                                       unsigned(regs(idx(bank, unsigned(ir(3 downto 0))))(4 downto 0)));
                            pc_written := rd = R_PC;
                        elsif ir(7 downto 6) = "01" then
                            if bus_i.ack = '1' then
                                if ir(5) = '0' then
                                    new_regs(idx_d) := bus_i.rdata;
                                    pc_written := rd = R_PC;
                                end if;
                            else
                                done := false;
                            end if;
                        end if;

                    when "1100" | "1101" =>
                        -- BITR: 110n dddd oo00 ssss; BITI: 110n dddd oo1b bbbb
                        if ir(5) = '0' then
                            operand := regs(idx(bank, unsigned(ir(3 downto 0))));
                        else
                            operand := std_logic_vector(shift_left(to_unsigned(1, 32), to_integer(unsigned(ir(4 downto 0)))));
                        end if;
                        if (ir(5) = '1' or ir(4) = '0') and not (ir(12) = '1' and ir(7 downto 6) = "00") then
                            if ir(12) = '1' then
                                operand := not operand;
                            end if;
                            new_regs(idx_d) := bitop(ir(7 downto 6), regs(idx_d), operand);
                            pc_written := rd = R_PC;
                        end if;

                    when others =>
                        -- MEM: 111L vvvv SHWD aaaa, presented by the process above
                        ra := unsigned(ir(3 downto 0));
                        if bus_i.ack = '1' then
                            if mem_size = 4 then size := 4; elsif mem_size = 2 then size := 2; else size := 1; end if;
                            if ir(12) = '0' then
                                d := lane_data(mem_addr, size, bus_i.rdata);
                                if ir(7) = '1' and ir(6) = '0' then
                                    new_regs(idx_d) := d(15 downto 0) & regs(idx_d)(15 downto 0);
                                else
                                    new_regs(idx_d) := d;
                                end if;
                                pc_written := rd = R_PC;
                            end if;
                            if ir(5) = '1' then
                                if ir(4) = '1' then
                                    new_regs(idx(bank, ra)) := std_logic_vector(unsigned(regs(idx(bank, ra))) + size);
                                else
                                    new_regs(idx(bank, ra)) := std_logic_vector(unsigned(regs(idx(bank, ra))) - size);
                                end if;
                                pc_written := pc_written or ra = R_PC;
                            elsif ir(4) = '1' then
                                new_regs(idx(bank, ra)) := mem_addr;       -- decreased before
                                pc_written := pc_written or ra = R_PC;
                            end if;
                        else
                            done := false;
                        end if;
                    end case;

                    if done then
                        if not pc_written then
                            new_regs(active_pc) := next_pc;
                        end if;
                        regs <= new_regs;
                        bank <= new_bank;
                        if wait_irq then
                            state <= S_WAIT_IRQ;
                        else
                            state <= S_FETCH;
                        end if;
                    end if;
                end case;
            end if;
        end if;
    end process;
end architecture;
