SUBDIRS = isa lib simulator

all check clean:
	@for d in $(SUBDIRS); do $(MAKE) -C $$d $@ || exit 1; done

docs: isa/isagen
	isa/isagen -d docs/reference.md isa/meow.isa

isa/isagen:
	$(MAKE) -C isa

.PHONY: all check clean docs
