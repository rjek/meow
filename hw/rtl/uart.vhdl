-- A UART of the IOC, reference section 6.2: 8N1, 16-byte FIFOs each
-- way, the bit rate the clock over 16 over the divisor plus one.
--
-- Time is counted in ticks, which on a board are every clock and in the
-- testbench are what msim counts, so that the status bits change at the
-- same instruction as msim's: the transmitter's frame counter starts
-- when it takes a byte and a byte is counted as gone 160 (divisor + 1)
-- ticks later, the next starting at once if one waits.  The receiver
-- samples sixteen times a bit from the start bit's edge.  A break is a
-- frame of zeros with the line still low at the stop bit.
--
-- The testbench's backdoor puts bytes straight into the receive FIFO
-- and raises the break, and sees each byte the transmitter takes, as
-- msim's console does; a board leaves those inputs at zero.
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.meow_pkg.all;

entity uart is
    generic (
        LOOPBACK : boolean := false         -- what is sent is received, as msim's UART 1
    );
    port (
        clk     : in  std_logic;
        rst_n   : in  std_logic;
        tick    : in  std_logic;

        -- the registers: offset within the part, word access
        sel     : in  std_logic;            -- a read or write of this UART, this cycle
        wr      : in  std_logic;
        off     : in  std_logic_vector(4 downto 2);
        wdata   : in  std_logic_vector(31 downto 0);
        rdata   : out std_logic_vector(31 downto 0);
        irq     : out std_logic;            -- held while an enabled condition holds

        rxd     : in  std_logic;
        txd     : out std_logic;

        tb_rx_n     : in  natural range 0 to 16;   -- testbench: this many bytes into the receive FIFO
        tb_rx_bytes : in  byte_array_t(0 to 15);
        tb_break    : in  std_logic;        -- testbench: a break has come
        tb_poll     : out std_logic;        -- the status was read with room to receive
        tb_room     : out natural range 0 to 16;    -- how many the FIFO can take
        tb_used     : out std_logic;        -- a register of this UART is being touched
        tb_tx_valid : out std_logic;        -- a byte went into the transmit FIFO
        tb_tx_data  : out std_logic_vector(7 downto 0)
    );
end entity;

architecture rtl of uart is
    constant DEPTH : natural := 16;
    type fifo_t is array (0 to DEPTH - 1) of std_logic_vector(7 downto 0);
    signal rx_fifo, tx_fifo : fifo_t := (others => (others => '0'));
    signal rx_head, rx_count, tx_head, tx_count : natural range 0 to DEPTH := 0;
    signal tx_pending : natural range 0 to DEPTH := 0;   -- in the FIFO or going: what room and idle count
    signal divisor : unsigned(31 downto 0) := (others => '0');
    signal ien : std_logic_vector(1 downto 0) := "00";
    signal overrun, framing, brk : std_logic := '0';

    -- the transmitter: a frame of ten bits, each 16 (divisor + 1) ticks
    signal tx_busy : std_logic := '0';
    signal tx_shift : std_logic_vector(9 downto 0) := (others => '1');
    signal tx_bit : natural range 0 to 9 := 0;
    signal tx_sub : unsigned(31 downto 0) := (others => '0');   -- ticks into the bit
    signal tx_bits_left : natural range 0 to 10 := 0;

    -- the receiver: sixteen samples a bit from the start edge
    signal rx_sync : std_logic_vector(1 downto 0) := "11";
    signal rx_busy : std_logic := '0';
    signal rx_sub : unsigned(31 downto 0) := (others => '0');
    signal rx_sample : natural range 0 to 15 := 0;
    signal rx_bit : natural range 0 to 9 := 0;
    signal rx_shift : std_logic_vector(7 downto 0) := (others => '0');
    signal rx_line : std_logic;
