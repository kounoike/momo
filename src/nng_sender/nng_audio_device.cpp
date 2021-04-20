#include "nng_sender/nng_audio_device.h"
#include <rtc_base/logging.h>
#include <rtc_base/time_utils.h>
#include <system_wrappers/include/sleep.h>
#include <nngpp/protocol/pull0.h>
#include <iostream>

const int kPlayoutFixedSampleRate = 8000;
const size_t kPlayoutNumChannels = 1;
const size_t kPlayoutBufferSize = kPlayoutFixedSampleRate / 100 * kPlayoutNumChannels * 2;

NNGAudioDevice::NNGAudioDevice() :
  socket_(nng::pull::v0::open()),
  _ptrAudioBuffer(NULL),
  _recordingBuffer(NULL),
  _playoutBuffer(NULL),
  _recordingFramesLeft(0),
  _playoutFramesLeft(0),
  _recordingBufferSizeIn10MS(0),
  _recordingFramesIn10MS(0),
  _writtenBufferSize(0),
  _playoutFramesIn10MS(0),
  _playing(false),
  _recording(false),
  _lastCallPlayoutMillis(0),
  _lastCallRecordMillis(0) {
  socket_.dial("tcp://127.0.0.1:5569", nng::flag::nonblock);
  std::cout << "NNGAudioDevice ctor." << std::endl;

  receive_thread_.reset(
    new rtc::PlatformThread(ReceiveThread, this, "NNGAudioReceiveThread", rtc::kNormalPriority));
  receive_thread_->Start();

}

NNGAudioDevice::~NNGAudioDevice() {

}

void NNGAudioDevice::ReceiveThread(void* obj) {
  auto audio = static_cast<NNGAudioDevice*>(obj);
  audio->ReceiveProcess();  
}

void NNGAudioDevice::ReceiveProcess() {
  while(true) {
    nng::buffer buf = socket_.recv();
    std::cout << "NNG Audio Device NNGmessage received size: " << buf.size() << " recording: " << (_recording ? "true": "false") << std::endl;
    uint8_t *data = static_cast<uint8_t*>(buf.data());
    int32_t bits_per_sample = static_cast<int32_t>((data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3]);
    int32_t sample_rate = static_cast<int32_t>((data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7]);
    int32_t number_of_channels = static_cast<int32_t>((data[8] << 24) | (data[9] << 16) | (data[10] << 8) | data[11]);
    int32_t number_of_frames = static_cast<int32_t>((data[12] << 24) | (data[13] << 16) | (data[14] << 8) | data[15]);

    size_t audio_data_size = bits_per_sample * number_of_channels * number_of_frames / 8;
    uint8_t *audio_data = data + 16;

    recording_sampling_rate_ = sample_rate;
    recording_bits_per_sample_ = bits_per_sample;
    recording_number_of_channels_ = number_of_channels;
    std::cout << "Call InitRecording" << std::endl;
    InitRecording();
    std::cout << "done." << std::endl;
    message_received_ = true;

    if (_recording) {
      size_t copied_data_size = 0;
      std::cout << "copy start" << std::endl;
      _mutex.Lock();
      while(_recording && copied_data_size != audio_data_size) {
        if (_recordingBufferSizeIn10MS - _writtenBufferSize <=
            audio_data_size - copied_data_size) {
          memcpy(_recordingBuffer + _writtenBufferSize, &audio_data[copied_data_size],
                _recordingBufferSizeIn10MS - _writtenBufferSize);
          copied_data_size += _recordingBufferSizeIn10MS - _writtenBufferSize;
          _writtenBufferSize = 0;
          _ptrAudioBuffer->SetRecordedBuffer(_recordingBuffer,
                                            _recordingFramesIn10MS);
          _mutex.Unlock();
          _ptrAudioBuffer->DeliverRecordedData();
          webrtc::SleepMs(10);
          _mutex.Lock();
        } else {
          memcpy(_recordingBuffer + _writtenBufferSize, &audio_data[copied_data_size],
                audio_data_size - copied_data_size);
          _writtenBufferSize += audio_data_size - copied_data_size;
          copied_data_size += audio_data_size - copied_data_size;
        }
      }
      _mutex.Unlock();
      std::cout << "while loop out." << std::endl;
    }
  }
}

