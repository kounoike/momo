#ifndef NNG_AUDIO_DEVICE_H_
#define NNG_AUDIO_DEVICE_H_

#include <modules/audio_device/audio_device_generic.h>
#include <rtc_base/synchronization/mutex.h>
#include <rtc_base/platform_thread.h>
#include <nngpp/nngpp.h>

class NNGAudioDevice: public webrtc::AudioDeviceGeneric {
 public:
  NNGAudioDevice(const std::string& nng_audio_endpoint);
  ~NNGAudioDevice();
  // Retrieve the currently utilized audio layer
  int32_t ActiveAudioLayer(
      webrtc::AudioDeviceModule::AudioLayer& audioLayer) const override;

  // Main initializaton and termination
  webrtc::AudioDeviceGeneric::InitStatus Init() override;
  int32_t Terminate() override;
  bool Initialized() const override;

  // Device enumeration
  int16_t PlayoutDevices() override;
  int16_t RecordingDevices() override;
  int32_t PlayoutDeviceName(uint16_t index,
                            char name[webrtc::kAdmMaxDeviceNameSize],
                            char guid[webrtc::kAdmMaxGuidSize]) override;
  int32_t RecordingDeviceName(uint16_t index,
                              char name[webrtc::kAdmMaxDeviceNameSize],
                              char guid[webrtc::kAdmMaxGuidSize]) override;

  // Device selection
  int32_t SetPlayoutDevice(uint16_t index) override;
  int32_t SetPlayoutDevice(
      webrtc::AudioDeviceModule::WindowsDeviceType device) override;
  int32_t SetRecordingDevice(uint16_t index) override;
  int32_t SetRecordingDevice(
      webrtc::AudioDeviceModule::WindowsDeviceType device) override;

  // Audio transport initialization
  int32_t PlayoutIsAvailable(bool& available) override;
  int32_t InitPlayout() override;
  bool PlayoutIsInitialized() const override;
  int32_t RecordingIsAvailable(bool& available) override;
  int32_t InitRecording() override;
  bool RecordingIsInitialized() const override;

  // Audio transport control
  int32_t StartPlayout() override;
  int32_t StopPlayout() override;
  bool Playing() const override;
  int32_t StartRecording() override;
  int32_t StopRecording() override;
  bool Recording() const override;

  // Audio mixer initialization
  int32_t InitSpeaker() override;
  bool SpeakerIsInitialized() const override;
  int32_t InitMicrophone() override;
  bool MicrophoneIsInitialized() const override;

  // Speaker volume controls
  int32_t SpeakerVolumeIsAvailable(bool& available) override;
  int32_t SetSpeakerVolume(uint32_t volume) override;
  int32_t SpeakerVolume(uint32_t& volume) const override;
  int32_t MaxSpeakerVolume(uint32_t& maxVolume) const override;
  int32_t MinSpeakerVolume(uint32_t& minVolume) const override;

  // Microphone volume controls
  int32_t MicrophoneVolumeIsAvailable(bool& available) override;
  int32_t SetMicrophoneVolume(uint32_t volume) override;
  int32_t MicrophoneVolume(uint32_t& volume) const override;
  int32_t MaxMicrophoneVolume(uint32_t& maxVolume) const override;
  int32_t MinMicrophoneVolume(uint32_t& minVolume) const override;

  // Speaker mute control
  int32_t SpeakerMuteIsAvailable(bool& available) override;
  int32_t SetSpeakerMute(bool enable) override;
  int32_t SpeakerMute(bool& enabled) const override;

  // Microphone mute control
  int32_t MicrophoneMuteIsAvailable(bool& available) override;
  int32_t SetMicrophoneMute(bool enable) override;
  int32_t MicrophoneMute(bool& enabled) const override;

  // Stereo support
  int32_t StereoPlayoutIsAvailable(bool& available) override;
  int32_t SetStereoPlayout(bool enable) override;
  int32_t StereoPlayout(bool& enabled) const override;
  int32_t StereoRecordingIsAvailable(bool& available) override;
  int32_t SetStereoRecording(bool enable) override;
  int32_t StereoRecording(bool& enabled) const override;

  // Delay information and control
  int32_t PlayoutDelay(uint16_t& delayMS) const override;

  void AttachAudioBuffer(webrtc::AudioDeviceBuffer* audioBuffer) override;

#if defined(WEBRTC_IOS)
  int GetPlayoutAudioParameters(webrtc::AudioParameters* params) const override;
  int GetRecordAudioParameters(webrtc::AudioParameters* params) const override;
#endif  // WEBRTC_IOS

 private:
  static void PlayThreadFunc(void*);
  void PlayThreadProcess();
  static void ReceiveThread(void* obj);
  void ReceiveProcess();

  int32_t _playout_index;
  int32_t _record_index;
  webrtc::AudioDeviceBuffer* _ptrAudioBuffer;
  int8_t* _recordingBuffer;  // In bytes.
  int8_t* _playoutBuffer;    // In bytes.
  uint32_t _recordingFramesLeft;
  uint32_t _playoutFramesLeft;
  webrtc::Mutex _mutex;

  size_t _recordingBufferSizeIn10MS;
  size_t _recordingFramesIn10MS;
  size_t _writtenBufferSize;
  size_t _playoutFramesIn10MS;

  std::unique_ptr<rtc::PlatformThread> _ptrThreadPlay;

  bool _playing;
  bool _recording;
  int64_t _lastCallPlayoutMillis;
  int64_t _lastCallRecordMillis;

  uint32_t recording_sampling_rate_;
  uint32_t recording_bits_per_sample_;
  uint32_t recording_number_of_channels_;
  bool message_received_ = false;

  nng::socket socket_;
  std::unique_ptr<rtc::PlatformThread> receive_thread_;
};

#endif