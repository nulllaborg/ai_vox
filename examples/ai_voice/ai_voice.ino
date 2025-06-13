#include <WiFi.h>

#include "ai_vox_engine.h"
#include "ai_vox_observer.h"
#include "i2s_std_audio_input_device.h"
#include "i2s_std_audio_output_device.h"

#ifndef WIFI_SSID
#define WIFI_SSID "ssid"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "password"
#endif

namespace {
#if defined(ARDUINO_ESP32_DEV)
constexpr gpio_num_t kMicPinBclk = GPIO_NUM_25;
constexpr gpio_num_t kMicPinWs = GPIO_NUM_26;
constexpr gpio_num_t kMicPinDin = GPIO_NUM_27;

constexpr gpio_num_t kSpeakerPinBclk = GPIO_NUM_33;
constexpr gpio_num_t kSpeakerPinWs = GPIO_NUM_32;
constexpr gpio_num_t kSpeakerPinDout = GPIO_NUM_23;

constexpr gpio_num_t kTriggerPin = GPIO_NUM_34;
constexpr gpio_num_t kLedPin = GPIO_NUM_2;

#elif defined(ARDUINO_ESP32S3_DEV)
constexpr gpio_num_t kMicPinBclk = GPIO_NUM_13;
constexpr gpio_num_t kMicPinWs = GPIO_NUM_11;
constexpr gpio_num_t kMicPinDin = GPIO_NUM_12;

constexpr gpio_num_t kSpeakerPinBclk = GPIO_NUM_40;
constexpr gpio_num_t kSpeakerPinWs = GPIO_NUM_41;
constexpr gpio_num_t kSpeakerPinDout = GPIO_NUM_42;

constexpr gpio_num_t kTriggerPin = GPIO_NUM_0;
constexpr gpio_num_t kLedPin = GPIO_NUM_6;
#endif

auto g_observer = std::make_shared<ai_vox::Observer>();
std::shared_ptr<ai_vox::iot::Entity> g_led_iot_entity;
std::shared_ptr<ai_vox::iot::Entity> g_speaker_iot_entity;
auto g_audio_output_device = std::make_shared<ai_vox::I2sStdAudioOutputDevice>(kSpeakerPinBclk, kSpeakerPinWs, kSpeakerPinDout);

void InitIot() {
  printf("InitIot\n");
  auto& ai_vox_engine = ai_vox::Engine::GetInstance();

  // Speaker
  // 1.Define the properties for the speaker entity
  std::vector<ai_vox::iot::Property> speaker_properties({
      {
          "volume",                        // property name
          "当前音量值",                    // property description
          ai_vox::iot::ValueType::kNumber  // property type: number, string or bool
      },
      // add more properties as needed
  });

  // 2.Define the functions for the speaker entity
  std::vector<ai_vox::iot::Function> speaker_functions({
      {"SetVolume",  // function name
       "设置音量",   // function description
       {
           {
               "volume",                         // parameter name
               "0到100之间的整数",               // parameter description
               ai_vox::iot::ValueType::kNumber,  // parameter type
               true                              // parameter required
           },
           // add more parameters as needed
       }},
      // add more functions as needed
  });

  // 3.Create the speaker entity
  g_speaker_iot_entity = std::make_shared<ai_vox::iot::Entity>("Speaker",                      // name
                                                               "扬声器",                       // description
                                                               std::move(speaker_properties),  // properties
                                                               std::move(speaker_functions)    // functions
  );

  // 4.Initialize the speaker entity with default values
  g_speaker_iot_entity->UpdateState("volume", g_audio_output_device->volume());

  // 5.Register the speaker entity with the AI Vox engine
  ai_vox_engine.RegisterIotEntity(g_speaker_iot_entity);

  // LED
  // 1.Define the properties for the LED entity
  std::vector<ai_vox::iot::Property> led_properties({
      {
          "state",                       // property name
          "LED灯开关状态",               // property description
          ai_vox::iot::ValueType::kBool  // property type
      },
      // add more properties as needed
  });

  // 2.Define the functions for the LED entity
  std::vector<ai_vox::iot::Function> led_functions({
      {"TurnOn",     // function name
       "打开LED灯",  // function description
       {
           // no parameters
       }},
      {"TurnOff",    // function name
       "关闭LED灯",  // function description
       {
           // no parameters
       }},
      // add more functions as needed
  });

  // 3.Create the LED entity
  g_led_iot_entity = std::make_shared<ai_vox::iot::Entity>("Led",                      // name
                                                           "LED灯",                    // description
                                                           std::move(led_properties),  // properties
                                                           std::move(led_functions)    // functions
  );

  // 4.Initialize the LED entity with default values
  g_led_iot_entity->UpdateState("state", false);

  // 5.Register the LED entity with the AI Vox engine
  ai_vox_engine.RegisterIotEntity(g_led_iot_entity);
}

#ifdef PRINT_HEAP_INFO_INTERVAL
void PrintMemInfo() {
  if (heap_caps_get_total_size(MALLOC_CAP_SPIRAM) > 0) {
    const auto total_size = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    const auto free_size = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    const auto min_free_size = heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM);
    printf("SPIRAM total size: %zu B (%zu KB), free size: %zu B (%zu KB), minimum free size: %zu B (%zu KB)\n",
           total_size,
           total_size >> 10,
           free_size,
           free_size >> 10,
           min_free_size,
           min_free_size >> 10);
  }

