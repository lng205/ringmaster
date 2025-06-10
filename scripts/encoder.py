import mock_data_frame
from matrix import Matrix

L = 12
M = 2
K = 4

video_encoder = mock_data_frame.mock_video_encoder(bitrate_bps=2_000_000)
matrix = Matrix(m=M, k=K, order=L)

def packetizer(data: bytes, MTU: int = 1500) -> list[bytes]:
    max_syms_per_packet = MTU // (L + 1)



    # Split the data into packets
    packets = [data[i:i+packet_size] for i in range(0, len(data), packet_size)]
    return packets

def fec_encoder(packets: list[bytes]) -> list[bytes]:
    # Add redundant packets
    redundant_packets = []
    m = matrix.get_encode_matrix()




if __name__ == "__main__":
    fec_encoder(video_encoder)