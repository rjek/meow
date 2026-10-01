-- The Chairman: reference section 5 and the bus of section 7.  It grants
-- one master at a time, round robin, answers its own chip select, 31,
-- and forwards everything else to the devices on one bus, with the chip
-- select decoded from the top five address bits.  Per CPU it keeps the
-- interrupt mask and pending word, a timer, and the control block; for
-- the system the chip-select table (generics), the locks and the
-- present mask.  The serial console registers are absent: a system has
-- an IOC.
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.meow_pkg.all;

entity chairman is
    generic (
        NCPU    : natural := 1;
        CLK_HZ  : natural := 1000000;
        CS_DEV  : reg_file_t := (others => DEV_NONE);   -- the table: device of each select
        CS_SIZE : reg_file_t := (others => (others => '0'))
    );
    port (
        clk      : in  std_logic;
        rst_n    : in  std_logic;
        tick     : in  std_logic;           -- the timers count when this is high: the clock, or the testbench's say

        m_i      : in  bus_m2s_array_t(0 to NCPU - 1);
        m_o      : out bus_s2m_array_t(0 to NCPU - 1);
        d_o      : out bus_m2s_t;           -- to the devices
        d_i      : in  bus_s2m_t;
        d_master : out natural range 0 to NCPU - 1;   -- who the forwarded transaction is from

        src      : in  std_logic_vector(29 downto 0);  -- the shared sources, held high while they hold
        irq      : out std_logic_vector(NCPU - 1 downto 0);
        cpu_run  : out std_logic_vector(NCPU - 1 downto 0);
        cpu_start : out reg_file_t          -- entry n: where CPU n starts
    );
end entity;

architecture rtl of chairman is
    signal mask, pending, reload, value, start_pc : word_array_t(0 to NCPU - 1) := (others => (others => '0'));
    signal running : std_logic_vector(NCPU - 1 downto 0) := (others => '0');
    signal lock : std_logic_vector(31 downto 0) := (others => '0');

    signal cur  : natural range 0 to NCPU - 1 := 0;
    signal busy : std_logic := '0';
    signal own_ack : std_logic;
    signal own_data : word_t;
    signal own_write : std_logic;
    signal m : bus_m2s_t;                   -- the granted master's request
    signal is_own : boolean;
