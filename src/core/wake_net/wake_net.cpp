#define WAKE_NET_MODEL "27"

#ifdef ARDUINO_ESP32S3_DEV

#include "wake_net.h"

#include <esp_afe_config.h>
#include <esp_afe_sr_models.h>
#include <esp_wn_models.h>
#include <model_path.h>

#include <cstring>

#include "core/flex_array/flex_array.h"
#include "core/silk_resampler.h"

#ifndef CLOGGER_SEVERITY
#define CLOGGER_SEVERITY CLOGGER_SEVERITY_WARN
#endif
#include "core/clogger/clogger.h"

namespace {
auto &g_afe_handle = ESP_AFE_SR_HANDLE;

constexpr uint8_t kSrmodels[] = {
#include "models/wn9_nihaoxiaozhi_tts_arduino.bin"
};

constexpr uint32_t kSampleRate = 16000;
}  // namespace

WakeNet::WakeNet(std::function<void()> &&handler, std::shared_ptr<ai_vox::AudioInputDevice> audio_input_device)
    : handler_(std::move(handler)), audio_input_device_(std::move(audio_input_device)) {
  srmodel_list_t *models = srmodel_load(kSrmodels);
  if (models) {
    for (int i = 0; i < models->num; i++) {
      if (strstr(models->model_name[i], ESP_WN_PREFIX) != NULL) {
        CLOGI("wakenet model in flash: %s", models->model_name[i]);
      }
    }
  }

  afe_config_t afe_config = AFE_CONFIG_DEFAULT();
  afe_config.wakenet_model_name = esp_srmodel_filter(models, ESP_WN_PREFIX, nullptr);
  afe_config.aec_init = false;
  afe_config.pcm_config.total_ch_num = 1;
  afe_config.pcm_config.mic_num = 1;
  afe_config.pcm_config.ref_num = 0;
  afe_config.pcm_config.sample_rate = kSampleRate;

  afe_data_ = g_afe_handle.create_from_config(&afe_config);

  if (afe_data_ == nullptr) {
    CLOGE("afe create failed");
    abort();
  }
}

WakeNet::~WakeNet() {
  Stop();
  g_afe_handle.destroy(afe_data_);
}

void WakeNet::Start() {
  CLOGI();

  if (detect_task_ != nullptr || feed_task_ != nullptr) {
    CLOGD("WakeNet already started");
    return;
  }

  audio_input_device_->OpenInput(kSampleRate);

  if (audio_input_device_->input_sample_rate() != kSampleRate) {
    resampler_ = std::make_unique<SilkResampler>(audio_input_device_->input_sample_rate(), kSampleRate);
  }

  feed_task_ = new ActiveTaskQueue("WakeNetFeed", 8 * 1024, tskIDLE_PRIORITY + 1);
  detect_task_ = new ActiveTaskQueue("WakeNetDetect", 4 * 1024, tskIDLE_PRIORITY + 1);

  feed_task_->Enqueue(
      [this, samples = g_afe_handle.get_feed_chunksize(afe_data_) * g_afe_handle.get_total_channel_num(afe_data_)]() mutable { FeedData(samples); });
  detect_task_->Enqueue([this]() { DetectWakeWord(); });
  CLOGI("OK");
}

void WakeNet::Stop() {
  delete feed_task_;
  feed_task_ = nullptr;

  delete detect_task_;
  detect_task_ = nullptr;

  resampler_.reset();
  audio_input_device_->CloseInput();
  CLOGI("OK");
}

void WakeNet::FeedData(const uint32_t samples) {
  auto pcm = ReadPcm(samples);
  g_afe_handle.feed(afe_data_, pcm.data());

  feed_task_->Enqueue([this, samples]() mutable { FeedData(samples); });
}

void WakeNet::DetectWakeWord() {
  afe_fetch_result_t *res = g_afe_handle.fetch(afe_data_);
  if (res != nullptr && res->wakeup_state == WAKENET_DETECTED) {
    CLOGI("Wake word detected");
    if (handler_) {
      handler_();
    }
  }
  taskYIELD();
  detect_task_->Enqueue([this]() { DetectWakeWord(); });
}

FlexArray<int16_t> WakeNet::ReadPcm(const uint32_t samples) {
  FlexArray<int16_t> pcm(samples);
  audio_input_device_->Read(pcm.data(), pcm.size());
  if (resampler_) {
    return resampler_->Resample(std::move(pcm));
  } else {
    return pcm;
  }
}

#endif  // ARDUINO_ESP32S3_DEV