#include <sys/sysinfo.h>
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <algorithm>

#include "decoder.hh"
#include "exception.hh"
#include "conversion.hh"
#include "image.hh"
#include "timestamp.hh"

using namespace std;
using namespace chrono;

Frame::Frame(const uint32_t frame_id,
             const FrameType frame_type,
             const uint16_t frag_cnt,
             const uint16_t repair_cnt,
             const size_t padding_size)
  : id_(frame_id), type_(frame_type), frags_(frag_cnt + repair_cnt),
  frag_need_(frag_cnt), padding_size_(padding_size), frag_cnt_(frag_cnt)
{
  if (frag_cnt == 0) {
    throw runtime_error("frame cannot have zero fragments");
  }
}

optional<size_t> Frame::frame_size() const
{
  if (not complete()) {
    return nullopt;
  }

  return frame_size_;
}

void Frame::validate_datagram(const Datagram & datagram) const
{
  if (datagram.frame_id != id_ or
      datagram.frame_type != type_) {
    throw runtime_error("unable to insert an incompatible datagram");
  }
}

void Frame::insert_frag(Datagram && datagram)
{
  validate_datagram(datagram);

  uint pos = datagram.fec_type == FECType::DATA ?
  datagram.frag_id : datagram.frag_id + datagram.frag_cnt;

  // insert only if the datagram does not exist yet
  if (not frags_[pos]) {
    if (datagram.fec_type == FECType::DATA) {
      frame_size_ += datagram.payload.size();
    }
    frag_need_--;
    frags_[pos] = move(datagram);
  }
}

Decoder::Decoder(const uint16_t display_width,
                 const uint16_t display_height,
                 const int lazy_level,
                 const string & output_path)
  : display_width_(display_width), display_height_(display_height),
    lazy_level_(), output_fd_(), decoder_epoch_(steady_clock::now())
{
  // validate lazy level
  if (lazy_level < DECODE_DISPLAY or lazy_level > NO_DECODE_DISPLAY) {
    throw runtime_error("Invalid lazy level: " + to_string(lazy_level));
  }
  lazy_level_ = static_cast<LazyLevel>(lazy_level);

  // both main and worker threads start from the same time for stats output
  last_stats_time_ = decoder_epoch_;

  // open the output file
  if (not output_path.empty()) {
    output_fd_ = FileDescriptor(check_syscall(
        open(output_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644)));
  }

  // start the worker thread only if we are going to decode or display frames
  if (lazy_level <= DECODE_ONLY) {
    worker_ = thread(&Decoder::worker_main, this);
    cerr << "Spawned a new thread for decoding and displaying frames" << endl;
  }
}

bool Decoder::add_datagram_common(const Datagram & datagram)
{
  const auto frame_id = datagram.frame_id;
  const auto frame_type = datagram.frame_type;
  const auto frag_cnt = datagram.frag_cnt;
  const auto repair_cnt = datagram.repair_cnt;
  const auto padding = datagram.padding;

  // ignore any datagrams from the old frames
  if (frame_id < next_frame_) {
    return false;
  }

  if (not frame_buf_.count(frame_id)) {
    // initialize a Frame instance for frame 'frame_id'
    frame_buf_.emplace(piecewise_construct,
                       forward_as_tuple(frame_id),
                       forward_as_tuple(frame_id, frame_type, frag_cnt,
                                        repair_cnt, padding));
  }

  return true;
}

void Decoder::add_datagram(Datagram && datagram)
{
  if (not add_datagram_common(datagram)) {
    return;
  }

  // move the fragment into the frame
  frame_buf_.at(datagram.frame_id).insert_frag(move(datagram));
}

bool Decoder::next_frame_complete()
{
  {
    // check if the next frame to expect is complete
    auto it = as_const(frame_buf_).find(next_frame_);
    if (it != frame_buf_.end() and it->second.complete()) {
      return true;
    }
  }

  // seek forward if a key frame in the future is already complete
  for (auto it = frame_buf_.rbegin(); it != frame_buf_.rend(); it++) {
    const auto frame_id = it->first;
    const auto & frame = it->second;

    // found a complete key frame ahead of next_frame_
    if (frame.type() == FrameType::KEY and frame.complete()) {
      assert(frame_id > next_frame_);

      // set next_frame_ to frame_id and clean up old frames
      const auto frame_diff = frame_id - next_frame_;
      advance_next_frame(frame_diff);

      cerr << "* Recovery: skipped " << frame_diff
           << " frames ahead to key frame " << frame_id << endl;

      return true;
    }
  }

  return false;
}

