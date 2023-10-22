# Makefile for building Viper
# See ../doc/readme.html for installation and customization instructions.

# == CHANGE THE SETTINGS BELOW TO SUIT YOUR ENVIRONMENT =======================

# Your platform. See PLATS for possible values.
PLAT= guess

CC= gcc -std=gnu99
CFLAGS= -O2 -Wall -Wextra -DVIPER_COMPAT_5_3 $(SYSCFLAGS) $(MYCFLAGS)
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

VIPER_A=	libviper.a
CORE_O=	lapi.o lcode.o lctype.o ldebug.o ldo.o ldump.o lfunc.o lgc.o llex.o lmem.o lobject.o lopcodes.o lparser.o lstate.o lstring.o ltable.o ltm.o lundump.o lvm.o lzio.o
LIB_O=	lauxlib.o lbaselib.o lcorolib.o ldblib.o liolib.o lmathlib.o loadlib.o loslib.o lstrlib.o ltablib.o lutf8lib.o linit.o
BASE_O= $(CORE_O) $(LIB_O) $(MYOBJS)

VIPER_T=	viper
VIPER_O=	viper.o

VIPERC_T=	viperc
VIPERC_O=	viperc.o

ALL_O= $(BASE_O) $(VIPER_O) $(VIPERC_O)
ALL_T= $(VIPER_A) $(VIPER_T) $(VIPERC_T)
ALL_A= $(VIPER_A)

# Targets start here.
default: $(PLAT)

all:	$(ALL_T)

o:	$(ALL_O)

a:	$(ALL_A)

$(VIPER_A): $(BASE_O)
	$(AR) $@ $(BASE_O)
	$(RANLIB) $@

$(VIPER_T): $(VIPER_O) $(VIPER_A)
	$(CC) -o $@ $(LDFLAGS) $(VIPER_O) $(VIPER_A) $(LIBS)

$(VIPERC_T): $(VIPERC_O) $(VIPER_A)
	$(CC) -o $@ $(LDFLAGS) $(VIPERC_O) $(VIPER_A) $(LIBS)

test:
	./$(VIPER_T) -v

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
	$(MAKE) $(ALL) CC="xlc" CFLAGS="-O2 -DVIPER_USE_POSIX -DVIPER_USE_DLOPEN" SYSLIBS="-ldl" SYSLDFLAGS="-brtl -bexpall"

bsd:
	$(MAKE) $(ALL) SYSCFLAGS="-DVIPER_USE_POSIX -DVIPER_USE_DLOPEN" SYSLIBS="-Wl,-E"

c89:
	$(MAKE) $(ALL) SYSCFLAGS="-DVIPER_USE_C89" CC="gcc -std=c89"
	@echo ''
	@echo '*** C89 does not guarantee 64-bit integers for Viper.'
	@echo '*** Make sure to compile all external Viper libraries'
	@echo '*** with VIPER_USE_C89 to ensure consistency'
	@echo ''

FreeBSD NetBSD OpenBSD freebsd:
	$(MAKE) $(ALL) SYSCFLAGS="-DVIPER_USE_LINUX -DVIPER_USE_READLINE -I/usr/include/edit" SYSLIBS="-Wl,-E -ledit" CC="cc"

generic: $(ALL)

Linux linux:	linux-noreadline

linux-noreadline:
	$(MAKE) $(ALL) SYSCFLAGS="-DVIPER_USE_LINUX" SYSLIBS="-Wl,-E -ldl"

linux-readline:
	$(MAKE) $(ALL) SYSCFLAGS="-DVIPER_USE_LINUX -DVIPER_USE_READLINE" SYSLIBS="-Wl,-E -ldl -lreadline"

Darwin macos macosx:
	$(MAKE) $(ALL) SYSCFLAGS="-DVIPER_USE_MACOSX -DVIPER_USE_READLINE" SYSLIBS="-lreadline"

mingw:
	$(MAKE) "VIPER_A=viper54.dll" "VIPER_T=viper.exe" \
	"AR=$(CC) -shared -o" "RANLIB=strip --strip-unneeded" \
	"SYSCFLAGS=-DVIPER_BUILD_AS_DLL" "SYSLIBS=" "SYSLDFLAGS=-s" viper.exe
	$(MAKE) "VIPERC_T=viperc.exe" viperc.exe

posix:
	$(MAKE) $(ALL) SYSCFLAGS="-DVIPER_USE_POSIX"

SunOS solaris:
	$(MAKE) $(ALL) SYSCFLAGS="-DVIPER_USE_POSIX -DVIPER_USE_DLOPEN -D_REENTRANT" SYSLIBS="-ldl"

# Targets that do not create files (not all makes understand .PHONY).
.PHONY: all $(PLATS) help test clean default o a depend echo

# Compiler modules may use special flags.
llex.o:
	$(CC) $(CFLAGS) $(CMCFLAGS) -c llex.c

lparser.o:
	$(CC) $(CFLAGS) $(CMCFLAGS) -c lparser.c

lcode.o:
	$(CC) $(CFLAGS) $(CMCFLAGS) -c lcode.c

# DO NOT DELETE

lapi.o: lapi.c lprefix.h viper.h viperconf.h lapi.h llimits.h lstate.h \
 lobject.h ltm.h lzio.h lmem.h ldebug.h ldo.h lfunc.h lgc.h lstring.h \
 ltable.h lundump.h lvm.h
lauxlib.o: lauxlib.c lprefix.h viper.h viperconf.h lauxlib.h
lbaselib.o: lbaselib.c lprefix.h viper.h viperconf.h lauxlib.h viperlib.h
lcode.o: lcode.c lprefix.h viper.h viperconf.h lcode.h llex.h lobject.h \
 llimits.h lzio.h lmem.h lopcodes.h lparser.h ldebug.h lstate.h ltm.h \
 ldo.h lgc.h lstring.h ltable.h lvm.h
