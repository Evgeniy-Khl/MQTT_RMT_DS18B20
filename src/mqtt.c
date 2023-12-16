#include "mqtt.h"
//-------------------------------------------------------------
static const char *TAG = "mqtt";
static EventGroupHandle_t mqtt_state_event_group;
//-------------------------------------------------------------
#define MQTT_EVT_CONNECTED BIT0
#define MQTT_EVT_SUBSCRIBED BIT1
#define MQTT_EVT_DISCONNECTED  BIT2
#define MQTT_EVT_UNSUBSCRIBED  BIT3
//-------------------------------------------------------------
static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
      ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}
//-------------------------------------------------------------
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
  ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%ld", base, event_id);
  esp_mqtt_event_handle_t event = event_data;
  esp_mqtt_client_handle_t client = event->client;
  int msg_id;
  switch ((esp_mqtt_event_id_t)event_id) {
  case MQTT_EVENT_CONNECTED:
    xEventGroupSetBits(mqtt_state_event_group, MQTT_EVT_CONNECTED);
    xEventGroupClearBits(mqtt_state_event_group, MQTT_EVT_DISCONNECTED);
    ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
    msg_id = esp_mqtt_client_publish(client, "house/s1", "data_1", 0, 1, 0);
    ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
    break;
  case MQTT_EVENT_DISCONNECTED:
    xEventGroupSetBits(mqtt_state_event_group, MQTT_EVT_DISCONNECTED);
    xEventGroupClearBits(mqtt_state_event_group, MQTT_EVT_CONNECTED);
    ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
    break;
  case MQTT_EVENT_SUBSCRIBED:
    xEventGroupSetBits(mqtt_state_event_group, MQTT_EVT_SUBSCRIBED);
    xEventGroupClearBits(mqtt_state_event_group, MQTT_EVT_UNSUBSCRIBED);
    ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
    break;
  case MQTT_EVENT_UNSUBSCRIBED:
    xEventGroupSetBits(mqtt_state_event_group, MQTT_EVT_UNSUBSCRIBED);
    xEventGroupClearBits(mqtt_state_event_group, MQTT_EVT_SUBSCRIBED);
    ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
    break;
  case MQTT_EVENT_DATA:
    ESP_LOGI(TAG, "MQTT_EVENT_DATA");
    printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
    printf("DATA=%.*s\r\n", event->data_len, event->data);
    break;
  case MQTT_EVENT_ERROR:
    ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
    if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
      log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
      log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
      log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
      ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
    }
    break;
  default:
    ESP_LOGI(TAG, "Other event id:%d", event->event_id);
    break;
  }
}
//-------------------------------------------------------------
void mqtt_start(const OneWireBus * bus, const DS18B20_Info * ds18b20_info[MAX_DEVICES], int num_devices)
{
  esp_mqtt_client_config_t mqtt_cfg = {
      .broker.address.uri = "mqtt://192.168.1.138",
      .credentials.authentication.password = "123456",
      .credentials.username = "mosquitto",
  };

  char str0[15] = {0};
  int errors_count[MAX_DEVICES] = {0};

  esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);

  mqtt_state_event_group = xEventGroupCreate();

  esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
  esp_mqtt_client_start(client);

  while(1)
  {
    TickType_t last_wake_time = xTaskGetTickCount();
    EventBits_t bits = xEventGroupWaitBits(mqtt_state_event_group,
        MQTT_EVT_CONNECTED,
            pdFALSE,
            pdTRUE,
            0);
    if (bits & MQTT_EVT_CONNECTED)
    {
      ds18b20_convert_all(bus);
      ds18b20_wait_for_conversion(ds18b20_info[0]);
      float readings[MAX_DEVICES] = { 0 };
      DS18B20_ERROR errors[MAX_DEVICES] = { 0 };
      for (int i = 0; i < num_devices; ++i)
      {
        errors[i] = ds18b20_read_temp(ds18b20_info[i], &readings[i]);
      }
      for (int i = 0; i < num_devices; ++i)
      {
        if (errors[i] != DS18B20_OK)
        {
          ++errors_count[i];
        }
        sprintf(str0, "%.1f", readings[i]);
        printf("%d: %s, %d errors\n\n", i, str0, errors_count[i]);
        if(i==0) esp_mqtt_client_publish(client, "house2/room1/temp0", str0, 0, 0, 0);
        if(i==1) esp_mqtt_client_publish(client, "house2/room1/temp1", str0, 0, 1, 0);
        if(i==2) esp_mqtt_client_publish(client, "house2/room1/temp2", str0, 0, 2, 0);
      }
      if(num_devices < 3) esp_mqtt_client_publish(client, "house2/room1/temp2", "0.0", 0, 2, 0);
      if(num_devices < 2) esp_mqtt_client_publish(client, "house2/room1/temp1", "0.0", 0, 1, 0);
    }
    vTaskDelayUntil(&last_wake_time, 2000 / portTICK_PERIOD_MS);
  }
}
//-------------------------------------------------------------
