#!/bin/bash
# suhc test runner — diffs compiled output against expected files
# Usage: ./tests/run_tests.sh [--verbose]
# No set -e: suhc returns non-zero on errors, which is expected behavior

SUHC="./suhc"
TESTS_DIR="./tests"
ORDBOK_DIR="./ordbok"
PASS=0
FAIL=0
SKIP=0
VERBOSE="${1:-}"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m'

run_test() {
    local szh="$1"
    local target="$2"
    local ext="$3"
    local ordbok_flag="$4"
    local base="${szh%.szh}"
    local expected="${base}.${ext}.expected"
    local output="${base}.${ext}"

    if [ ! -f "$expected" ]; then
        if [ "$VERBOSE" = "--verbose" ]; then
            echo -e "  ${YELLOW}SKIP${NC} ${base##*/} → ${ext} (no expected file)"
        fi
        SKIP=$((SKIP + 1))
        return
    fi

    # Compile (suppress all console output — only care about generated file)
    $SUHC "$szh" $ordbok_flag --target "$target" >/dev/null 2>&1
    if [ ! -f "$output" ]; then
        echo -e "  ${RED}FAIL${NC} ${base##*/} → ${ext} (compilation error)"
        FAIL=$((FAIL + 1))
        return
    fi

    # Diff (ignoring trailing whitespace)
    if diff -q -b "$output" "$expected" >/dev/null 2>&1; then
        echo -e "  ${GREEN}PASS${NC} ${base##*/} → ${ext}"
        PASS=$((PASS + 1))
    else
        echo -e "  ${RED}FAIL${NC} ${base##*/} → ${ext}"
        if [ "$VERBOSE" = "--verbose" ]; then
            diff -u --color=always "$expected" "$output" | head -30
        fi
        FAIL=$((FAIL + 1))
    fi
}

run_error_test() {
    local szh="$1"
    local base="${szh%.szh}"
    local name="${base##*/}"

    # Should produce errors → no output file generated
    output=$($SUHC "$szh" --target ts 2>&1)
    if echo "$output" | grep -q ": error \["; then
        echo -e "  ${GREEN}PASS${NC} ${name} (expected errors detected)"
        PASS=$((PASS + 1))
    else
        echo -e "  ${RED}FAIL${NC} ${name} (expected errors but got none)"
        FAIL=$((FAIL + 1))
    fi
}

run_circular_test() {
    local szh="$1"
    local base="${szh%.szh}"
    local name="${base##*/}"

    output=$($SUHC "$szh" --ordbok "$TESTS_DIR" --target ts 2>&1)
    if echo "$output" | grep -q "circular_import"; then
        echo -e "  ${GREEN}PASS${NC} ${name} (circular import detected)"
        PASS=$((PASS + 1))
    else
        echo -e "  ${RED}FAIL${NC} ${name} (expected circular import error)"
        FAIL=$((FAIL + 1))
    fi
}

echo "suhc test suite"
echo "================"
echo ""

# --- Unit tests (diff-based) ---
echo "Unit tests:"
for szh in "$TESTS_DIR"/minimal.szh "$TESTS_DIR"/membership.szh "$TESTS_DIR"/match_test.szh; do
    [ -f "$szh" ] || continue
    run_test "$szh" "ts" "ts" ""
    run_test "$szh" "sql" "sql" ""
    run_test "$szh" "c" "h" ""
done

# Import test needs --ordbok
run_test "$TESTS_DIR/import_test.szh" "ts" "ts" "--ordbok $TESTS_DIR"
run_test "$TESTS_DIR/import_test.szh" "c" "h" "--ordbok $TESTS_DIR"

echo ""

# --- Error detection tests ---
echo "Error detection:"
run_error_test "$TESTS_DIR/errors.szh"
run_circular_test "$TESTS_DIR/circular_a.szh"

echo ""

# --- Validation tests ---
echo "Validation tests:"

# Exhaustiveness: exhaust_test should warn about missing 'gamma'
run_warning_test() {
    local szh="$1"
    local pattern="$2"
    local description="$3"
    local base="${szh%.szh}"
    local name="${base##*/}"

    output=$($SUHC "$szh" 2>&1)
    if echo "$output" | grep -q "$pattern"; then
        echo -e "  ${GREEN}PASS${NC} ${name} (${description})"
        PASS=$((PASS + 1))
    else
        echo -e "  ${RED}FAIL${NC} ${name} (expected: ${description})"
        FAIL=$((FAIL + 1))
    fi
}

