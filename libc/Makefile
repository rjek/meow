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

STDLIB_OBJS=	stdlib/rand.o \
		stdlib/rand_r.o \
		stdlib/srand.o \
		stdlib/abs.o \
		stdlib/labs.o

all: string.a ctype.a stdlib.a
	

clean:
	rm -rf $(STRING_OBJS) $(STDLIB_OBJS) string.a ctype.a stdlib.a

string.a: $(STRING_OBJS)
	$(AR) r string.a $(STRING_OBJS)

ctype.a: ctype.o
	$(AR) r ctype.a ctype.o

stdlib.a: $(STDLIB_OBJS)
	$(AR) q stdlib.a $(STDLIB_OBJS)
