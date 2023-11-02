# Makefile for building Venom
# See ../doc/readme.html for installation and customization instructions.

# == CHANGE THE SETTINGS BELOW TO SUIT YOUR ENVIRONMENT =======================

# Your platform. See PLATS for possible values.
PLAT= guess

CC= gcc -std=gnu99
CFLAGS= -O2 -Wall -Wextra -DVENOM_COMPAT_5_3 $(SYSCFLAGS) $(MYCFLAGS)
LDFLAGS= $(SYSLDFLAGS) $(MYLDFLAGS)
LIBS= -lm $(SYSLIBS) $(MYLIBS)

AR= ar rcu
RANLIB= ranlib
RM= rm -f
UNAME= uname

SYSCFLAGS=
SYSLDFLAGS=
SYSLIBS=

MYCFLAGS=
MYLDFLAGS=
MYLIBS=
MYOBJS=

# Special flags for compiler modules; -Os reduces code size.
CMCFLAGS= 

# == END OF USER SETTINGS -- NO NEED TO CHANGE ANYTHING BELOW THIS LINE =======

PLATS= guess aix bsd c89 freebsd generic linux linux-readline macosx mingw posix solaris

VENOM_A=	libvenom.a
CORE_O=	api.o code.o ctype.o debug.o do.o dump.o function.o garbageCollection.o lexer.o memory.o object.o opcodes.o parser.o state.o string.o table.o tagMethods.o undump.o virtualMachine.o zio.o
LIB_O=	auxlib.o baselib.o corolib.o dblib.o iolib.o mathlib.o loadlib.o oslib.o strlib.o tablib.o utf8lib.o Initialize.o
BASE_O= $(CORE_O) $(LIB_O) $(MYOBJS)

VENOM_T=	venom
VENOM_O=	venom.o

VENOMC_T=	venomc
VENOMC_O=	venomc.o

ALL_O= $(BASE_O) $(VENOM_O) $(VENOMC_O)
ALL_T= $(VENOM_A) $(VENOM_T) $(VENOMC_T)
ALL_A= $(VENOM_A)

# Targets start here.
default: $(PLAT)

all:	$(ALL_T)

o:	$(ALL_O)

a:	$(ALL_A)

$(VENOM_A): $(BASE_O)
	$(AR) $@ $(BASE_O)
	$(RANLIB) $@

$(VENOM_T): $(VENOM_O) $(VENOM_A)
	$(CC) -o $@ $(LDFLAGS) $(VENOM_O) $(VENOM_A) $(LIBS)

$(VENOMC_T): $(VENOMC_O) $(VENOM_A)
	$(CC) -o $@ $(LDFLAGS) $(VENOMC_O) $(VENOM_A) $(LIBS)

test:
	./$(VENOM_T) -v

clean:
	$(RM) $(ALL_T) $(ALL_O)

depend:
	@$(CC) $(CFLAGS) -MM l*.c

echo:
	@echo "PLAT= $(PLAT)"
	@echo "CC= $(CC)"
	@echo "CFLAGS= $(CFLAGS)"
	@echo "LDFLAGS= $(LDFLAGS)"
	@echo "LIBS= $(LIBS)"
	@echo "AR= $(AR)"
	@echo "RANLIB= $(RANLIB)"
	@echo "RM= $(RM)"
	@echo "UNAME= $(UNAME)"

# Convenience targets for popular platforms.
ALL= all

help:
	@echo "Do 'make PLATFORM' where PLATFORM is one of these:"
	@echo "   $(PLATS)"
	@echo "See doc/readme.html for complete instructions."

guess:
	@echo Guessing `$(UNAME)`
	@$(MAKE) `$(UNAME)`

AIX aix:
	$(MAKE) $(ALL) CC="xlc" CFLAGS="-O2 -DVENOM_USE_POSIX -DVENOM_USE_DLOPEN" SYSLIBS="-ldl" SYSLDFLAGS="-brtl -bexpall"

bsd:
	$(MAKE) $(ALL) SYSCFLAGS="-DVENOM_USE_POSIX -DVENOM_USE_DLOPEN" SYSLIBS="-Wl,-E"

