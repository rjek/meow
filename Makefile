SUBDIRS = isa lib as ld rt simulator libc

all check clean:
	@for d in $(SUBDIRS); do $(MAKE) -C $$d $@ || exit 1; done

docs: isa/isagen
	isa/isagen -d docs/reference.md isa/meow.isa

bench: all
	bench/run.sh

isa/isagen:
	$(MAKE) -C isa

.PHONY: all check clean docs bench
