#pragma once

#include <driver/i2s_std.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <condition_variable>
#include <functional>
#include <list>
#include <memory>
#include <thread>
#include <vector>

#include "audio_output_device.h"
#include "espressif_esp_audio_codec/esp_audio_simple_dec.h"
#include "task_queue/task_queue.h"

class OpusDecoder;
class AudioOutputEngine {
 public:
  AudioOutputEngine(std::shared_ptr<ai_vox::AudioOutputDevice> audio_output_device);
  ~AudioOutputEngine();

  void Write(std::vector<uint8_t>&& data);
  void NotifyDataEnd(std::function<void()>&& callback);

 private:
  void ProcessData(std::vector<uint8_t>&& data);

  esp_audio_simple_dec_handle_t audio_dec_handle_;
  std::shared_ptr<ai_vox::AudioOutputDevice> audio_output_device_;
  TaskQueue* task_queue_ = nullptr;
};