int32_t NNGAudioDevice::ActiveAudioLayer(
    webrtc::AudioDeviceModule::AudioLayer& audio_layer) const {
  return -1;
}

webrtc::AudioDeviceGeneric::InitStatus NNGAudioDevice::Init() {
  return webrtc::AudioDeviceGeneric::InitStatus::OK;
}

int32_t NNGAudioDevice::Terminate() {
  StopRecording();
  return 0;
}

bool NNGAudioDevice::Initialized() const {
  return true;
}

int16_t NNGAudioDevice::PlayoutDevices() {
  return 1;
}

int16_t NNGAudioDevice::RecordingDevices() {
  return 1;
}

int32_t NNGAudioDevice::PlayoutDeviceName(
    uint16_t index,
    char name[webrtc::kAdmMaxDeviceNameSize],
    char guid[webrtc::kAdmMaxGuidSize]) {
  const char* kName = "dummy_device";
  const char* kGuid = "dummy_device_unique_id";
  if (index < 1) {
    memset(name, 0, webrtc::kAdmMaxDeviceNameSize);
    memset(guid, 0, webrtc::kAdmMaxGuidSize);
    memcpy(name, kName, strlen(kName));
    memcpy(guid, kGuid, strlen(guid));
    return 0;
  }
  return -1;
}

int32_t NNGAudioDevice::RecordingDeviceName(
    uint16_t index,
    char name[webrtc::kAdmMaxDeviceNameSize],
    char guid[webrtc::kAdmMaxGuidSize]) {
  const char* kName = "dummy_device";
  const char* kGuid = "dummy_device_unique_id";
  if (index < 1) {
    memset(name, 0, webrtc::kAdmMaxDeviceNameSize);
    memset(guid, 0, webrtc::kAdmMaxGuidSize);
    memcpy(name, kName, strlen(kName));
    memcpy(guid, kGuid, strlen(guid));
    return 0;
  }
  return -1;
}

int32_t NNGAudioDevice::SetPlayoutDevice(uint16_t index) {
  if (index == 0) {
    _playout_index = index;
    return 0;
  }
  return -1;
}

int32_t NNGAudioDevice::SetPlayoutDevice(
    webrtc::AudioDeviceModule::WindowsDeviceType device) {
  return -1;
}

int32_t NNGAudioDevice::SetRecordingDevice(uint16_t index) {
  if (index == 0) {
    _record_index = index;
    return _record_index;
  }
  return -1;
}

int32_t NNGAudioDevice::SetRecordingDevice(
    webrtc::AudioDeviceModule::WindowsDeviceType device) {
  return -1;
}

int32_t NNGAudioDevice::PlayoutIsAvailable(bool& available) {
  if (_playout_index == 0) {
    available = true;
    return _playout_index;
  }
  available = false;
  return -1;
}

int32_t NNGAudioDevice::InitPlayout() {
  webrtc::MutexLock lock(&_mutex);

  if (_playing) {
    return -1;
  }

  _playoutFramesIn10MS = static_cast<size_t>(kPlayoutFixedSampleRate / 100);

  if (_ptrAudioBuffer) {
    _ptrAudioBuffer->SetPlayoutSampleRate(kPlayoutFixedSampleRate);
    _ptrAudioBuffer->SetPlayoutChannels(kPlayoutNumChannels);
  }
  return 0;
}

bool NNGAudioDevice::PlayoutIsInitialized() const {
  return _playoutFramesIn10MS != 0;
}

int32_t NNGAudioDevice::RecordingIsAvailable(bool& available) {
  if (_record_index == 0) {
    available = true;
    return _record_index;
  }
  available = false;
  return -1;
}

int32_t NNGAudioDevice::InitRecording() {
  webrtc::MutexLock lock(&_mutex);

  if (_recording) {
    return -1;
  }

  _recordingFramesIn10MS =
      static_cast<size_t>(recording_sampling_rate_ / 100);

  if (_ptrAudioBuffer) {
    _ptrAudioBuffer->SetRecordingSampleRate(recording_sampling_rate_);
    _ptrAudioBuffer->SetRecordingChannels(recording_number_of_channels_);
  }
  return 0;
}

