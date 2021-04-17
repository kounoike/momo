#include "nng_sender.h"
#include "nng_data_manager.h"
#include <iostream>
#include <nngpp/nngpp.h>
#include <nngpp/protocol/pull0.h>


NNGDataManager::NNGDataManager(NNGSender* sender) : sender_(sender), socket_(nng::pull::v0::open()) {
  socket_.dial("tcp://127.0.0.1:5570", nng::flag::nonblock);

  receive_thread_.reset(
      new rtc::PlatformThread(ReceiveThread, this,
                              "NNGReceiveThread", rtc::kNormalPriority));
  receive_thread_->Start();
}

NNGDataManager::~NNGDataManager() {
  receive_thread_->Stop();
  for (auto channel: data_channels_) {
    delete channel;
  }
}

void NNGDataManager::ReceiveThread(void* obj) {
  auto data_manager = static_cast<NNGDataManager*>(obj);
  while(true) {
    nng::buffer buf = data_manager->socket_.recv();
    std::cout << "NNG DataChannel message received size: " << buf.size() << std::endl;
    for (auto channel: data_manager->data_channels_) {
      channel->Send(buf);
    }
  }
}


void NNGDataManager::OnDataChannel(
      rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) {
  NNGDataChannel* channel = new NNGDataChannel(sender_, data_channel);
  webrtc::MutexLock lock(&data_channels_lock_);
  data_channels_.push_back(channel);
}


NNGDataManager::NNGDataChannel::NNGDataChannel(
    NNGSender* sender,
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) : sender_(sender), data_channel_(data_channel) {
  std::cout << "NNGDataChannel ctor id: " << data_channel_->id()
    << " label: " << data_channel_->label()
    << " state: " << data_channel_->state()
    << " dataStateString : " << data_channel_->DataStateString(data_channel_->state())
    << " protocol: " << data_channel_->protocol()
    << std::endl;
  std::string msg("data/start/" + std::to_string(data_channel_->id()));
  sender_->SendStringMessage(msg);
  data_channel_->RegisterObserver(this);  
}


NNGDataManager::NNGDataChannel::~NNGDataChannel() {
  data_channel_->UnregisterObserver();
}


void NNGDataManager::NNGDataChannel::OnStateChange() {
  std::cout << "OnStateChange" << std::endl;
}

void NNGDataManager::NNGDataChannel::OnMessage(const webrtc::DataBuffer& buffer) {
  std::cout << "OnMessage"
    << " size: " << buffer.size()
    << std::endl;
  sender_->SendDataChannelMessage(data_channel_->id(), buffer);
}

void NNGDataManager::NNGDataChannel::Send(const nng::buffer& buffer) {
  rtc::CopyOnWriteBuffer cow_buffer(static_cast<uint8_t*>(buffer.data()), buffer.size());
  webrtc::DataBuffer data_buffer(cow_buffer, true);
  data_channel_->Send(data_buffer);
}