run_no_match_test() {
    local szh="$1"
    local pattern="$2"
    local description="$3"
    local base="${szh%.szh}"
    local name="${base##*/}"

    output=$($SUHC "$szh" 2>&1)
    if echo "$output" | grep -q "$pattern"; then
        echo -e "  ${RED}FAIL${NC} ${name} (${description} — but found match)"
        FAIL=$((FAIL + 1))
    else
        echo -e "  ${GREEN}PASS${NC} ${name} (${description})"
        PASS=$((PASS + 1))
    fi
}

# Exhaustiveness tests
run_warning_test "$TESTS_DIR/exhaust_test.szh" "missing explicit case for dimension member 'gamma'" \
    "warns about missing gamma"
run_no_match_test "$TESTS_DIR/exhaust_complete.szh" "missing explicit case" \
    "no missing-member warnings"
run_error_test "$TESTS_DIR/exhaust_no_default.szh"
run_warning_test "$TESTS_DIR/exhaust_no_default.szh" "no wildcard default arm" \
    "warns about missing default"

# Cross-product exhaustiveness tests (Sprint 3B)
run_note_test() {
    local szh="$1"
    local pattern="$2"
    local description="$3"
    local base="${szh%.szh}"
    local name="${base##*/}"

    output=$($SUHC "$szh" 2>&1)
    if echo "$output" | grep -q "$pattern"; then
        echo -e "  ${GREEN}PASS${NC} ${name} (${description})"
        PASS=$((PASS + 1))
    else
        echo -e "  ${RED}FAIL${NC} ${name} (expected: ${description})"
        FAIL=$((FAIL + 1))
    fi
}

run_note_test "$TESTS_DIR/cross_product_test.szh" "missing explicit case for pair (owner, cancel)" \
    "cross-product note for (owner, cancel)"
run_note_test "$TESTS_DIR/cross_product_test.szh" "missing explicit case for pair (admin, cancel)" \
    "cross-product note for (admin, cancel)"
run_no_match_test "$TESTS_DIR/cross_product_test.szh" "missing explicit case for pair (member" \
    "no cross-product note for member (covered by partial wildcard)"

# Meihua validation tests
run_warning_test "$TESTS_DIR/meihua_validate.szh" "calls unknown function 'undefined_thing'" \
    "flags undefined_thing"
run_warning_test "$TESTS_DIR/meihua_validate.szh" "references undefined identifier 'unknown_var'" \
    "flags unknown_var"
run_no_match_test "$TESTS_DIR/meihua_validate.szh" "meihua 'good_fn' calls unknown" \
    "good_fn has no unknown-call warnings"

# Meihua arity tests (Sprint 3B)
run_warning_test "$TESTS_DIR/meihua_arity.szh" "calls 'add' with 3 args but 'add' expects 2" \
    "arity mismatch: add(p,q,99)"
run_warning_test "$TESTS_DIR/meihua_arity.szh" "calls 'double' with 2 args but 'double' expects 1" \
    "arity mismatch: double(p,q)"

echo ""

# --- Sprint 4A: Drift detection + S-measurement tests ---
echo "Drift detection tests:"

# Drift test: known matches
run_diff_test() {
    local szh="$1"
    local ts="$2"
    local pattern="$3"
    local description="$4"

    output=$($SUHC --diff "$szh" "$ts" 2>&1)
    if echo "$output" | grep -q "$pattern"; then
        echo -e "  ${GREEN}PASS${NC} drift (${description})"
        PASS=$((PASS + 1))
    else
        echo -e "  ${RED}FAIL${NC} drift (expected: ${description})"
        echo "  output: $output" | head -5
        FAIL=$((FAIL + 1))
    fi
}

run_diff_test "$TESTS_DIR/drift_test.szh" "$TESTS_DIR/drift_test_spoxis.ts" \
    "MATCH.*app_version.*values match" \
    "app_version matches"

run_diff_test "$TESTS_DIR/drift_test.szh" "$TESTS_DIR/drift_test_spoxis.ts" \
    "DRIFT.*max_retries.*ordbok=5 spoxis=3" \
    "max_retries drift detected"

