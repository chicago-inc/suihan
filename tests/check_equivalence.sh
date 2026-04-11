#!/bin/bash
# check_equivalence.sh — measures ordbok coverage against Spoxis lib/ files
# Usage: ./tests/check_equivalence.sh

SUHC="./suhc"
ORDBOK_DIR="./ordbok"

GREEN='\033[0;32m'
YELLOW='\033[0;33m'
CYAN='\033[0;36m'
NC='\033[0m'

echo "Ordbok–Spoxis Equivalence Check"
echo "================================"
echo ""

# Count ordbok stats
total_files=$(ls "$ORDBOK_DIR"/*.szh 2>/dev/null | wc -l)
echo -e "${CYAN}Ordbok files:${NC} $total_files"

# Run audit for declaration counts
audit=$($SUHC --audit "$ORDBOK_DIR" 2>&1)
total_decls=$(echo "$audit" | grep "declarations:" | awk '{print $2}')
total_proj=$(echo "$audit" | grep "projections:" | awk '{print $2}')
total_meihua=$(echo "$audit" | grep "meihua:" | awk '{print $2}')
total_trav=$(echo "$audit" | grep "traversals:" | awk '{print $2}')
total_morph=$(echo "$audit" | grep "morphisms:" | awk '{print $2}')

echo -e "${CYAN}Declarations:${NC}  $total_decls"
echo -e "${CYAN}Projections:${NC}   $total_proj"
echo -e "${CYAN}Meihua:${NC}        $total_meihua"
echo -e "${CYAN}Traversals:${NC}    $total_trav"
echo -e "${CYAN}Morphisms:${NC}     $total_morph"
echo ""

# Spoxis known structural files (manually maintained)
spoxis_total=46
spoxis_structural=22
ordbok_covered=19

coverage_structural=$((ordbok_covered * 100 / spoxis_structural))
coverage_all=$((ordbok_covered * 100 / spoxis_total))

echo "Spoxis Coverage:"
echo -e "  structural files: ${GREEN}${ordbok_covered}/${spoxis_structural}${NC} (${coverage_structural}%)"
echo -e "  all lib/ files:   ${YELLOW}${ordbok_covered}/${spoxis_total}${NC} (${coverage_all}%)"
echo ""

# S measurement
s_value=$(echo "scale=2; 1 - $ordbok_covered / $spoxis_structural" | bc -l)
echo -e "Ordbok S (structural): ${CYAN}${s_value}${NC}"
echo ""

# Compile all ordbok files and count total case arms
total_arms=0
for szh in "$ORDBOK_DIR"/*.szh; do
    [ -f "$szh" ] || continue
    name="${szh##*/}"
    $SUHC "$szh" --ordbok "$ORDBOK_DIR" --target ts >/dev/null 2>&1
    ts_file="${szh%.szh}.ts"
    if [ -f "$ts_file" ]; then
        arms=$(grep -c "===" "$ts_file" 2>/dev/null || echo 0)
        total_arms=$((total_arms + arms))
    fi
done

echo "Total case arms across all generated TS: $total_arms"
echo ""
echo "================================"
echo "See tests/equivalence_report.md for detailed per-file analysis."