void Decoder::consume_next_frame()
{
  Frame & frame = frame_buf_.at(next_frame_);
  if (not frame.complete()) {
    throw runtime_error("next frame must be complete before consuming it");
  }

  // found a decodable frame; update (and output) stats
  num_decodable_frames_++;
  const size_t frame_size = frame.frame_size().value();
  total_decodable_frame_size_ += frame_size;

  const auto stats_now = steady_clock::now();
  while (stats_now >= last_stats_time_ + 1s) {
    cerr << "Decodable frames in the last ~1s: "
         << num_decodable_frames_ << endl;

    const double diff_ms = duration<double, milli>(
                           stats_now - last_stats_time_).count();
    if (diff_ms > 0) {
      cerr << "  - Bitrate (kbps): "
           << double_to_string(total_decodable_frame_size_ * 8 / diff_ms)
           << endl;
    }

    // reset stats
    num_decodable_frames_ = 0;
    total_decodable_frame_size_ = 0;
    last_stats_time_ += 1s;
  }

  // push repaired fragments to the repaired queue
  for (int i = 0; i < frame.frag_cnt(); i++) {
    // access frags before its moved to the worker thread
    if (!frame.frags()[i]) {
      repaired_.emplace_back(frame.id(), i);
    }
  }

  if (lazy_level_ <= DECODE_ONLY) {
    // dispatch the frame to worker thread
    {
      lock_guard<mutex> lock(mtx_);
      shared_queue_.emplace_back(move(frame));
    } // release the lock before notifying the worker thread

    // notify worker thread
    cv_.notify_one();
  } else {
    // main thread outputs frame information if no worker thread
    if (output_fd_) {
      const auto frame_decodable_ts = timestamp_us();

      output_fd_->write(to_string(next_frame_) + "," +
                        to_string(frame_size) + "," +
                        to_string(frame_decodable_ts) + "\n");
    }
  }

  // move onto the next frame
  advance_next_frame();
}

void Decoder::advance_next_frame(const unsigned int n)
{
  next_frame_ += n;

  // clean up state up to next_frame_
  clean_up_to(next_frame_);
}

void Decoder::clean_up_to(const uint32_t frontier)
{
  for (auto it = frame_buf_.begin(); it != frame_buf_.end(); ) {
    if (it->first >= frontier) {
      break;
    }

    it = frame_buf_.erase(it);
  }
}

vector<pair<uint32_t, uint16_t>> Decoder::get_repaired()
{
  auto temp = move(repaired_);
  repaired_.clear();
  return temp;
}

double Decoder::decode_frame(AVCodecContext* codec_ctx, const Frame & frame)
{
  if (not frame.complete()) {
    throw runtime_error("frame must be complete before decoding");
  }

  // fec decode
  vector<optional<FECDatagram>> datagrams;
  for (const auto & datagram : frame.frags()) {
    if (datagram) {
      datagrams.emplace_back(make_optional<FECDatagram>(
        datagram->frame_id, datagram->fec_type, datagram->frag_id,
        datagram->frag_cnt, datagram->repair_cnt, datagram->padding,
        datagram->payload
      ));
    } else {
      datagrams.emplace_back(nullopt);
    }
  }
  vector<uint8_t> decode_buf = fec_.decode(datagrams);

  // decode the compressed frame in 'decode_buf'
  const auto decode_start = steady_clock::now();
  
  AVPacket* packet = av_packet_alloc();
  if (!packet) {
    throw runtime_error("Could not allocate packet for decoding");
  }

  packet->data = decode_buf.data();
  packet->size = decode_buf.size();

  int ret = avcodec_send_packet(codec_ctx, packet);
  if (ret < 0) {
    av_packet_free(&packet);
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
    throw runtime_error("Error sending packet to decoder: " + string(errbuf));
  }

  av_packet_free(&packet);
  
  const auto decode_end = steady_clock::now();
  return duration<double, milli>(decode_end - decode_start).count();
}

