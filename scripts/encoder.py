import mock_data_frame

L = 12

video_encoder = mock_data_frame.mock_video_encoder(bitrate_bps=2_000_000)

def packetizer(data: bytes, MTU: int = 1500) -> list[bytes]:
    # Uniformly split the data into packets
    packet_size = len(data) // MTU
    # Round up to the nearest multiple of L
    packet_size = (packet_size + L - 1) // L * L
    # Split the data into packets
    packets = [data[i:i+packet_size] for i in range(0, len(data), packet_size)]
    return packets

def fec_encoder(packets: list[bytes]) -> list[bytes]:
    # Add redundant packets
    redundant_packets = []