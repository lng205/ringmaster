#include <sys/sysinfo.h>
#include <cassert>
#include <iostream>
#include <string>
#include <stdexcept>
#include <chrono>
#include <algorithm>
#include <limits>

#include "encoder.hh"
#include "conversion.hh"
#include "timestamp.hh"

using namespace std;
using namespace chrono;

Encoder::Encoder(const uint16_t display_width,
                 const uint16_t display_height,
                 const uint16_t frame_rate,
                 const string & output_path)
  : display_width_(display_width), display_height_(display_height),
    frame_rate_(frame_rate), output_fd_(), fec_(Datagram::max_payload, 1),
    codec_(nullptr), codec_ctx_(nullptr), frame_(nullptr), packet_(nullptr), sws_ctx_(nullptr)
{
  // open the output file
  if (not output_path.empty()) {
    output_fd_ = FileDescriptor(check_syscall(
        open(output_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644)));
  }

  // Find H265 encoder
  codec_ = avcodec_find_encoder(AV_CODEC_ID_H265);
  if (!codec_) {
    throw runtime_error("H265 encoder not found");
  }

  // Allocate codec context
  codec_ctx_ = avcodec_alloc_context3(codec_);
  if (!codec_ctx_) {
    throw runtime_error("Could not allocate video codec context");
  }

  // Set encoder parameters
  codec_ctx_->bit_rate = target_bitrate_ * 1000; // convert kbps to bps
  codec_ctx_->width = display_width_;
  codec_ctx_->height = display_height_;
  codec_ctx_->time_base = {1, static_cast<int>(frame_rate_)};
  codec_ctx_->framerate = {static_cast<int>(frame_rate_), 1};
  codec_ctx_->gop_size = numeric_limits<int>::max(); // disable keyframes for now
  codec_ctx_->max_b_frames = 0; // disable B-frames for low latency
  codec_ctx_->pix_fmt = AV_PIX_FMT_YUV420P;

  // Set H265 specific options for real-time encoding
  av_opt_set(codec_ctx_->priv_data, "preset", "ultrafast", 0);
  av_opt_set(codec_ctx_->priv_data, "tune", "zerolatency", 0);
  av_opt_set(codec_ctx_->priv_data, "crf", "23", 0); // constant rate factor

  // Open codec
  int ret = avcodec_open2(codec_ctx_, codec_, nullptr);
  if (ret < 0) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
    throw runtime_error("Could not open codec: " + string(errbuf));
  }

  // Allocate frame
  frame_ = av_frame_alloc();
  if (!frame_) {
    throw runtime_error("Could not allocate video frame");
  }
  frame_->format = codec_ctx_->pix_fmt;
  frame_->width = codec_ctx_->width;
  frame_->height = codec_ctx_->height;

  ret = av_frame_get_buffer(frame_, 0);
  if (ret < 0) {
    throw runtime_error("Could not allocate the video frame data");
  }

  // Allocate packet
  packet_ = av_packet_alloc();
  if (!packet_) {
    throw runtime_error("Could not allocate packet");
  }

  // Initialize software scaler for YUV420P conversion
  sws_ctx_ = sws_getContext(display_width_, display_height_, AV_PIX_FMT_YUV420P,
                           display_width_, display_height_, AV_PIX_FMT_YUV420P,
                           SWS_BILINEAR, nullptr, nullptr, nullptr);
  if (!sws_ctx_) {
    throw runtime_error("Could not initialize the conversion context");
  }

  cerr << "Initialized H265 encoder (resolution: " << display_width_ 
       << "x" << display_height_ << ", fps: " << frame_rate_ << ")" << endl;
}

Encoder::~Encoder()
{
  if (packet_) {
    av_packet_free(&packet_);
  }
  if (frame_) {
    av_frame_free(&frame_);
  }
  if (codec_ctx_) {
    avcodec_free_context(&codec_ctx_);
  }
  if (sws_ctx_) {
    sws_freeContext(sws_ctx_);
  }
}

