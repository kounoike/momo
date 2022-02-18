#include "ndi_publisher.h"

#include <vector>

#include <api/video/i420_buffer.h>
#include <rtc_base/logging.h>
#include <third_party/libyuv/include/libyuv/convert_from.h>
#include <third_party/libyuv/include/libyuv/video_common.h>

NDIPublisher::NDIPublisher() {
  //
}

NDIPublisher::~NDIPublisher() {
  //
}

void NDIPublisher::AddTrack(webrtc::VideoTrackInterface* track) {
  std::unique_ptr<Sink> sink(new Sink(track));
  webrtc::MutexLock lock(&sinks_lock_);
  sinks_.push_back(std::make_pair(track, std::move(sink)));
}

void NDIPublisher::RemoveTrack(webrtc::VideoTrackInterface* track) {
  webrtc::MutexLock lock(&sinks_lock_);
  sinks_.erase(
      std::remove_if(sinks_.begin(), sinks_.end(),
                     [track](const VideoTrackSinkVector::value_type& sink) {
                       return sink.first == track;
                     }),
      sinks_.end());
}

NDIPublisher::Sink::Sink(webrtc::VideoTrackInterface* track) : track_(track) {
  //
  NDIlib_send_create_t NDI_send_create_desc;
  NDI_send_create_desc.p_ndi_name = track->id().c_str();
  pNDI_send_ = NDIlib_send_create(&NDI_send_create_desc);
  RTC_LOG(LS_WARNING) << "pNDI_send_ created for " << track->id() << " : "
                      << (pNDI_send_ ? "NOT NULL" : "NULL");
  track_->AddOrUpdateSink(this, rtc::VideoSinkWants());
}

NDIPublisher::Sink::~Sink() {
  NDIlib_send_destroy(pNDI_send_);
  track_->RemoveSink(this);
}

void NDIPublisher::Sink::OnFrame(const webrtc::VideoFrame& frame) {
  //
  auto buffer_if = frame.video_frame_buffer()->ToI420();
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
  // RTC_LOG(LS_WARNING) << "OnFrame done.";
}