run_diff_test "$TESTS_DIR/drift_test.szh" "$TESTS_DIR/drift_test_spoxis.ts" \
    "DRIFT.*status_label.*ordbok-only" \
    "status_label projection drift (missing pending)"

run_diff_test "$TESTS_DIR/drift_test.szh" "$TESTS_DIR/drift_test_spoxis.ts" \
    "MATCH.*retry_delay.*matching arity" \
    "retry_delay meihua arity match"

# S-measurement in build report
echo ""
echo "S-measurement tests:"

run_s_test() {
    local szh="$1"
    local pattern="$2"
    local description="$3"

    output=$($SUHC "$szh" 2>&1)
    if echo "$output" | grep -q "$pattern"; then
        echo -e "  ${GREEN}PASS${NC} S-measurement (${description})"
        PASS=$((PASS + 1))
    else
        echo -e "  ${RED}FAIL${NC} S-measurement (expected: ${description})"
        FAIL=$((FAIL + 1))
    fi
}

run_s_test "$TESTS_DIR/cross_product_test.szh" "S (solipsism):.*0.00.*fully situated" \
    "cross_product_test S=0.00"
run_s_test "$TESTS_DIR/meihua_arity.szh" "S (solipsism):.*0.33.*moderate" \
    "meihua_arity S=0.33 (moderate)"

echo ""

# --- Ordbok integration tests ---
echo "Ordbok integration (compile-only):"
ordbok_errors=0
for szh in "$ORDBOK_DIR"/*.szh; do
    [ -f "$szh" ] || continue
    name="${szh##*/}"
    for target in ts sql c; do
        output=$($SUHC "$szh" --ordbok "$ORDBOK_DIR" --target "$target" 2>&1)
        if echo "$output" | grep -q "^.*: error \["; then
            echo -e "  ${RED}FAIL${NC} ${name} → ${target}"
            ordbok_errors=$((ordbok_errors + 1))
        else
            echo -e "  ${GREEN}PASS${NC} ${name} → ${target}"
            PASS=$((PASS + 1))
        fi
    done
done

# --- Sprint 4B: SQL validation tests ---
echo "SQL validation tests:"

for szh in "$ORDBOK_DIR"/*.szh; do
    [ -f "$szh" ] || continue
    name="${szh##*/}"
    base="${szh%.szh}"

    # Generate .sql if not already present
    $SUHC "$szh" --ordbok "$ORDBOK_DIR" --target sql >/dev/null 2>&1

    sql_file="${base}.sql"
    if [ ! -f "$sql_file" ]; then
        echo -e "  ${YELLOW}SKIP${NC} ${name} → sql validation (no .sql generated)"
        SKIP=$((SKIP + 1))
        continue
    fi

    output=$($SUHC --validate-sql "$sql_file" 2>&1)
    if echo "$output" | grep -q "SQL OK"; then
        echo -e "  ${GREEN}PASS${NC} ${name} → sql syntax valid"
        PASS=$((PASS + 1))
    else
        echo -e "  ${RED}FAIL${NC} ${name} → sql syntax invalid"
        echo "  $output"
        FAIL=$((FAIL + 1))
    fi
done

echo ""

# --- Sprint 4B: Graph output test ---
echo "Graph tests:"

graph_output=$($SUHC --graph "$ORDBOK_DIR" 2>&1)
if echo "$graph_output" | grep -q "graph LR"; then
    echo -e "  ${GREEN}PASS${NC} --graph produces valid Mermaid (graph LR found)"
    PASS=$((PASS + 1))
else
    echo -e "  ${RED}FAIL${NC} --graph does not produce valid Mermaid"
    FAIL=$((FAIL + 1))
fi

if echo "$graph_output" | grep -q "foundational --> auth"; then
    echo -e "  ${GREEN}PASS${NC} --graph shows foundational --> auth edge"
    PASS=$((PASS + 1))
else
    echo -e "  ${RED}FAIL${NC} --graph missing foundational --> auth edge"
    FAIL=$((FAIL + 1))
fi

echo ""

# --- Sprint 4B: Audit S-measurement test ---
echo "Audit S-measurement tests:"

