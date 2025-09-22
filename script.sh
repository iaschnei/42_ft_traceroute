#!/bin/bash

BINARY="./ft_traceroute"
TARGET_IP="8.8.8.8"
TARGET_HOST="google.com"
BAD_IP="0.999.0.1"
BAD_HOST="idontexist.localdomain"

# You may need to change this if your real traceroute output differs
REAL_TRACEROUTE_OUTPUT=$(command -v traceroute)

if [[ $EUID -ne 0 ]]; then
  echo "⚠️  Please run this script with sudo: sudo $0"
  exit 1
fi

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

echo "🔁 Rebuilding project..."
make fclean && make
if [[ $? -ne 0 ]]; then
  echo -e "${RED}❌ Build failed.${NC}"
  exit 1
fi

function run_test() {
  desc=$1
  shift
  echo -e "\n📦 ${desc}"
  echo -e "🔹 Command: ${BINARY} $@\n"
  $BINARY "$@"
}

echo "🚀 Running ft_traceroute test cases..."

run_test "1. Help message" --help

run_test "2. Valid IP" "$TARGET_IP"
run_test "3. Valid hostname" "$TARGET_HOST"
run_test "4. Invalid IP" "$BAD_IP"
run_test "5. Invalid hostname" "$BAD_HOST"

run_test "6. -m: max TTL = 5" -m 5 "$TARGET_IP"
run_test "7. -q: 1 probe per hop" -q 1 "$TARGET_IP"
run_test "8. -n: no DNS resolution" -n "$TARGET_HOST"
run_test "9. -p: base port 44444" -p 44444 "$TARGET_IP"

run_test "10. Invalid -m (too high)" -m 999 "$TARGET_IP"
run_test "11. Invalid -q (zero)" -q 0 "$TARGET_IP"
run_test "12. Invalid -p (too low)" -p 80 "$TARGET_IP"

run_test "13. Missing value for -m" -m "$TARGET_IP"
run_test "14. Missing value for -q" -q "$TARGET_IP"
run_test "15. Missing value for -p" -p "$TARGET_IP"

# Optionally compare with system traceroute
echo -e "\n🆚 Comparing with system traceroute (1 hop only)..."
echo "System traceroute:"
traceroute -m 1 "$TARGET_IP"
echo "Your ft_traceroute:"
$BINARY -m 1 "$TARGET_IP"

echo -e "\n✅ ${GREEN}All tests ran.${NC} Please verify outputs manually above if needed."
