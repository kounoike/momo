#include "nng_sender.h"

#include <iostream>
#include <third_party/libyuv/include/libyuv/convert_from.h>
#include <third_party/libyuv/include/libyuv/video_common.h>
#include <nngpp/protocol/pub0.h>

NNGSender::NNGSender() :
  socket_(nng::pub::v0::open()),
  video_track_receiver_(this),
  audio_track_receiver_(this),
  data_manager_(this)
{
  socket_.dial("tcp://127.0.0.1:5567", nng::flag::nonblock);
  std::cout << "NNGSender::ctor" << std::endl;
}

NNGSender::~NNGSender() {
  std::cout << "NNGSender::dtor" << std::endl;
}

NNGSender::NNGAudioTrackReceiver::NNGAudioTrackReceiver(NNGSender* sender) : sender_(sender) {
  std::cout << "AudioTrackReceiver ctor" << std::endl;
}

NNGSender::NNGAudioTrackReceiver::~NNGAudioTrackReceiver() {
  std::cout << "AudioTrackReceiver dtor" << std::endl;
  webrtc::MutexLock lock(&sinks_lock_);
  sinks_.clear();
}

void NNGSender::NNGAudioTrackReceiver::AddTrack(webrtc::AudioTrackInterface* track, const std::vector<std::string>& stream_ids) {
  if (stream_ids.size() != 1) {
    std::cerr << "stream_ids.size() != 1 : " << stream_ids.size() << std::endl;
    return;
  }
  const std::string msg(std::string("track/audio/add/") + stream_ids[0] + "/" + track->id());
  sender_->SendStringMessage(msg);

  std::cout << "AddTrack" << std::endl;
  std::unique_ptr<Sink> sink(new Sink(sender_, track, stream_ids[0]));
  webrtc::MutexLock lock(&sinks_lock_);
  sinks_.push_back(std::make_pair(track, std::move(sink)));
}

void NNGSender::NNGAudioTrackReceiver::RemoveTrack(webrtc::AudioTrackInterface* track) {
  const std::string msg(std::string("track/audio/remove/") + track->id());
  sender_->SendStringMessage(msg);
  std::cout << "RemoveTrack" << std::endl;

  webrtc::MutexLock lock(&sinks_lock_);
  sinks_.erase(
      std::remove_if(sinks_.begin(), sinks_.end(),
                     [track](const AudioTrackSinkVector::value_type& sink) {
                       return sink.first == track;
                     }),
      sinks_.end());
}

NNGSender::NNGAudioTrackReceiver::Sink::Sink(NNGSender* sender,
                        webrtc::AudioTrackInterface* track, const std::string& stream_id)
    : sender_(sender),
      stream_id_(stream_id),
      track_(track),
      frame_count_(0) {
  int signal_level = 0;
  track_->GetSignalLevel(&signal_level);
  std::cout << "AudioSink ctor: " << stream_id_ << "/" << track->id() << " "
    << track_->enabled() << " "
    << track_->state() << " "
    << signal_level
    << std::endl;
  track_->AddSink(this);
}

NNGSender::NNGAudioTrackReceiver::Sink::~Sink() {
  track_->RemoveSink(this);
  std::cout << "AudioSink dtor: " << stream_id_ << "/" << track_->id() << std::endl;
}

void NNGSender::NNGAudioTrackReceiver::Sink::OnData(const void* audio_data,
            int bits_per_sample,
            int sample_rate,
            size_t number_of_channels,
            size_t number_of_frames) {
  frame_count_ += number_of_frames;
  std::cout << "AudioSink OnData1: " << bits_per_sample << " " << sample_rate << " " << number_of_channels << " " << number_of_frames << std::endl;
}

