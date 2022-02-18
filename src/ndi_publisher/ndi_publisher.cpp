#include "ndi_publisher.h"

#include <vector>

#include <api/video/i420_buffer.h>
#include <rtc_base/logging.h>
#include <third_party/libyuv/include/libyuv/convert_from.h>
#include <third_party/libyuv/include/libyuv/video_common.h>

void NDIPublisher::AddStream(webrtc::MediaStreamInterface* stream) {
  RTC_LOG(LS_WARNING) << "AddStream " << stream->id();
}
void NDIPublisher::RemoveStream(webrtc::MediaStreamInterface* stream) {
  RTC_LOG(LS_WARNING) << "RemoveStream " << stream->id();
  auto send = ndi_sends_[stream->id()];
  ndi_sends_.erase(stream->id());
  NDIlib_send_destroy(send);
}

NDIlib_send_instance_t NDIPublisher::GetSendInstance(
    webrtc::MediaStreamInterface* stream) {
  auto it = ndi_sends_.find(stream->id());
  if (it == ndi_sends_.end()) {
    NDIlib_send_create_t NDI_send_create_desc;
    NDI_send_create_desc.p_ndi_name = stream->id().c_str();
    auto send = NDIlib_send_create(&NDI_send_create_desc);
    ndi_sends_.insert(std::make_pair(stream->id(), send));
  }
  return ndi_sends_[stream->id()];
}

void NDIPublisher::VideoReceiver::AddTrack(
    webrtc::VideoTrackInterface* track,
    webrtc::MediaStreamInterface* stream) {
  RTC_LOG(LS_WARNING) << "AddTrack(video) " << stream->id() << " "
                      << track->id();
  auto send = publisher_->GetSendInstance(stream);
  std::unique_ptr<Sink> sink(new Sink(track, send));
  webrtc::MutexLock lock(&sinks_lock_);
  sinks_.push_back(std::make_pair(track, std::move(sink)));
}

void NDIPublisher::VideoReceiver::RemoveTrack(
    webrtc::VideoTrackInterface* track) {
  RTC_LOG(LS_WARNING) << "RemoveTrack(video) " << track->id();
  webrtc::MutexLock lock(&sinks_lock_);
  sinks_.erase(
      std::remove_if(sinks_.begin(), sinks_.end(),
                     [track](const VideoTrackSinkVector::value_type& sink) {
                       return sink.first == track;
                     }),
      sinks_.end());
}

NDIPublisher::VideoReceiver::Sink::Sink(webrtc::VideoTrackInterface* track,
                                        NDIlib_send_instance_t send)
    : track_(track), pNDI_send_(send) {
  //
  track_->AddOrUpdateSink(this, rtc::VideoSinkWants());
}

NDIPublisher::VideoReceiver::Sink::~Sink() {
  track_->RemoveSink(this);
}

void NDIPublisher::VideoReceiver::Sink::OnFrame(
    const webrtc::VideoFrame& frame) {
  //
  rtc::scoped_refptr<webrtc::I420BufferInterface> buffer_if =
      frame.video_frame_buffer()->ToI420();
  // RTC_LOG(LS_WARNING) << "OnFrame type: " << frame.video_frame_buffer()->type()
  //                     << ": " << frame.width() << "x" << frame.height();
  // RTC_LOG(LS_WARNING) << "Original Buffer DataY: "
  //                     << frame.video_frame_buffer()->ToI420() << " DataU: "
  //                     << frame.video_frame_buffer()->GetI420()->DataY() +
  //                            frame.width() * frame.height()
  //                     << " / " << frame.video_frame_buffer()->GetI420()->DataU()
  //                     << " DataV: "
  //                     << frame.video_frame_buffer()->GetI420()->DataV();
  // RTC_LOG(LS_WARNING) << "ToI420 Buffer DataY: " << buffer_if->DataY()
  //                     << " StrideY: " << buffer_if->StrideY() << " DataU: "
  //                     << buffer_if->DataY() + frame.width() * frame.height()
  //                     << " / " << buffer_if->DataU()
  //                     << " StrideU: " << buffer_if->StrideU()
  //                     << " DataV: " << buffer_if->DataV();
  NDIlib_video_frame_v2_t ndi_frame;
  std::vector<uint8_t> buffer(frame.width() * frame.height() * 3 / 2);

  // 色変換してないが詰め込みなおしてくれることを期待して・・・
  libyuv::ConvertFromI420(buffer_if->DataY(), buffer_if->StrideY(),
                          buffer_if->DataU(), buffer_if->StrideU(),
                          buffer_if->DataV(), buffer_if->StrideV(), &buffer[0],
                          buffer_if->width(), buffer_if->width(),
                          buffer_if->height(), libyuv::FOURCC_I420);

  ndi_frame.xres = frame.width();
  ndi_frame.yres = frame.height();
  ndi_frame.FourCC = NDIlib_FourCC_video_type_I420;

  ndi_frame.p_data = &buffer[0];
  NDIlib_send_send_video_v2(pNDI_send_, &ndi_frame);
  // RTC_LOG(LS_WARNING) << "OnFrame done." << (pNDI_send_ ? "NOT NULL" : "NULL");
}

