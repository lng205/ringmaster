#include <getopt.h>
#include <iostream>
#include <string>
#include <map>
#include <set>
#include <vector>
#include <cstdlib>
#include <optional>
#include <poll.h>

#include "conversion.hh"
#include "udp_socket.hh"
#include "protocol.hh"
#include "galois.hh"
#include "redundancy_controller.hh"
#include "timestamp.hh"

using namespace std;

void print_usage(const string & program_name)
{
  cerr <<
  "Usage: " << program_name << " [options] upstream_host upstream_port listen_port\n\n"
  "Options:\n"
  "-R, --redundancy <R>  initial/fixed redundancy ratio (default: 0.1)\n"
  "-A, --adaptive        enable adaptive redundancy based on next-hop loss\n"
  "-v, --verbose         enable logging\n\n"
  "Example: relay -A 127.0.0.1 12345 12346\n"
  "  - Connects to sender at 127.0.0.1:12345\n"
  "  - Listens for receiver on port 12346\n"
  "  - Uses hop-by-hop ACK to measure downstream loss and adjust redundancy"
  << endl;
}

struct FrameState {
  uint32_t frame_id = 0;
  FrameType frame_type = FrameType::UNKNOWN;
  uint16_t k = 0;
  uint16_t padding = 0;
  size_t payload_size = 0;
  vector<vector<uint8_t>> coeffs;  // 每个包的系数向量
  vector<string> payloads;          // 对应的 payload
  bool recoded = false;

  // 添加一个包，提取系数
  void add_packet(const string& payload) {
    if (k == 0 || payload.size() < k) return;
    vector<uint8_t> coef(payload.begin(), payload.begin() + k);
    coeffs.push_back(move(coef));
    payloads.push_back(payload);
  }
};

// 检查系数矩阵的秩是否达到 k（GF(2^8) 高斯消元）
bool check_rank(const vector<vector<uint8_t>>& coeffs, int k) {
  if ((int)coeffs.size() < k) return false;

  Galois& gf = Galois::get_instance();
  auto matrix = coeffs;  // 复制，不修改原矩阵
  int pivot = 0;

  for (int col = 0; col < k && pivot < (int)matrix.size(); col++) {
    // 找主元
    int sel = -1;
    for (int i = pivot; i < (int)matrix.size(); i++) {
      if (matrix[i][col] != 0) { sel = i; break; }
    }
    if (sel == -1) continue;

    // 交换行
    swap(matrix[pivot], matrix[sel]);

    // 归一化主元行
    uint8_t inv = gf.div(1, matrix[pivot][col]);
    for (int j = col; j < k; j++) {
      matrix[pivot][j] = gf.mul(matrix[pivot][j], inv);
    }

    // 消元其他行
    for (int i = 0; i < (int)matrix.size(); i++) {
      if (i != pivot && matrix[i][col] != 0) {
        uint8_t f = matrix[i][col];
        for (int j = col; j < k; j++) {
          matrix[i][j] = gf.sub(matrix[i][j], gf.mul(matrix[pivot][j], f));
        }
      }
    }
    pivot++;
  }

  return pivot >= k;
}

class Relay {
public:
  Relay(float redundancy, bool verbose, bool adaptive)
    : redundancy_(redundancy), verbose_(verbose), adaptive_(adaptive) {}

