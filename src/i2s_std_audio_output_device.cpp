#include "i2s_std_audio_output_device.h"

#include <cmath>

namespace ai_vox {
I2sStdAudioOutputDevice::I2sStdAudioOutputDevice(gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout)
    : gpio_cfg_({
          .mclk = I2S_GPIO_UNUSED,
          .bclk = bclk,
          .ws = ws,
          .dout = dout,
          .din = I2S_GPIO_UNUSED,
          .invert_flags =
              {
                  .mclk_inv = false,
                  .bclk_inv = false,
                  .ws_inv = false,
              },
      }) {
}

I2sStdAudioOutputDevice::I2sStdAudioOutputDevice(const i2s_std_slot_config_t& slot_cfg, const i2s_std_gpio_config_t& gpio_cfg)
    : slot_cfg_(slot_cfg), gpio_cfg_(gpio_cfg) {
}

bool I2sStdAudioOutputDevice::Open(uint32_t sample_rate) {
  Close();

  i2s_chan_config_t tx_chan_cfg = {
      .id = I2S_NUM_0,
      .role = I2S_ROLE_MASTER,
      .dma_desc_num = 2,
      .dma_frame_num = 480,
      .auto_clear_after_cb = true,
      .auto_clear_before_cb = false,
      .allow_pd = false,
      .intr_priority = 0,
  };

  ESP_ERROR_CHECK(i2s_new_channel(&tx_chan_cfg, &i2s_tx_handle_, nullptr));

  i2s_std_config_t tx_std_cfg = {
      .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate),
      .slot_cfg = slot_cfg_,
      .gpio_cfg = gpio_cfg_,
  };

  ESP_ERROR_CHECK(i2s_channel_init_std_mode(i2s_tx_handle_, &tx_std_cfg));
  ESP_ERROR_CHECK(i2s_channel_enable(i2s_tx_handle_));
  return true;
}

void I2sStdAudioOutputDevice::Close() {
  if (i2s_tx_handle_ == nullptr) {
    return;
  }

  i2s_channel_disable(i2s_tx_handle_);
  i2s_del_channel(i2s_tx_handle_);
  i2s_tx_handle_ = nullptr;
}

I2sStdAudioOutputDevice::~I2sStdAudioOutputDevice() {
  Close();
}

uint16_t I2sStdAudioOutputDevice::volume() const {
  return volume_;
}

void I2sStdAudioOutputDevice::SetVolume(uint16_t volume) {
  if (volume > kMaxVolume) {
    volume = kMaxVolume;
  }
  volume_ = volume;
  volume_factor_ = pow(double(volume_) / 100.0, 2) * 65536;
}

size_t I2sStdAudioOutputDevice::Write(std::vector<int16_t>&& pcm) {
  std::vector<int32_t> buffer(pcm.size());

  for (size_t i = 0; i < pcm.size(); i++) {
    int64_t temp = int64_t(pcm[i]) * volume_factor_;
    if (temp > INT32_MAX) {
      buffer[i] = INT32_MAX;
    } else if (temp < INT32_MIN) {
      buffer[i] = INT32_MIN;
    } else {
      buffer[i] = static_cast<int32_t>(temp);
    }
  }

  size_t bytes_written = 0;
  ESP_ERROR_CHECK(i2s_channel_write(i2s_tx_handle_, buffer.data(), buffer.size() * sizeof(int32_t), &bytes_written, 1000));
  return buffer.size();
}

size_t I2sStdAudioOutputDevice::Write(const uint8_t* data, const size_t length) {
  auto* pcm = reinterpret_cast<const int16_t*>(data);
  if (length % sizeof(int16_t) != 0) {
    printf("Invalid length: %zu\n", length);
    return 0;
  }
  size_t samples = length / sizeof(int16_t);
  std::vector<int32_t> buffer(samples);

  for (size_t i = 0; i < samples; i++) {
    int64_t temp = int64_t(pcm[i]) * volume_factor_;
    if (temp > INT32_MAX) {
      buffer[i] = INT32_MAX;
    } else if (temp < INT32_MIN) {
      buffer[i] = INT32_MIN;
    } else {
      buffer[i] = static_cast<int32_t>(temp);
    }
  }

  size_t bytes_written = 0;
  ESP_ERROR_CHECK(i2s_channel_write(i2s_tx_handle_, buffer.data(), buffer.size() * sizeof(int32_t), &bytes_written, 1000));
  return buffer.size();
}

}  // namespace ai_vox