audit_output=$($SUHC --audit "$ORDBOK_DIR" 2>&1)
if echo "$audit_output" | grep -q "aggregate S:"; then
    echo -e "  ${GREEN}PASS${NC} audit shows aggregate S"
    PASS=$((PASS + 1))
else
    echo -e "  ${RED}FAIL${NC} audit missing aggregate S"
    FAIL=$((FAIL + 1))
fi

if echo "$audit_output" | grep -q "S = 0.00"; then
    echo -e "  ${GREEN}PASS${NC} audit shows per-file S = 0.00"
    PASS=$((PASS + 1))
else
    echo -e "  ${RED}FAIL${NC} audit missing per-file S values"
    FAIL=$((FAIL + 1))
fi

# --- Sprint 5A: Conditional meihua tests ---
echo "Conditional meihua tests (diff-based):"
run_test "$TESTS_DIR/meihua_conditional.szh" "ts" "ts" ""
run_test "$TESTS_DIR/meihua_conditional.szh" "sql" "sql" ""
run_test "$TESTS_DIR/meihua_conditional.szh" "c" "h" ""

echo ""

# --- Sprint 5A: Typed emission tests ---
echo "Typed emission tests:"

typed_output=$($SUHC "$TESTS_DIR/typed_emission.szh" --target ts 2>&1)
if grep -q "export type Color" "$TESTS_DIR/typed_emission.ts" 2>/dev/null; then
    echo -e "  ${GREEN}PASS${NC} typed_emission (Color union type emitted)"
    PASS=$((PASS + 1))
else
    echo -e "  ${RED}FAIL${NC} typed_emission (expected Color union type)"
    FAIL=$((FAIL + 1))
fi

if grep -q "export type Size" "$TESTS_DIR/typed_emission.ts" 2>/dev/null; then
    echo -e "  ${GREEN}PASS${NC} typed_emission (Size union type emitted)"
    PASS=$((PASS + 1))
else
    echo -e "  ${RED}FAIL${NC} typed_emission (expected Size union type)"
    FAIL=$((FAIL + 1))
fi

if grep -q "color: Color" "$TESTS_DIR/typed_emission.ts" 2>/dev/null; then
    echo -e "  ${GREEN}PASS${NC} typed_emission (parameter uses Color type)"
    PASS=$((PASS + 1))
else
    echo -e "  ${RED}FAIL${NC} typed_emission (parameter should use Color type)"
    FAIL=$((FAIL + 1))
fi

if grep -q "size: Size" "$TESTS_DIR/typed_emission.ts" 2>/dev/null; then
    echo -e "  ${GREEN}PASS${NC} typed_emission (parameter uses Size type)"
    PASS=$((PASS + 1))
else
    echo -e "  ${RED}FAIL${NC} typed_emission (parameter should use Size type)"
    FAIL=$((FAIL + 1))
fi

echo ""

# --- Sprint 5A: Semcheck tests ---
echo "Semcheck tests:"

semcheck_output=$($SUHC "$TESTS_DIR/semcheck_test.szh" 2>&1)
if echo "$semcheck_output" | grep -q "'mango' is not a member of dimension 'fruit'"; then
    echo -e "  ${GREEN}PASS${NC} semcheck (detects mango not in fruit dimension)"
    PASS=$((PASS + 1))
else
    echo -e "  ${RED}FAIL${NC} semcheck (expected mango dimension mismatch)"
    FAIL=$((FAIL + 1))
fi

run_no_match_test "$TESTS_DIR/semcheck_test.szh" "'apple' is not a member" \
    "no false positive for apple (valid member)"
run_no_match_test "$TESTS_DIR/semcheck_test.szh" "'banana' is not a member" \
    "no false positive for banana (valid member)"

echo ""

# --- Sprint 5A: Conditional + comparison in meihua ---
echo "Conditional validation tests:"

run_warning_test "$TESTS_DIR/meihua_conditional.szh" "S (solipsism):.*0.00" \
    "conditional meihua S=0.00"

# Verify conditional meihua emits ternary in TS
cond_ts=$($SUHC "$TESTS_DIR/meihua_conditional.szh" --target ts 2>/dev/null && cat "$TESTS_DIR/meihua_conditional.ts")
if echo "$cond_ts" | grep -q "? "; then
    echo -e "  ${GREEN}PASS${NC} conditional emits ternary operators in TS"
    PASS=$((PASS + 1))
