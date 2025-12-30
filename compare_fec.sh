#!/bin/bash
VIDEO_FILE="ice_4cif.y4m"
DURATION=10

echo "=== Experiment 1: Baseline (No FEC) ==="
echo "Redundancy: 0.0"
./build/sender 12345 $VIDEO_FILE --redundancy 0.0 -v > sender_baseline.log 2>&1 &
pid1=$!
./build/receiver 127.0.0.1 12345 704 576 --fps 60 --cbr 1000 -v > receiver_baseline.log 2>&1 &
pid2=$!

sleep $DURATION
kill $pid1 $pid2
wait $pid1 $pid2

echo "Baseline done."
echo ""

echo "=== Experiment 2: FEC Enabled (20% Redundancy) ==="
echo "Redundancy: 0.2"
./build/sender 12346 $VIDEO_FILE --redundancy 0.2 -v > sender_fec.log 2>&1 &
pid3=$!
./build/receiver 127.0.0.1 12346 704 576 --fps 60 --cbr 1000 -v > receiver_fec.log 2>&1 &
pid4=$!

sleep $DURATION
kill $pid3 $pid4
wait $pid3 $pid4

echo "FEC done."
