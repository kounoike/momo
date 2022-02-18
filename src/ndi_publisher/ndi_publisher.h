#ifndef NDI_PUBLISHER_H_
#define NDI_PUBLISHER_H_

#include <memory>
#include <string>
#include <vector>

// Boost
#include <boost/asio.hpp>

// WebRTC
#include <api/media_stream_interface.h>
#include <api/scoped_refptr.h>
#include <api/video/video_frame.h>
#include <api/video/video_sink_interface.h>
#include <rtc/audio_track_receiver.h>
#include <rtc/video_track_receiver.h>
#include <rtc_base/synchronization/mutex.h>

// NDI
#include <Processing.NDI.Lib.h>

class NDIPublisher {
 public:
  NDIPublisher(){};
  ~NDIPublisher(){};
  VideoTrackReceiver* GetVideoTrackReceiver() {
    return static_cast<VideoTrackReceiver*>(&video_receiver_);
  };
  AudioTrackReceiver* GetAudioTrackReceiver();

 protected:
  class VideoReceiver : public VideoTrackReceiver {
   public:
    VideoReceiver(){};
    ~VideoReceiver(){};

    void AddTrack(webrtc::VideoTrackInterface* track) override;
    void RemoveTrack(webrtc::VideoTrackInterface* track) override;

   protected:
    class Sink : public rtc::VideoSinkInterface<webrtc::VideoFrame> {
     public:
      Sink(webrtc::VideoTrackInterface* track);
      ~Sink();

      void OnFrame(const webrtc::VideoFrame& frame) override;

     private:
      rtc::scoped_refptr<webrtc::VideoTrackInterface> track_;
      NDIlib_send_instance_t pNDI_send_;
    };

   private:
    webrtc::Mutex sinks_lock_;
    typedef std::vector<
        std::pair<webrtc::VideoTrackInterface*, std::unique_ptr<Sink> > >
        VideoTrackSinkVector;
    VideoTrackSinkVector sinks_;
    std::atomic<bool> running_;
    std::function<void(std::function<void()>)> dispatch_;
  };

  class AudioReceiver : public AudioTrackReceiver {
   public:
    AudioReceiver(){};
    ~AudioReceiver(){};

    void AddTrack(webrtc::AudioTrackInterface* track) override;
    void RemoveTrack(webrtc::AudioTrackInterface* track) override;

   protected:
    class Sink : public webrtc::AudioTrackSinkInterface {
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
      rtc::scoped_refptr<webrtc::AudioTrackInterface> track_;
      NDIlib_send_instance_t pNDI_send_;
    };

   private:
    webrtc::Mutex sinks_lock_;
    typedef std::vector<
        std::pair<webrtc::AudioTrackInterface*, std::unique_ptr<Sink> > >
        AudioTrackSinkVector;
    AudioTrackSinkVector sinks_;
    std::atomic<bool> running_;
    std::function<void(std::function<void()>)> dispatch_;
  };

 private:
  VideoReceiver video_receiver_;
  AudioReceiver audio_receiver_;
};

#endif
