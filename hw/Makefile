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

RTL = rtl/meow_pkg.vhdl rtl/meow_core.vhdl rtl/rom.vhdl rtl/ram.vhdl rtl/local_mem.vhdl rtl/chairman.vhdl \
      rtl/uart.vhdl rtl/spi.vhdl rtl/ioc.vhdl rtl/meow_soc.vhdl
TB = sim/sd_model.vhdl sim/tb_soc.vhdl

# the simulator's tests a one-CPU system can run: all but the two-CPU one
TRACE_TESTS = alu banks branch flags input irq mem uart spi
# the two-CPU test: no trace to match, since the CPUs' pace against each
# other is the machine's own, but the output must be msim's
OUTPUT_TESTS = cpus

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

# msim's trace of the same ROM, with the test's input and SD card if it
# has them (an SD card image is 1 MB of zeros with the test's text first)
obj/%.trace: obj/%.bin
	if [ -f ../tests/sim/$*.in ]; then in=../tests/sim/$*.in; else in=/dev/null; fi; \
	if [ -f ../tests/sim/$*.sd ]; then \
	    dd if=/dev/zero of=obj/$*.sdimg bs=1024 count=1024 2>/dev/null; \
	    dd if=../tests/sim/$*.sd of=obj/$*.sdimg conv=notrunc 2>/dev/null; sd="-D obj/$*.sdimg"; \
	else sd=; fi; \
	$(MSIM) -q -r $< -c 200000 $$sd -T $@ < $$in > /dev/null 2>&1; true

# the card's image as hex words, for the testbench's model
obj/%.sdhex: obj/%.trace
	if [ -f obj/$*.sdimg ]; then python3 -c "import sys; d = open(sys.argv[1], 'rb').read(); \
	    print('\n'.join('%08x' % int.from_bytes(d[i:i + 4], 'little') for i in range(0, len(d), 4)))" obj/$*.sdimg > $@; \
	else : > $@; fi

# each test: the trace must match to the instruction, and what the
# program wrote must match msim's expected output
check: obj/tb_soc $(addprefix obj/,$(addsuffix .hex,$(TRACE_TESTS)) $(addsuffix .trace,$(TRACE_TESTS)) $(addsuffix .sdhex,$(TRACE_TESTS))) \
       $(addprefix obj/,$(addsuffix .hex,$(OUTPUT_TESTS)))
	@pass=0; fail=0; \
	for t in $(OUTPUT_TESTS); do \
	    if $(GHDL) -r $(GHDLFLAGS) tb_soc -gNCPU=2 -gROM=obj/$$t.hex -gTRACE=none -gOUT_FILE=obj/$$t.hwout > obj/$$t.log 2>&1 \
	       && cmp -s obj/$$t.hwout ../tests/sim/$$t.out; then \
	        pass=$$((pass + 1)); echo "ok   $$t: $$(grep -o 'ran to the halt in [0-9]* cycles' obj/$$t.log), output as msim's"; \
	    else \
	        fail=$$((fail + 1)); echo "FAIL $$t"; grep -v '^$$' obj/$$t.log | tail -5; \
	        cmp obj/$$t.hwout ../tests/sim/$$t.out 2>&1 | head -2; \
	    fi; \
	done; \
	for t in $(TRACE_TESTS); do \
	    in=; if [ -f ../tests/sim/$$t.in ]; then in="-gIN_FILE=../tests/sim/$$t.in"; fi; \
	    sd=; if [ -f ../tests/sim/$$t.sd ]; then sd="-gHAVE_SD=true -gSD_IMAGE=obj/$$t.sdhex"; fi; \
	    if $(GHDL) -r $(GHDLFLAGS) tb_soc -gROM=obj/$$t.hex -gTRACE=obj/$$t.trace -gOUT_FILE=obj/$$t.hwout $$in $$sd > obj/$$t.log 2>&1 \
	       && cmp -s obj/$$t.hwout ../tests/sim/$$t.out; then \
	        pass=$$((pass + 1)); echo "ok   $$t: $$(grep -o '[0-9]* instructions matched in [0-9]* cycles' obj/$$t.log)"; \
	    else \
	        fail=$$((fail + 1)); echo "FAIL $$t"; grep -v '^$$' obj/$$t.log | tail -5; \
	        cmp obj/$$t.hwout ../tests/sim/$$t.out 2>&1 | head -2; \
	    fi; \
	done; \
	echo "hardware tests: $$pass passed, $$fail failed"; [ $$fail = 0 ]

# Catflap itself: the ROM booted under msim with sim/catflap.in as the
# console's input, traced, and the same under GHDL, 256 KB of RAM and
# 4 KB of local memory, as the operating system's tests have.  The
# trace is large (a few hundred bytes an instruction) and the run takes
# minutes; it is not part of check.
CATFLAP_ROM = ../os/catflap.rom

obj/catflap.hex: $(CATFLAP_ROM)
	@mkdir -p obj
	python3 -c "import sys; d = open(sys.argv[1], 'rb').read(); d += b'\0' * (-len(d) % 4); \
	    print('\n'.join('%08x' % int.from_bytes(d[i:i + 4], 'little') for i in range(0, len(d), 4)))" $< > $@

obj/catflap.trace obj/catflap.out: $(CATFLAP_ROM) sim/catflap.in
	$(MSIM) -q -r $(CATFLAP_ROM) -m 256 -l 4 -c 20000000 -T obj/catflap.trace < sim/catflap.in > obj/catflap.out 2>/dev/null; \
	echo "exit $$?" >> obj/catflap.out

check-catflap: obj/tb_soc obj/catflap.hex obj/catflap.trace
	$(GHDL) -r $(GHDLFLAGS) tb_soc -gROM=obj/catflap.hex -gTRACE=obj/catflap.trace -gIN_FILE=sim/catflap.in \
	    -gOUT_FILE=obj/catflap.hwout -gRAM_WORDS=65536 > obj/catflap.log 2>&1; \
	grep -v '^$$' obj/catflap.log | tail -3; cmp obj/catflap.hwout obj/catflap.out && echo "catflap: output matches"

# and with two CPUs, on the output alone, as the two-CPU test is
obj/catflap2.out: $(CATFLAP_ROM) sim/catflap2.in
	$(MSIM) -q -r $(CATFLAP_ROM) -m 256 -l 4 -n 2 -c 20000000 < sim/catflap2.in > $@ 2>/dev/null; echo "exit $$?" >> $@

check-catflap2: obj/tb_soc obj/catflap.hex obj/catflap2.out
	$(GHDL) -r $(GHDLFLAGS) tb_soc -gNCPU=2 -gROM=obj/catflap.hex -gTRACE=none -gIN_FILE=sim/catflap2.in \
	    -gOUT_FILE=obj/catflap2.hwout -gRAM_WORDS=65536 > obj/catflap2.log 2>&1; \
	grep -v '^$$' obj/catflap2.log | tail -2; cmp obj/catflap2.hwout obj/catflap2.out && echo "catflap on two CPUs: output matches"

# size and speed on the ECP5, through the GHDL plugin
synth: $(RTL)
	@mkdir -p obj
	$(YOSYS) -q -m ghdl -p "ghdl $(GHDLFLAGS) $(RTL) -e meow_soc; synth_ecp5 -top meow_soc; tee -o obj/synth.stat stat" > obj/synth.log 2>&1
	grep -A 40 'Number of cells' obj/synth.stat | head -40

clean:
	rm -rf obj

.PHONY: all check check-catflap check-catflap2 synth clean
