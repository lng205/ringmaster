import os
import random

from typing import Iterator, Tuple

def mock_video_encoder(
    bitrate_bps: int,
    frame_rate: int = 30,
    gop: int = 30,
    key_frame_factor: float = 2.0,
    fluctuation: float = 0.2,  # 20% fluctuation
) -> Iterator[Tuple[int, bool, bytes]]:
    """
    Simulate a video encoder outputting frames as random bytes.
    Yields (frame_index, is_key_frame, bytes_data) indefinitely.
    Key frames (I-frames) are larger by key_frame_factor.
    Each frame's size fluctuates within ±fluctuation of its base size.
    """
    avg_frame_size = bitrate_bps / frame_rate / 8  # bytes per frame
    p_frame_size = int(avg_frame_size)
    i_frame_size = int(avg_frame_size * key_frame_factor)

    frame_idx = 0
    while True:
        is_key = (frame_idx % gop == 0)
        base_size = i_frame_size if is_key else p_frame_size
        # Fluctuate size within ±fluctuation
        min_size = int(base_size * (1 - fluctuation))
        max_size = int(base_size * (1 + fluctuation))
        size = random.randint(min_size, max_size)
        data = os.urandom(size)
        yield (frame_idx, is_key, data)
        frame_idx += 1

if __name__ == "__main__":
    bitrate = 2_000_000  # 2 Mbps
    frame_rate = 30
    duration_sec = 3
    total_frames = frame_rate * duration_sec
    gen = mock_video_encoder(bitrate_bps=bitrate, frame_rate=frame_rate)
    total_bytes = 0
    for i, (idx, is_key, data) in enumerate(gen):
        print(f"Frame {idx}: {'Key' if is_key else 'P'}-frame, {len(data)} bytes")
        total_bytes += len(data)
        if i >= total_frames - 1:
            break
    achieved_bps = (total_bytes * 8) / duration_sec
    print(f"\nSimulated {total_frames} frames over {duration_sec} sec.")
    print(f"Total bytes: {total_bytes}")
    print(f"Achieved bitrate: {achieved_bps:.2f} bps")
