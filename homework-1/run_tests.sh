echo "~~~ Test 1: Build ~~~"
make clean && make > /dev/null 2>/dev/null
if [ $? -ne 0 ]; then
    echo "FAIL: Build failed"
    exit 1
fi
echo "PASS: Build successful"
echo ""

echo "~~~ Test 2: Normal Run ~~~"
rm -f port.log
./port_sim config.txt 0 > /dev/null 2>/dev/null
if grep -q "Ships departed:             4" port.log; then
    echo "PASS: All ships departed"
else
    echo "FAIL: Not all ships departed"
fi
echo ""

echo "~~~ Test 3: Warehouse Overflow ~~~"
rm -f port.log
echo "10 2 1 1 20 2 3 3 42 0 1 1 1 0 1 1 1 0 30 0 1 0 30 2 1 0 30 4" > small_warehouse.txt
./port_sim small_warehouse.txt 0 > /dev/null 2>/dev/null
if grep -q "warehouse full" port.log; then
    echo "PASS: Warehouse overflow detected"
else
    echo "FAIL: Warehouse overflow not detected"
fi
rm -f small_warehouse.txt
echo ""

echo "~~~ Test 4: Reproducibility ~~~"
rm -f port.log log1.txt log2.txt
./port_sim config.txt 0 > /dev/null 2>/dev/null
cp port.log log1.txt
./port_sim config.txt 0 > /dev/null 2>/dev/null
cp port.log log2.txt
if diff -q log1.txt log2.txt > /dev/null; then
    echo "PASS: Results are identical"
else
    echo "FAIL: Results differ"
fi
rm -f log1.txt log2.txt
echo ""

echo "~~~ All tests finished ~~~"