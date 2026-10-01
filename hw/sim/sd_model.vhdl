-- An SD card in SPI mode for the testbench: what simulator/msim_sd.c
-- answers, at the bit level on the SPI pins, mode 0.  The image is a
-- file of hex words, kept in memory; writes stay in memory.
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use std.textio.all;

entity sd_model is
    generic (
        IMAGE : string := "";
        WORDS : natural := 262144           -- 1 MB
    );
    port (
        sclk : in  std_logic;
        mosi : in  std_logic;
        miso : out std_logic;
        cs_n : in  std_logic
    );
end entity;

architecture sim of sd_model is
    constant BLOCK_BYTES : natural := 512;
    constant UNIT_BLOCKS : natural := 1024;         -- the CSD counts 512 KB
    type mem_t is array (0 to WORDS - 1) of std_logic_vector(31 downto 0);
    type bytes_t is array (natural range <>) of std_logic_vector(7 downto 0);

    impure function load(name : string) return mem_t is
        file f : text;
        variable l : line;
        variable m : mem_t := (others => (others => '0'));
        variable w : std_logic_vector(31 downto 0);
        variable i : natural := 0;
        variable good : boolean;
        variable status : file_open_status;
    begin
        if name'length = 0 then
            return m;
        end if;
        file_open(status, f, name, read_mode);
        if status /= open_ok then
            return m;
        end if;
        while not endfile(f) and i < WORDS loop
            readline(f, l);
            hread(l, w, good);
            if good then
                m(i) := w;
                i := i + 1;
            end if;
        end loop;
        file_close(f);
        return m;
    end function;

    constant BLOCKS : natural := (WORDS * 4 / (UNIT_BLOCKS * BLOCK_BYTES)) * UNIT_BLOCKS;

    function byte_of(m : mem_t; n : natural) return std_logic_vector is
        variable w : std_logic_vector(31 downto 0);
    begin
        w := m(n / 4);
        return w(8 * (n mod 4) + 7 downto 8 * (n mod 4));
    end function;

    signal out_byte : std_logic_vector(7 downto 0) := x"ff";
