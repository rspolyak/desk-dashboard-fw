#include "network.hpp"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>

namespace app::network {
namespace {
constexpr const char* log_tag = "network";
constexpr const char* wifi_ssid = CONFIG_DASHBOARD_WIFI_SSID;
constexpr const char* wifi_password = CONFIG_DASHBOARD_WIFI_PASSWORD;
constexpr const char* time_zone = "CET-1CEST,M3.5.0,M10.5.0/3";
constexpr const char* time_server = "pool.ntp.org";

/**
 * @brief Starts connecting once Wi-Fi is up and again whenever the connection drops; runs in the ESP-IDF event task.
 * @param[in] context Unused.
 * @param[in] base Unused; the handler is registered for Wi-Fi events only.
 * @param[in] id The Wi-Fi event.
 * @param[in] data Unused.
 * @note esp_wifi_connect() only hands a request to the Wi-Fi task, so this handler does not block the event task.
 */
void on_wifi_event([[maybe_unused]] void* context, [[maybe_unused]] esp_event_base_t base, std::int32_t id,
                   [[maybe_unused]] void* data)
{
    if (id != WIFI_EVENT_STA_START && id != WIFI_EVENT_STA_DISCONNECTED) {
        return;
    }

    const esp_err_t status = esp_wifi_connect();
    if (status != ESP_OK) {
        ESP_LOGE(log_tag, "starting the Wi-Fi connection failed: %s", esp_err_to_name(status));
    }
}

/**
 * @brief Starts setting the clock from the internet as soon as the board has an IP address; runs in the ESP-IDF
 *        event task.
 * @param[in] context Unused.
 * @param[in] base Unused; the handler is registered for the "got IP address" event only.
 * @param[in] id Unused.
 * @param[in] data Unused.
 * @note esp_netif_sntp_start() hands the start to the network task and waits for it, which takes only a moment.
 */
void on_got_ip([[maybe_unused]] void* context, [[maybe_unused]] esp_event_base_t base,
               [[maybe_unused]] std::int32_t id, [[maybe_unused]] void* data)
{
    const esp_err_t status = esp_netif_sntp_start();
    if (status != ESP_OK) {
        ESP_LOGE(log_tag, "starting internet time failed: %s", esp_err_to_name(status));
    }
}

/**
 * @brief Opens the flash storage the Wi-Fi driver keeps its calibration in, erasing it if its layout is outdated.
 * @return ESP_OK, or the error of the storage setup.
 */
esp_err_t start_storage()
{
    esp_err_t status = nvs_flash_init();
    if (status == ESP_ERR_NVS_NO_FREE_PAGES || status == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        status = nvs_flash_erase();
        if (status != ESP_OK) {
            ESP_LOGE(log_tag, "erasing the settings storage failed: %s", esp_err_to_name(status));
            return status;
        }
        status = nvs_flash_init();
    }
    if (status != ESP_OK) {
        ESP_LOGE(log_tag, "settings storage setup failed: %s", esp_err_to_name(status));
    }
    return status;
}

/**
 * @brief Sets Wi-Fi up as a station for the configured network; it starts connecting once esp_wifi_start() is called.
 * @return ESP_OK, or the error of the step that failed.
 */
esp_err_t set_up_wifi()
{
    esp_err_t status = esp_netif_init();
    if (status != ESP_OK) {
        ESP_LOGE(log_tag, "network interface setup failed: %s", esp_err_to_name(status));
        return status;
    }

    status = esp_event_loop_create_default();
    if (status != ESP_OK) {
        ESP_LOGE(log_tag, "event loop setup failed: %s", esp_err_to_name(status));
        return status;
    }

    if (esp_netif_create_default_wifi_sta() == nullptr) {
        ESP_LOGE(log_tag, "Wi-Fi network interface setup failed");
        return ESP_FAIL;
    }

    const wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    status = esp_wifi_init(&init_config);
    if (status != ESP_OK) {
        ESP_LOGE(log_tag, "Wi-Fi driver setup failed: %s", esp_err_to_name(status));
        return status;
    }

    status = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi_event, nullptr);
    if (status != ESP_OK) {
        ESP_LOGE(log_tag, "Wi-Fi event handler setup failed: %s", esp_err_to_name(status));
        return status;
    }

    wifi_config_t wifi_config = {};
    std::strncpy(reinterpret_cast<char*>(wifi_config.sta.ssid), wifi_ssid, sizeof(wifi_config.sta.ssid));
    std::strncpy(reinterpret_cast<char*>(wifi_config.sta.password), wifi_password,
                 sizeof(wifi_config.sta.password));

    status = esp_wifi_set_mode(WIFI_MODE_STA);
    if (status != ESP_OK) {
        ESP_LOGE(log_tag, "Wi-Fi station mode setup failed: %s", esp_err_to_name(status));
        return status;
    }

    status = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (status != ESP_OK) {
        ESP_LOGE(log_tag, "Wi-Fi network settings failed: %s", esp_err_to_name(status));
    }
    return status;
}

/**
 * @brief Sets the local time zone and prepares the internet time service, which on_got_ip() starts once online.
 * @return ESP_OK, or the error of the time service setup.
 */
esp_err_t start_time()
{
    constexpr int overwrite = 1;

    if (setenv("TZ", time_zone, overwrite) != 0) {
        ESP_LOGE(log_tag, "setting the time zone failed");
        return ESP_FAIL;
    }
    tzset();

    esp_sntp_config_t time_config = ESP_NETIF_SNTP_DEFAULT_CONFIG(time_server);
    time_config.start = false;
    esp_err_t status = esp_netif_sntp_init(&time_config);
    if (status != ESP_OK) {
        ESP_LOGE(log_tag, "internet time setup failed: %s", esp_err_to_name(status));
        return status;
    }

    status = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_got_ip, nullptr);
    if (status != ESP_OK) {
        ESP_LOGE(log_tag, "IP address event handler setup failed: %s", esp_err_to_name(status));
    }
    return status;
}
} // namespace

/**
 * @brief Connects to the Wi-Fi network, keeps reconnecting when it drops, and sets the clock from the internet.
 * @return ESP_OK, or the error of the setup step that failed.
 */
esp_err_t start()
{
    esp_err_t status = start_storage();
    if (status != ESP_OK) {
        return status;
    }

    status = set_up_wifi();
    if (status != ESP_OK) {
        return status;
    }

    status = start_time();
    if (status != ESP_OK) {
        return status;
    }

    status = esp_wifi_start();
    if (status != ESP_OK) {
        ESP_LOGE(log_tag, "Wi-Fi start failed: %s", esp_err_to_name(status));
    }
    return status;
}

/**
 * @brief Tells whether the board is connected to the Wi-Fi network.
 * @return true while connected to an access point.
 */
bool connected()
{
    wifi_ap_record_t access_point = {};
    return esp_wifi_sta_get_ap_info(&access_point) == ESP_OK;
}
} // namespace app::network