  if (heap_caps_get_total_size(MALLOC_CAP_INTERNAL) > 0) {
    const auto total_size = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
    const auto free_size = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    const auto min_free_size = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
    printf("IRAM total size: %zu B (%zu KB), free size: %zu B (%zu KB), minimum free size: %zu B (%zu KB)\n",
           total_size,
           total_size >> 10,
           free_size,
           free_size >> 10,
           min_free_size,
           min_free_size >> 10);
  }

  if (heap_caps_get_total_size(MALLOC_CAP_DEFAULT) > 0) {
    const auto total_size = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
    const auto free_size = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    const auto min_free_size = heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT);
    printf("DRAM total size: %zu B (%zu KB), free size: %zu B (%zu KB), minimum free size: %zu B (%zu KB)\n",
           total_size,
           total_size >> 10,
           free_size,
           free_size >> 10,
           min_free_size,
           min_free_size >> 10);
  }
}
#endif
}  // namespace

void setup() {
  Serial.begin(115200);
  printf("Init\n");

  if (heap_caps_get_total_size(MALLOC_CAP_SPIRAM) > 0) {
    WiFi.useStaticBuffers(true);
  } else {
    WiFi.useStaticBuffers(false);
  }

  printf("Connecting to WiFi, ssid: %s, password: %s\n", WIFI_SSID, WIFI_PASSWORD);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    printf("Connecting to WiFi, ssid: %s, password: %s\n", WIFI_SSID, WIFI_PASSWORD);
  }

  printf("WiFi connected, IP address: %s\n", WiFi.localIP().toString().c_str());

  pinMode(kLedPin, OUTPUT);
  digitalWrite(kLedPin, LOW);
  InitIot();

  auto audio_input_device = std::make_shared<ai_vox::I2sStdAudioInputDevice>(kMicPinBclk, kMicPinWs, kMicPinDin);

  auto& ai_vox_engine = ai_vox::Engine::GetInstance();
  ai_vox_engine.SetObserver(g_observer);
  ai_vox_engine.SetTrigger(kTriggerPin);
  ai_vox_engine.SetOtaUrl("https://api.tenclass.net/xiaozhi/ota/");
  ai_vox_engine.ConfigWebsocket("wss://api.tenclass.net/xiaozhi/v1/",
                                {
                                    {"Authorization", "Bearer test-token"},
                                });
  ai_vox_engine.Start(audio_input_device, g_audio_output_device);
  printf("AI Vox engine started\n");
}

