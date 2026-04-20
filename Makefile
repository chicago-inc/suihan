# suhc — the suihan compiler
# M7: Full bootstrap — the compiler builds itself from its own ordbok.
#
# No dependencies beyond libc. Buildable on any POSIX system.
#
# Build order:
#   1. generate  — compile ordbok/compiler/*.szh → include/gen/*.h
#   2. compile   — build suhc using generated headers
# If include/gen/*.h already exist (committed), step 1 is skipped.
# Run 'make regenerate' to re-run step 1 with the current suhc.
# Run 'make bootstrap' to verify the full two-stage fixed point.

CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -std=c11 -O2 -D_GNU_SOURCE
CFLAGS  += -Iinclude
LDFLAGS ?= -lm

# Automatic header dependency generation
DEPFLAGS = -MMD -MP

# Debug build: make DEBUG=1
ifdef DEBUG
CFLAGS  := -Wall -Wextra -std=c11 -g -O0 -DDEBUG -D_GNU_SOURCE -Iinclude
endif

SRCDIR  = src
INCDIR  = include
GENDIR  = include/gen
BUILDDIR = build
TESTDIR = tests

SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SOURCES))
DEPFILES = $(OBJECTS:.o=.d)
TARGET  = suhc

# Generated headers (M1–M6)
GEN_HEADERS = $(GENDIR)/kinds.h $(GENDIR)/decl_types.h $(GENDIR)/expr_types.h $(GENDIR)/token_types.h \
              $(GENDIR)/kind_names.h $(GENDIR)/decl_type_names.h $(GENDIR)/token_type_names.h \
              $(GENDIR)/kind_inference.h \
              $(GENDIR)/emit_c_dispatch.h $(GENDIR)/emit_ts_dispatch.h $(GENDIR)/emit_sql_dispatch.h \
              $(GENDIR)/prec_levels.h $(GENDIR)/token_prec.h \
              $(GENDIR)/math_fns.h $(GENDIR)/math_fn_c.h $(GENDIR)/math_fn_sql.h \
              $(GENDIR)/keyword_dispatch.h $(GENDIR)/kind_sigil_dispatch.h \
              $(GENDIR)/emit_asm_dispatch.h

.PHONY: all clean test check debug dump-tokens dump-ast regenerate bootstrap lint security release-linux emit-visual literal-check

all: $(TARGET)

# Emit visual.szh + primitives.szh → src/primitives/*.ts
# Re-run after any ordbok edit. Generated files are committed (DO NOT EDIT).
emit-visual: $(TARGET)
	@mkdir -p ../../src/primitives
	./$(TARGET) ../ordbok/visual.szh --target typescript --ordbok ordbok -o ../../src/primitives/visualTokens.ts
	./$(TARGET) ../ordbok/primitives.szh --target typescript --ordbok ordbok -o ../../src/primitives/primitiveTokens.ts
	@echo "emitted src/primitives/visualTokens.ts + primitiveTokens.ts"

# Literal-rejection lint over project src + app trees.
# Skips theme.ts, src/primitives/, .test.*, node_modules, .expo, build, dist.
# Exits non-zero on any inline hex / rgba — wired into pre-push and CI.
literal-check: $(TARGET)
	@./$(TARGET) --literal-check ../../src
	@./$(TARGET) --literal-check ../../app

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# Include generated dependency files (silently skip if not yet built)
-include $(DEPFILES)

# Regenerate headers from ordbok using the current suhc binary
regenerate: $(TARGET)
	@echo "=== Regenerating include/gen/ from ordbok/compiler/ ==="
	@mkdir -p $(GENDIR)
	./$(TARGET) ordbok/compiler/compiler_kinds.szh --target c -o $(GENDIR)/kinds.h
	./$(TARGET) ordbok/compiler/compiler_decl_types.szh --target c -o $(GENDIR)/decl_types.h
	./$(TARGET) ordbok/compiler/compiler_expr_types.szh --target c -o $(GENDIR)/expr_types.h
	./$(TARGET) ordbok/compiler/compiler_token_types.szh --target c -o $(GENDIR)/token_types.h
	./$(TARGET) ordbok/compiler/compiler_kind_names.szh --target c --ordbok ordbok/compiler -o $(GENDIR)/kind_names.h
	./$(TARGET) ordbok/compiler/compiler_decl_type_names.szh --target c --ordbok ordbok/compiler -o $(GENDIR)/decl_type_names.h
	./$(TARGET) ordbok/compiler/compiler_token_type_names.szh --target c --ordbok ordbok/compiler -o $(GENDIR)/token_type_names.h
	./$(TARGET) ordbok/compiler/compiler_kind_inference.szh --target c --ordbok ordbok/compiler -o $(GENDIR)/kind_inference.h
	./$(TARGET) ordbok/compiler/compiler_emit_c_dispatch.szh --target c --ordbok ordbok/compiler -o $(GENDIR)/emit_c_dispatch.h
	./$(TARGET) ordbok/compiler/compiler_emit_ts_dispatch.szh --target c --ordbok ordbok/compiler -o $(GENDIR)/emit_ts_dispatch.h
	./$(TARGET) ordbok/compiler/compiler_emit_sql_dispatch.szh --target c --ordbok ordbok/compiler -o $(GENDIR)/emit_sql_dispatch.h
	./$(TARGET) ordbok/compiler/compiler_prec_levels.szh --target c -o $(GENDIR)/prec_levels.h
	./$(TARGET) ordbok/compiler/compiler_token_prec.szh --target c --ordbok ordbok/compiler -o $(GENDIR)/token_prec.h
	./$(TARGET) ordbok/compiler/compiler_math_fns.szh --target c -o $(GENDIR)/math_fns.h
	./$(TARGET) ordbok/compiler/compiler_math_fn_c.szh --target c --ordbok ordbok/compiler -o $(GENDIR)/math_fn_c.h
	./$(TARGET) ordbok/compiler/compiler_math_fn_sql.szh --target c --ordbok ordbok/compiler -o $(GENDIR)/math_fn_sql.h
	./$(TARGET) ordbok/compiler/compiler_keyword_dispatch.szh --target c --ordbok ordbok/compiler -o $(GENDIR)/keyword_dispatch.h
	./$(TARGET) ordbok/compiler/compiler_kind_sigil_dispatch.szh --target c --ordbok ordbok/compiler -o $(GENDIR)/kind_sigil_dispatch.h
	./$(TARGET) ordbok/compiler/compiler_emit_asm_dispatch.szh --target c --ordbok ordbok/compiler -o $(GENDIR)/emit_asm_dispatch.h
	@echo "=== Headers regenerated (M1–M8) — rebuild with 'make' ==="