  void run(UDPSocket& up_sock, UDPSocket& down_sock) {
    // up_sock: 连接到 sender
    // down_sock: 监听给 receiver

    pollfd fds[2];
    fds[0].fd = up_sock.fd_num();
    fds[0].events = POLLIN;
    fds[1].fd = down_sock.fd_num();
    fds[1].events = POLLIN;

    uint64_t last_stats_ts = timestamp_us();
    constexpr uint64_t STATS_INTERVAL_US = 1000000; // 1 second

    while (true) {
      int ret = poll(fds, 2, 100); // 100ms timeout for periodic stats
      if (ret < 0) continue;

      // 从 sender 收到数据
      if (fds[0].revents & POLLIN) {
        auto data_opt = up_sock.recv();
        if (data_opt) {
          Datagram pkt;
          if (pkt.parse_from_string(*data_opt)) {
            handle_datagram(pkt, up_sock, down_sock);
          }
        }
      }

      // 从 receiver 收到数据
      if (fds[1].revents & POLLIN) {
        auto [src_addr, data_opt] = down_sock.recvfrom();
        if (data_opt) {
          // 记录 receiver 地址
          if (!receiver_addr_) {
            receiver_addr_ = src_addr;
            cerr << "Receiver: " << src_addr.str() << endl;
          }

          auto msg = Msg::parse_from_string(*data_opt);
          if (!msg) continue;

          if (msg->type == Msg::Type::HOP_ACK) {
            // Handle hop-by-hop ACK from downstream
            handle_hop_ack(dynamic_pointer_cast<HopAckMsg>(msg));
          } else {
            // Forward other messages (ConfigMsg, ACK) to sender
            up_sock.send(*data_opt);

            if (verbose_) {
              if (msg->type == Msg::Type::CONFIG) {
                cerr << "FWD ConfigMsg → Sender" << endl;
              } else if (msg->type == Msg::Type::ACK) {
                cerr << "FWD ACK → Sender" << endl;
              }
            }
          }
        }
      }

      // Periodic stats update for adaptive redundancy
      if (adaptive_) {
        uint64_t now = timestamp_us();
        if (now - last_stats_ts >= STATS_INTERVAL_US) {
          update_redundancy();
          last_stats_ts = now;
        }
      }
    }
  }

private:
  float redundancy_;
  bool verbose_;
  bool adaptive_;
  optional<Address> receiver_addr_;
  map<uint32_t, FrameState> frames_;
  uint16_t next_frag_id_ = 0;

  // Hop-by-hop ACK tracking for adaptive redundancy
  RedundancyController redundancy_ctrl_;
  uint32_t packets_sent_ = 0;      // DATA packets forwarded downstream
  uint32_t hop_acks_received_ = 0; // HOP_ACKs received from downstream
  set<SeqNum> pending_hop_acks_;   // Track which packets await HOP_ACK

  void handle_hop_ack(const shared_ptr<HopAckMsg>& ack) {
    if (!ack) return;

    SeqNum seq = {ack->frame_id, ack->frag_id};
    if (pending_hop_acks_.erase(seq) > 0) {
      hop_acks_received_++;
    }

    if (verbose_) {
      cerr << "HOP_ACK frame=" << ack->frame_id
           << " frag=" << ack->frag_id << endl;
    }
  }

  void update_redundancy() {
    if (packets_sent_ == 0) return;

    float new_redundancy = redundancy_ctrl_.update(packets_sent_, hop_acks_received_);
    
    if (verbose_) {
      cerr << "ADAPTIVE sent=" << packets_sent_
           << " acked=" << hop_acks_received_
           << " loss=" << (100.0 * redundancy_ctrl_.loss_rate())
           << "% R=" << redundancy_ << " → " << new_redundancy << endl;
    }

    redundancy_ = new_redundancy;
    packets_sent_ = 0;
    hop_acks_received_ = 0;
    
    // Clean up old pending acks
    cleanup_pending_acks();
  }

  void cleanup_pending_acks() {
    // Remove entries for frames that are too old
    if (pending_hop_acks_.empty()) return;
    
    auto it = pending_hop_acks_.begin();
    uint32_t max_frame = pending_hop_acks_.rbegin()->first;
    while (it != pending_hop_acks_.end()) {
      if (it->first + 30 < max_frame) {
        it = pending_hop_acks_.erase(it);
      } else {
        ++it;
      }
    }
  }