begin
    rx_line <= rx_sync(1);
    irq <= '1' when (ien(0) = '1' and rx_count /= 0) or (ien(1) = '1' and tx_pending < DEPTH) else '0';

    rdata <= (others => '0') when sel = '0' else
             x"00" & std_logic_vector(to_unsigned(DEPTH, 8)) & std_logic_vector(to_unsigned(rx_count, 8)) &
             "00" & brk & framing & overrun & bool_to_sl(tx_pending = 0) &
             bool_to_sl(tx_pending < DEPTH) & bool_to_sl(rx_count /= 0)
                 when off = "000" else
             x"000000" & rx_fifo(rx_head mod DEPTH) when off = "001" else
             std_logic_vector(divisor) when off = "010" else
             x"0000000" & "00" & ien when off = "011" else
             (others => '0');
    tb_poll <= '1' when sel = '1' and wr = '0' and off = "000" and rx_count < DEPTH else '0';
    tb_room <= DEPTH - rx_count;
    tb_used <= sel;

    process (clk)
        variable rxc, txc, txp : natural range 0 to DEPTH;
        variable rxh, txh : natural range 0 to DEPTH - 1;
        variable push_rx : boolean;
        variable push_data : std_logic_vector(7 downto 0);
        variable bit_ticks : unsigned(31 downto 0);
        variable tbusy : std_logic;
        variable tshift : std_logic_vector(9 downto 0);
        variable tsub : unsigned(31 downto 0);
        variable tleft : natural range 0 to 10;
    begin
        if rising_edge(clk) then
            tb_tx_valid <= '0';
            tb_tx_data <= (others => '0');
            if rst_n = '0' then
                rx_head <= 0; rx_count <= 0; tx_head <= 0; tx_count <= 0; tx_pending <= 0;
                divisor <= (others => '0');
                ien <= "00";
                overrun <= '0'; framing <= '0'; brk <= '0';
                tx_busy <= '0'; tx_bits_left <= 0; txd <= '1';
                rx_busy <= '0'; rx_sync <= "11";
            else
                rxc := rx_count; rxh := rx_head mod DEPTH;
                txc := tx_count; txh := tx_head mod DEPTH;
                txp := tx_pending;
                tbusy := tx_busy; tshift := tx_shift; tsub := tx_sub; tleft := tx_bits_left;
                bit_ticks := shift_left(divisor + 1, 4);            -- 16 (divisor + 1)
                push_rx := false;
                push_data := (others => '0');
                rx_sync <= rx_sync(0) & rxd;

                -- the registers, first
                if sel = '1' and wr = '1' then
                    case off is
                    when "001" =>
                        if txp < DEPTH then
                            tx_fifo((txh + txc) mod DEPTH) <= wdata(7 downto 0);
                            txc := txc + 1;
                            txp := txp + 1;
                            tb_tx_valid <= '1';
                            tb_tx_data <= wdata(7 downto 0);
                            if LOOPBACK then
                                push_rx := true;
                                push_data := wdata(7 downto 0);
                            end if;
                        end if;
                    when "010" => divisor <= unsigned(wdata);
                    when "011" => ien <= wdata(1 downto 0);
                    when "100" =>
                        if wdata(3) = '1' then overrun <= '0'; end if;
                        if wdata(4) = '1' then framing <= '0'; end if;
                        if wdata(5) = '1' then brk <= '0'; end if;
                    when others => null;
                    end case;
                elsif sel = '1' and wr = '0' and off = "001" and rxc /= 0 then
                    rxh := (rxh + 1) mod DEPTH;
                    rxc := rxc - 1;
                end if;

                -- the testbench's part
                for i in 0 to 15 loop
                    if i < tb_rx_n and rxc < DEPTH then
                        rx_fifo((rxh + rxc) mod DEPTH) <= tb_rx_bytes(i);
                        rxc := rxc + 1;
                    end if;
                end loop;
                if tb_break = '1' then
                    brk <= '1';
                end if;

                -- the transmitter: takes a byte when idle, then counts
                -- this tick as the frame's first
                if tbusy = '0' and txc /= 0 then
                    tshift := '1' & tx_fifo(txh) & '0';             -- stop, data low first, start
                    txh := (txh + 1) mod DEPTH;
                    txc := txc - 1;
                    tbusy := '1';
                    tleft := 10;
                    tsub := (others => '0');
                    txd <= '0';
                end if;
                if tick = '1' and tbusy = '1' then
                    if tsub + 1 = bit_ticks then
                        tsub := (others => '0');
                        tshift := '1' & tshift(9 downto 1);
                        tleft := tleft - 1;
                        if tleft = 0 then
                            tbusy := '0';
                            txp := txp - 1;                             -- gone
                            txd <= '1';
                        else
                            txd <= tshift(0);
                        end if;
                    else
                        tsub := tsub + 1;
                    end if;
                end if;

                -- the receiver
                if tick = '1' then
                    if rx_busy = '0' then
                        if rx_sync = "10" then                          -- a falling edge: the start bit
                            rx_busy <= '1';
                            rx_sub <= (others => '0');
                            rx_sample <= 0;
                            rx_bit <= 0;
                        end if;
                    else
                        if rx_sub + 1 = (divisor + 1) then              -- one sample
                            rx_sub <= (others => '0');
                            if rx_sample = 7 then                       -- the middle of the bit
                                if rx_bit = 0 then
                                    if rx_line = '1' then
                                        rx_busy <= '0';                 -- not a start bit after all
                                    end if;
                                elsif rx_bit <= 8 then
                                    rx_shift <= rx_line & rx_shift(7 downto 1);
                                else
                                    rx_busy <= '0';
                                    if rx_line = '1' then
                                        push_rx := true;
                                        push_data := rx_shift;
                                    elsif rx_shift = x"00" then
                                        brk <= '1';
                                    else
                                        framing <= '1';
                                    end if;
                                end if;
                            end if;
                            if rx_sample = 15 then
                                rx_sample <= 0;
                                rx_bit <= rx_bit + 1;
                            else
                                rx_sample <= rx_sample + 1;
                            end if;
                        else
                            rx_sub <= rx_sub + 1;
                        end if;
                    end if;
                end if;

                if push_rx then
                    if rxc = DEPTH then
                        overrun <= '1';
                    else
                        rx_fifo((rxh + rxc) mod DEPTH) <= push_data;
                        rxc := rxc + 1;
                    end if;
                end if;

                rx_head <= rxh; rx_count <= rxc;
                tx_head <= txh; tx_count <= txc; tx_pending <= txp;
                tx_busy <= tbusy; tx_shift <= tshift; tx_sub <= tsub; tx_bits_left <= tleft;
            end if;
        end if;
    end process;
end architecture;