void NNGSender::NNGAudioTrackReceiver::Sink::OnData(const void* audio_data,
            int bits_per_sample,
            int sample_rate,
            size_t number_of_channels,
            size_t number_of_frames,
            absl::optional<int64_t> absolute_capture_timestamp_ms) {
  frame_count_ += number_of_frames;
  if (frame_count_ >= sample_rate) {
    std::cout << "AudioSink OnData2: " << bits_per_sample << " " << sample_rate << " " << number_of_channels << " " << number_of_frames
      << " " << absolute_capture_timestamp_ms.value_or(-1) << " " << frame_count_
      << std::endl;
    frame_count_ = 0;
  }

  sender_->SendAudioDataMessage(stream_id_, track_->id(), audio_data, bits_per_sample, sample_rate, number_of_channels, number_of_frames);
}

NNGSender::NNGVideoTrackReceiver::NNGVideoTrackReceiver(NNGSender* sender) : sender_(sender) {
}

NNGSender::NNGVideoTrackReceiver::~NNGVideoTrackReceiver() {
  webrtc::MutexLock lock(&sinks_lock_);
  sinks_.clear();
}

void NNGSender::NNGVideoTrackReceiver::AddTrack(webrtc::VideoTrackInterface* track, const std::vector<std::string>& stream_ids) {
  if (stream_ids.size() != 1) {
    std::cerr << "stream_ids.size() != 1 : " << stream_ids.size() << std::endl;
    return;
  }
  const std::string msg(std::string("track/video/add/") + stream_ids[0] + "/" + track->id());
  sender_->SendStringMessage(msg);

  std::cout << "AddTrack: " << stream_ids[0] << "/" << track->id() << " " << track->GetSource()->SupportsEncodedOutput() << std::endl;
  std::unique_ptr<Sink> sink(new Sink(sender_, track, stream_ids[0]));
  std::unique_ptr<EncodedSink> encoded_sink(new EncodedSink(sender_, track, stream_ids[0]));
  webrtc::MutexLock lock(&sinks_lock_);
  sinks_.push_back(std::make_tuple(track, std::move(sink), std::move(encoded_sink)));
}

void NNGSender::NNGVideoTrackReceiver::RemoveTrack(webrtc::VideoTrackInterface* track) {
  const std::string msg(std::string("track/video/remove/") + "/" + track->id());
  sender_->SendStringMessage(msg);
  std::cout << "RemoveTrack" << std::endl;

  webrtc::MutexLock lock(&sinks_lock_);
  sinks_.erase(
      std::remove_if(sinks_.begin(), sinks_.end(),
                     [track](const VideoTrackSinkVector::value_type& sink) {
                       return std::get<0>(sink) == track;
                     }),
      sinks_.end());
}

void NNGSender::SendStringMessage(const std::string& msg) {
  auto buf = nng::make_buffer(msg.size());
  memcpy(buf.data(), msg.data(), msg.size());
  webrtc::MutexLock lock(&socket_lock_);
  socket_.send(std::move(buf));
}

void NNGSender::SendFrameMessage(const std::string& stream_id, const std::string& track_id, uint32_t frame_count, uint32_t width, uint32_t height, const uint8_t* image_ptr) {
  std::string topic_header("frame/" + stream_id + "/" + track_id + "/");
  size_t sz = topic_header.size() + sizeof(int32_t) * 3 + width * height * 3;
  auto nngbuf = nng::make_buffer(sz);
  uint8_t* buf = static_cast<uint8_t*>(nngbuf.data());
  size_t pos = 0;
  memcpy(&buf[pos], topic_header.data(), topic_header.size());
  pos += topic_header.size();
  //愚直に詰め込み・・・
  // フレームのタイムスタンプも送信しても良いかも
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
 
  webrtc::MutexLock lock(&socket_lock_);
  socket_.send(std::move(nngbuf));
}