begin
    process
        variable mem : mem_t := load(IMAGE);
        variable in_shift, out_shift : std_logic_vector(7 downto 0);
        variable bit : natural;
        type state_t is (S_CMD, S_RESP, S_WRITE_WAIT, S_WRITE_DATA, S_BUSY);
        variable state : state_t := S_CMD;
        variable cmd : bytes_t(0 to 5);
        variable cmdlen : natural := 0;
        variable resp : bytes_t(0 to 520);
        variable resplen, respi : natural := 0;
        variable idle : boolean := true;
        variable acmd : boolean := false;
        variable acmd41s : natural := 0;
        variable arg : unsigned(31 downto 0);
        variable c : natural;
        variable r1 : std_logic_vector(7 downto 0);
        variable addr : natural;
        variable datai : natural;
        variable busy : natural;
        variable write_next : boolean := false;
        variable c_size : natural;
        variable w : std_logic_vector(31 downto 0);
        variable datab : bytes_t(0 to 513);

        procedure respond(bytes : bytes_t) is
        begin
            resp(0) := x"ff";                   -- a byte's delay before the answer
            for i in bytes'range loop
                resp(1 + i - bytes'low) := bytes(i);
            end loop;
            resplen := 1 + bytes'length;
            respi := 0;
            state := S_RESP;
        end procedure;

        procedure respond_r1(v : std_logic_vector(7 downto 0)) is
            variable b : bytes_t(0 to 0);
        begin
            b(0) := v;
            respond(b);
        end procedure;

        procedure command is
            variable b : bytes_t(0 to 519);
        begin
            c := to_integer(unsigned(cmd(0)(5 downto 0)));
            arg := unsigned(cmd(1)) & unsigned(cmd(2)) & unsigned(cmd(3)) & unsigned(cmd(4));
            if idle then r1 := x"01"; else r1 := x"00"; end if;
            write_next := false;
            if acmd and c = 41 then
                acmd := false;
                acmd41s := acmd41s + 1;
                if acmd41s >= 2 then idle := false; end if;
                if idle then respond_r1(x"01"); else respond_r1(x"00"); end if;
                return;
            end if;
            acmd := false;
            case c is
            when 0 =>
                idle := true; acmd41s := 0;
                respond_r1(x"01");
            when 8 =>
                b(0) := r1; b(1) := x"00"; b(2) := x"00"; b(3) := x"01"; b(4) := cmd(4);
                respond(b(0 to 4));
            when 55 =>
                acmd := true;
                respond_r1(r1);
            when 58 =>
                b(0) := r1;
                if idle then b(1) := x"40"; else b(1) := x"c0"; end if;
                b(2) := x"ff"; b(3) := x"80"; b(4) := x"00";
                respond(b(0 to 4));
            when 9 | 10 =>
                b(0) := r1; b(1) := x"fe";
                for i in 2 to 17 loop b(i) := x"00"; end loop;
                if c = 9 then
                    c_size := BLOCKS / 1024 - 1;
                    b(2) := x"40"; b(3) := x"0e"; b(5) := x"32"; b(6) := x"5b"; b(7) := x"59";
                    b(9) := std_logic_vector(to_unsigned((c_size / 65536) mod 64, 8));
                    b(10) := std_logic_vector(to_unsigned((c_size / 256) mod 256, 8));
                    b(11) := std_logic_vector(to_unsigned(c_size mod 256, 8));
                    b(12) := x"7f"; b(13) := x"80"; b(14) := x"0a"; b(15) := x"40"; b(17) := x"01";
                else
                    b(2) := x"01"; b(17) := x"01";
                end if;
                b(18) := x"00"; b(19) := x"00";
                respond(b(0 to 19));
            when 16 =>
                if arg = 512 then respond_r1(r1); else respond_r1(r1 or x"40"); end if;
            when 12 =>
                respond_r1(r1);
            when 17 =>
                if idle then
                    respond_r1(x"01");
                elsif to_integer(arg) >= BLOCKS then
                    respond_r1(x"40");
                else
                    b(0) := x"00"; b(1) := x"fe";
                    for i in 0 to BLOCK_BYTES - 1 loop
                        b(2 + i) := byte_of(mem, to_integer(arg) * BLOCK_BYTES + i);
                    end loop;
                    b(2 + BLOCK_BYTES) := x"00"; b(3 + BLOCK_BYTES) := x"00";
                    respond(b(0 to 3 + BLOCK_BYTES));
                end if;
            when 24 =>
                if idle then
                    respond_r1(x"01");
                elsif to_integer(arg) >= BLOCKS then
                    respond_r1(x"40");
                else
                    addr := to_integer(arg);
                    respond_r1(x"00");
                    write_next := true;
                end if;
            when others =>
                respond_r1(r1 or x"04");
            end case;
        end procedure;

        variable out_v : std_logic_vector(7 downto 0);
    begin
        miso <= '1';
        bit := 0;
        in_shift := (others => '0');
        out_shift := x"ff";
        loop
            wait on sclk, cs_n;
            if cs_n = '1' then
                -- deselected: a transaction half done is forgotten
                if state /= S_BUSY then
                    state := S_CMD;
                    cmdlen := 0;
                    resplen := 0;
                end if;
                bit := 0;
                out_shift := x"ff";
                miso <= '1';
            elsif rising_edge(sclk) then
                in_shift := in_shift(6 downto 0) & mosi;
                bit := bit + 1;
                if bit = 8 then
                    bit := 0;
                    -- a byte each way: what came in, and what goes out next
                    out_v := x"ff";
                    case state is
                    when S_CMD =>
                        if cmdlen /= 0 or in_shift(7 downto 6) = "01" then
                            cmd(cmdlen) := in_shift;
                            cmdlen := cmdlen + 1;
                            if cmdlen = 6 then
                                cmdlen := 0;
                                command;
                            end if;
                        end if;
                    when S_RESP =>
                        null;
                    when S_WRITE_WAIT =>
                        if in_shift = x"fe" then
                            state := S_WRITE_DATA;
                            datai := 0;
                        end if;
                    when S_WRITE_DATA =>
                        datab(datai) := in_shift;
                        datai := datai + 1;
                        if datai = BLOCK_BYTES + 2 then
                            for i in 0 to BLOCK_BYTES / 4 - 1 loop
                                w := datab(4 * i + 3) & datab(4 * i + 2) & datab(4 * i + 1) & datab(4 * i);
                                mem(addr * (BLOCK_BYTES / 4) + i) := w;
                            end loop;
                            state := S_BUSY;
                            busy := 3;                      -- the data response, then two busy bytes
                        end if;
                    when S_BUSY =>
                        null;
                    end case;
                    -- the byte to send while the next comes in
                    case state is
                    when S_RESP =>
                        out_v := resp(respi);
                        respi := respi + 1;
                        if respi = resplen then
                            resplen := 0;
                            if write_next then
                                state := S_WRITE_WAIT;
                                write_next := false;
                            else
                                state := S_CMD;
                            end if;
                        end if;
                    when S_BUSY =>
                        if busy = 3 then
                            out_v := x"05";                 -- accepted
                        elsif busy > 0 then
                            out_v := x"00";
                        end if;
                        if busy = 0 then
                            state := S_CMD;
                            cmdlen := 0;
                        else
                            busy := busy - 1;
                        end if;
                    when others =>
                        null;
                    end case;
                    out_shift := out_v;
                end if;
            elsif falling_edge(sclk) then
                -- the master samples on the rising edge: put the next bit out now
                miso <= out_shift(7);
                out_shift := out_shift(6 downto 0) & '1';
            end if;
        end loop;
    end process;
end architecture;
