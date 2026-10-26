## Makefile.am -- Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = subdir-objects

bin_PROGRAMS = xasm xlnk
xasm_LDADD = $(LDADD)
xlnk_LDADD = $(LDADD)

xasm_SOURCES = \
  astnode.c astnode.h \
  ir/frontend.c ir/frontend.h astproc.h expr_int.h \
  charmap.c charmap.h \
  loc.h \
  opcode.c opcode.h \
  parser.c scanner.c \
  symtab.c symtab.h \
  listing.c listing.h fceux_nl.c fceux_nl.h \
  output_file.c output_file.h \
  dependencies.c dependencies.h sha256.c sha256.h utf8.h \
  xasm.c xasm.h version.h \
  my_getopt.c my_getopt.h getopt.h \
  ir/value.c ir/value.h ir/extent.h ir/layout.c ir/layout.h ir/graph.h ir/heap.c ir/heap.h ir/object.c ir/object.h ir/program.c ir/program.h ir/path.h

xlnk_SOURCES = xlnk.c ir/link.c ir/link.h ir/object.c ir/object.h ir/path.h \
  ir/value.c ir/value.h ir/graph.h ir/heap.c ir/heap.h ir/extent.h ir/layout.c ir/layout.h \
  script.c script.h opcode.c opcode.h output_file.c output_file.h \
  my_getopt.c my_getopt.h getopt.h version.h

# Install the generated (but checked-in) man pages alongside the tools.
dist_man1_MANS = man1/xasm.1 man1/xlnk.1

# Distribute the grammar/lexer sources, but don’t use them by default to build
EXTRA_DIST = astproc.c parser.y scanner.l parser.h \
  tests/fixtures/object-wire-example.hex tests/check_wire_example.py \
  tests/projects/README.md tests/projects/baselines.json \
  tests/projects/freeze_baselines.py tests/projects/freeze_gate_assets.py \
  tests/projects/check_object_projects.py tests/projects/measure_object_projects.py \
  tests/projects/measure_analysis.py \
  ir/value.c ir/value.h ir/extent.h ir/layout.c ir/layout.h ir/README.md \
  man1/xasm.h2m man1/xlnk.h2m \
  tests/coverage_directives_control.asm \
  tests/coverage_directives_layout.asm \
  tests/coverage_directives_macros_io.asm \
  tests/coverage_directives_types_symbols.asm \
  tests/coverage_error_directive.asm \
  tests/coverage_instructions.asm \
  tests/coverage_org_pure.asm \
  tests/dependency_driver.c \
  tests/fixtures/charmap.txt \
  tests/fixtures/data.bin \
  tests/fixtures/include_target.asm \
  tests/fixtures/xref_data_directives.asm \
  tests/fixtures/xref_data_directives_object.asm \
  tests/ifndef.asm \
  tests/macro.asm \
  tests/regression.sh \
  tests/test_access_classification.py \
  tests/test_analysis_faults.c \
  tests/test_analysis_symbols.py \
  tests/test_astnode_alloc.c \
  tests/test_listing_values.c \
  tests/test_backend_performance.py \
  tests/test_backend_work.c \
  tests/test_build_configuration.py \
  tests/test_dependencies.py \
  tests/test_fceux_nl.py \
  tests/test_fceux_nl_alloc.c \
  tests/test_field_initializers.py \
  tests/test_expressions.py \
  tests/test_frontend_performance.py \
  tests/test_frontend_work.c \
  tests/test_inputs.py \
  tests/test_installation.py \
  tests/test_instruction_records.py \
  tests/test_listing_offsets.py \
  tests/test_io_faults.c \
  tests/test_linker_outputs.py \
  tests/test_output_failures.py \
  tests/test_pathconf_faults.c \
  tests/test_symtab.c \
  tests/test_symtab.py \
  tests/test_symtab_alloc.c \
  tests/test_ir_alloc.c tests/test_ir_values.c tests/test_ir_values.py \
  tests/test_ir_layout_alloc.c tests/test_ir_layout.c tests/test_ir_layout.py tests/test_ir_cache.c tests/test_ir_objects.py tests/test_ir_link_io.c tests/test_ir_object_codec.c tests/test_ir_object_failures.c \
  tests/test_ir_object_alloc.c tests/test_ir_program_alloc.c tests/test_ir_link_alloc.c tests/test_ir_heap.c tests/test_ir_heap_alloc.c tests/test_ir_frontend.py tests/test_ir_frontend_faults.c tests/test_ir_value_faults.c tests/test_ir_source_locations.c tests/test_ir_source_driver.c tests/test_ir_dependencies.c tests/perf_ir_frontend.py tests/ir_sizeof_cases.json \
  tests/tool_test_support.py \
  tests/verify_fceux_projects.py \
  doc/Assembler-Directives.html \
  doc/Custom-Character-Maps.html \
  doc/Error-and-Warning-Messages.html \
  doc/Implementation-Details.html \
  doc/Known-Bugs-and-Limitations.html \
  doc/Linker-Script-Commands.html \
  doc/Object-Code-Format.html \
  doc/Overview.html \
  doc/The-Assembler.html \
  doc/The-Linker.html \
  doc/What_0027s-New.html \
  doc/index.html

