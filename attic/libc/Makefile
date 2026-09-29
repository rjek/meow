CFLAGS=-I. -fno-builtin 

STRING_OBJS=	string/memchr.o \
		string/memcpy.o \
		string/memmove.o \
		string/strcat.o \
		string/strchr.o \
		string/strcmp.o \
		string/strcpy.o \
		string/strlen.o \
		string/strncmp.o \
		string/strncpy.o

CTYPE_OBJS=	ctype.o
ERRNO_OBJS=	errno.o

STDLIB_OBJS=	stdlib/rand.o \
		stdlib/rand_r.o \
		stdlib/srand.o \
		stdlib/abs.o \
		stdlib/labs.o \
		stdlib/atexit_table.o \
		stdlib/atexit.o \
		stdlib/exit.o \
		stdlib/getenv.o \
		stdlib/system.o \
		stdlib/qsort.o \
		stdlib/bsearch.o \
		stdlib/strtol.o \
		stdlib/atoi.o \
		stdlib/atol.o

all: string.a ctype.a stdlib.a errno.a
	$(AR) r libc.a $(STDLIB_OBJS) $(STRING_OBJS) $(CTYPE_OBJS) \
			$(ERRNO_OBJS)

clean:
	rm -rf $(STRING_OBJS) $(STDLIB_OBJS) $(CTYPE_OBJS) $(ERRNO_OBJS) \
		string.a ctype.a stdlib.a errno.a libc.a

string.a: $(STRING_OBJS)
	$(AR) r string.a $(STRING_OBJS)

ctype.a: ctype.o
	$(AR) r ctype.a ctype.o
	
errno.a: errno.o
	$(AR) r errno.a errno.o

stdlib.a: $(STDLIB_OBJS)
	$(AR) q stdlib.a $(STDLIB_OBJS)