c89:
	$(MAKE) $(ALL) SYSCFLAGS="-DVENOM_USE_C89" CC="gcc -std=c89"
	@echo ''
	@echo '*** C89 does not guarantee 64-bit integers for Venom.'
	@echo '*** Make sure to compile all external Venom libraries'
	@echo '*** with VENOM_USE_C89 to ensure consistency'
	@echo ''

FreeBSD NetBSD OpenBSD freebsd:
	$(MAKE) $(ALL) SYSCFLAGS="-DVENOM_USE_LINUX -DVENOM_USE_READLINE -I/usr/include/edit" SYSLIBS="-Wl,-E -ledit" CC="cc"

generic: $(ALL)

Linux linux:	linux-noreadline

linux-noreadline:
	$(MAKE) $(ALL) SYSCFLAGS="-DVENOM_USE_LINUX" SYSLIBS="-Wl,-E -ldl"

linux-readline:
	$(MAKE) $(ALL) SYSCFLAGS="-DVENOM_USE_LINUX -DVENOM_USE_READLINE" SYSLIBS="-Wl,-E -ldl -lreadline"

Darwin macos macosx:
	$(MAKE) $(ALL) SYSCFLAGS="-DVENOM_USE_MACOSX -DVENOM_USE_READLINE" SYSLIBS="-lreadline"

mingw:
	$(MAKE) "VENOM_A=venom54.dll" "VENOM_T=venom.exe" \
	"AR=$(CC) -shared -o" "RANLIB=strip --strip-unneeded" \
	"SYSCFLAGS=-DVENOM_BUILD_AS_DLL" "SYSLIBS=" "SYSLDFLAGS=-s" venom.exe
	$(MAKE) "VENOMC_T=venomc.exe" venomc.exe

posix:
	$(MAKE) $(ALL) SYSCFLAGS="-DVENOM_USE_POSIX"

SunOS solaris:
	$(MAKE) $(ALL) SYSCFLAGS="-DVENOM_USE_POSIX -DVENOM_USE_DLOPEN -D_REENTRANT" SYSLIBS="-ldl"

# Targets that do not create files (not all makes understand .PHONY).
.PHONY: all $(PLATS) help test clean default o a depend echo

# Compiler modules may use special flags.
lexer.o:
	$(CC) $(CFLAGS) $(CMCFLAGS) -c lexer.c

parser.o:
	$(CC) $(CFLAGS) $(CMCFLAGS) -c parser.c

code.o:
	$(CC) $(CFLAGS) $(CMCFLAGS) -c code.c

# DO NOT DELETE

api.o: api.c prefix.h venom.h venomconf.h api.h limits.h state.h \
 object.h tagMethods.h zio.h memory.h debug.h do.h function.h garbageCollection.h string.h \
 table.h undump.h virtualMachine.h
auxlib.o: auxlib.c prefix.h venom.h venomconf.h auxlib.h
baselib.o: baselib.c prefix.h venom.h venomconf.h auxlib.h venomlib.h
code.o: code.c prefix.h venom.h venomconf.h code.h lexer.h object.h \
 limits.h zio.h memory.h opcodes.h parser.h debug.h state.h tagMethods.h \
 do.h garbageCollection.h string.h table.h virtualMachine.h
corolib.o: corolib.c prefix.h venom.h venomconf.h auxlib.h venomlib.h
ctype.o: ctype.c prefix.h ctype.h venom.h venomconf.h limits.h
dblib.o: dblib.c prefix.h venom.h venomconf.h auxlib.h venomlib.h
debug.o: debug.c prefix.h venom.h venomconf.h api.h limits.h state.h \
 object.h tagMethods.h zio.h memory.h code.h lexer.h opcodes.h parser.h \
 debug.h do.h function.h string.h garbageCollection.h table.h virtualMachine.h
do.o: do.c prefix.h venom.h venomconf.h api.h limits.h state.h \
 object.h tagMethods.h zio.h memory.h debug.h do.h function.h garbageCollection.h opcodes.h \
 parser.h string.h table.h undump.h virtualMachine.h
