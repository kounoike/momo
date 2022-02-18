#ifndef AUDIO_TRACK_RECEIVER_H_
#define AUDIO_TRACK_RECEIVER_H_

#include <string>

// WebRTC
#include <api/media_stream_interface.h>

class AudioTrackReceiver {
 public:
  virtual void AddTrack(webrtc::AudioTrackInterface* track) = 0;
  virtual void RemoveTrack(webrtc::AudioTrackInterface* track) = 0;
};

#endif