  void handle_datagram(Datagram& pkt, UDPSocket& up_sock, UDPSocket& down_sock) {
    if (!receiver_addr_) {
      // receiver 还没连接，丢弃
      return;
    }

    auto& state = frames_[pkt.frame_id];

    if (state.k == 0) {
      state.frame_id = pkt.frame_id;
      state.frame_type = pkt.frame_type;
      state.k = pkt.frag_cnt;
      state.padding = pkt.padding;
      state.payload_size = pkt.payload.size();
    }

    // 缓存所有包（DATA + REPAIR），提取系数
    state.add_packet(pkt.payload);

    // 发送 HOP_ACK 给上游（所有包都发）
    HopAckMsg hop_ack(pkt);
    up_sock.send(hop_ack.serialize_to_string());

    // 透传
    down_sock.sendto(*receiver_addr_, pkt.serialize_to_string());

    // Track for adaptive redundancy
    if (adaptive_) {
      packets_sent_++;
      pending_hop_acks_.insert({pkt.frame_id, pkt.frag_id});
    }

    if (verbose_) {
      cerr << "FWD frame=" << pkt.frame_id 
           << " frag=" << pkt.frag_id
           << " " << (pkt.fec_type == FECType::DATA ? "DATA" : "REPAIR")
           << " (rank " << state.coeffs.size() << "/" << state.k << ")" << endl;
    }

    // 秩达到 k 时重编码
    if (!state.recoded && check_rank(state.coeffs, state.k)) {
      int target = static_cast<int>(state.k * redundancy_);
      for (int i = 0; i < target; i++) {
        Datagram coded = recode(state);
        down_sock.sendto(*receiver_addr_, coded.serialize_to_string());

        // Track recoded packets too
        if (adaptive_) {
          packets_sent_++;
          pending_hop_acks_.insert({coded.frame_id, coded.frag_id});
        }
      }
      state.recoded = true;
      if (verbose_ && target > 0) {
        cerr << "RECODE frame=" << state.frame_id << " +" << target << endl;
      }
    }

    cleanup(pkt.frame_id);
  }

  Datagram recode(FrameState& state) {
    Galois& gf = Galois::get_instance();
    int n = state.payloads.size();
    size_t len = state.payload_size;

    // 生成随机系数（确保至少一个非零）
    vector<uint8_t> r(n);
    bool ok = false;
    while (!ok) {
      for (int i = 0; i < n; i++) {
        r[i] = rand() % 256;
        if (r[i]) ok = true;
      }
    }

    // 线性组合所有缓存的 payload
    string out(len, 0);
    for (int i = 0; i < n; i++) {
      if (!r[i]) continue;
      for (size_t j = 0; j < len; j++) {
        out[j] = gf.add((uint8_t)out[j], gf.mul((uint8_t)state.payloads[i][j], r[i]));
      }
    }

    Datagram pkt;
    pkt.frame_id = state.frame_id;
    pkt.frame_type = state.frame_type;
    pkt.fec_type = FECType::REPAIR;
    pkt.frag_id = next_frag_id_++;
    pkt.frag_cnt = state.k;
    pkt.padding = state.padding;
    pkt.send_ts = 0;
    pkt.payload = move(out);
    return pkt;
  }

  void cleanup(uint32_t curr) {
    if (curr < 10) return;
    uint32_t thresh = curr - 10;
    for (auto it = frames_.begin(); it != frames_.end(); ) {
      if (it->first < thresh) it = frames_.erase(it);
      else ++it;
    }
  }
};

int main(int argc, char* argv[])
{
  float redundancy = 0.1;
  bool verbose = false;
  bool adaptive = false;

  const option opts[] = {
    {"redundancy", required_argument, nullptr, 'R'},
    {"adaptive",   no_argument,       nullptr, 'A'},
    {"verbose",    no_argument,       nullptr, 'v'},
    {nullptr,      0,                 nullptr,  0 },
  };

  while (true) {
    int c = getopt_long(argc, argv, "R:Av", opts, nullptr);
    if (c == -1) break;
    switch (c) {
      case 'R': redundancy = stof(optarg); break;
      case 'A': adaptive = true; break;
      case 'v': verbose = true; break;
      default: print_usage(argv[0]); return 1;
    }
  }

  if (optind != argc - 3) {
    print_usage(argv[0]);
    return 1;
  }

  const string upstream_host = argv[optind];
  uint16_t upstream_port = narrow_cast<uint16_t>(strict_stoi(argv[optind + 1]));
  uint16_t listen_port = narrow_cast<uint16_t>(strict_stoi(argv[optind + 2]));

  // 连接到 sender
  Address upstream_addr{upstream_host, upstream_port};
  UDPSocket up_sock;
  up_sock.connect(upstream_addr);
  cerr << "Connected to sender: " << upstream_addr.str() << endl;

  // 监听 receiver
  UDPSocket down_sock;
  down_sock.bind({"0", listen_port});
  cerr << "Listening for receiver on port " << listen_port << endl;
  cerr << "Redundancy: " << redundancy << (adaptive ? " (adaptive)" : " (fixed)") << endl;

  Relay relay(redundancy, verbose, adaptive);
  relay.run(up_sock, down_sock);

  return 0;
}
