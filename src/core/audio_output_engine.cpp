#include "audio_output_engine.h"

#ifndef CLOGGER_SEVERITY
#define CLOGGER_SEVERITY CLOGGER_SEVERITY_WARN
#endif
#include "clogger/clogger.h"
#include "espressif_esp_audio_codec/esp_mp3_dec.h"

AudioOutputEngine::AudioOutputEngine(std::shared_ptr<ai_vox::AudioOutputDevice> audio_output_device)
    : audio_output_device_(std::move(audio_output_device)) {
  esp_mp3_dec_register();
  esp_audio_simple_dec_cfg_t audio_dec_cfg{
      .dec_type = ESP_AUDIO_SIMPLE_DEC_TYPE_MP3,
      .dec_cfg = nullptr,
      .cfg_size = 0,
  };
  auto ret = esp_audio_simple_dec_open(&audio_dec_cfg, &audio_dec_handle_);
  CLOGI("esp_audio_dec_open, ret: %d", ret);
  if (ret != ESP_AUDIO_ERR_OK) {
    CLOGE("esp_audio_simple_dec_open failed");
    abort();
  }
  audio_output_device_->Open(24000);
  task_queue_ = new TaskQueue("AudioOutput", 4096, tskIDLE_PRIORITY + 1);
  CLOGD();
}

AudioOutputEngine::~AudioOutputEngine() {
  delete task_queue_;
  audio_output_device_->Close();
  esp_audio_simple_dec_close(audio_dec_handle_);
  CLOGD();
}

void AudioOutputEngine::Write(std::vector<uint8_t>&& data) {
  task_queue_->Enqueue([this, data = std::move(data)]() mutable { ProcessData(std::move(data)); });
}

void AudioOutputEngine::NotifyDataEnd(std::function<void()>&& callback) {
  CLOGI();
  task_queue_->Enqueue([this, callback = std::move(callback)]() mutable { callback(); });
}

void AudioOutputEngine::ProcessData(std::vector<uint8_t>&& data) {
  esp_audio_simple_dec_raw_t raw = {
      .buffer = data.data(),
      .len = data.size(),
      .eos = false,
      .consumed = 0,
      .frame_recover = ESP_AUDIO_SIMPLE_DEC_RECOVERY_NONE,
  };
  uint8_t* frame_data = (uint8_t*)malloc(4096);
  esp_audio_simple_dec_out_t out_frame = {
      .buffer = frame_data,
      .len = 4096,
      .needed_size = 0,
      .decoded_size = 0,
  };

  while (raw.len > 0) {
    const auto ret = esp_audio_simple_dec_process(audio_dec_handle_, &raw, &out_frame);
    if (ret == ESP_AUDIO_ERR_BUFF_NOT_ENOUGH) {
      // Handle output buffer not enough case
      out_frame.buffer = reinterpret_cast<uint8_t*>(realloc(out_frame.buffer, out_frame.needed_size));
      if (out_frame.buffer == nullptr) {
        break;
      }
      out_frame.len = out_frame.needed_size;
      continue;
    }

    if (ret != ESP_AUDIO_ERR_OK) {
      CLOGE("Fail to decode data ret %d", ret);
      break;
    }

    audio_output_device_->Write(out_frame.buffer, out_frame.decoded_size);
    raw.len -= raw.consumed;
    raw.buffer += raw.consumed;
  }
  free(out_frame.buffer);
}