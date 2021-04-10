#ifndef ZMQ_SENDER_H_
#define ZMQ_SENDER_H_

#include <rtc/video_track_receiver.h>
#include <rtc_base/synchronization/mutex.h>
#include <zmqpp/zmqpp.hpp>

class ZMQSender : public VideoTrackReceiver {
 public:
  ZMQSender();
  ~ZMQSender();

  void AddTrack(webrtc::VideoTrackInterface* track) override;
  void RemoveTrack(webrtc::VideoTrackInterface* track) override;
  void SendMessage(zmqpp::message& msg);

 protected:
  class Sink : public rtc::VideoSinkInterface<webrtc::VideoFrame> {
   public:
    Sink(ZMQSender* sender, webrtc::VideoTrackInterface* track);
    ~Sink();

    void OnFrame(const webrtc::VideoFrame& frame) override;
    webrtc::Mutex* GetMutex();
   private:
    ZMQSender* sender_;
    rtc::scoped_refptr<webrtc::VideoTrackInterface> track_;
    webrtc::Mutex frame_params_lock_;
    std::unique_ptr<uint8_t[]> image_;
    int frame_count_;
    int input_width_;
    int input_height_;
  };
 private:
  zmqpp::socket socket_;
  static zmqpp::context context_;
  webrtc::Mutex sinks_lock_;
  webrtc::Mutex socket_lock_;
  typedef std::vector<
      std::pair<webrtc::VideoTrackInterface*, std::unique_ptr<Sink> > >
      VideoTrackSinkVector;
  VideoTrackSinkVector sinks_;
  std::function<void(std::function<void()>)> dispatch_;
};

#endif
