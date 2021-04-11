#ifndef ZMQ_SENDER_H_
#define ZMQ_SENDER_H_

#include <rtc/video_track_receiver.h>
#include <rtc_base/synchronization/mutex.h>
#include <nngpp/nngpp.h>

class NNGSender : public VideoTrackReceiver {
 public:
  NNGSender();
  ~NNGSender();

  void AddTrack(webrtc::VideoTrackInterface* track) override;
  void RemoveTrack(webrtc::VideoTrackInterface* track) override;
  void SendStringMessage(const std::string& msg);
  void SendFrameMessage(const std::string& id, uint32_t frame_count, uint32_t width, uint32_t height, const uint8_t* image_ptr);

 protected:
  class Sink : public rtc::VideoSinkInterface<webrtc::VideoFrame> {
   public:
    Sink(NNGSender* sender, webrtc::VideoTrackInterface* track);
    ~Sink();

    void OnFrame(const webrtc::VideoFrame& frame) override;
    webrtc::Mutex* GetMutex();
   private:
    NNGSender* sender_;
    rtc::scoped_refptr<webrtc::VideoTrackInterface> track_;
    webrtc::Mutex frame_params_lock_;
    std::unique_ptr<uint8_t[]> image_;
    uint32_t frame_count_;
    uint32_t input_width_;
    uint32_t input_height_;
  };
 private:
//   zmqpp::socket socket_;
//   static zmqpp::context context_;
  nng::socket socket_;
  webrtc::Mutex sinks_lock_;
  webrtc::Mutex socket_lock_;
  typedef std::vector<
      std::pair<webrtc::VideoTrackInterface*, std::unique_ptr<Sink> > >
      VideoTrackSinkVector;
  VideoTrackSinkVector sinks_;
  std::function<void(std::function<void()>)> dispatch_;
};

#endif