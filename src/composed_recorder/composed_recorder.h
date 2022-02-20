#ifndef COMPOSED_RECORDER_H_
#define COMPOSED_RECORDER_H_

#include <atomic>
#include <chrono>
#include <deque>
#include <thread>
#include <vector>

#include <api/media_stream_interface.h>
#include <api/scoped_refptr.h>
#include <api/video/video_frame.h>
#include <api/video/video_sink_interface.h>
#include <rtc/audio_track_receiver.h>
#include <rtc/stream_receiver.h>
#include <rtc/video_track_receiver.h>
#include <rtc_base/synchronization/mutex.h>

class ComposedRecorder {
 public:
  ComposedRecorder();
  ~ComposedRecorder();

  class VideoRecorder : public VideoTrackReceiver {
    friend class ComposedRecorder;

   public:
    void AddTrack(webrtc::VideoTrackInterface* track) override;
    void RemoveTrack(webrtc::VideoTrackInterface* track) override;

    class Sink : public rtc::VideoSinkInterface<webrtc::VideoFrame> {
      friend class ComposedRecorder;

     public:
      Sink(webrtc::VideoTrackInterface* track);
      ~Sink();

      void OnFrame(const webrtc::VideoFrame& frame) override;

     private:
      webrtc::VideoTrackInterface* track_;
      int32_t timestamp_;
      rtc::scoped_refptr<webrtc::VideoFrameBuffer> current_buffer_;
    };

   private:
    webrtc::Mutex sinks_lock_;
    typedef std::vector<
        std::pair<webrtc::VideoTrackInterface*, std::unique_ptr<Sink> > >
        VideoTrackSinkVector;
    VideoTrackSinkVector sinks_;
  };

  class AudioRecorder : public AudioTrackReceiver {
    friend class ComposedRecorder;

   public:
    void AddTrack(webrtc::AudioTrackInterface* track) override;
    void RemoveTrack(webrtc::AudioTrackInterface* track) override;

    class Sink : public webrtc::AudioTrackSinkInterface {
      friend class ComposedRecorder;

     public:
      Sink(webrtc::AudioTrackInterface* track);
      ~Sink();

      void OnData(const void* audio_data,
                  int bits_per_sample,
                  int sample_rate,
                  size_t number_of_channels,
                  size_t number_of_frames,
                  absl::optional<int64_t> absolute_capture_timestamp_ms);

     private:
      webrtc::AudioTrackInterface* track_;
    };

   private:
    webrtc::Mutex sinks_lock_;
    typedef std::vector<
        std::pair<webrtc::AudioTrackInterface*, std::unique_ptr<Sink> > >
        AudioTrackSinkVector;
    AudioTrackSinkVector sinks_;
  };

  VideoRecorder& GetVideoRecorder() { return video_recorder_; }
  AudioRecorder& GetAudioRecorder() { return audio_recorder_; }

  void RecorderThreadFunc();

 private:
  std::thread recorder_thread_;
  std::atomic<bool> running_;

  VideoRecorder video_recorder_;
  AudioRecorder audio_recorder_;
};

#endif
