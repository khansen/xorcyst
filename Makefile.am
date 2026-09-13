## Makefile.am -- Process this file with automake to produce Makefile.in

bin_PROGRAMS = xasm xlnk
xasm_LDADD = $(LDADD) $(DARWIN_LIBS)

# Use the *generated* files for normal builds
xasm_SOURCES = \
  astnode.c astnode.h \
  astproc.c astproc.h \
  charmap.c charmap.h \
  codegen.c codegen.h \
  loc.h objdef.h \
  opcode.c opcode.h \
  parser.c scanner.c \
  symtab.c symtab.h \
  listing.c listing.h fceux_nl.c fceux_nl.h \
  output_file.c output_file.h \
  dependencies.c dependencies.h sha256.c sha256.h utf8.h \
  xasm.c version.h \
  my_getopt.c my_getopt.h getopt.h

xlnk_SOURCES = \
  xlnk.c version.h \
  output_file.c output_file.h \
  script.c script.h \
  unit.c unit.h \
  hashtab.c hashtab.h \
  opcode.c opcode.h \
  my_getopt.c my_getopt.h getopt.h

# Distribute the grammar/lexer sources, but don’t use them by default to build
EXTRA_DIST = parser.y scanner.l parser.h

AM_CFLAGS = -Wall
AM_LFLAGS = -i
AM_YFLAGS = -d

# Optional maintainer-only regeneration rules
# (These are only used if someone runs make after editing parser.y/scanner.l)

if MAINTAINER_MODE

# Regenerate both parser.c and parser.h from parser.y
parser.c parser.h: parser.y
	@if test "x$(YACC)" = "x:"; then \
	  echo "Cannot regenerate parser.c/parser.h: no yacc/bison/byacc found (YACC is ':')"; \
	  exit 1; \
	fi
	$(AM_V_YACC)$(YACC) $(AM_YFLAGS) -d -o parser.c $<

scanner.c: scanner.l
	@if test "x$(LEX)" = "x:"; then \
	  echo "Cannot regenerate scanner.c: no flex/lex found (LEX is ':')"; \
	  exit 1; \
	fi
	$(AM_V_LEX)$(LEX) $(AM_LFLAGS) -o $@ $<

endif