begin
    -- the granted master, held while its transaction runs
    m <= m_i(cur);
    is_own <= m.addr(31 downto 27) = "11111";

    process (clk)
        variable n : natural;
    begin
        if rising_edge(clk) then
            if rst_n = '0' then
                busy <= '0';
                cur <= 0;
            elsif busy = '0' then
                -- grant the next requester after cur, round robin
                for k in 1 to NCPU loop
                    n := (cur + k) mod NCPU;
                    if m_i(n).req = '1' then
                        cur <= n;
                        busy <= '1';
                        exit;
                    end if;
                end loop;
            elsif (is_own and own_ack = '1') or (not is_own and d_i.ack = '1') then
                busy <= '0';
            end if;
        end if;
    end process;

    -- the devices see the granted transaction while it is not ours
    d_o.req   <= m.req when busy = '1' and not is_own else '0';
    d_o.wr    <= m.wr;
    d_o.addr  <= m.addr;
    d_o.be    <= m.be;
    d_o.wdata <= m.wdata;
    d_master  <= cur;

    gen_ack : for k in 0 to NCPU - 1 generate
        m_o(k).ack <= '1' when busy = '1' and cur = k and ((is_own and own_ack = '1') or (not is_own and d_i.ack = '1'))
                      else '0';
        m_o(k).rdata <= own_data when is_own else d_i.rdata;
        irq(k) <= '1' when (pending(k) and mask(k)) /= x"00000000" else '0';
        cpu_run(k) <= running(k);
        cpu_start(k) <= start_pc(k);
    end generate;
    gen_rest : for k in NCPU to 31 generate
        cpu_start(k) <= (others => '0');
    end generate;

    -- our own registers: answered in the cycle they are asked, which for
    -- a lock's read is also the moment it is taken
    own_ack <= m.req when busy = '1' and is_own else '0';

    process (all)
        variable off : unsigned(15 downto 0);
        variable n : natural;
        variable d : word_t;
    begin
        off := unsigned(m.addr(15 downto 0));
        d := (others => '0');
        if m.addr(26 downto 16) = (26 downto 16 => '0') then
            if off < x"2000" then
                n := to_integer(off(12 downto 8));
                if off(7 downto 0) = x"00" then
                    d := CS_DEV(n);
                elsif off(7 downto 0) = x"04" then
                    d := CS_SIZE(n);
                end if;
            elsif off >= x"2000" and off < x"2080" then
                n := to_integer(off(6 downto 2));
                if n < NCPU then d := mask(n); end if;
            elsif off >= x"2200" and off < x"2280" then
                n := to_integer(off(6 downto 2));
                if n < NCPU then d := pending(n); end if;
            elsif off = x"2400" then
                d := pending(cur);
            elsif off = x"2404" then
                d := std_logic_vector(to_unsigned(CLK_HZ, 32));
            elsif off = x"2408" then
                d := reload(cur);
            elsif off = x"240c" then
                d := value(cur);
            elsif off >= x"2800" and off < x"2c00" then
                n := to_integer(off(9 downto 5));
                if n < NCPU then
                    case off(4 downto 2) is
                        when "000" => d := x"0000000" & "00" & running(n) & '1';
                        when "001" => d := start_pc(n);
                        when "100" => d := reload(n);
                        when "101" => d := value(n);
                        when others => null;
                    end case;
                end if;
            elsif off = x"2c00" then
                for k in 0 to NCPU - 1 loop
                    d(k) := '1';
                end loop;
            elsif off >= x"2e00" and off < x"2e80" then
                d := x"0000000" & "000" & lock(to_integer(off(6 downto 2)));
            end if;
        end if;
        own_data <= d;
    end process;

    -- what changes: writes, the locks' taking, the timers and the sources.
    -- A write lands first and the timers and sources act on the result,
    -- as msim's cycle has the instruction then the devices.
    process (clk)
        variable off : unsigned(15 downto 0);
        variable n : natural;
        variable p, v, r : word_array_t(0 to NCPU - 1);
    begin
        if rising_edge(clk) then
            if rst_n = '0' then
                mask <= (others => (others => '0'));
                pending <= (others => (others => '0'));
                reload <= (others => (others => '0'));
                value <= (others => (others => '0'));
                start_pc <= (others => (others => '0'));
                running <= (others => '0');
                running(0) <= '1';
                lock <= (others => '0');
            else
                p := pending;
                v := value;
                r := reload;
                -- a register access of ours
                if own_ack = '1' and m.addr(26 downto 16) = (26 downto 16 => '0') then
                    off := unsigned(m.addr(15 downto 0));
                    if m.wr = '0' then
                        if off >= x"2e00" and off < x"2e80" then
                            lock(to_integer(off(6 downto 2))) <= '1';
                        end if;
                    else
                        if off >= x"2000" and off < x"2080" then
                            n := to_integer(off(6 downto 2));
                            if n < NCPU then mask(n) <= m.wdata; end if;
                        elsif off >= x"2200" and off < x"2280" then
                            n := to_integer(off(6 downto 2));
                            if n < NCPU then p(n) := p(n) and not m.wdata; end if;
                        elsif off = x"2400" then
                            p(cur) := p(cur) and not m.wdata;
                        elsif off = x"2408" then
                            r(cur) := m.wdata;
                            v(cur) := m.wdata;
                        elsif off = x"240c" then
                            v(cur) := m.wdata;
                        elsif off >= x"2800" and off < x"2c00" then
                            n := to_integer(off(9 downto 5));
                            if n < NCPU then
                                case off(4 downto 2) is
                                    when "001" => start_pc(n) <= m.wdata;
                                    when "010" =>
                                        if n /= 0 then
                                            if m.wdata = x"00000001" then
                                                running(n) <= '1';
                                            elsif m.wdata = x"00000000" then
                                                running(n) <= '0';
                                                mask(n) <= (others => '0');
                                                p(n) := (others => '0');
                                                r(n) := (others => '0');
                                                v(n) := (others => '0');
                                            end if;
                                        end if;
                                    when "011" => p(n)(30) := '1';
                                    when "100" => r(n) := m.wdata; v(n) := m.wdata;
                                    when "101" => v(n) := m.wdata;
                                    when others => null;
                                end case;
                            end if;
                        elsif off >= x"2e00" and off < x"2e80" then
                            lock(to_integer(off(6 downto 2))) <= m.wdata(0);
                        end if;
                    end if;
                end if;
                -- the sources: set in every CPU's word while they hold
                for k in 0 to NCPU - 1 loop
                    p(k)(29 downto 0) := p(k)(29 downto 0) or src;
                end loop;
                -- the timers
                if tick = '1' then
                    for k in 0 to NCPU - 1 loop
                        if running(k) = '1' and r(k) /= x"00000000" then
                            if unsigned(v(k)) = 1 then
                                v(k) := r(k);
                                p(k)(31) := '1';
                            else
                                v(k) := std_logic_vector(unsigned(v(k)) - 1);
                            end if;
                        end if;
                    end loop;
                end if;
                pending <= p;
                value <= v;
                reload <= r;
            end if;
        end if;
    end process;
end architecture;