lcorolib.o: lcorolib.c lprefix.h viper.h viperconf.h lauxlib.h viperlib.h
lctype.o: lctype.c lprefix.h lctype.h viper.h viperconf.h llimits.h
ldblib.o: ldblib.c lprefix.h viper.h viperconf.h lauxlib.h viperlib.h
ldebug.o: ldebug.c lprefix.h viper.h viperconf.h lapi.h llimits.h lstate.h \
 lobject.h ltm.h lzio.h lmem.h lcode.h llex.h lopcodes.h lparser.h \
 ldebug.h ldo.h lfunc.h lstring.h lgc.h ltable.h lvm.h
ldo.o: ldo.c lprefix.h viper.h viperconf.h lapi.h llimits.h lstate.h \
 lobject.h ltm.h lzio.h lmem.h ldebug.h ldo.h lfunc.h lgc.h lopcodes.h \
 lparser.h lstring.h ltable.h lundump.h lvm.h
ldump.o: ldump.c lprefix.h viper.h viperconf.h lobject.h llimits.h lstate.h \
 ltm.h lzio.h lmem.h lundump.h
lfunc.o: lfunc.c lprefix.h viper.h viperconf.h ldebug.h lstate.h lobject.h \
 llimits.h ltm.h lzio.h lmem.h ldo.h lfunc.h lgc.h
lgc.o: lgc.c lprefix.h viper.h viperconf.h ldebug.h lstate.h lobject.h \
 llimits.h ltm.h lzio.h lmem.h ldo.h lfunc.h lgc.h lstring.h ltable.h
linit.o: linit.c lprefix.h viper.h viperconf.h viperlib.h lauxlib.h
liolib.o: liolib.c lprefix.h viper.h viperconf.h lauxlib.h viperlib.h
llex.o: llex.c lprefix.h viper.h viperconf.h lctype.h llimits.h ldebug.h \
 lstate.h lobject.h ltm.h lzio.h lmem.h ldo.h lgc.h llex.h lparser.h \
 lstring.h ltable.h
lmathlib.o: lmathlib.c lprefix.h viper.h viperconf.h lauxlib.h viperlib.h
lmem.o: lmem.c lprefix.h viper.h viperconf.h ldebug.h lstate.h lobject.h \
 llimits.h ltm.h lzio.h lmem.h ldo.h lgc.h
loadlib.o: loadlib.c lprefix.h viper.h viperconf.h lauxlib.h viperlib.h
lobject.o: lobject.c lprefix.h viper.h viperconf.h lctype.h llimits.h \
 ldebug.h lstate.h lobject.h ltm.h lzio.h lmem.h ldo.h lstring.h lgc.h \
 lvm.h
lopcodes.o: lopcodes.c lprefix.h lopcodes.h llimits.h viper.h viperconf.h
loslib.o: loslib.c lprefix.h viper.h viperconf.h lauxlib.h viperlib.h
lparser.o: lparser.c lprefix.h viper.h viperconf.h lcode.h llex.h lobject.h \
 llimits.h lzio.h lmem.h lopcodes.h lparser.h ldebug.h lstate.h ltm.h \
 ldo.h lfunc.h lstring.h lgc.h ltable.h
lstate.o: lstate.c lprefix.h viper.h viperconf.h lapi.h llimits.h lstate.h \
 lobject.h ltm.h lzio.h lmem.h ldebug.h ldo.h lfunc.h lgc.h llex.h \
 lstring.h ltable.h
lstring.o: lstring.c lprefix.h viper.h viperconf.h ldebug.h lstate.h \
 lobject.h llimits.h ltm.h lzio.h lmem.h ldo.h lstring.h lgc.h
lstrlib.o: lstrlib.c lprefix.h viper.h viperconf.h lauxlib.h viperlib.h
ltable.o: ltable.c lprefix.h viper.h viperconf.h ldebug.h lstate.h lobject.h \
 llimits.h ltm.h lzio.h lmem.h ldo.h lgc.h lstring.h ltable.h lvm.h
ltablib.o: ltablib.c lprefix.h viper.h viperconf.h lauxlib.h viperlib.h
ltm.o: ltm.c lprefix.h viper.h viperconf.h ldebug.h lstate.h lobject.h \
 llimits.h ltm.h lzio.h lmem.h ldo.h lgc.h lstring.h ltable.h lvm.h
viper.o: viper.c lprefix.h viper.h viperconf.h lauxlib.h viperlib.h
viperc.o: viperc.c lprefix.h viper.h viperconf.h lauxlib.h ldebug.h lstate.h \
 lobject.h llimits.h ltm.h lzio.h lmem.h lopcodes.h lopnames.h lundump.h
lundump.o: lundump.c lprefix.h viper.h viperconf.h ldebug.h lstate.h \
 lobject.h llimits.h ltm.h lzio.h lmem.h ldo.h lfunc.h lstring.h lgc.h \
 lundump.h
lutf8lib.o: lutf8lib.c lprefix.h viper.h viperconf.h lauxlib.h viperlib.h
lvm.o: lvm.c lprefix.h viper.h viperconf.h ldebug.h lstate.h lobject.h \
 llimits.h ltm.h lzio.h lmem.h ldo.h lfunc.h lgc.h lopcodes.h lstring.h \
 ltable.h lvm.h ljumptab.h
lzio.o: lzio.c lprefix.h viper.h viperconf.h llimits.h lmem.h lstate.h \
 lobject.h ltm.h lzio.h

# (end of Makefile)
