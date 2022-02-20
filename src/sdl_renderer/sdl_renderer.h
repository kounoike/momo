#ifndef SDL_RENDERER_H_
#define SDL_RENDERER_H_

#include <memory>
#include <string>
#include <vector>

// SDL
#include <SDL.h>

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

#include "rtc/rtc_manager.h"

class SDLRenderer {
 public:
  SDLRenderer(int width, int height, bool fullscreen)
      : audio_receiver_(), video_receiver_(width, height, fullscreen, audio_receiver_) {};
  ~SDLRenderer() = default;

  void SetupReceiver(RTCManager* rtc_manager);
  void SetDispatchFunction(std::function<void(std::function<void()>)> dispatch);

  class AudioReceiver : public AudioTrackReceiver {
   public:
    AudioReceiver(){};
    ~AudioReceiver(){};

    double GetVolumeAndReset(webrtc::AudioTrackInterface* track);

    void AddTrack(webrtc::AudioTrackInterface* track) override{};
    void AddTrack(webrtc::AudioTrackInterface* track,
                  webrtc::MediaStreamInterface* stream) override;
    void RemoveTrack(webrtc::AudioTrackInterface* track) override;

   protected:
    class Sink : public webrtc::AudioTrackSinkInterface {
     public:
      Sink(webrtc::AudioTrackInterface* track);
      ~Sink();

      double GetVolume();
      void ResetVolumeCalculation();

      void OnData(const void* audio_data,
                  int bits_per_sample,
                  int sample_rate,
                  size_t number_of_channels,
                  size_t number_of_frames,
                  absl::optional<int64_t> absolute_capture_timestamp_ms);

     private:
      webrtc::AudioTrackInterface* track_;
      int count_;
      double sum_of_square_;
    };

   private:
    webrtc::Mutex sinks_lock_;
    typedef std::vector<
        std::pair<webrtc::AudioTrackInterface*, std::unique_ptr<Sink> > >
        AudioTrackSinkVector;
    AudioTrackSinkVector sinks_;
  };

  class VideoReceiver : public VideoTrackReceiver {
   public:
    VideoReceiver(int width,
                  int height,
                  bool fullscreen,
                  AudioReceiver& audio_receiver);
    ~VideoReceiver();

    double GetVolumeAndReset() { return audio_receiver_.GetVolumeAndReset(track); }

    void SetDispatchFunction(
        std::function<void(std::function<void()>)> dispatch);

    static int RenderThreadExec(void* data);
    int RenderThread();

    void SetOutlines();
    void AddTrack(webrtc::VideoTrackInterface* track) override{};
    void AddTrack(webrtc::VideoTrackInterface* track,
                  webrtc::MediaStreamInterface* stream) override;
    void RemoveTrack(webrtc::VideoTrackInterface* track) override;

   protected:
    class Sink : public rtc::VideoSinkInterface<webrtc::VideoFrame> {
     public:
      Sink(VideoReceiver* renderer, webrtc::VideoTrackInterface* track);
      ~Sink();

      void OnFrame(const webrtc::VideoFrame& frame) override;

      void SetOutlineRect(int x, int y, int width, int height);

      webrtc::Mutex* GetMutex();
      bool GetOutlineChanged();
      int GetOffsetX();
      int GetOffsetY();
      int GetFrameWidth();
      int GetFrameHeight();
      int GetWidth();
      int GetHeight();
      uint8_t* GetImage();

     private:
      VideoReceiver* renderer_;
      rtc::scoped_refptr<webrtc::VideoTrackInterface> track_;
      webrtc::Mutex frame_params_lock_;
      int outline_offset_x_;
      int outline_offset_y_;
      int outline_width_;
      int outline_height_;
      bool outline_changed_;
      float outline_aspect_;
      int input_width_;
      int input_height_;
      bool scaled_;
      std::unique_ptr<uint8_t[]> image_;
      int offset_x_;
      int offset_y_;
      int width_;
      int height_;
    };

   private:
    bool IsFullScreen();
    void SetFullScreen(bool fullscreen);
    void PollEvent();

    AudioReceiver& audio_receiver_;
    webrtc::Mutex sinks_lock_;
    typedef std::vector<
        std::pair<webrtc::VideoTrackInterface*, std::unique_ptr<Sink> > >
        VideoTrackSinkVector;
    VideoTrackSinkVector sinks_;
    std::atomic<bool> running_;
    SDL_Thread* thread_;
    SDL_Window* window_;
    SDL_Renderer* renderer_;
    std::function<void(std::function<void()>)> dispatch_;
    int width_;
    int height_;
    int rows_;
    int cols_;
  };

 private:
  VideoReceiver video_receiver_;
  AudioReceiver audio_receiver_;
};

#endif
