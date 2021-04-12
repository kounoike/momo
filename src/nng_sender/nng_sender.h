#ifndef ZMQ_SENDER_H_
#define ZMQ_SENDER_H_

#include <rtc/video_track_receiver.h>
#include <rtc_base/synchronization/mutex.h>
#include <nngpp/nngpp.h>

class NNGSender {
 public:
  NNGSender();
  ~NNGSender();

  class NNGAudioTrackReceiver : public AudioTrackReceiver {
   public:
    NNGAudioTrackReceiver(NNGSender* sender);
    ~NNGAudioTrackReceiver();
    void AddTrack(webrtc::AudioTrackInterface* track, const std::vector<std::string>& stream_ids) override;
    void RemoveTrack(webrtc::AudioTrackInterface* track) override;
   protected:
    class Sink : public webrtc::AudioTrackSinkInterface {
     public:
      Sink(NNGSender* sender, webrtc::AudioTrackInterface* track, const std::string& stream_id);
      ~Sink();
      int NumPreferredChannels() const override { return 1; }
      void OnData(const void* audio_data,
                  int bits_per_sample,
                  int sample_rate,
                  size_t number_of_channels,
                  size_t number_of_frames) override;
      void OnData(const void* audio_data,
                  int bits_per_sample,
                  int sample_rate,
                  size_t number_of_channels,
                  size_t number_of_frames,
                  absl::optional<int64_t> absolute_capture_timestamp_ms) override;

     private:
      NNGSender* sender_;
      std::string stream_id_;
      rtc::scoped_refptr<webrtc::AudioTrackInterface> track_;
      uint32_t frame_count_;
    };
   private:
    NNGSender* sender_;
    typedef std::vector<
        std::pair<webrtc::AudioTrackInterface*, std::unique_ptr<Sink> > >
        AudioTrackSinkVector;
    webrtc::Mutex sinks_lock_;
    AudioTrackSinkVector sinks_;
  };

  class NNGVideoTrackReceiver : public VideoTrackReceiver {
   public:
    NNGVideoTrackReceiver(NNGSender* sender);
    ~NNGVideoTrackReceiver();
    void AddTrack(webrtc::VideoTrackInterface* track, const std::vector<std::string>& stream_ids) override;
    void RemoveTrack(webrtc::VideoTrackInterface* track) override;
   protected:
    class Sink : public rtc::VideoSinkInterface<webrtc::VideoFrame> {
    public:
      Sink(NNGSender* sender, webrtc::VideoTrackInterface* track, const std::string& stream_id);
      ~Sink();

      void OnFrame(const webrtc::VideoFrame& frame) override;
      webrtc::Mutex* GetMutex();
    private:
      NNGSender* sender_;
      std::string stream_id_;
      rtc::scoped_refptr<webrtc::VideoTrackInterface> track_;
      webrtc::Mutex frame_params_lock_;
      std::unique_ptr<uint8_t[]> image_;
      uint32_t frame_count_;
      uint32_t input_width_;
      uint32_t input_height_;
    };
   private:
    webrtc::Mutex sinks_lock_;
    NNGSender* sender_;
    typedef std::vector<
        std::pair<webrtc::VideoTrackInterface*, std::unique_ptr<Sink> > >
        VideoTrackSinkVector;
    VideoTrackSinkVector sinks_;
  };

  void SendStringMessage(const std::string& msg);
  void SendFrameMessage(const std::string& stream_id, const std::string& track_id, uint32_t frame_count, uint32_t width, uint32_t height, const uint8_t* image_ptr);
  void SendAudioDataMessage(
    const std::string& stream_id,
    const std::string& track_id,
    const void* audio_data,
    int bits_per_sample,
    int sample_rate,
    size_t number_of_channels,
    size_t number_of_frames);


 public:
  NNGVideoTrackReceiver* GetVideoTrackReceiver() {
    return &video_track_receiver_;
  }
  NNGAudioTrackReceiver* GetAudioTrackReceiver() {
    return &audio_track_receiver_;
  }

 private:
  nng::socket socket_;
  webrtc::Mutex socket_lock_;

  NNGVideoTrackReceiver video_track_receiver_;
  NNGAudioTrackReceiver audio_track_receiver_;

};

#endif