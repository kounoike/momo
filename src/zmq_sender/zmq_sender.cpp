#include "zmq_sender.h"

#include <iostream>
#include <third_party/libyuv/include/libyuv/convert_from.h>
#include <third_party/libyuv/include/libyuv/video_common.h>

ZMQSender::ZMQSender() :
  socket_(context_, zmqpp::socket_type::pub)
{
  const zmqpp::endpoint_t endpoint = "tcp://127.0.0.1:5567";
  std::cout << "ZMQSender::ctor" << std::endl;
  socket_.set(zmqpp::socket_option::send_high_water_mark, 1);
  socket_.connect(endpoint);
}

ZMQSender::~ZMQSender() {
  std::cout << "ZMQSender::dtor" << std::endl;
  {
    webrtc::MutexLock lock(&sinks_lock_);
    sinks_.clear();
  }
  {
    webrtc::MutexLock lock(&socket_lock_);
    socket_.close();
  }
}

void ZMQSender::AddTrack(webrtc::VideoTrackInterface* track) {
  const std::string topic(std::string("track/add"));
  zmqpp::message msg;
  msg << topic.c_str() << track->id().c_str();
  SendMessage(msg);

  std::cout << "AddTrack" << std::endl;
  std::unique_ptr<Sink> sink(new Sink(this, track));
  webrtc::MutexLock lock(&sinks_lock_);
  sinks_.push_back(std::make_pair(track, std::move(sink)));
}

void ZMQSender::RemoveTrack(webrtc::VideoTrackInterface* track) {
  const std::string topic(std::string("track/remove"));
  zmqpp::message msg;
  msg << topic.c_str() << track->id().c_str();
  SendMessage(msg);
  std::cout << "RemoveTrack" << std::endl;

  webrtc::MutexLock lock(&sinks_lock_);
  sinks_.erase(
      std::remove_if(sinks_.begin(), sinks_.end(),
                     [track](const VideoTrackSinkVector::value_type& sink) {
                       return sink.first == track;
                     }),
      sinks_.end());
}

zmqpp::context ZMQSender::context_;

void ZMQSender::SendMessage(zmqpp::message& msg) {
  webrtc::MutexLock lock(&socket_lock_);
  socket_.send(msg);
}

ZMQSender::Sink::Sink(ZMQSender* sender,
                        webrtc::VideoTrackInterface* track)
    : sender_(sender),
      track_(track),
      frame_count_(0),
      input_width_(0),
      input_height_(0) {
  std::cout << "Sink ctor: " << track->id() << std::endl;
  track_->AddOrUpdateSink(this, rtc::VideoSinkWants());
}

ZMQSender::Sink::~Sink() {
  track_->RemoveSink(this);
  std::cout << "Sink dtor: " << track_->id() << std::endl;
}

webrtc::Mutex* ZMQSender::Sink::GetMutex() {
  return &frame_params_lock_;
}

void ZMQSender::Sink::OnFrame(const webrtc::VideoFrame& frame) {
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
  zmqpp::message msg;
  msg << topic.c_str() << frame_count_ << input_width_ << input_height_;
  msg.add_raw(image_.get(), input_width_ * input_height_ * 3);
  sender_->SendMessage(msg);
}
