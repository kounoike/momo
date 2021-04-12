#include "peer_connection_observer.h"

#include <iostream>

// WebRTC
#include <rtc_base/logging.h>

PeerConnectionObserver::~PeerConnectionObserver() {
  // Ayame 再接続時などには kIceConnectionDisconnected の前に破棄されているため
  ClearAllRegisteredTracks();
}

void PeerConnectionObserver::OnDataChannel(
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) {
  if (data_manager_ != nullptr) {
    data_manager_->OnDataChannel(data_channel);
  }
}

void PeerConnectionObserver::OnStandardizedIceConnectionChange(
    webrtc::PeerConnectionInterface::IceConnectionState new_state) {
  RTC_LOG(LS_INFO) << __FUNCTION__ << " :" << new_state;
  if (new_state == webrtc::PeerConnectionInterface::IceConnectionState::
                       kIceConnectionDisconnected) {
    ClearAllRegisteredTracks();
  }
  if (sender_ != nullptr) {
    sender_->OnIceConnectionStateChange(new_state);
  }
}

void PeerConnectionObserver::OnIceCandidate(
    const webrtc::IceCandidateInterface* candidate) {
  std::string sdp;
  if (candidate->ToString(&sdp)) {
    if (sender_ != nullptr) {
      sender_->OnIceCandidate(candidate->sdp_mid(),
                              candidate->sdp_mline_index(), sdp);
    }
  } else {
    RTC_LOG(LS_ERROR) << "Failed to serialize candidate";
  }
}

void PeerConnectionObserver::OnTrack(
    rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver) {
  if (video_track_receiver_ == nullptr && audio_track_receiver_ == nullptr)
    return;
  rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track =
      transceiver->receiver()->track();
  if (track->kind() == webrtc::MediaStreamTrackInterface::kVideoKind && video_track_receiver_ != nullptr) {
    webrtc::VideoTrackInterface* video_track =
        static_cast<webrtc::VideoTrackInterface*>(track.get());
    video_tracks_.push_back(video_track);
    video_track_receiver_->AddTrack(video_track, transceiver->receiver()->stream_ids());
  }
  if (track->kind() == webrtc::MediaStreamTrackInterface::kAudioKind && audio_track_receiver_ != nullptr) {
    webrtc::AudioTrackInterface* audio_track =
        static_cast<webrtc::AudioTrackInterface*>(track.get());
    audio_tracks_.push_back(audio_track);
    audio_track_receiver_->AddTrack(audio_track, transceiver->receiver()->stream_ids());
  }
}

void PeerConnectionObserver::OnRemoveTrack(
    rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver) {
  if (video_track_receiver_ == nullptr && audio_track_receiver_ == nullptr)
    return;
  rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track =
      receiver->track();
  if (track->kind() == webrtc::MediaStreamTrackInterface::kVideoKind && video_track_receiver_ != nullptr) {
    webrtc::VideoTrackInterface* video_track =
        static_cast<webrtc::VideoTrackInterface*>(track.get());
    video_tracks_.erase(
        std::remove_if(video_tracks_.begin(), video_tracks_.end(),
                       [video_track](const webrtc::VideoTrackInterface* track) {
                         return track == video_track;
                       }),
        video_tracks_.end());
    video_track_receiver_->RemoveTrack(video_track);
  }
  if (track->kind() == webrtc::MediaStreamTrackInterface::kAudioKind && audio_track_receiver_ != nullptr) {
    webrtc::AudioTrackInterface* audio_track =
        static_cast<webrtc::AudioTrackInterface*>(track.get());
    audio_tracks_.erase(
        std::remove_if(audio_tracks_.begin(), audio_tracks_.end(),
                       [audio_track](const webrtc::AudioTrackInterface* track) {
                         return track == audio_track;
                       }),
        audio_tracks_.end());
    audio_track_receiver_->RemoveTrack(audio_track);
  }
}

void PeerConnectionObserver::ClearAllRegisteredTracks() {
  if (video_track_receiver_ != nullptr) {
    for (webrtc::VideoTrackInterface* video_track : video_tracks_) {
      video_track_receiver_->RemoveTrack(video_track);
    }
  }
  video_tracks_.clear();
  if (audio_track_receiver_ != nullptr) {
    for (webrtc::AudioTrackInterface* audio_track : audio_tracks_) {
      audio_track_receiver_->RemoveTrack(audio_track);
    }
  }
  audio_tracks_.clear();
}