void Decoder::display_decoded_frame(AVCodecContext* codec_ctx, AVFrame* frame, VideoDisplay & display)
{
  // Receive decoded frame
  int ret = avcodec_receive_frame(codec_ctx, frame);
  if (ret == 0) {
    // Successfully received a frame, display it
    display.show_frame(RawImage(frame));
  }
  // If ret == AVERROR(EAGAIN), no frame available yet, which is normal
}

void Decoder::worker_main()
{
  // worker does nothing if not decode or display
  if (lazy_level_ == NO_DECODE_DISPLAY) {
    return;
  }

  // initialize H265 decoding context
  const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H265);
  if (!codec) {
    throw runtime_error("H265 decoder not found");
  }

  AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
  if (!codec_ctx) {
    throw runtime_error("Could not allocate video codec context");
  }

  // Set decoder parameters
  codec_ctx->width = display_width_;
  codec_ctx->height = display_height_;
  codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;

  int ret = avcodec_open2(codec_ctx, codec, nullptr);
  if (ret < 0) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
    avcodec_free_context(&codec_ctx);
    throw runtime_error("Could not open codec: " + string(errbuf));
  }

  // Allocate frame for decoded data
  AVFrame* frame = av_frame_alloc();
  if (!frame) {
    avcodec_free_context(&codec_ctx);
    throw runtime_error("Could not allocate video frame");
  }

  // Allocate packet for input data
  AVPacket* packet = av_packet_alloc();
  if (!packet) {
    av_frame_free(&frame);
    avcodec_free_context(&codec_ctx);
    throw runtime_error("Could not allocate packet");
  }

  cerr << "[worker] Initialized H265 decoder (resolution: "
       << display_width_ << "x" << display_height_ << ")" << endl;

  // video display
  unique_ptr<VideoDisplay> display;
  if (lazy_level_ == DECODE_DISPLAY) {
    display = make_unique<VideoDisplay>(display_width_, display_height_);
  }

  // local queue of frames
  deque<Frame> local_queue;

  // stats maintained by the worker thread
  unsigned int num_decoded_frames = 0;
  double total_decode_time_ms = 0.0;
  double max_decode_time_ms = 0.0;
  auto last_stats_time = decoder_epoch_;

  while (true) {
    // destroy display if it has been signalled to quit
    if (display and display->signal_quit()) {
      display.reset(nullptr);
    }

    {
      // wait until the shared queue is not empty
      unique_lock<mutex> lock(mtx_);
      cv_.wait(lock, [this] { return not shared_queue_.empty(); });

      // worker owns the lock after wait and should copy shared queue quickly
      while (not shared_queue_.empty()) {
        local_queue.emplace_back(move(shared_queue_.front()));
        shared_queue_.pop_front();
      }
    } // worker releases the lock so it doesn't block the main thread anymore

    // now worker can take its time to decode and render the frames kept locally
    while (not local_queue.empty()) {
      const Frame & frame_data = local_queue.front();
      const double decode_time_ms = decode_frame(codec_ctx, frame_data);

      if (output_fd_) {
        const auto frame_decoded_ts = timestamp_us();

        output_fd_->write(to_string(frame_data.id()) + "," +
                          to_string(frame_data.frame_size().value()) + "," +
                          to_string(frame_decoded_ts) + "\n");
      }

      if (display) {
        display_decoded_frame(codec_ctx, frame, *display);
      }

      local_queue.pop_front();

      // update stats
      num_decoded_frames++;
      total_decode_time_ms += decode_time_ms;
      max_decode_time_ms = max(max_decode_time_ms, decode_time_ms);

      // worker thread also outputs stats roughly every second
      const auto stats_now = steady_clock::now();
      while (stats_now >= last_stats_time + 1s) {
        if (num_decoded_frames > 0) {
          cerr << "[worker] Avg/Max decoding time (ms) of "
               << num_decoded_frames << " frames: "
               << double_to_string(total_decode_time_ms / num_decoded_frames)
               << "/" << double_to_string(max_decode_time_ms) << endl;
        }

        // reset stats
        num_decoded_frames = 0;
        total_decode_time_ms = 0.0;
        max_decode_time_ms = 0.0;
        last_stats_time += 1s;
      }
    }
  }

  // Cleanup
  av_packet_free(&packet);
  av_frame_free(&frame);
  avcodec_free_context(&codec_ctx);
}