void Encoder::compress_frame(const RawImage & raw_img)
{
  const auto frame_generation_ts = timestamp_us();

  // encode raw_img into frame 'frame_id_'
  encode_frame(raw_img);

  // packetize frame 'frame_id_' into datagrams
  const size_t frame_size = packetize_encoded_frame();

  // output frame information
  if (output_fd_) {
    const auto frame_encoded_ts = timestamp_us();

    output_fd_->write(to_string(frame_id_) + "," +
                      to_string(target_bitrate_) + "," +
                      to_string(frame_size) + "," +
                      to_string(frame_generation_ts) + "," +
                      to_string(frame_encoded_ts) + "\n");
  }

  // move onto the next frame
  frame_id_++;
}

void Encoder::encode_frame(const RawImage & raw_img)
{
  if (raw_img.display_width() != display_width_ or
      raw_img.display_height() != display_height_) {
    throw runtime_error("Encoder: image dimensions don't match");
  }

  // check if a key frame needs to be encoded
  bool force_keyframe = false;
  if (not unacked_.empty()) {
    const auto & first_unacked = unacked_.cbegin()->second;

    // give up if first unacked datagram was initially sent MAX_UNACKED_US ago
    const auto us_since_first_send = timestamp_us() - first_unacked.send_ts;

    if (us_since_first_send > MAX_UNACKED_US) {
      force_keyframe = true;

      cerr << "* Recovery: gave up retransmissions and forced a key frame "
           << frame_id_ << endl;

      if (verbose_) {
        cerr << "Giving up on lost datagram: frame_id="
             << first_unacked.frame_id << " frag_id=" << first_unacked.frag_id
             << " rtx=" << first_unacked.num_rtx
             << " us_since_first_send=" << us_since_first_send << endl;
      }

      // clean up
      send_buf_.clear();
      unacked_.clear();
    }
  }

  // Copy raw image data to AVFrame
  const auto& y_plane = raw_img.y();
  const auto& u_plane = raw_img.u();
  const auto& v_plane = raw_img.v();

  // Make sure frame is writable
  int ret = av_frame_make_writable(frame_);
  if (ret < 0) {
    throw runtime_error("Could not make frame writable");
  }

  // Copy Y plane
  for (int y = 0; y < display_height_; y++) {
    memcpy(frame_->data[0] + y * frame_->linesize[0],
           y_plane.data() + y * display_width_,
           display_width_);
  }

  // Copy U plane  
  for (int y = 0; y < display_height_ / 2; y++) {
    memcpy(frame_->data[1] + y * frame_->linesize[1],
           u_plane.data() + y * (display_width_ / 2),
           display_width_ / 2);
  }

  // Copy V plane
  for (int y = 0; y < display_height_ / 2; y++) {
    memcpy(frame_->data[2] + y * frame_->linesize[2],
           v_plane.data() + y * (display_width_ / 2),
           display_width_ / 2);
  }

  frame_->pts = frame_id_;

  // Force keyframe if needed
  if (force_keyframe) {
    frame_->pict_type = AV_PICTURE_TYPE_I;
  } else {
    frame_->pict_type = AV_PICTURE_TYPE_NONE;
  }

  // encode a frame and calculate encoding time
  const auto encode_start = steady_clock::now();
  
  ret = avcodec_send_frame(codec_ctx_, frame_);
  if (ret < 0) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
    throw runtime_error("Error sending frame for encoding: " + string(errbuf));
  }

  const auto encode_end = steady_clock::now();
  const double encode_time_ms = duration<double, milli>(
                                encode_end - encode_start).count();

  // track stats in the current period
  num_encoded_frames_++;
  total_encode_time_ms_ += encode_time_ms;
  max_encode_time_ms_ = max(max_encode_time_ms_, encode_time_ms);
}

size_t Encoder::packetize_encoded_frame()
{
  size_t frame_size = 0;
  
  // Receive encoded packets from encoder
  while (true) {
    int ret = avcodec_receive_packet(codec_ctx_, packet_);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
      break; // No more packets available or need more input
    } else if (ret < 0) {
      char errbuf[AV_ERROR_MAX_STRING_SIZE];
      av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
      throw runtime_error("Error receiving packet: " + string(errbuf));
    }

    frame_size = packet_->size;
    assert(frame_size > 0);

    // Determine frame type (keyframe or not)
    auto frame_type = FrameType::NONKEY;
    if (packet_->flags & AV_PKT_FLAG_KEY) {
      frame_type = FrameType::KEY;

      if (verbose_) {
        cerr << "Encoded a key frame: frame_id=" << frame_id_ << endl;
      }
    }

    // FEC encoding
    vector<FECDatagram> fec_datagrams = 
      fec_.encode(frame_id_, packet_->data, frame_size);

    for (FECDatagram & datagram : fec_datagrams) {
      send_buf_.emplace_back(frame_id_, frame_type, datagram.fec_type,
        datagram.frag_id, datagram.frag_cnt, datagram.repair_cnt,
        datagram.padding, datagram.payload);
    }

    av_packet_unref(packet_);
    break; // Process one packet at a time
  }

  return frame_size;
}

