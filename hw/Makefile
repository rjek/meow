# The MEOW hardware: VHDL for the core, the Chairman and the system,
# simulated with GHDL against msim's traces and synthesised with Yosys
# through the GHDL plugin.  `make check` needs the toolchain built
# (../as/mas, ../simulator/msim); `make synth` needs yosys with the ghdl
# plugin and nextpnr-ecp5 for the figures.
GHDL = ghdl
GHDLFLAGS = --std=08 --workdir=obj
YOSYS = yosys
MAS = ../as/mas
MSIM = ../simulator/msim

RTL = rtl/meow_pkg.vhdl rtl/meow_core.vhdl rtl/rom.vhdl rtl/ram.vhdl rtl/chairman.vhdl rtl/meow_soc.vhdl
TB = sim/tb_soc.vhdl

# the simulator's tests a one-CPU system without an IOC can run
TRACE_TESTS = alu banks branch flags input irq mem

all: obj/tb_soc

.SECONDARY:                             # keep the images and traces between runs

obj/tb_soc: $(RTL) $(TB)
	@mkdir -p obj
	$(GHDL) -a $(GHDLFLAGS) $(RTL) $(TB)
	$(GHDL) -e $(GHDLFLAGS) -o $@ tb_soc

# a ROM image as the hex words the VHDL reads
obj/%.hex: obj/%.bin
	python3 -c "import sys; d = open(sys.argv[1], 'rb').read(); d += b'\0' * (-len(d) % 4); \
	    print('\n'.join('%08x' % int.from_bytes(d[i:i + 4], 'little') for i in range(0, len(d), 4)))" $< > $@

obj/%.bin: ../tests/sim/%.s ../tests/sim/msim.inc.s
	@mkdir -p obj
	cd ../tests/sim && ../../as/mas -o ../../hw/$@ $*.s

# msim's trace of the same ROM, with the test's input if it has one
obj/%.trace: obj/%.bin
	if [ -f ../tests/sim/$*.in ]; then in=../tests/sim/$*.in; else in=/dev/null; fi; \
	$(MSIM) -q -r $< -c 200000 -T $@ < $$in > /dev/null 2>&1; true

check: obj/tb_soc $(addprefix obj/,$(addsuffix .hex,$(TRACE_TESTS)) $(addsuffix .trace,$(TRACE_TESTS)))
	@pass=0; fail=0; \
	for t in $(TRACE_TESTS); do \
	    if $(GHDL) -r $(GHDLFLAGS) tb_soc -gROM=obj/$$t.hex -gTRACE=obj/$$t.trace > obj/$$t.log 2>&1; then \
	        pass=$$((pass + 1)); echo "ok   $$t: $$(grep -o '[0-9]* instructions matched in [0-9]* cycles' obj/$$t.log)"; \
	    else \
	        fail=$$((fail + 1)); echo "FAIL $$t"; grep -v '^$$' obj/$$t.log | tail -5; \
	    fi; \
	done; \
	echo "hardware tests: $$pass passed, $$fail failed"; [ $$fail = 0 ]

# size and speed on the ECP5, through the GHDL plugin
synth: $(RTL)
	@mkdir -p obj
	$(YOSYS) -q -m ghdl -p "ghdl $(GHDLFLAGS) $(RTL) -e meow_soc; synth_ecp5 -top meow_soc; tee -o obj/synth.stat stat" > obj/synth.log 2>&1
	grep -A 40 'Number of cells' obj/synth.stat | head -40

clean:
	rm -rf obj

.PHONY: all check synth clean
