.MAKE.MODE+=		meta
.ifndef .MAKE.JOBS
.MAKE.JOBS!=		sysctl -n hw.ncpu
.endif

# Guess the $PREFIX
.ifndef PREFIX
PREFIX!=		sysctl -n user.localbase
.endif
PREFIX?=		/usr/local

# Infer $BINDIR and DEBUGFILEDIR.
BINDIR?=		$(PREFIX)/bin
DEBUGFILEDIR?=		$(PREFIX)/lib/debug/bin
SHAREDIR?=              $(PREFIX)/share

# Pick the newest C standards supported by the FreeBSD 14.x system compiler (15.0 should support C23).
CSTD=			c17
CC=			cc

PROG=			kassiber

SRCS+=			main.c

CFLAGS_DEBUG?=		-O0 -g -pipe -DRACONIC
CFLAGS_RELEASE?=	-O2 -pipe

# Enable draconic compiler errors for debug builds.
.if empty(.TARGETS:tw:Mdebug)
CFLAGS_MODE=		$(CFLAGS_RELEASE)
MODE=			release
.else
CFLAGS_MODE=		$(CFLAGS_DEBUG)
MODE=			debug
.endif
CFLAGS=			$(CFLAGS_MODE)
.-include		"$(.OBJDIR)/.mode"

LDFLAGS+=		-ljail

.include <bsd.prog.mk>

.PHONY: compile_commands.json
compile_commands.json:
	set -Cefu -- $(SRCS:@.SRC.@$(.SRC.:Q)@); \
	sep=$$'\t'; \
	{ \
		echo '['; \
		for src; do \
			printf '%s{\n\t\t"directory": "%s",\n\t\t"command": "%s",\n\t\t"file": "%s",\n\t\t"output": "%s"\n\t}' \
				"$$sep" \
				"$(.CURDIR)" \
				"$(CC) $(CFLAGS) -c $$src -o $(.OBJDIR)/$${src%.*}.o" \
			       	"$(.CURDIR)/$$src" \
				"$(.OBJDIR)/$${src%.*}.o"; \
			sep=$$', '; \
		done && \
		echo $$'\n]'; \
	} >|$(.TARGET)

.PHONY: debug
.if defined(OLD_MODE) && $(MODE) == $(OLD_MODE)
debug: all
.else
debug: clean all
	echo "OLD_MODE=	"$(MODE:Q) > $(.OBJDIR:Q)/.mode
.endif

.PHONY: debug release
.if defined(OLD_MODE) && $(MODE) == $(OLD_MODE)
release: all
.else
release: clean all
	echo "OLD_MODE=	"$(MODE:Q) > $(.OBJDIR:Q)/.mode
.endif

.PHONY: scan-build
scan-build: clean
	scan-build $(.MAKE) -C $(.CURDIR) debug

.PHONY: xolint
xolint: $(SRCS)
	xolint $(SRCS)