bool NNGAudioDevice::RecordingIsInitialized() const {
  return _recordingFramesIn10MS != 0;
}

int32_t NNGAudioDevice::StartPlayout() {
  if (_playing) {
    return 0;
  }

  _playing = true;
  _playoutFramesLeft = 0;

  if (!_playoutBuffer) {
    _playoutBuffer = new int8_t[kPlayoutBufferSize];
  }
  if (!_playoutBuffer) {
    _playing = false;
    return -1;
  }

  _ptrThreadPlay.reset(new rtc::PlatformThread(
      PlayThreadFunc, this, "webrtc_audio_module_play_thread",
      rtc::kRealtimePriority));
  _ptrThreadPlay->Start();

  RTC_LOG(LS_INFO) << __FUNCTION__ << " Started playout capture";
  std::cout << __FUNCTION__ << " Start playout." << std::endl;
  return 0;
}

int32_t NNGAudioDevice::StopPlayout() {
  {
    webrtc::MutexLock lock(&_mutex);
    _playing = false;
  }

  if (_ptrThreadPlay) {
    _ptrThreadPlay->Stop();
    _ptrThreadPlay.reset();
  }

  webrtc::MutexLock lock(&_mutex);

  _playoutFramesLeft = 0;
  delete[] _playoutBuffer;
  _playoutBuffer = NULL;

  RTC_LOG(LS_INFO) << __FUNCTION__ << " Stopped playout capture";
  return 0;
}

bool NNGAudioDevice::Playing() const {
  return _playing;
}

int32_t NNGAudioDevice::StartRecording() {
  if (_recording) {
    return -1;
  }
  _recording = true;

  // Make sure we only create the buffer once.
  _recordingBufferSizeIn10MS =
      _recordingFramesIn10MS * recording_number_of_channels_ * 2;
  if (!_recordingBuffer) {
    _recordingBuffer = new int8_t[_recordingBufferSizeIn10MS];
  }

  RTC_LOG(LS_INFO) << __FUNCTION__ << " Started recording";
  std::cout << __FUNCTION__ << " Start recording." << std::endl;

  return 0;
}

int32_t NNGAudioDevice::StopRecording() {
  {
    webrtc::MutexLock lock(&_mutex);
    if (!_recording) {
      return -1;
    }
    _recording = false;
  }

  webrtc::MutexLock lock(&_mutex);
  _recordingFramesLeft = 0;
  if (_recordingBuffer) {
    delete[] _recordingBuffer;
    _recordingBuffer = NULL;
  }

  RTC_LOG(LS_INFO) << __FUNCTION__ << " Stopped recording";
  return 0;
}

bool NNGAudioDevice::Recording() const {
  return _recording;
}

int32_t NNGAudioDevice::InitSpeaker() {
  return 0;
}

bool NNGAudioDevice::SpeakerIsInitialized() const {
  return true;
}

int32_t NNGAudioDevice::InitMicrophone() {
  return 0;
}

bool NNGAudioDevice::MicrophoneIsInitialized() const {
  return true;
}

int32_t NNGAudioDevice::SpeakerVolumeIsAvailable(bool& available) {
  return -1;
}

int32_t NNGAudioDevice::SetSpeakerVolume(uint32_t volume) {
  return -1;
}

int32_t NNGAudioDevice::SpeakerVolume(uint32_t& volume) const {
  return -1;
}

int32_t NNGAudioDevice::MaxSpeakerVolume(uint32_t& maxVolume) const {
  return -1;
}

int32_t NNGAudioDevice::MinSpeakerVolume(uint32_t& minVolume) const {
  return -1;
}

int32_t NNGAudioDevice::MicrophoneVolumeIsAvailable(bool& available) {
  return -1;
}

int32_t NNGAudioDevice::SetMicrophoneVolume(uint32_t volume) {
  return -1;
}

