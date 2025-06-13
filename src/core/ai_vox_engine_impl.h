#pragma once

#ifndef _AI_VOX_ENGINE_IMPL_H_
#define _AI_VOX_ENGINE_IMPL_H_

#include <esp_event_base.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mqtt_client.h>

#include <condition_variable>
#include <cstdint>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "ai_vox_engine.h"
#include "ai_vox_observer.h"
#include "espressif_esp_websocket_client/esp_websocket_client.h"
#include "iot/iot_manager.h"
#include "messaging/message_queue.h"
#include "task_queue/task_queue.h"

struct button_dev_t;
class AudioInputEngine;
class AudioOutputEngine;

namespace ai_vox {

class EngineImpl : public Engine {
 public:
  static EngineImpl &GetInstance();
  EngineImpl();
  ~EngineImpl();
  void SetObserver(std::shared_ptr<Observer> observer) override;
  void SetTrigger(const gpio_num_t gpio) override;
  void SetOtaUrl(const std::string url) override;
  void ConfigWebsocket(const std::string url, const std::map<std::string, std::string> headers) override;
  void RegisterIotEntity(std::shared_ptr<iot::Entity> entity) override;
  void Start(std::shared_ptr<AudioInputDevice> audio_input_device, std::shared_ptr<AudioOutputDevice> audio_output_device) override;

 private:
  enum class State : uint8_t {
    kIdle,
    kInited,
    kWebsocketConnecting,
    kStandby,
    kListening,
    kSpeaking,
  };

  enum class MessageType : uint8_t {
    kOnButtonClick,
    kOnWebsocketConnected,
    kOnWebsocketDisconnected,
    kOnWebsocketEventData,
    kOnWebsocketFinish,
    kOnOutputDataComsumed,
  };

  static void OnButtonClick(void *button_handle, void *usr_data);
  static void OnWebsocketEvent(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

  void OnButtonClick();
  void OnWebsocketEvent(esp_event_base_t base, int32_t event_id, void *event_data);
  void OnJsonData(std::vector<uint8_t> &&data);
  void OnWebSocketConnected();
  void OnWebSocketDisconnected();
  void OnAudioOutputDataConsumed(std::string &&task_id, std::string &&session_id, std::string &&session_status);
  void OnAudioFrameBegin(std::vector<uint8_t> &&code);
  void OnAudioFrame(std::vector<uint8_t> &&data);
  void OnAudioFrameEnd();
  void OnTriggered();

  void LoadProtocol();
  void BindDevice();
  void StartListening();
  void ConnectWebSocket();
  void SendIotDescriptions();
  void SendIotUpdatedStates(const bool force);
  void ChangeState(const State new_state);

  mutable std::mutex mutex_;
  MessageQueue<MessageType> message_queue_;
  State state_ = State::kIdle;
  ChatState chat_state_ = ChatState::kIdle;
  button_dev_t *button_handle_ = nullptr;
  gpio_num_t trigger_pin_ = GPIO_NUM_0;
  std::vector<uint8_t> recving_websocket_data_;
  std::shared_ptr<AudioInputDevice> audio_input_device_;
  std::shared_ptr<AudioOutputDevice> audio_output_device_;
  std::shared_ptr<Observer> observer_;
  ai_vox::iot::Manager iot_manager_;
  esp_websocket_client_handle_t web_socket_client_ = nullptr;
  std::shared_ptr<AudioInputEngine> audio_input_engine_;
  std::shared_ptr<AudioOutputEngine> audio_output_engine_;
  std::string ota_url_;
  std::string websocket_url_;
  std::map<std::string, std::string> websocket_headers_;
  TaskQueue main_task_queue_;
  std::unique_ptr<TaskQueue> transmit_task_queue_;
  std::string tts_task_id_;
  std::string tts_session_id_;
  std::string tts_session_status_;
};
}  // namespace ai_vox

#endif