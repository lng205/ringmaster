#include <cstring>
#include <stdexcept>

#include "image.hh"

using namespace std;

// constructor that allocates and owns the vpx_image
RawImage::RawImage(const uint16_t display_width, const uint16_t display_height)
  : vpx_img_(vpx_img_alloc(nullptr, VPX_IMG_FMT_I420,
                           display_width, display_height, 1)),
    own_vpx_img_(true),
    display_width_(display_width),
    display_height_(display_height)
{}

// constructor with a non-owning pointer to vpx_image
RawImage::RawImage(vpx_image_t * const vpx_img)
  : vpx_img_(vpx_img),
    own_vpx_img_(false),
    display_width_(),
    display_height_()
{
  if (not vpx_img) {
    throw runtime_error("RawImage: unable to construct from a null vpx_img");
  }

  if (vpx_img->fmt != VPX_IMG_FMT_I420) {
    throw runtime_error("RawImage: only supports I420");
  }

  display_width_ = vpx_img->d_w;
  display_height_ = vpx_img->d_h;
}

// constructor with a non-owning pointer to AVFrame
RawImage::RawImage(AVFrame * const av_frame)
  : vpx_img_(nullptr),
    own_vpx_img_(true),
    display_width_(),
    display_height_()
{
  if (not av_frame) {
    throw runtime_error("RawImage: unable to construct from a null AVFrame");
  }

  if (av_frame->format != AV_PIX_FMT_YUV420P) {
    throw runtime_error("RawImage: only supports YUV420P format");
  }

  display_width_ = av_frame->width;
  display_height_ = av_frame->height;

  // Allocate and copy from AVFrame to vpx_image for compatibility
  vpx_img_ = vpx_img_alloc(nullptr, VPX_IMG_FMT_I420, display_width_, display_height_, 1);
  if (!vpx_img_) {
    throw runtime_error("RawImage: failed to allocate vpx_image from AVFrame");
  }

  // Copy Y plane
  for (int y = 0; y < display_height_; y++) {
    memcpy(vpx_img_->planes[VPX_PLANE_Y] + y * vpx_img_->stride[VPX_PLANE_Y],
           av_frame->data[0] + y * av_frame->linesize[0],
           display_width_);
  }

  // Copy U plane
  for (int y = 0; y < display_height_ / 2; y++) {
    memcpy(vpx_img_->planes[VPX_PLANE_U] + y * vpx_img_->stride[VPX_PLANE_U],
           av_frame->data[1] + y * av_frame->linesize[1],
           display_width_ / 2);
  }

  // Copy V plane
  for (int y = 0; y < display_height_ / 2; y++) {
    memcpy(vpx_img_->planes[VPX_PLANE_V] + y * vpx_img_->stride[VPX_PLANE_V],
           av_frame->data[2] + y * av_frame->linesize[2],
           display_width_ / 2);
  }
}

RawImage::~RawImage()
{
  // free vpx_image only if the class owns it
  if (own_vpx_img_) {
    vpx_img_free(vpx_img_);
  }
}

void RawImage::copy_from_yuyv(const string_view src)
{
  // expects YUYV to have size of 2 * W * H
  if (src.size() != y_size() * 2) {
    throw runtime_error("RawImage: invalid YUYV size");
  }

  uint8_t * dst_y = y_plane();
  uint8_t * dst_u = u_plane();
  uint8_t * dst_v = v_plane();

  // copy Y plane
  const uint8_t * p = reinterpret_cast<const uint8_t *>(src.data());
  for (unsigned i = 0; i < y_size(); i++, p += 2) {
    *dst_y++ = *p;
  }

  // copy U and V planes
  p = reinterpret_cast<const uint8_t *>(src.data());
  for (unsigned i = 0; i < display_height_ / 2; i++, p += 2 * display_width_) {
    for (unsigned j = 0; j < display_width_ / 2; j++, p += 4) {
      *dst_u++ = p[1];
      *dst_v++ = p[3];
    }
  }
}

void RawImage::copy_y_from(const string_view src)
{
  if (src.size() != y_size()) {
    throw runtime_error("RawImage: invalid size for Y plane");
  }

  memcpy(y_plane(), src.data(), src.size());
}

void RawImage::copy_u_from(const string_view src)
{
  if (src.size() != uv_size()) {
    throw runtime_error("RawImage: invalid size for U plane");
  }

  memcpy(u_plane(), src.data(), src.size());
}

void RawImage::copy_v_from(const string_view src)
{
  if (src.size() != uv_size()) {
    throw runtime_error("RawImage: invalid size for V plane");
  }

  memcpy(v_plane(), src.data(), src.size());
}

std::vector<uint8_t> RawImage::y() const
{
  std::vector<uint8_t> y_data(y_size());
  uint8_t * src = y_plane();
  for (int row = 0; row < display_height_; row++) {
    memcpy(y_data.data() + row * display_width_,
           src + row * y_stride(),
           display_width_);
  }
  return y_data;
}

std::vector<uint8_t> RawImage::u() const
{
  std::vector<uint8_t> u_data(uv_size());
  uint8_t * src = u_plane();
  for (int row = 0; row < display_height_ / 2; row++) {
    memcpy(u_data.data() + row * (display_width_ / 2),
           src + row * u_stride(),
           display_width_ / 2);
  }
  return u_data;
}

std::vector<uint8_t> RawImage::v() const
{
  std::vector<uint8_t> v_data(uv_size());
  uint8_t * src = v_plane();
  for (int row = 0; row < display_height_ / 2; row++) {
    memcpy(v_data.data() + row * (display_width_ / 2),
           src + row * v_stride(),
           display_width_ / 2);
  }
  return v_data;
}