# Install the current feature documentation alongside the tools.
dist_doc_DATA = README NEWS MIGRATING_TO_2.0.md xorcyst.texinfo \
  XORCYST_V2_DESIGN.md \
  XORCYST_V2_CONTRACTS.md \
  XASM_ANALYSIS_FEATURES_GUIDE.md \
  XASM_DATA_DIRECTIVE_REFERENCES_SPEC.md \
  XASM_DEPENDENCY_MANIFEST_SPEC.md \
  XASM_FCEUX_NL_EXPORT_SPEC.md \
  XASM_INDEX_BOUND_ANALYSIS_SPEC.md \
  XASM_INSTRUCTION_RECORDS_SPEC.md \
  XASM_INSTRUCTION_RECORDS_V2_SPEC.md \
  XASM_INSTRUCTION_RECORDS_V3_SPEC.md \
  XASM_REVERSE_ENGINEERING_FEATURES_SPEC.md

# Install maintained public protocols; private captures and historical review
# records are not part of the release.
nobase_dist_doc_DATA = doc/performance.md \
  doc/object-format.md

# Use the build-tree tools even when distcheck builds outside the source tree.
check-local:
	PYTHONDONTWRITEBYTECODE=1 XASM="$(abs_builddir)/xasm" XLNK="$(abs_builddir)/xlnk" \
	  $(SHELL) "$(srcdir)/tests/regression.sh"

installcheck-local:
	python3 "$(srcdir)/tests/test_installation.py" "$(DESTDIR)$(bindir)" "$(DESTDIR)$(docdir)" "$(DESTDIR)$(mandir)/man1"

DISTCHECK_CONFIGURE_FLAGS = --disable-maintainer-mode YACC=: LEX=: HELP2MAN=:


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

# Regenerate the man pages from --help/--version output of the just-built
# tools, plus the SEE ALSO/COPYRIGHT material in man1/*.h2m.
man1/xasm.1: xasm$(EXEEXT) $(srcdir)/man1/xasm.h2m
	@if test "x$(HELP2MAN)" = "x:"; then \
	  echo "Cannot regenerate man1/xasm.1: no help2man found (HELP2MAN is ':')"; \
	  exit 1; \
	fi
	$(AM_V_GEN)$(MKDIR_P) man1 && \
	  $(HELP2MAN) --no-info --section=1 --manual="XORcyst Manual" \
	    --name="assembler for 6502 machine code" \
	    --include=$(srcdir)/man1/xasm.h2m \
	    --output=$@ ./xasm$(EXEEXT)

man1/xlnk.1: xlnk$(EXEEXT) $(srcdir)/man1/xlnk.h2m
	@if test "x$(HELP2MAN)" = "x:"; then \
	  echo "Cannot regenerate man1/xlnk.1: no help2man found (HELP2MAN is ':')"; \
	  exit 1; \
	fi
	$(AM_V_GEN)$(MKDIR_P) man1 && \
	  $(HELP2MAN) --no-info --section=1 --manual="XORcyst Manual" \
	    --name="linker for xasm object files" \
	    --include=$(srcdir)/man1/xlnk.h2m \
	    --output=$@ ./xlnk$(EXEEXT)

endif