# M7: Full bootstrap — verify the two-stage fixed point.
# Stage 1: build from committed headers.
# Stage 2: regenerate headers, rebuild, compare output.
# If both stages produce identical output on all ordbok files,
# the compiler has reached a fixed point — it is self-consistent.
bootstrap: $(TARGET)
	@echo ""
	@echo "╔══════════════════════════════════════════════════════╗"
	@echo "║         suhc bootstrap — two-stage verification     ║"
	@echo "╚══════════════════════════════════════════════════════╝"
	@echo ""
	@echo "── Stage 1: build from committed headers ──"
	@rm -rf build
	@$(MAKE) -s $(TARGET)
	@cp $(TARGET) $(TARGET).stage1
	@echo "   stage1 binary: $$(wc -c < $(TARGET).stage1) bytes"
	@echo ""
	@echo "── Stage 2: regenerate headers from stage-1 ordbok ──"
	@$(MAKE) -s regenerate 2>/dev/null
	@echo ""
	@echo "── Stage 3: rebuild from regenerated headers ──"
	@rm -rf build
	@$(MAKE) -s $(TARGET)
	@cp $(TARGET) $(TARGET).stage2
	@echo "   stage2 binary: $$(wc -c < $(TARGET).stage2) bytes"
	@echo ""
	@echo "── Stage 4: verify fixed point ──"
	@bash $(TESTDIR)/bootstrap_verify.sh
	@echo ""
	@echo "── Stage 5: test suite on stage-2 binary ──"
	@bash $(TESTDIR)/run_tests.sh
	@echo ""
	@rm -f $(TARGET).stage1 $(TARGET).stage2

clean:
	rm -rf $(BUILDDIR) $(TARGET) $(TARGET).stage1 $(TARGET).stage2

debug:
	$(MAKE) DEBUG=1

# Run the full test suite (tests/run_tests.sh)
test: $(TARGET)
	@bash $(TESTDIR)/run_tests.sh

# Compile all ordbok files (TS + SQL + C) as a smoke test
check: $(TARGET)
	@echo "=== suhc check: compiling all ordbok files ==="
	@fail=0; \
	for f in ordbok/*.szh; do \
		name=$$(basename "$$f" .szh); \
		printf "  %-20s " "$$name"; \
		if ./$(TARGET) "$$f" --target typescript -o /dev/null 2>/dev/null; then \
			printf "ts:OK "; \
		else \
			printf "ts:FAIL "; fail=1; \
		fi; \
		if ./$(TARGET) "$$f" --target sql -o /dev/null 2>/dev/null; then \
			printf "sql:OK "; \
		else \
			printf "sql:FAIL "; fail=1; \
		fi; \
		if ./$(TARGET) "$$f" --target c -o /dev/null 2>/dev/null; then \
			printf "c:OK "; \
		else \
			printf "c:FAIL "; fail=1; \
		fi; \
		if ./$(TARGET) "$$f" --target asm -o /dev/null 2>/dev/null; then \
			printf "asm:OK\n"; \
		else \
			printf "asm:FAIL\n"; fail=1; \
		fi; \
	done; \
	if [ $$fail -eq 0 ]; then \
		echo "=== All ordbok files compile successfully ==="; \
	else \
		echo "=== Some ordbok files failed ==="; exit 1; \
	fi

# P1: Structural lint target
lint: $(TARGET)
	@if [ -z "$(ORDBOK)" ] || [ -z "$(TARGETS)" ]; then \
		echo "Usage: make lint ORDBOK=ordbok/ TARGETS=../src/"; exit 1; \
	fi
	./$(TARGET) --lint --ordbok $(ORDBOK) --targets $(TARGETS) $(if $(FORMAT),--format $(FORMAT))

# P1: Security audit
security: $(TARGET)
	@if [ -z "$(ORDBOK)" ] || [ -z "$(TARGETS)" ]; then \
		echo "Usage: make security ORDBOK=ordbok/ TARGETS=../src/"; exit 1; \
	fi
	./$(TARGET) --lint --ordbok $(ORDBOK) --targets $(TARGETS) --security

# Cross-compilation for Linux x86_64 (for GitHub Action distribution)
release-linux:
	CC=x86_64-linux-gnu-gcc $(MAKE) clean all
	strip $(TARGET)
	mv $(TARGET) $(TARGET)-linux-x86_64

# Dump tokens for a specific file
dump-tokens: $(TARGET)
	@if [ -z "$(FILE)" ]; then echo "Usage: make dump-tokens FILE=path.szh"; exit 1; fi
	./$(TARGET) --dump-tokens $(FILE)

# Dump AST for a specific file
dump-ast: $(TARGET)
	@if [ -z "$(FILE)" ]; then echo "Usage: make dump-ast FILE=path.szh"; exit 1; fi
	./$(TARGET) --dump-ast $(FILE)
