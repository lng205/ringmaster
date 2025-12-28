#!/bin/bash
# Using the downloaded real video file
VIDEO_FILE="ice_4cif.y4m"

# Sender: verbose
./build/sender 12345 $VIDEO_FILE -v > sender.log 2>&1 &
pid1=$!

# Receiver: 127.0.0.1, Port 12345, 704x576, 60 FPS, 1000 kbps CBR
./build/receiver 127.0.0.1 12345 704 576 --fps 60 --cbr 1000 -v > receiver.log 2>&1 &
pid2=$!

# Run for 15 seconds
sleep 15

# Cleanup
kill $pid1 $pid2
wait $pid1 $pid2