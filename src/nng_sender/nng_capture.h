#ifndef NNG_CAPTURE_H_
#define NNG_CAPTURE_H_

#include <rtc/scalable_track_source.h>
#include <rtc_base/platform_thread.h>
#include <rtc_base/synchronization/mutex.h>
#include <nngpp/nngpp.h>

class NNGCapture : public ScalableVideoTrackSource {
 public:
  NNGCapture(const std::string& nng_capture_endpoint);
  ~NNGCapture();

 private:
  static void ReceiveThread(void* obj);
  
  webrtc::Mutex socket_lock_;
  nng::socket socket_;
  std::unique_ptr<rtc::PlatformThread> receive_thread_;
};


#endif