void NNGSender::SendAudioDataMessage(
  const std::string& stream_id,
  const std::string& track_id,
  const void* audio_data,
  int bits_per_sample,
  int sample_rate,
  size_t number_of_channels,
  size_t number_of_frames) {
  std::string topic_header("audio/" + stream_id + "/" + track_id + "/");
  size_t audio_data_size = number_of_channels * number_of_frames * sample_rate * bits_per_sample / 8;
  size_t sz = topic_header.size() + sizeof(int32_t) * 4 + audio_data_size;
  auto nngbuf = nng::make_buffer(sz);
  uint8_t* buf = static_cast<uint8_t*>(nngbuf.data());
  size_t pos = 0;
  memcpy(&buf[pos], topic_header.data(), topic_header.size());
  pos += topic_header.size();
  //愚直に詰め込み・・・
  buf[pos++] = static_cast<uint8_t>(bits_per_sample >> 24);
  buf[pos++] = static_cast<uint8_t>(bits_per_sample >> 16);
  buf[pos++] = static_cast<uint8_t>(bits_per_sample >> 8);
  buf[pos++] = static_cast<uint8_t>(bits_per_sample);
  buf[pos++] = static_cast<uint8_t>(sample_rate >> 24);
  buf[pos++] = static_cast<uint8_t>(sample_rate >> 16);
  buf[pos++] = static_cast<uint8_t>(sample_rate >> 8);
  buf[pos++] = static_cast<uint8_t>(sample_rate);
  buf[pos++] = static_cast<uint8_t>(number_of_channels >> 24);
  buf[pos++] = static_cast<uint8_t>(number_of_channels >> 16);
  buf[pos++] = static_cast<uint8_t>(number_of_channels >> 8);
  buf[pos++] = static_cast<uint8_t>(number_of_channels);
  buf[pos++] = static_cast<uint8_t>(number_of_frames >> 24);
  buf[pos++] = static_cast<uint8_t>(number_of_frames >> 16);
  buf[pos++] = static_cast<uint8_t>(number_of_frames >> 8);
  buf[pos++] = static_cast<uint8_t>(number_of_frames);
  memcpy(&buf[pos], audio_data, audio_data_size);
 
  webrtc::MutexLock lock(&socket_lock_);
  socket_.send(std::move(nngbuf));
}

void NNGSender::SendEncodedFrameMessage(const std::string& stream_id, const std::string& track_id, const webrtc::RecordableEncodedFrame& frame) {
  std::string topic_header("encodedframe/" + stream_id + "/" + track_id + "/");
  // TODO: codecとかcolorspaceとか色々の詰め込み
  uint32_t data_size = frame.encoded_buffer()->size();
  size_t sz = topic_header.size() + sizeof(uint32_t) + data_size;

  auto nngbuf = nng::make_buffer(sz);
  uint8_t* buf = static_cast<uint8_t*>(nngbuf.data());
  size_t pos = 0;
  memcpy(&buf[pos], topic_header.data(), topic_header.size());
  pos += topic_header.size();
  buf[pos++] = static_cast<uint8_t>(data_size >> 24);
  buf[pos++] = static_cast<uint8_t>(data_size >> 16);
  buf[pos++] = static_cast<uint8_t>(data_size >> 8);
  buf[pos++] = static_cast<uint8_t>(data_size);
  memcpy(&buf[pos], frame.encoded_buffer()->data(), data_size);
 
  webrtc::MutexLock lock(&socket_lock_);
  socket_.send(std::move(nngbuf));
}

void NNGSender::SendDataChannelMessage(int id, const webrtc::DataBuffer& buffer) {
  std::string topic_header("data/message/" + std::to_string(id) + "/");
  uint32_t data_size = buffer.size();
  size_t sz = topic_header.size() + sizeof(uint32_t)  + data_size;

  auto nngbuf = nng::make_buffer(sz);
  uint8_t* buf = static_cast<uint8_t*>(nngbuf.data());
  size_t pos = 0;
  memcpy(&buf[pos], topic_header.data(), topic_header.size());
  pos += topic_header.size();
  buf[pos++] = static_cast<uint8_t>(data_size >> 24);
  buf[pos++] = static_cast<uint8_t>(data_size >> 16);
  buf[pos++] = static_cast<uint8_t>(data_size >> 8);
  buf[pos++] = static_cast<uint8_t>(data_size);
  memcpy(&buf[pos], buffer.data.data<uint8_t>(), data_size);
 
  webrtc::MutexLock lock(&socket_lock_);
  socket_.send(std::move(nngbuf));
}

