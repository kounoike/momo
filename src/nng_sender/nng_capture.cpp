#include "nng_sender/nng_capture.h"

#include <iostream>
#include <nngpp/nngpp.h>
#include <nngpp/protocol/pull0.h>
#include <third_party/libyuv/include/libyuv/convert.h>
#include <api/video/i420_buffer.h>

NNGCapture::NNGCapture() : socket_(nng::pull::v0::open()) {
  socket_.dial("tcp://127.0.0.1:5568", nng::flag::nonblock);

  receive_thread_.reset(
      new rtc::PlatformThread(ReceiveThread, this,
                              "NNGCaptureReceiveThread", rtc::kNormalPriority));
  receive_thread_->Start();
}

NNGCapture::~NNGCapture() {
  receive_thread_->Stop();
}

void NNGCapture::ReceiveThread(void* obj) {
  std::cout << "Start NNGCapture::ReceiveThread" << std::endl;
  auto capture = static_cast<NNGCapture*>(obj);
  while(true) {
    nng::buffer buf = capture->socket_.recv();
    // std::cout << "NNG Capture NNGmessage received size: " << buf.size() << std::endl;
    uint8_t *data = static_cast<uint8_t*>(buf.data());
    int32_t width = static_cast<int32_t>((data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3]);
    int32_t height = static_cast<int32_t>((data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7]);
    // std::cout << " Image width: " << width << " height: " << height << std::endl;

    auto i420buffer = webrtc::I420Buffer::Create(width, height);
    i420buffer->InitializeData();

    libyuv::RAWToI420(data + sizeof(int32_t) * 2, width * 3,
        i420buffer->MutableDataY(), i420buffer->StrideY(),
        i420buffer->MutableDataU(), i420buffer->StrideU(),
        i420buffer->MutableDataV(), i420buffer->StrideV(),
        width, height);

    auto video_frame = webrtc::VideoFrame::Builder()
        .set_video_frame_buffer(i420buffer)
        .set_timestamp_rtp(0)
        .set_timestamp_ms(rtc::TimeMillis())
        .set_timestamp_us(rtc::TimeMicros())
        .set_rotation(webrtc::kVideoRotation_0)
        .build();

    capture->OnCapturedFrame(video_frame);
  }

}