void loop() {
#ifdef PRINT_HEAP_INFO_INTERVAL
  static uint32_t s_print_heap_info_time = 0;
  if (s_print_heap_info_time == 0 || millis() - s_print_heap_info_time >= PRINT_HEAP_INFO_INTERVAL) {
    s_print_heap_info_time = millis();
    PrintMemInfo();
  }
#endif

  const auto events = g_observer->PopEvents();
  for (auto& event : events) {
    if (auto activation_event = std::get_if<ai_vox::Observer::ActivationEvent>(&event)) {
      printf("activation code: %s, message: %s\n", activation_event->code.c_str(), activation_event->message.c_str());
    } else if (auto state_changed_event = std::get_if<ai_vox::Observer::StateChangedEvent>(&event)) {
      printf("state changed from %" PRIu8 " to %" PRIu8 "\n",
             static_cast<uint8_t>(state_changed_event->old_state),
             static_cast<uint8_t>(state_changed_event->new_state));
      switch (state_changed_event->new_state) {
        case ai_vox::ChatState::kIdle: {
          printf("Idle\n");
          break;
        }
        case ai_vox::ChatState::kIniting: {
          printf("Initing...\n");
          break;
        }
        case ai_vox::ChatState::kStandby: {
          printf("Standby\n");
          break;
        }
        case ai_vox::ChatState::kConnecting: {
          printf("Connecting...\n");
          break;
        }
        case ai_vox::ChatState::kListening: {
          printf("Listening...\n");
          break;
        }
        case ai_vox::ChatState::kSpeaking: {
          printf("Speaking...\n");
          break;
        }
        default: {
          break;
        }
      }
    } else if (auto emotion_event = std::get_if<ai_vox::Observer::EmotionEvent>(&event)) {
      printf("emotion: %s\n", emotion_event->emotion.c_str());
    } else if (auto chat_message_event = std::get_if<ai_vox::Observer::ChatMessageEvent>(&event)) {
      switch (chat_message_event->role) {
        case ai_vox::ChatRole::kAssistant: {
          printf("role: assistant, content: %s\n", chat_message_event->content.c_str());
          break;
        }
        case ai_vox::ChatRole::kUser: {
          printf("role: user, content: %s\n", chat_message_event->content.c_str());
          break;
        }
      }
    } else if (auto iot_message_event = std::get_if<ai_vox::Observer::IotMessageEvent>(&event)) {
      printf("IOT message: %s, function: %s\n", iot_message_event->name.c_str(), iot_message_event->function.c_str());
      for (const auto& [key, value] : iot_message_event->parameters) {
        if (std::get_if<bool>(&value)) {
          printf("key: %s, value: %s\n", key.c_str(), std::get<bool>(value) ? "true" : "false");
        } else if (std::get_if<std::string>(&value)) {
          printf("key: %s, value: %s\n", key.c_str(), std::get<std::string>(value).c_str());
        } else if (std::get_if<int64_t>(&value)) {
          printf("key: %s, value: %lld\n", key.c_str(), std::get<int64_t>(value));
        }
      }

      if (iot_message_event->name == "Led") {
        if (iot_message_event->function == "TurnOn") {
          printf("turn on led\n");
          digitalWrite(kLedPin, HIGH);
          g_led_iot_entity->UpdateState("state", true);  // Note: Must UpdateState after change the device state
        } else if (iot_message_event->function == "TurnOff") {
          printf("turn off led\n");
          digitalWrite(kLedPin, LOW);
          g_led_iot_entity->UpdateState("state", false);  // Note: Must UpdateState after change the device state
        }
      } else if (iot_message_event->name == "Speaker") {
        if (iot_message_event->function == "SetVolume") {
          if (const auto it = iot_message_event->parameters.find("volume"); it != iot_message_event->parameters.end()) {
            auto volume = it->second;
            if (std::get_if<int64_t>(&volume)) {
              printf("Speaker volume: %lld\n", std::get<int64_t>(volume));
              g_audio_output_device->SetVolume(std::get<int64_t>(volume));
              g_speaker_iot_entity->UpdateState("volume", std::get<int64_t>(volume));  // Note: Must UpdateState after change the device state
            }
          }
        }
      }
    }
  }

  taskYIELD();
}