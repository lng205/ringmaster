build/sender 12345 ice_4cif_30fps.y4m -v > "sender.log" 2>&1 &
pid1=$!

build/receiver 127.0.0.1 12345 704 576 --fps 30 --cbr 1000 -v > "receiver.log" 2>&1 &
pid2=$!

cleanup() {
    echo "Stopping programs..."
    kill $pid1 $pid2
    wait $pid1 $pid2
}

trap cleanup EXIT

wait