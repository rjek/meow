/*	File codemeow.c: 2.2 (84/08/31,10:05:13) */
/*% cc -O -c %
 *
 */

#include <stdio.h>
#include "defs.h"
#include "data.h"

/*
 *	Some predefinitions:
 *
 *	INTSIZE is the size of an integer in the target machine
 *	BYTEOFF is the offset of an byte within an integer on the
 *		target machine. (ie: 8080,pdp11 = 0, 6809 = 1,
 *		360 = 3)
 *	This compiler assumes that an integer is the SAME length as
 *	a pointer - in fact, the compiler uses INTSIZE for both.
 */
#define	INTSIZE	4
#define	BYTEOFF	0

/*
 *	print all assembler info before any code is generated
 *
 */
header ()
{
	outstr("\t;;Small C MEOW\n\t;;Code generator 0.0 (2007/03/08) ");
	FEvers();
	nl ();
	ol ("define\tprimary\tr0");
	ol ("define\tsecondary\tr1");
	ol ("define\taddr\tr3");
        ol ("define\targcount\tr5");
        ol ("macro\timmed\t$0");
        ol ("\tldi\t$0");
        ol ("\tmov\tprimary\tir");
        ol ("endmacro");
}
nl ()
{
	outbyte (EOL);
}

initmac() {
	defmac("smallc\t1");
	defmac("meowver\t0");
}

galign(t)
int t;
{
        /*
        ot (";; TODO: galign(");
        onum(t);
        outbyte(')');
        nl();
        */
	return (t);
}


/*
 *	return size of an integer
 */
intsize() {
	return(INTSIZE);
}

/*
 *	return offset of ls byte within word
 *	(ie: 8080 & pdp11 is 0, 6809 is 1, 360 is 3)
 */
byteoff() {
	return(BYTEOFF);
}

/*
 *	Output internal generated label prefix
 */
olprfix() {
	outstr("_SC_M_");
}

/*
 *	Output a label definition terminator
 */
col ()
{
	outstr ("\t");
}

/*
 *	begin a comment line for the assembler
 *
 */
comment ()
{
	outstr ("\t;;");
}

/*
 *	Output a prefix in front of user labels
 */
prefix () {
	outbyte ('_');
}


/*
 *	print any assembler stuff needed after all code
 *
 */
trailer ()
{
	ol (";;EOF");
}

/*
 *	function prologue
 */
prologue ()
{
        ol ("push\tlr");
        /* ol ("push\targcount"); */
        stkp = stkp - (INTSIZE * 2);
}

/*
 *	text (code) segment
 */
gtext ()
{
        nl ();
	ol (";; Text Segment Follows");
        nl ();
}

/*
 *	data segment
 */
gdata ()
{
        nl ();
	ol (";; Data Segment Follows");
        nl ();
}

/*
 *  Output the variable symbol at scptr as an extrn or a public
 */
ppubext(scptr) char *scptr; {
	if (scptr[STORAGE] == STATIC) return;
	ot ("global\t");
	prefix ();
	outstr (scptr);
	nl();
}

/*
 * Output the function symbol at scptr as an extrn or a public
 */
fpubext(scptr) char *scptr; {
	ppubext(scptr);
}

/*
 *  Output a decimal number to the assembler file
 */
onum(num) int num; {
	outdec(num);
}


/*
 *	fetch a static memory cell into the primary register
 */
getmem (sym)
char	*sym;
{
	ot ("adr\taddr\t");
	prefix ();
	outstr (sym + NAME);
	nl ();
	if ((sym[IDENT] != POINTER) & (sym[TYPE] == CCHAR)) {
		ot ("ldrb\tprimary\taddr");
		nl ();
	} else {
		ot ("ldr\tprimary\taddr");
		nl ();
	}
}

/*
 *	fetch the address of the specified symbol into the primary register
 *
 */
getloc (sym)
char	*sym;
{
	if (sym[STORAGE] == LSTATIC) {
                ot ("adr\tprimary\t");
		printlabel(glint(sym));
                nl();
	} else {
                /* glint(sym) is the addr on the stack of the symbol */
                /* stkp is the cur stack pos */
                /* Thus sp is at stkp, so glint(sym)-stkp should be what we need */
                ol ("mov\tprimary\tsp");
                ot ("add\tprimary\t#");
                onum (glint(sym) - stkp);
                nl();
	}
}

/*
 *	store the primary register into the specified static memory cell
 *
 */
putmem (sym)
char	*sym;
{
	ot ("adr\taddr\t");
	prefix ();
	outstr (sym + NAME);
	nl ();
	if ((sym[IDENT] != POINTER) & (sym[TYPE] == CCHAR))
		ol ("strb\tprimary\taddr");
	else
		ol ("str\tprimary\taddr");
}

