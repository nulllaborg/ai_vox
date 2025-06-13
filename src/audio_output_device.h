#pragma once

#ifndef _AUDIO_OUTPUT_DEVICE_H_
#define _AUDIO_OUTPUT_DEVICE_H_

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ai_vox {
class AudioOutputDevice {
 public:
  constexpr static uint16_t kMaxVolume = 100;

  virtual ~AudioOutputDevice() = default;
  virtual bool Open(uint32_t sample_rate) = 0;
  virtual void Close() = 0;
  virtual size_t Write(std::vector<int16_t>&& pcm) = 0;
  virtual size_t Write(const uint8_t* data, const size_t length) = 0;
  virtual void SetVolume(uint16_t volume) = 0;
  virtual uint16_t volume() const = 0;
};
}  // namespace ai_vox

#endif