dump.o: dump.c prefix.h venom.h venomconf.h object.h limits.h state.h \
 tagMethods.h zio.h memory.h undump.h
function.o: function.c prefix.h venom.h venomconf.h debug.h state.h object.h \
 limits.h tagMethods.h zio.h memory.h do.h function.h garbageCollection.h
garbageCollection.o: garbageCollection.c prefix.h venom.h venomconf.h debug.h state.h object.h \
 limits.h tagMethods.h zio.h memory.h do.h function.h garbageCollection.h string.h table.h
Initialize.o: Initialize.c prefix.h venom.h venomconf.h venomlib.h auxlib.h
iolib.o: iolib.c prefix.h venom.h venomconf.h auxlib.h venomlib.h
lexer.o: lexer.c prefix.h venom.h venomconf.h ctype.h limits.h debug.h \
 state.h object.h tagMethods.h zio.h memory.h do.h garbageCollection.h lexer.h parser.h \
 string.h table.h
mathlib.o: mathlib.c prefix.h venom.h venomconf.h auxlib.h venomlib.h
memory.o: memory.c prefix.h venom.h venomconf.h debug.h state.h object.h \
 limits.h tagMethods.h zio.h memory.h do.h garbageCollection.h
loadlib.o: loadlib.c prefix.h venom.h venomconf.h auxlib.h venomlib.h
object.o: object.c prefix.h venom.h venomconf.h ctype.h limits.h \
 debug.h state.h object.h tagMethods.h zio.h memory.h do.h string.h garbageCollection.h \
 virtualMachine.h
opcodes.o: opcodes.c prefix.h opcodes.h limits.h venom.h venomconf.h
oslib.o: oslib.c prefix.h venom.h venomconf.h auxlib.h venomlib.h
parser.o: parser.c prefix.h venom.h venomconf.h code.h lexer.h object.h \
 limits.h zio.h memory.h opcodes.h parser.h debug.h state.h tagMethods.h \
 do.h function.h string.h garbageCollection.h table.h
state.o: state.c prefix.h venom.h venomconf.h api.h limits.h state.h \
 object.h tagMethods.h zio.h memory.h debug.h do.h function.h garbageCollection.h lexer.h \
 string.h table.h
string.o: string.c prefix.h venom.h venomconf.h debug.h state.h \
 object.h limits.h tagMethods.h zio.h memory.h do.h string.h garbageCollection.h
strlib.o: strlib.c prefix.h venom.h venomconf.h auxlib.h venomlib.h
table.o: table.c prefix.h venom.h venomconf.h debug.h state.h object.h \
 limits.h tagMethods.h zio.h memory.h do.h garbageCollection.h string.h table.h virtualMachine.h
tablib.o: tablib.c prefix.h venom.h venomconf.h auxlib.h venomlib.h
tagMethods.o: tagMethods.c prefix.h venom.h venomconf.h debug.h state.h object.h \
 limits.h tagMethods.h zio.h memory.h do.h garbageCollection.h string.h table.h virtualMachine.h
venom.o: venom.c prefix.h venom.h venomconf.h auxlib.h venomlib.h
venomc.o: venomc.c prefix.h venom.h venomconf.h auxlib.h debug.h state.h \
 object.h limits.h tagMethods.h zio.h memory.h opcodes.h opnames.h undump.h
undump.o: undump.c prefix.h venom.h venomconf.h debug.h state.h \
 object.h limits.h tagMethods.h zio.h memory.h do.h function.h string.h garbageCollection.h \
 undump.h
utf8lib.o: utf8lib.c prefix.h venom.h venomconf.h auxlib.h venomlib.h
virtualMachine.o: virtualMachine.c prefix.h venom.h venomconf.h debug.h state.h object.h \
 limits.h tagMethods.h zio.h memory.h do.h function.h garbageCollection.h opcodes.h string.h \
 table.h virtualMachine.h jumptab.h
zio.o: zio.c prefix.h venom.h venomconf.h limits.h memory.h state.h \
 object.h tagMethods.h zio.h

# (end of Makefile)