/*
 *	store the specified object type in the primary register
 *	at the address on the top of the stack
 *
 */
putstk (typeobj)
char	typeobj;
{
        ol ("pop\tsecondary");
	if (typeobj == CCHAR)
		ol ("strb\tprimary\tsecondary");
	else
		ol ("str\tprimary\tsecondary");
	stkp = stkp + INTSIZE;
}

/*
 *	fetch the specified object type indirect through the primary
 *	register into the primary register
 *
 */
indirect (typeobj)
char	typeobj;
{
	if (typeobj == CCHAR)
		ol ("ldrb\tprimary\tprimary");
	else
		ol ("ldr\tprimary\tprimary");
}

/*
 *	print partial instruction to get an immediate value into
 *	the primary register
 *
 */
immed ()
{
        ot("immed\t");
}

/*
 *	push the primary register onto the stack
 *
 */
gpush ()
{
        ol("push\tprimary");
	stkp = stkp - INTSIZE;
}

/*
 *	pop the top of the stack into the secondary register
 *
 */
gpop ()
{
        ol("pop\tsecondary");
	stkp = stkp + INTSIZE;
}

/*
 *	swap the primary register and the top of the stack
 *
 */
swapstk ()
{
        ol("pop\tsecondary");
        ol("push\tprimary");
        ol("mov\tprimary\tsecondary");
}

/*
 *	call the specified subroutine name
 *
 */
gcall (sname)
char	*sname;
{
	ot ("bl\t");
	if (*sname == '^')
		outstr (++sname);
	else {
		prefix ();
		outstr (sname);
	}
	nl ();
}

/*
 *	return from subroutine
 *
 */
gret ()
{
        /* ol ("pop\targcount"); */
        ol ("pop\tlr");
        ol ("mov\tpc\tlr");
}

/*
 *	perform subroutine call to value on top of stack
 *
 */
callstk ()
{
        ol(";; TODO: Call function at top of stack");
}

/*
 *	jump to specified internal label number
 *
 */
jump (label)
int	label;
{
        ot ("b\t");
	printlabel (label);
	nl ();
}

/*
 *	test the primary register and jump if false to label
 *
 */
testjump (label, ft)
int	label,
	ft;
{
        ol ("cmp\tprimary\t#0");
	if (ft)
		ot ("bne\t");
	else
		ot ("beq\t");
	printlabel (label);
	nl ();
}

/*
 *	print pseudo-op  to define a byte
 *
 */
defbyte ()
{
	ot (".dcb\t");
}

/*
 *	print pseudo-op to define storage
 *
 */
defstorage ()
{
	ot (";; TODO: defstorage");
}

/*
 *	print pseudo-op to define a word
 *
 */
defword ()
{
	ot (".dcd\t");
}

/*
 *	modify the stack pointer to the new value indicated
 *
 */
modstk (newstkp)
int	newstkp;
{
	int	k;

	k = galign(newstkp - stkp);
	if (k == 0)
		return (newstkp);
        if (newstkp < stkp)
                ot ("sub\tsp\t#");
        else
                ot ("add\tsp\t#");
        onum(abs(newstkp-stkp));
        nl ();
	return (newstkp);
}

/*
 *	multiply the primary register by INTSIZE
 */
gaslint ()
{
        ol ("lsl\tprimary\t#2"); /* primary *= 4 */
}

/*
 *	divide the primary register by INTSIZE
 */
gasrint()
{
        ol ("lsr\tprimary\t#2"); /* primary /= 4 */
}

/*
 *	Case jump instruction
 */
gjcase() {
        ol (";; TODO: case jump instruction");
}

/*
 *	add the primary and secondary registers
 *	if lval2 is int pointer and lval is int, scale lval
 */
gadd (lval, lval2) int *lval, *lval2;
{
        /* I.E. primary += pop(), but if dbltest, pop is << 2 */
        gpop ();
        if (dbltest (lval2, lval))
                ol("lsl\tsecondary\t#2");
        ol ("add\tprimary\tsecondary");
	stkp = stkp + INTSIZE;
}

/*
 *	subtract the primary register from the secondary
 *
 */
gsub ()
{
        ol (";; TODO: gsub. The following might not be right");
        ol ("sub\tsecondary\tprimary");
}

/*
 *	multiply the primary and secondary registers
 *	(result in primary)
 *
 */
gmult ()
{
	gcall ("^smul");
}

/*
 *	divide the secondary register by the primary
 *	(quotient in primary, remainder in secondary)
 *
 */
