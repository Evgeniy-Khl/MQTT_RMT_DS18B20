#include "main.h"
//-------------------------------------------------------------
static const char *TAG = "main";
//-------------------------------------------------------------
#define DS18B20_RESOLUTION   (DS18B20_RESOLUTION_12_BIT)
#define MAX_DEVICES  (3)
//-------------------------------------------------------------
void app_main(void)
{
  gpio_reset_pin(CONFIG_LED_GPIO);
  gpio_set_direction(CONFIG_LED_GPIO, GPIO_MODE_OUTPUT);
  gpio_set_level(CONFIG_LED_GPIO, 0);
  //Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ret = nvs_flash_erase();
    ESP_LOGI(TAG, "nvs_flash_erase: 0x%04x", ret);
    ret = nvs_flash_init();
    ESP_LOGI(TAG, "nvs_flash_init: 0x%04x", ret);
  }
  ESP_LOGI(TAG, "nvs_flash_init: 0x%04x", ret);

  ret = esp_netif_init();
  ESP_LOGI(TAG, "esp_netif_init: %d", ret);
  ret = esp_event_loop_create_default();
  ESP_LOGI(TAG, "esp_event_loop_create_default: %d", ret);
  ret = wifi_init_sta();
  ESP_LOGI(TAG, "wifi_init_sta: %d", ret);
  vTaskDelay(2000 / portTICK_PERIOD_MS);

  ESP_LOGI(TAG, "OWB Start");
  // Create a 1-Wire bus, using the RMT timeslot driver
  OneWireBus * owb;
  owb_rmt_driver_info rmt_driver_info;
  owb = owb_rmt_initialize(&rmt_driver_info, CONFIG_ONE_WIRE_GPIO, RMT_CHANNEL_1, RMT_CHANNEL_0);
  owb_use_crc(owb, true);  // enable CRC check for ROM code
  // Find all connected devices
  printf("Find devices:\n");
  OneWireBus_ROMCode device_rom_codes[MAX_DEVICES] = {0};
  int num_devices = 0;
  OneWireBus_SearchState search_state = {0};
  bool found = false;
  owb_search_first(owb, &search_state, &found);
  while (found)
  {
    char rom_code_s[17];
    owb_string_from_rom_code(search_state.rom_code, rom_code_s, sizeof(rom_code_s));
    printf("  %d : %s\n", num_devices, rom_code_s);
    device_rom_codes[num_devices] = search_state.rom_code;
    ++num_devices;
    owb_search_next(owb, &search_state, &found);
  }
  printf("Found %d device%s\n", num_devices, num_devices == 1 ? "" : "s");
  if (num_devices == 1)
  {
    OneWireBus_ROMCode rom_code;
    owb_status status = owb_read_rom(owb, &rom_code);
    if (status == OWB_STATUS_OK)
    {
      char rom_code_s[OWB_ROM_CODE_STRING_LENGTH];
      owb_string_from_rom_code(rom_code, rom_code_s, sizeof(rom_code_s));
      printf("Single device %s present\n", rom_code_s);
    }
    else
    {
      printf("An error occurred reading ROM code: %d", status);
    }
  }
  
  // Create DS18B20 devices on the 1-Wire bus
  DS18B20_Info * devices[MAX_DEVICES] = {0};
  for (int i = 0; i < num_devices; ++i)
  {
    DS18B20_Info * ds18b20_info = ds18b20_malloc();  // heap allocation
    devices[i] = ds18b20_info;
    if (num_devices == 1)
    {
        printf("Single device optimisations enabled\n");
        ds18b20_init_solo(ds18b20_info, owb);          // only one device on bus
    }
    else
    {
        ds18b20_init(ds18b20_info, owb, device_rom_codes[i]); // associate with bus and device
    }
    ds18b20_use_crc(ds18b20_info, true);           // enable CRC check on all reads
    ds18b20_set_resolution(ds18b20_info, DS18B20_RESOLUTION);
  }

  if (num_devices > 0)
  {
    mqtt_start(owb, devices, num_devices);
  }
  else
  {
    printf("\nNo DS18B20 devices detected!\n");
  }
  for (int i = 0; i < num_devices; ++i)
  {
      ds18b20_free(&devices[i]);
  }
  owb_uninitialize(owb);
  printf("Restarting now.\n");
  fflush(stdout);
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  esp_restart();
}