NNGSender::NNGVideoTrackReceiver::Sink::Sink(NNGSender* sender,
                        webrtc::VideoTrackInterface* track, const std::string& stream_id)
    : sender_(sender),
      stream_id_(stream_id),
      track_(track),
      frame_count_(0),
      input_width_(0),
      input_height_(0) {
  std::cout << "VideoSink ctor: " << stream_id_ << "/" << track->id() << std::endl;
  track_->AddOrUpdateSink(this, rtc::VideoSinkWants());
}

NNGSender::NNGVideoTrackReceiver::Sink::~Sink() {
  track_->RemoveSink(this);
  std::cout << "VideoSink dtor: " << stream_id_ << "/" << track_->id() << std::endl;
}

webrtc::Mutex* NNGSender::NNGVideoTrackReceiver::Sink::GetMutex() {
  return &frame_params_lock_;
}

void NNGSender::NNGVideoTrackReceiver::Sink::OnFrame(const webrtc::VideoFrame& frame) {
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
    std::cout << "OnFrame: " << frame.width() << "x" << frame.height() << " " << input_width_ << "x" << input_height_ << " "
      <<  static_cast<int>(frame.video_frame_buffer()->type())
      << " Chroma: " << i420->ChromaWidth() << "x" << i420->ChromaHeight()
      << " Stride: " << i420->StrideY() << ", " << i420->StrideU() << ", " << i420->StrideV() 
      << " count: " << frame_count_
      << " timestamp: " << frame.timestamp() << " timestamp_us: " << frame.timestamp_us()
      << std::endl;
  }
  sender_->SendFrameMessage(stream_id_, track_->id(), frame_count_, input_width_, input_height_, image_.get());
}

NNGSender::NNGVideoTrackReceiver::EncodedSink::EncodedSink(NNGSender* sender,
                        webrtc::VideoTrackInterface* track, const std::string& stream_id)
    : sender_(sender),
      stream_id_(stream_id),
      track_(track),
      frame_count_(0),
      input_width_(0),
      input_height_(0) {
  std::cout << "EncodedVideoSink ctor: " << stream_id_ << "/" << track->id() << std::endl;
  track_->GetSource()->AddEncodedSink(this);
}

NNGSender::NNGVideoTrackReceiver::EncodedSink::~EncodedSink() {
  track_->GetSource()->RemoveEncodedSink(this);
  std::cout << "EncodedVideoSink dtor: " << stream_id_ << "/" << track_->id() << std::endl;
}

void NNGSender::NNGVideoTrackReceiver::EncodedSink::OnFrame(const webrtc::RecordableEncodedFrame& frame) {
  sender_->SendEncodedFrameMessage(stream_id_, track_->id(), frame);

  const char* codec_names[] = {
    "kVideoCodecGeneric",
    "kVideoCodecVP8",
    "kVideoCodecVP9",
    "kVideoCodecAV1",
    "kVideoCodecH264",
    "kVideoCodecMultiplex",
  };
  if (frame_count_++ % 100 == 0 || frame.is_key_frame()) {
    std::cout << "EncodedVideoSink OnFrame"
      << " resolution: " << frame.resolution().width << "x" << frame.resolution().height
      << " codec: " << codec_names[frame.codec()]
      << " " << (frame.is_key_frame() ? "key frame" : "not key frame")
      << " " << frame.encoded_buffer()->size()
      << std::endl;
  }
}