gdiv ()
{
	gcall ("^sdiv");
}

/*
 *	compute the remainder (mod) of the secondary register
 *	divided by the primary register
 *	(remainder in primary, quotient in secondary)
 *
 */
gmod ()
{
	gcall ("^smod");
}

/*
 *	inclusive 'or' the primary and secondary registers
 *
 */
gor ()
{
        ol (";; TODO: gor, is this right?");
        ol ("orr\tprimary\tsecondary");
}

/*
 *	exclusive 'or' the primary and secondary registers
 *
 */
gxor ()
{
        ol (";; TODO: gxor, is this right?");
        ol ("eor\tprimary\tsecondary");
}

/*
 *	'and' the primary and secondary registers
 *
 */
gand ()
{
        ol (";; TODO: gand, is this right?");
        ol ("and\tprimary\tsecondary");
}

/*
 *	arithmetic shift right the secondary register the number of
 *	times in the primary register
 *	(results in primary register)
 *
 */
gasr ()
{
        ol ("pop\tsecondary");
        ol ("asr\tsecondary\tprimary");
        ol ("mov\tprimary\tsecondary");
}

/*
 *	arithmetic shift left the secondary register the number of
 *	times in the primary register
 *	(results in primary register)
 *
 */
gasl ()
{
        ol ("pop\tsecondary");
        ol ("asl\tsecondary\tprimary");
        ol ("mov\tprimary\tsecondary");
}

/*
 *	two's complement of primary register
 *
 */
gneg ()
{
        ol ("not\tprimary");
        ol ("add\tprimary\t#1");
}

/*
 *	logical complement of primary register
 *
 */
glneg ()
{
	gcall ("^lneg");
}

/*
 *	one's complement of primary register
 *
 */
gcom ()
{
        ol ("not\tprimary");
}

/*
 *	convert primary register into logical value
 *
 */
gbool ()
{
	gcall ("^bool");
}
/*
 *	increment the primary register by 1 if char, INTSIZE if
 *      int
 */
ginc (lval) int lval[];
{
        ot ("add\tprimary\t#");
	if (lval[2] == CINT)
		onum(4);
	else
		onum(1);
        nl();
}

/*
 *	decrement the primary register by one if char, INTSIZE if
 *	int
 */
gdec (lval) int lval[];
{
        ot ("sub\tprimary\t#");
	if (lval[2] == CINT)
		onum(4);
	else
		onum(1);
        nl();
}

/*
 *	following are the conditional operators.
 *	they compare the secondary register against the primary register
 *	and put a literl 1 in the primary if the condition is true,
 *	otherwise they clear the primary register
 *
 */

#define CMP_OP(skipif) \
	ol ("pop\tsecondary"); \
	ol ("cmp\tprimary\tsecondary"); \
	ol ("b" #skipif "\t#4"); \
	ol ("add\tprimary\t#1")

/*
 *	equal
 *
 */
geq ()
{
        CMP_OP(ne);
}

/*
 *	not equal
 *
 */
gne ()
{
        CMP_OP(eq);
}

/*
 *	less than (signed)
 *
 */
glt ()
{
        CMP_OP(ge);
}
/*
 *	less than or equal (signed)
 *
 */
gle ()
{
        CMP_OP(gt);
}

/*
 *	greater than (signed)
 *
 */
ggt ()
{
        CMP_OP(le);
}

/*
 *	greater than or equal (signed)
 *
 */
gge ()
{
        CMP_OP(lt);
}

/*
 *	less than (unsigned)
 *
 */
gult ()
{
        CMP_OP(hs);
}

/*
 *	less than or equal (unsigned)
 *
 */
gule ()
{
        CMP_OP(hi);
}

/*
 *	greater than (unsigned)
 *
 */
gugt ()
{
        CMP_OP(ls);
}

/*
 *	greater than or equal (unsigned)
 *
 */
guge ()
{
        CMP_OP(lo);
}

inclib() {
	return(INCDIR);
}

/*	Squirrel away argument count in a register that modstk/getloc/stloc
	doesn't touch.
*/

gnargs(d)
int	d; {
	/*
        ot ("ldi\t#");
        onum(d);
        nl ();
        ol ("mov\targcount\tir");
        */
}

assemble(s)
char	*s; {
#ifdef	ASNM
	char buf[100];
	strcpy(buf, ASNM);
	strcat(buf, " ");
	strcat(buf, s);
	buf[strlen(buf)-1] = 's';
	return(system(buf));
#else
	return(0);
#endif
}

link() {
#ifdef	LDNM
	fputs("I don't know how to link files yet\n", stderr);
#else
	return(0);
#endif
}