void Encoder::add_unacked(Datagram && datagram)
{
  if (datagram.fec_type == FECType::REPAIR) return;

  const auto seq_num = make_pair(datagram.frame_id, datagram.frag_id);
  auto [it, success] = unacked_.emplace(seq_num, move(datagram));

  if (not success) {
    throw runtime_error("datagram already exists in unacked");
  }

  it->second.last_send_ts = it->second.send_ts;
}

void Encoder::handle_ack(const shared_ptr<AckMsg> & ack)
{
  const auto curr_ts = timestamp_us();

  // observed an RTT sample
  if (ack->send_ts > 0) {
    add_rtt_sample(curr_ts - ack->send_ts);
  }

  // find the acked datagram in 'unacked_'
  const auto acked_seq_num = make_pair(ack->frame_id, ack->frag_id);
  auto acked_it = unacked_.find(acked_seq_num);

  if (acked_it == unacked_.end()) {
    // do nothing else if ACK is not for an unacked datagram
    return;
  }

  // // retransmit all unacked datagrams before the acked one (backward)
  // for (auto rit = make_reverse_iterator(acked_it);
  //      rit != unacked_.rend(); rit++) {
  //   auto & datagram = rit->second;

  //   // skip if a datagram has been retransmitted MAX_NUM_RTX times
  //   if (datagram.num_rtx >= MAX_NUM_RTX) {
  //     continue;
  //   }

  //   // retransmit if it's the first RTX or the last RTX was about one RTT ago
  //   if (datagram.num_rtx == 0 or
  //       curr_ts - datagram.last_send_ts > ewma_rtt_us_.value()) {
  //     datagram.num_rtx++;
  //     datagram.last_send_ts = curr_ts;

  //     // retransmissions are more urgent
  //     send_buf_.emplace_front(datagram);
  //   }
  // }

  // finally, erase the acked datagram from 'unacked_'
  unacked_.erase(acked_it);
}

void Encoder::add_rtt_sample(const unsigned int rtt_us)
{
  // min RTT
  if (not min_rtt_us_ or rtt_us < *min_rtt_us_) {
    min_rtt_us_ = rtt_us;
  }

  // EWMA RTT
  if (not ewma_rtt_us_) {
    ewma_rtt_us_ = rtt_us;
  } else {
    ewma_rtt_us_ = ALPHA * rtt_us + (1 - ALPHA) * (*ewma_rtt_us_);
  }
}

void Encoder::output_periodic_stats()
{
  cerr << "Frames encoded in the last ~1s: " << num_encoded_frames_ << endl;

  if (num_encoded_frames_ > 0) {
    cerr << "  - Avg/Max encoding time (ms): "
         << double_to_string(total_encode_time_ms_ / num_encoded_frames_)
         << "/" << double_to_string(max_encode_time_ms_) << endl;
  }

  if (min_rtt_us_ and ewma_rtt_us_) {
    cerr << "  - Min/EWMA RTT (ms): " << double_to_string(*min_rtt_us_ / 1000.0)
         << "/" << double_to_string(*ewma_rtt_us_ / 1000.0) << endl;
  }

  // reset all but RTT-related stats
  num_encoded_frames_ = 0;
  total_encode_time_ms_ = 0.0;
  max_encode_time_ms_ = 0.0;
}

void Encoder::set_target_bitrate(const unsigned int bitrate_kbps)
{
  target_bitrate_ = bitrate_kbps;

  // Update the codec context with new bitrate
  codec_ctx_->bit_rate = target_bitrate_ * 1000; // convert kbps to bps
}