void NDIPublisher::AudioReceiver::AddTrack(
    webrtc::AudioTrackInterface* track,
    webrtc::MediaStreamInterface* stream) {
  RTC_LOG(LS_WARNING) << "AddTrack(audio) " << stream->id() << " "
                      << track->id();
  track->set_enabled(false);
  auto send = publisher_->GetSendInstance(stream);
  std::unique_ptr<Sink> sink(new Sink(track, send));
  webrtc::MutexLock lock(&sinks_lock_);
  sinks_.push_back(std::make_pair(track, std::move(sink)));
}

void NDIPublisher::AudioReceiver::RemoveTrack(
    webrtc::AudioTrackInterface* track) {
  RTC_LOG(LS_WARNING) << "RemoveTrack(video) " << track->id();
  webrtc::MutexLock lock(&sinks_lock_);
  sinks_.erase(
      std::remove_if(sinks_.begin(), sinks_.end(),
                     [track](const AudioTrackSinkVector::value_type& sink) {
                       return sink.first == track;
                     }),
      sinks_.end());
}

NDIPublisher::AudioReceiver::Sink::Sink(webrtc::AudioTrackInterface* track,
                                        NDIlib_send_instance_t send)
    : track_(track), pNDI_send_(send) {
  //
  track_->AddSink(this);
}

NDIPublisher::AudioReceiver::Sink::~Sink() {
  track_->RemoveSink(this);
}

// void NDIPublisher::AudioReceiver::Sink::OnData(
//     const webrtc::AudioSinkInterface::Data& audio) {
//   RTC_LOG(LS_WARNING) << "OnData called" << audio.;
// }
void NDIPublisher::AudioReceiver::Sink::OnData(
    const void* audio_data,
    int bits_per_sample,
    int sample_rate,
    size_t number_of_channels,
    size_t number_of_frames,
    absl::optional<int64_t> absolute_capture_timestamp_ms) {
  // RTC_LOG(LS_WARNING) << "bits_per_sample:" << bits_per_sample
  //                     << " sample_rate:" << sample_rate
  //                     << " number_of_channels:" << number_of_channels
  //                     << " number_of_frames:" << number_of_frames;
  NDIlib_audio_frame_interleaved_16s_t ndi_audio_frame;
  ndi_audio_frame.no_channels = number_of_channels;
  ndi_audio_frame.no_samples = number_of_frames;
  ndi_audio_frame.sample_rate = sample_rate;
  // if (absolute_capture_timestamp_ms.has_value()) {
  //   ndi_audio_frame.timestamp = absolute_capture_timestamp_ms.value();
  // }
  ndi_audio_frame.p_data =
      const_cast<int16_t*>(reinterpret_cast<const int16_t*>(audio_data));
  NDIlib_util_send_send_audio_interleaved_16s(pNDI_send_, &ndi_audio_frame);
  // RTC_LOG(LS_WARNING) << "Audio send done.";
}
