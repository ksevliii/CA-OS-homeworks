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
./port_sim config.txt 10 > /dev/null 2>/dev/null
if grep -q "Ships departed:             4" port.log; then
    echo "PASS: All ships departed"
else
    echo "FAIL: Not all ships departed"
fi
echo ""

echo "~~~ Test 3: Warehouse Overflow ~~~"
rm -f port.log
echo "300 2 2 2 10 2 3 3 42 0 1 1 1 1 1 1 1 0 1 1 1 1 1 1 0 30 0 1 0 30 0 1 0 30 1" > small_warehouse.txt
./port_sim small_warehouse.txt 10 > /dev/null 2>/dev/null
if grep -q "unloading suspended" port.log && grep -q "unloading resumed" port.log; then
    echo "PASS: Unloading suspended and resumed"
else
    echo "FAIL: Warehouse overflow not detected"
fi
if grep -q "Ships departed:             3" port.log; then
    echo "PASS: All ships departed"
else
    echo "FAIL: Not all ships departed"
fi
rm -f small_warehouse.txt
echo ""

echo "~~~ Test 4: Time Limit ~~~"
rm -f port.log
echo "6 1 1 1 100 3 4 3 42 0 1 1 1 0 1 1 1 0 30 0 1 0 30 1 1 0 30 2" > short_time.txt
./port_sim short_time.txt 10 > /dev/null 2>/dev/null
if grep -q "not finished" port.log; then
    echo "PASS: Unfinished ships are reported"
else
    echo "FAIL: Unfinished ships are not reported"
fi
rm -f short_time.txt
echo ""

echo "~~~ Test 5: Interrupt (Ctrl+C) ~~~"
rm -f port.log
echo "0 1 1 1 50 2 2 2 3 0 1 0 0 0 1 1 0 0 10 0 2 0 10 1" > unlimited.txt
./port_sim unlimited.txt 20 > /dev/null 2>/dev/null &
sleep 1
kill -INT $!
wait $!
if grep -q "Interrupted" port.log; then
    echo "PASS: Interrupted by signal, log saved"
else
    echo "FAIL: Interruption not detected"
fi
rm -f unlimited.txt
echo ""


echo "~~~ All tests finished ~~~"