else
    echo -e "  ${RED}FAIL${NC} conditional should emit ternary operators"
    FAIL=$((FAIL + 1))
fi

# Verify conditional meihua emits CASE WHEN in SQL
cond_sql=$($SUHC "$TESTS_DIR/meihua_conditional.szh" --target sql 2>/dev/null && cat "$TESTS_DIR/meihua_conditional.sql")
if echo "$cond_sql" | grep -q "CASE WHEN"; then
    echo -e "  ${GREEN}PASS${NC} conditional emits CASE WHEN in SQL"
    PASS=$((PASS + 1))
else
    echo -e "  ${RED}FAIL${NC} conditional should emit CASE WHEN in SQL"
    FAIL=$((FAIL + 1))
fi

# Verify === in TS (not ==)
if echo "$cond_ts" | grep -q "==="; then
    echo -e "  ${GREEN}PASS${NC} TS emits === (not ==)"
    PASS=$((PASS + 1))
else
    echo -e "  ${RED}FAIL${NC} TS should emit === for equality"
    FAIL=$((FAIL + 1))
fi

# Verify conditional meihua emits ternary in C
cond_c=$($SUHC "$TESTS_DIR/meihua_conditional.szh" --target c 2>/dev/null && cat "$TESTS_DIR/meihua_conditional.h")
if echo "$cond_c" | grep -q "? "; then
    echo -e "  ${GREEN}PASS${NC} conditional emits ternary operators in C"
    PASS=$((PASS + 1))
else
    echo -e "  ${RED}FAIL${NC} conditional should emit ternary operators in C"
    FAIL=$((FAIL + 1))
fi

# --- Sprint 5B: Expression completeness tests ---
echo "Expression completeness tests (diff-based):"
run_test "$TESTS_DIR/expr_complete.szh" "ts" "ts" ""
run_test "$TESTS_DIR/expr_complete.szh" "sql" "sql" ""
run_test "$TESTS_DIR/expr_complete.szh" "c" "h" ""

echo ""

# --- Sprint 5B: Songqiao + Zhulin tests ---
echo "Songqiao/Zhulin tests:"

# Songqiao compile tests
sq_ts=$($SUHC "$TESTS_DIR/songqiao_test.szh" --target ts 2>&1)
if [ -f "$TESTS_DIR/songqiao_test.ts" ] && grep -q "export function" "$TESTS_DIR/songqiao_test.ts" 2>/dev/null; then
    echo -e "  ${GREEN}PASS${NC} songqiao_test → ts"
    PASS=$((PASS + 1))
else
    echo -e "  ${RED}FAIL${NC} songqiao_test → ts"
    FAIL=$((FAIL + 1))
fi

sq_sql=$($SUHC "$TESTS_DIR/songqiao_test.szh" --target sql 2>&1)
if [ -f "$TESTS_DIR/songqiao_test.sql" ] && grep -q "songqiao_" "$TESTS_DIR/songqiao_test.sql" 2>/dev/null; then
    echo -e "  ${GREEN}PASS${NC} songqiao_test → sql"
    PASS=$((PASS + 1))
else
    echo -e "  ${RED}FAIL${NC} songqiao_test → sql"
    FAIL=$((FAIL + 1))
fi

# Songqiao SQL syntax validation
sq_valid=$($SUHC --validate-sql "$TESTS_DIR/songqiao_test.sql" 2>&1)
if echo "$sq_valid" | grep -q "SQL OK"; then
    echo -e "  ${GREEN}PASS${NC} songqiao_test SQL syntax valid"
    PASS=$((PASS + 1))
else
    echo -e "  ${RED}FAIL${NC} songqiao_test SQL syntax invalid"
    FAIL=$((FAIL + 1))
fi

# Songqiao C emission
sq_c=$($SUHC "$TESTS_DIR/songqiao_test.szh" --target c 2>&1)
if [ -f "$TESTS_DIR/songqiao_test.h" ] && grep -q "songqiao_" "$TESTS_DIR/songqiao_test.h" 2>/dev/null; then
    echo -e "  ${GREEN}PASS${NC} songqiao_test → c"
    PASS=$((PASS + 1))