int32_t NNGAudioDevice::MicrophoneVolume(uint32_t& volume) const {
  return -1;
}

int32_t NNGAudioDevice::MaxMicrophoneVolume(uint32_t& maxVolume) const {
  return -1;
}

int32_t NNGAudioDevice::MinMicrophoneVolume(uint32_t& minVolume) const {
  return -1;
}

int32_t NNGAudioDevice::SpeakerMuteIsAvailable(bool& available) {
  return -1;
}

int32_t NNGAudioDevice::SetSpeakerMute(bool enable) {
  return -1;
}

int32_t NNGAudioDevice::SpeakerMute(bool& enabled) const {
  return -1;
}

int32_t NNGAudioDevice::MicrophoneMuteIsAvailable(bool& available) {
  return -1;
}

int32_t NNGAudioDevice::SetMicrophoneMute(bool enable) {
  return -1;
}

int32_t NNGAudioDevice::MicrophoneMute(bool& enabled) const {
  return -1;
}

int32_t NNGAudioDevice::StereoPlayoutIsAvailable(bool& available) {
  available = false;
  return 0;
}
int32_t NNGAudioDevice::SetStereoPlayout(bool enable) {
  return 0;
}

int32_t NNGAudioDevice::StereoPlayout(bool& enabled) const {
  enabled = true;
  return 0;
}

int32_t NNGAudioDevice::StereoRecordingIsAvailable(bool& available) {
  available = false;
  return 0;
}

int32_t NNGAudioDevice::SetStereoRecording(bool enable) {
  if (enable) {
    return -1;
  } else {
    return 0;
  }
}

int32_t NNGAudioDevice::StereoRecording(bool& enabled) const {
  return -1;
}

int32_t NNGAudioDevice::PlayoutDelay(uint16_t& delayMS) const {
  return 0;
}

void NNGAudioDevice::AttachAudioBuffer(webrtc::AudioDeviceBuffer* audioBuffer) {
  webrtc::MutexLock lock(&_mutex);

  _ptrAudioBuffer = audioBuffer;

  // Inform the AudioBuffer about default settings for this implementation.
  // Set all values to zero here since the actual settings will be done by
  // InitPlayout and InitRecording later.
  _ptrAudioBuffer->SetRecordingSampleRate(0);
  _ptrAudioBuffer->SetPlayoutSampleRate(0);
  _ptrAudioBuffer->SetRecordingChannels(0);
  _ptrAudioBuffer->SetPlayoutChannels(0);
}

#if defined(WEBRTC_IOS)
int NNGAudioDevice::GetPlayoutAudioParameters(
    webrtc::AudioParameters* params) const {
  RTC_LOG(INFO) << __FUNCTION__;
  return 0;
}

int NNGAudioDevice::GetRecordAudioParameters(
    webrtc::AudioParameters* params) const {
  RTC_LOG(INFO) << __FUNCTION__;
  return 0;
}
#endif  // WEBRTC_IOS

void NNGAudioDevice::PlayThreadFunc(void* pThis) {
  (static_cast<NNGAudioDevice*>(pThis)->PlayThreadProcess());
}

void NNGAudioDevice::PlayThreadProcess() {
  if (!_playing) {
    return;
  }
  int64_t currentTime = rtc::TimeMillis();
  {
    _mutex.Lock();

    if (_lastCallPlayoutMillis == 0 ||
        currentTime - _lastCallPlayoutMillis >= 5) {
      _mutex.Unlock();
      _ptrAudioBuffer->RequestPlayoutData(_playoutFramesIn10MS);
      _mutex.Lock();

      _playoutFramesLeft = _ptrAudioBuffer->GetPlayoutData(_playoutBuffer);
      RTC_DCHECK_EQ(_playoutFramesIn10MS, _playoutFramesLeft);
      _lastCallPlayoutMillis = currentTime;
    }
    _playoutFramesLeft = 0;
  }

  int64_t deltaTimeMillis = rtc::TimeMillis() - currentTime;
  if (deltaTimeMillis < 5) {
    webrtc::SleepMs(5 - deltaTimeMillis);
  }
}
