# Lua is an optional extra: make WITH_LUA=1 builds lua/lua.bin and puts
# /bin/lua in the Catflap ROM, and the tests that need it run
WITH_LUA ?= 0
export WITH_LUA
SUBDIRS = isa lib as ld rt simulator libc $(if $(filter 1,$(WITH_LUA)),lua) os

all check clean:
	@for d in $(SUBDIRS); do $(MAKE) -C $$d $@ || exit 1; done

docs: isa/isagen
	isa/isagen -d docs/reference.md isa/meow.isa

bench: all
	bench/run.sh

isa/isagen:
	$(MAKE) -C isa

.PHONY: all check clean docs bench
