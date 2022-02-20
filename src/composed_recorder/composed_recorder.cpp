#include "composed_recorder.h"

#include <cmath>

#include <third_party/libyuv/include/libyuv/scale.h>

#include <media/base/media_constants.h>
#include <rtc_base/ref_counted_object.h>

#include <video/buffer_vpx_encoder.hpp>
#include <video/vpx.hpp>
#include <frame.hpp>
#include <muxer/async_webm_muxer.hpp> // src/config.hppがincludeしにくいので・・・


const int RECORDING_FPS = 25;
const long long EXPECTED_DURATION = 1000000 / RECORDING_FPS;
const int RECORDING_WIDTH = 1920;
const int RECORDING_HEIGHT = 1080;


ComposedRecorder::ComposedRecorder()
    : running_(true),
      recorder_thread_([this]() { this->RecorderThreadFunc(); }) {}

ComposedRecorder::~ComposedRecorder() {
  running_ = false;
  recorder_thread_.join();
}

void ComposedRecorder::RecorderThreadFunc() {
  printf("RecorderThreadFunc start.\n");

  std::queue<hisui::Frame> frame_queue;
  hisui::Config config;
  hisui::video::VPXEncoderConfig vpx_config(RECORDING_WIDTH, RECORDING_HEIGHT, config);
  hisui::video::BufferVPXEncoder video_encoder(&frame_queue, vpx_config);

  hisui::webm::output::Context context("composed.webm");
  context.init();
  context.setVideoTrack(RECORDING_WIDTH, RECORDING_HEIGHT, video_encoder.getFourcc());

  while (running_) {
    if (video_recorder_.sinks_.empty() ||
        video_recorder_.sinks_[0].second->current_buffer_ == nullptr) {
      continue;
    }
    printf("RecorderThreadFunc loop start ok.\n");
    auto start = std::chrono::high_resolution_clock::now();

    std::vector<rtc::scoped_refptr<webrtc::VideoFrameBuffer>> buffers;

    {
      webrtc::MutexLock lock(&video_recorder_.sinks_lock_);
      for (auto& sink : video_recorder_.sinks_) {
        buffers.push_back(sink.second->current_buffer_);
      }
    }
    int separate_num = static_cast<int>(std::ceil(std::sqrt(buffers.size())));
    int separate_width = RECORDING_WIDTH / separate_num;
    int separate_height = RECORDING_HEIGHT / separate_num;

    std::vector<uint8_t> canvas(RECORDING_WIDTH * RECORDING_HEIGHT * 3 / 2);

    for (int idx = 0; idx < buffers.size(); idx++) {
      if (!buffers[idx]) {
        continue;
      }
      int row = idx / separate_num;
      int col = idx % separate_num;
      auto buffer = buffers[idx]->ToI420();

      size_t dst_y_idx = 0;
      uint8_t* dst_y = &canvas[dst_y_idx];
      int dst_stride_y = RECORDING_WIDTH;

      size_t dst_u_idx = RECORDING_WIDTH * RECORDING_HEIGHT;
      uint8_t* dst_u = &canvas[dst_u_idx];
      int dst_stride_u = RECORDING_WIDTH / 2;

      size_t dst_v_idx = RECORDING_WIDTH * RECORDING_HEIGHT * 5 / 4;
      uint8_t* dst_v = &canvas[dst_v_idx];
      int dst_stride_v = RECORDING_WIDTH / 2;

      libyuv::I420Scale(buffer->DataY(), buffer->StrideY(), buffer->DataU(),
                        buffer->StrideU(), buffer->DataV(), buffer->StrideV(),
                        buffer->width(), buffer->height(), dst_y, dst_stride_y,
                        dst_u, dst_stride_u, dst_v, dst_stride_v,
                        separate_width, separate_height,
                        libyuv::FilterMode::kFilterBilinear);
    }
    printf("RecorderThreadFunc Compose ok.\n");

    video_encoder.outputImage(canvas);
    while(!frame_queue.empty()) {
      auto &frame = frame_queue.front();
      context.addVideoFrame(frame.data, frame.data_size, frame.timestamp, frame.is_key);
      frame_queue.pop();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start)
            .count();
    std::this_thread::sleep_for(
        std::chrono::microseconds(EXPECTED_DURATION - duration));
  }
}

void ComposedRecorder::VideoRecorder::AddTrack(
    webrtc::VideoTrackInterface* track) {
  std::unique_ptr<Sink> sink(new Sink(track));
  webrtc::MutexLock lock(&sinks_lock_);
  sinks_.push_back(std::make_pair(track, std::move(sink)));
}

void ComposedRecorder::VideoRecorder::RemoveTrack(
    webrtc::VideoTrackInterface* track) {
  webrtc::MutexLock lock(&sinks_lock_);
  sinks_.erase(
      std::remove_if(sinks_.begin(), sinks_.end(),
                     [track](const VideoTrackSinkVector::value_type& sink) {
                       return sink.first == track;
                     }),
      sinks_.end());
}

ComposedRecorder::VideoRecorder::Sink::Sink(webrtc::VideoTrackInterface* track)
    : track_(track), current_buffer_(nullptr) {
  track_->AddOrUpdateSink(this, rtc::VideoSinkWants());
}
ComposedRecorder::VideoRecorder::Sink::~Sink() {
  track_->RemoveSink(this);
}

void ComposedRecorder::VideoRecorder::Sink::OnFrame(
    const webrtc::VideoFrame& frame) {
  timestamp_ = frame.timestamp();
  current_buffer_ = frame.video_frame_buffer();
}

void ComposedRecorder::AudioRecorder::AddTrack(
    webrtc::AudioTrackInterface* track) {
  std::unique_ptr<Sink> sink(new Sink(track));
  webrtc::MutexLock lock(&sinks_lock_);
  sinks_.push_back(std::make_pair(track, std::move(sink)));
}

void ComposedRecorder::AudioRecorder::RemoveTrack(
    webrtc::AudioTrackInterface* track) {
  webrtc::MutexLock lock(&sinks_lock_);
  sinks_.erase(
      std::remove_if(sinks_.begin(), sinks_.end(),
                     [track](const AudioTrackSinkVector::value_type& sink) {
                       return sink.first == track;
                     }),
      sinks_.end());
}

ComposedRecorder::AudioRecorder::Sink::Sink(webrtc::AudioTrackInterface* track)
    : track_(track) {
  track_->AddSink(this);
}

ComposedRecorder::AudioRecorder::Sink::~Sink() {
  track_->RemoveSink(this);
}

void ComposedRecorder::AudioRecorder::Sink::OnData(
    const void* audio_data,
    int bits_per_sample,
    int sample_rate,
    size_t number_of_channels,
    size_t number_of_frames,
    absl::optional<int64_t> absolute_capture_timestamp_ms) {
  // TODO: copy data.
}