else
    echo -e "  ${RED}FAIL${NC} songqiao_test → c"
    FAIL=$((FAIL + 1))
fi

# Zhulin compile tests
zh_ts=$($SUHC "$TESTS_DIR/zhulin_test.szh" --target ts 2>&1)
if [ -f "$TESTS_DIR/zhulin_test.ts" ] && grep -q "export function" "$TESTS_DIR/zhulin_test.ts" 2>/dev/null; then
    echo -e "  ${GREEN}PASS${NC} zhulin_test → ts"
    PASS=$((PASS + 1))
else
    echo -e "  ${RED}FAIL${NC} zhulin_test → ts"
    FAIL=$((FAIL + 1))
fi

zh_sql=$($SUHC "$TESTS_DIR/zhulin_test.szh" --target sql 2>&1)
if [ -f "$TESTS_DIR/zhulin_test.sql" ] && grep -q "CREATE OR REPLACE" "$TESTS_DIR/zhulin_test.sql" 2>/dev/null; then
    echo -e "  ${GREEN}PASS${NC} zhulin_test → sql"
    PASS=$((PASS + 1))
else
    echo -e "  ${RED}FAIL${NC} zhulin_test → sql"
    FAIL=$((FAIL + 1))
fi

# Zhulin SQL syntax validation
zh_valid=$($SUHC --validate-sql "$TESTS_DIR/zhulin_test.sql" 2>&1)
if echo "$zh_valid" | grep -q "SQL OK"; then
    echo -e "  ${GREEN}PASS${NC} zhulin_test SQL syntax valid"
    PASS=$((PASS + 1))
else
    echo -e "  ${RED}FAIL${NC} zhulin_test SQL syntax invalid"
    FAIL=$((FAIL + 1))
fi

# Zhulin C emission
zh_c=$($SUHC "$TESTS_DIR/zhulin_test.szh" --target c 2>&1)
if [ -f "$TESTS_DIR/zhulin_test.h" ] && grep -q "static inline" "$TESTS_DIR/zhulin_test.h" 2>/dev/null; then
    echo -e "  ${GREEN}PASS${NC} zhulin_test → c"
    PASS=$((PASS + 1))
else
    echo -e "  ${RED}FAIL${NC} zhulin_test → c"
    FAIL=$((FAIL + 1))
fi

echo ""

# --- Sprint 5B: Convergence tool tests ---
echo "Convergence tests:"

conv_output=$($SUHC --convergence "$ORDBOK_DIR" 2>&1)
if echo "$conv_output" | grep -q "Convergence Report"; then
    echo -e "  ${GREEN}PASS${NC} --convergence produces report"
    PASS=$((PASS + 1))
else
    echo -e "  ${RED}FAIL${NC} --convergence should produce report"
    FAIL=$((FAIL + 1))
fi

if echo "$conv_output" | grep -q "ratio:"; then
    echo -e "  ${GREEN}PASS${NC} --convergence shows ratio"
    PASS=$((PASS + 1))
else
    echo -e "  ${RED}FAIL${NC} --convergence should show ratio"
    FAIL=$((FAIL + 1))
fi

echo ""

# --- Sprint 5B: New ordbok audit validation ---
echo "New ordbok validation:"

# Audit should show S = 0.00 for new files
audit_5b=$($SUHC --audit "$ORDBOK_DIR" 2>&1)
if echo "$audit_5b" | grep -q "adversarial.szh"; then
    echo -e "  ${GREEN}PASS${NC} audit includes adversarial.szh"
    PASS=$((PASS + 1))
else
    echo -e "  ${RED}FAIL${NC} audit missing adversarial.szh"
    FAIL=$((FAIL + 1))
fi

if echo "$audit_5b" | grep -q "oracle_ceiling.szh"; then
    echo -e "  ${GREEN}PASS${NC} audit includes oracle_ceiling.szh"
    PASS=$((PASS + 1))
else
    echo -e "  ${RED}FAIL${NC} audit missing oracle_ceiling.szh"
    FAIL=$((FAIL + 1))
fi

echo ""

echo "================"
echo -e "Results: ${GREEN}${PASS} passed${NC}, ${RED}${FAIL} failed${NC}, ${YELLOW}${SKIP} skipped${NC}"

if [ "$FAIL" -gt 0 ]; then
    exit 1
fi
