#include "audio_input_engine.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#ifndef CLOGGER_SEVERITY
#define CLOGGER_SEVERITY CLOGGER_SEVERITY_WARN
#endif

#include "clogger/clogger.h"

#ifdef ARDUINO
#include "libopus/opus.h"
#else
#include "opus.h"
#endif

namespace {
constexpr size_t kMaxOpusPacketSize = 1500;
constexpr uint32_t kFrameDuration = 100;  // ms
}  // namespace

AudioInputEngine::AudioInputEngine(std::shared_ptr<ai_vox::AudioInputDevice> audio_input_device, const AudioInputEngine::DataHandler& handler)
    : handler_(handler), audio_input_device_(std::move(audio_input_device)), task_queue_(new TaskQueue("AudioInput", 4096, tskIDLE_PRIORITY + 1)) {
  // constexpr uint32_t frame_rate = 16000;
  // constexpr uint32_t channels = 1;
  // int error = 0;
  // opus_encoder_ = opus_encoder_create(frame_rate, channels, OPUS_APPLICATION_VOIP, &error);
  // assert(opus_encoder_ != nullptr);
  // if (opus_encoder_ == nullptr) {
  //   CLOGE("opus_encoder_create failed: %d", error);
  //   return;
  // }

  // opus_encoder_ctl(opus_encoder_, OPUS_SET_DTX(1));
  // opus_encoder_ctl(opus_encoder_, OPUS_SET_COMPLEXITY(0));
  // opus_encoder_ctl(opus_encoder_, OPUS_SET_BITRATE(8000));

  audio_input_device_->Open(16000);
  task_queue_->Enqueue([this] { PullData(); });
  CLOGD("OK");
}

AudioInputEngine::~AudioInputEngine() {
  delete task_queue_;
  audio_input_device_->Close();
  opus_encoder_destroy(opus_encoder_);
  CLOGD("OK");
}

void AudioInputEngine::PullData() {
  auto pcm = audio_input_device_->Read(16000 / 1000 * kFrameDuration);
  handler_(std::move(pcm));
  task_queue_->Enqueue([this] { PullData(); });
}