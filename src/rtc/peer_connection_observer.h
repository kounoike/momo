#ifndef PEER_CONNECTION_OBSERVER_H_
#define PEER_CONNECTION_OBSERVER_H_

#include <vector>

// WebRTC
#include <api/peer_connection_interface.h>

#include "audio_track_receiver.h"
#include "rtc_data_manager.h"
#include "rtc_message_sender.h"
#include "stream_receiver.h"
#include "video_track_receiver.h"

class PeerConnectionObserver : public webrtc::PeerConnectionObserver {
 public:
  PeerConnectionObserver(RTCMessageSender* sender,
                         std::vector<StreamReceiver*>& stream_receivers,
                         std::vector<VideoTrackReceiver*>& video_receivers,
                         std::vector<AudioTrackReceiver*>& audio_receivers,
                         RTCDataManager* data_manager)
      : sender_(sender), data_manager_(data_manager) {
    std::copy(stream_receivers.begin(), stream_receivers.end(),
              stream_receivers_.end());
    std::copy(video_receivers.begin(), video_receivers.end(),
              video_receivers_.end());
    std::copy(audio_receivers.begin(), audio_receivers.end(),
              audio_receivers_.end());
  }
  ~PeerConnectionObserver();

 private:
  void OnSignalingChange(
      webrtc::PeerConnectionInterface::SignalingState new_state) override {}
  void OnDataChannel(
      rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) override;
  void OnRenegotiationNeeded() override {}
  void OnStandardizedIceConnectionChange(
      webrtc::PeerConnectionInterface::IceConnectionState new_state) override;
  void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state) override{};
  void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override;
  void OnAddStream(
      rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override;
  void OnRemoveStream(
      rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override;
  void OnTrack(
      rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver) override;
  void OnRemoveTrack(
      rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver) override;

 private:
  void ClearAllRegisteredTracks();

  RTCMessageSender* sender_;
  std::vector<StreamReceiver*> stream_receivers_;
  std::vector<VideoTrackReceiver*> video_receivers_;
  std::vector<AudioTrackReceiver*> audio_receivers_;
  RTCDataManager* data_manager_;
  std::vector<webrtc::VideoTrackInterface*> video_tracks_;
  std::vector<webrtc::AudioTrackInterface*> audio_tracks_;
};

#endif
