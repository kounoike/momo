#include "nng_sender.h"

#include <iostream>
#include <third_party/libyuv/include/libyuv/convert_from.h>
#include <third_party/libyuv/include/libyuv/video_common.h>
#include <nngpp/protocol/pub0.h>

NNGSender::NNGSender() :
  socket_(nng::pub::v0::open())
{
//   const zmqpp::endpoint_t endpoint = "tcp://127.0.0.1:5567";
  socket_.dial("tcp://127.0.0.1:5567", nng::flag::nonblock);
  std::cout << "NNGSender::ctor" << std::endl;
//   socket_.set(zmqpp::socket_option::send_high_water_mark, 1);
//   socket_.connect(endpoint);
}

NNGSender::~NNGSender() {
  std::cout << "NNGSender::dtor" << std::endl;
  webrtc::MutexLock lock(&sinks_lock_);
  sinks_.clear();
}

void NNGSender::AddTrack(webrtc::VideoTrackInterface* track) {
  const std::string msg(std::string("track/add/") + track->id());
  SendStringMessage(msg);

  std::cout << "AddTrack" << std::endl;
  std::unique_ptr<Sink> sink(new Sink(this, track));
  webrtc::MutexLock lock(&sinks_lock_);
  sinks_.push_back(std::make_pair(track, std::move(sink)));
}

void NNGSender::RemoveTrack(webrtc::VideoTrackInterface* track) {
  const std::string msg(std::string("track/remove/") + track->id());
  SendStringMessage(msg);
  std::cout << "RemoveTrack" << std::endl;

  webrtc::MutexLock lock(&sinks_lock_);
  sinks_.erase(
      std::remove_if(sinks_.begin(), sinks_.end(),
                     [track](const VideoTrackSinkVector::value_type& sink) {
                       return sink.first == track;
                     }),
      sinks_.end());
}

void NNGSender::SendStringMessage(const std::string& msg) {
  auto buf = nng::make_buffer(msg.size());
  memcpy(buf.data(), msg.data(), msg.size());
  webrtc::MutexLock lock(&socket_lock_);
  socket_.send(std::move(buf));
}

void NNGSender::SendFrameMessage(const std::string& id, uint32_t frame_count, uint32_t width, uint32_t height, const uint8_t* image_ptr) {
  std::string topic_header("frame/" + id + "/");
  size_t sz = topic_header.size() + sizeof(int32_t) * 3 + width * height * 3;
  uint8_t buf[sz];
  size_t pos = 0;
  memcpy(&buf[pos], topic_header.data(), topic_header.size());
  pos += topic_header.size();
  //愚直に詰め込み・・・
  buf[pos++] = static_cast<uint8_t>(frame_count >> 24);
  buf[pos++] = static_cast<uint8_t>(frame_count >> 16);
  buf[pos++] = static_cast<uint8_t>(frame_count >> 8);
  buf[pos++] = static_cast<uint8_t>(frame_count);
  buf[pos++] = static_cast<uint8_t>(width >> 24);
  buf[pos++] = static_cast<uint8_t>(width >> 16);
  buf[pos++] = static_cast<uint8_t>(width >> 8);
  buf[pos++] = static_cast<uint8_t>(width);
  buf[pos++] = static_cast<uint8_t>(height >> 24);
  buf[pos++] = static_cast<uint8_t>(height >> 16);
  buf[pos++] = static_cast<uint8_t>(height >> 8);
  buf[pos++] = static_cast<uint8_t>(height);
  memcpy(&buf[pos], image_ptr, width * height * 3);

  auto nngbuf = nng::make_buffer(sz);
  memcpy(nngbuf.data(), buf, sz);
  webrtc::MutexLock lock(&socket_lock_);
  socket_.send(std::move(nngbuf));
}

NNGSender::Sink::Sink(NNGSender* sender,
                        webrtc::VideoTrackInterface* track)
    : sender_(sender),
      track_(track),
      frame_count_(0),
      input_width_(0),
      input_height_(0) {
  std::cout << "Sink ctor: " << track->id() << std::endl;
  track_->AddOrUpdateSink(this, rtc::VideoSinkWants());
}

NNGSender::Sink::~Sink() {
  track_->RemoveSink(this);
  std::cout << "Sink dtor: " << track_->id() << std::endl;
}

webrtc::Mutex* NNGSender::Sink::GetMutex() {
  return &frame_params_lock_;
}

void NNGSender::Sink::OnFrame(const webrtc::VideoFrame& frame) {
  if (frame.width() == 0 || frame.height() == 0)
    return;
  webrtc::MutexLock lock(GetMutex());
  if (frame.width() != input_width_ || frame.height() != input_height_) {
    input_width_ = frame.width();
    input_height_ = frame.height();
    image_.reset(new uint8_t[input_width_ * input_height_ * 3]);
  }
  auto i420 = frame.video_frame_buffer()->ToI420();

  libyuv::ConvertFromI420(
    i420->DataY(), i420->StrideY(), i420->DataU(),
    i420->StrideU(), i420->DataV(), i420->StrideV(),
    image_.get(), input_width_ * 3, input_width_,
    input_height_, libyuv::FOURCC_RAW);

  if (frame_count_++ % 100 == 0) {
    std::cout << "OnFrame: " << frame.width() << "x" << frame.height() << " " 
      <<  static_cast<int>(frame.video_frame_buffer()->type())
      << " Chroma: " << i420->ChromaWidth() << "x" << i420->ChromaHeight()
      << " Stride: " << i420->StrideY() << ", " << i420->StrideU() << ", " << i420->StrideV() 
      << " count: " << frame_count_ << std::endl;
  }
  const std::string topic(std::string("frame/") + track_->id());
//   zmqpp::message msg;
//   msg << topic.c_str() << frame_count_ << input_width_ << input_height_;
//   msg.add_raw(image_.get(), input_width_ * input_height_ * 3);
//   sender_->SendMessage(msg);
  sender_->SendFrameMessage(track_->id(), frame_count_, input_width_, input_height_, &image_[0]);
}