#ifndef STREAM_RECEIVER_H_
#define STREAM_RECEIVER_H_

#include <string>

// WebRTC
#include <api/media_stream_interface.h>

class StreamReceiver {
 public:
  virtual void AddStream(webrtc::MediaStreamInterface* stream) = 0;
  virtual void RemoveStream(webrtc::MediaStreamInterface* stream) = 0;
};

#endif
