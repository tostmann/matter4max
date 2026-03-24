#include "matter_bridge.h"
#include <esp_log.h>
#include <esp_err.h>
#include <esp_mac.h>
#include <esp_matter.h>
#include <esp_matter_core.h>
#include <esp_matter_console.h>
#include <esp_matter_cluster.h>
#include <esp_matter_attribute.h>
#include <platform/ConnectivityManager.h>
#include "esp_wifi.h"
#include "max_nvs.h"

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::endpoint;

static const char *TAG = "MATTER_BRIDGE";

static void app_event_cb(const ChipDeviceEvent *event, intptr_t arg)
{
    switch (event->Type) {
    case chip::DeviceLayer::DeviceEventType::PublicEventTypes::kInterfaceIpAddressChanged:
        ESP_LOGI(TAG, "Interface IP Address Changed");
        break;
    default:
        break;
    }
}

static bool g_auto_discovery_enabled = true;

extern "C" bool get_auto_discovery_state(void)
{
    return g_auto_discovery_enabled;
}

extern "C" void send_max_set_temperature(uint32_t target_address, float setpoint_temp, uint8_t max_mode);
static bool g_rx_is_updating_matter = false;

static esp_err_t app_attribute_update_cb(callback_type_t type, uint16_t endpoint_id, uint32_t cluster_id,
                                         uint32_t attribute_id, esp_matter_attr_val_t *val, void *priv_data)
{
    ESP_LOGI(TAG, "app_attribute_update_cb: ep=%d, cl=0x%lx, attr=0x%lx, cb_type=%d, val_type=%d", 
             endpoint_id, cluster_id, attribute_id, type, val ? val->type : -1);

    // Check if it's the On/Off cluster (0x0006) and OnOff attribute (0x0000)
    if (cluster_id == 0x0006 && attribute_id == 0x0000 && val && val->type == ESP_MATTER_VAL_TYPE_BOOLEAN && type == attribute::POST_UPDATE) {
        if (!g_rx_is_updating_matter) {
            max_device_t* dev = max_nvs_get_device_by_ep(endpoint_id);
            if (dev) {
                if (endpoint_id == dev->endpoint_id && dev->type == MAX_DEV_TYPE_PLUG_ADAPTER) {
                    bool is_on = val->val.b;
                    ESP_LOGI(TAG, "Plug Adapter toggled to %d for MAX! 0x%06X", is_on, (unsigned)dev->address);
                    send_max_set_temperature(dev->address, is_on ? 30.5f : 4.5f, 0x40);
                }
            } else {
                g_auto_discovery_enabled = val->val.b;
                ESP_LOGI(TAG, "Auto-Discovery mode set to: %s", g_auto_discovery_enabled ? "ON" : "OFF");
            }
        }
    }
    
    // Check if it's Thermostat cluster (0x0201)
    if (cluster_id == chip::app::Clusters::Thermostat::Id) {
        if (!g_rx_is_updating_matter && val && type == attribute::POST_UPDATE) {
            max_device_t* dev = max_nvs_get_device_by_ep(endpoint_id);
            if (dev && (dev->type == MAX_DEV_TYPE_THERMOSTAT || dev->type == MAX_DEV_TYPE_THERMOSTAT_PLUS || dev->type == MAX_DEV_TYPE_WALL_THERMOSTAT || dev->type == MAX_DEV_TYPE_PUSH_BUTTON)) {
                
                if (attribute_id == chip::app::Clusters::Thermostat::Attributes::OccupiedHeatingSetpoint::Id) {
                    float target_temp = 0.0f;
                    if (val->type == ESP_MATTER_VAL_TYPE_INT16 || val->type == ESP_MATTER_VAL_TYPE_NULLABLE_INT16) target_temp = val->val.i16 / 100.0f;
                    else if (val->type == ESP_MATTER_VAL_TYPE_INTEGER || val->type == ESP_MATTER_VAL_TYPE_INT32) target_temp = val->val.i32 / 100.0f;
                    
                    uint8_t current_mode = 4; // Default Heat
                    esp_matter_attr_val_t mode_val = esp_matter_invalid(NULL);
                    if (attribute::get_val(attribute::get(endpoint_id, cluster_id, chip::app::Clusters::Thermostat::Attributes::SystemMode::Id), &mode_val) == ESP_OK) {
                        current_mode = mode_val.val.u8;
                    }
                    
                    uint8_t max_mode = 0x40; // Default MANU
                    if (current_mode == 1) max_mode = 0x00; // AUTO
                    else if (current_mode == 5) max_mode = 0xC0; // BOOST
                    
                    ESP_LOGI(TAG, "Matter requested setpoint change: %.1f C for MAX! 0x%06X (SystemMode: %d)", target_temp, (unsigned int)dev->address, current_mode);
                    send_max_set_temperature(dev->address, target_temp, max_mode);
                }
                else if (attribute_id == chip::app::Clusters::Thermostat::Attributes::SystemMode::Id) {
                    uint8_t new_mode = val->val.u8;
                    
                    float target_temp = 20.0f; // Default fallback
                    esp_matter_attr_val_t temp_val = esp_matter_invalid(NULL);
                    if (attribute::get_val(attribute::get(endpoint_id, cluster_id, chip::app::Clusters::Thermostat::Attributes::OccupiedHeatingSetpoint::Id), &temp_val) == ESP_OK) {
                        if (temp_val.type == ESP_MATTER_VAL_TYPE_INT16 || temp_val.type == ESP_MATTER_VAL_TYPE_NULLABLE_INT16) {
                            target_temp = temp_val.val.i16 / 100.0f;
                        }
                    }
                    
                    uint8_t max_mode = 0x40; // Default MANU
                    if (new_mode == 0) { max_mode = 0x40; target_temp = 4.5f; } // OFF -> Manu 4.5
                    else if (new_mode == 1) { max_mode = 0x00; } // AUTO
                    else if (new_mode == 4) { max_mode = 0x40; } // HEAT
                    else if (new_mode == 5) { max_mode = 0xC0; } // EMERGENCY HEAT (BOOST)
                    
                    ESP_LOGI(TAG, "Matter requested mode change: %d for MAX! 0x%06X (Target: %.1f)", new_mode, (unsigned int)dev->address, target_temp);
                    send_max_set_temperature(dev->address, target_temp, max_mode);
                }
            } else {
                ESP_LOGW(TAG, "No matching MAX! device found for endpoint %d", endpoint_id);
            }
        }
    }
    
    return ESP_OK;
}

static esp_err_t app_identification_cb(identification::callback_type_t type, uint16_t endpoint_id, uint8_t effect_id,
                                       uint8_t effect_variant, void *priv_data)
{
    ESP_LOGI(TAG, "Identification callback on endpoint %d", endpoint_id);
    return ESP_OK;
}

// Wi-Fi is managed dynamically by the Matter BLE Commissioning process.
// We no longer hardcode credentials here.

static endpoint_t *g_aggregator_endpoint = NULL;

extern "C" void matter_bridge_init(void)
{
    ESP_LOGI(TAG, "Initializing Matter Bridge Node...");

    // Create the Root Node
    node::config_t node_config;
    
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char node_label[32];
    snprintf(node_label, sizeof(node_label), "MAX! Bridge %02X%02X", mac[4], mac[5]);
    
    strncpy(node_config.root_node.basic_information.node_label, node_label, sizeof(node_config.root_node.basic_information.node_label) - 1);
    
    node_t *node = node::create(&node_config, app_attribute_update_cb, app_identification_cb);
    if (!node) {
        ESP_LOGE(TAG, "Failed to create node");
        return;
    }
    
    char ble_name[16];
    snprintf(ble_name, sizeof(ble_name), "MAX-%02X%02X", mac[4], mac[5]);
    chip::DeviceLayer::ConnectivityMgr().SetBLEDeviceName(ble_name);

    // Create the Aggregator (Bridge)
    aggregator::config_t bridge_config;
    endpoint_t *bridge_endpoint = aggregator::create(node, &bridge_config, ENDPOINT_FLAG_NONE, NULL);
    if (!bridge_endpoint) {
        ESP_LOGE(TAG, "Failed to create aggregator endpoint");
        return;
    }
    g_aggregator_endpoint = bridge_endpoint;

    ESP_LOGI(TAG, "Bridge endpoint created successfully");

    // Create Virtual Switch for Auto-Discovery Mode as a Bridged Node
    endpoint::bridged_node::config_t bridged_node_config;
    endpoint_t *discovery_ep = endpoint::bridged_node::create(node, &bridged_node_config, ENDPOINT_FLAG_NONE, NULL);
    if (!discovery_ep) {
        ESP_LOGE(TAG, "Failed to create discovery switch endpoint");
    } else {
        endpoint::on_off_plugin_unit::config_t switch_config;
        switch_config.on_off.on_off = false;
        endpoint::on_off_plugin_unit::add(discovery_ep, &switch_config);
        
        cluster_t *bdbi = cluster::get(discovery_ep, 0x0039);
        if (bdbi) {
            char name[] = "MAX! Auto-Discovery";
            cluster::bridged_device_basic_information::attribute::create_node_label(bdbi, name, strlen(name));
            cluster::bridged_device_basic_information::attribute::create_product_name(bdbi, name, strlen(name));
            cluster::bridged_device_basic_information::attribute::create_vendor_name(bdbi, (char *)"busware.de", 10);

            char uid[] = "MAX-DISCOVERY";
            cluster::bridged_device_basic_information::attribute::create_unique_id(bdbi, uid, strlen(uid));
        }

        if (g_aggregator_endpoint) {
            endpoint::set_parent_endpoint(discovery_ep, g_aggregator_endpoint);
        }
        ESP_LOGI(TAG, "Virtual switch for Auto-Discovery created (Endpoint ID %d)", endpoint::get_id(discovery_ep));
    }
}

extern "C" void matter_bridge_start(void) {
    // Start Matter (this internally brings up ESP Wi-Fi stack and BLE if configured)
    esp_err_t err = esp_matter::start(app_event_cb);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start Matter stack: %d", err);
    } else {
        ESP_LOGI(TAG, "Matter stack started successfully. Waiting for BLE Commissioning...");
    }
}

extern "C" void update_matter_thermostat(uint32_t address, float actual_temp, float setpoint_temp, uint8_t max_mode)
{
    node_t *node = esp_matter::node::get();
    if (!node) return;

    max_device_t* dev = max_nvs_get_device(address);
    if (!dev || dev->endpoint_id == 0) return;

    endpoint_t *ep = endpoint::get(node, dev->endpoint_id);
    if (!ep) return;

    cluster_t *cluster = cluster::get(ep, chip::app::Clusters::Thermostat::Id);
    if (!cluster) return;

    int16_t target_temp = (int16_t)(setpoint_temp * 100);

    g_rx_is_updating_matter = true;

    uint8_t system_mode = 4; // Default Heat (Manual)
    if (max_mode == 0 || max_mode == 1) { // MAX! Auto (0) or Manu (1) mapped to Matter Heat (4)
        if (setpoint_temp <= 4.5f && setpoint_temp > -10.0f) system_mode = 0; // Off
        else system_mode = 4; // Heat
    } else if (max_mode == 2) system_mode = 4; // Party -> Heat
    else if (max_mode == 3) system_mode = 5; // Boost -> Emergency Heat

    esp_matter_attr_val_t mode_val = esp_matter_enum8(system_mode);
    esp_err_t err_mode = attribute::update(dev->endpoint_id, chip::app::Clusters::Thermostat::Id, chip::app::Clusters::Thermostat::Attributes::SystemMode::Id, &mode_val);
    if (err_mode != ESP_OK) ESP_LOGE(TAG, "Failed to update SystemMode: %d", err_mode);

    

    if (actual_temp > -100.0f) {
        int16_t current_temp = (int16_t)(actual_temp * 100);
        // Update LocalTemperature attribute (0x0000) using the built-in helper
        esp_matter_attr_val_t actual_val = esp_matter_nullable_int16(nullable<int16_t>(current_temp));
        esp_err_t err = attribute::update(dev->endpoint_id, chip::app::Clusters::Thermostat::Id, chip::app::Clusters::Thermostat::Attributes::LocalTemperature::Id, &actual_val);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to update LocalTemperature: %d", err);
        }
    }

    if (setpoint_temp > -100.0f) {
        // Update OccupiedHeatingSetpoint attribute (0x0012)
        esp_matter_attr_val_t target_val = esp_matter_int16(target_temp);
        esp_err_t err_targ = attribute::update(dev->endpoint_id, chip::app::Clusters::Thermostat::Id, chip::app::Clusters::Thermostat::Attributes::OccupiedHeatingSetpoint::Id, &target_val);
        if (err_targ != ESP_OK) {
            ESP_LOGE(TAG, "Failed to update TargetTemperature: %d", err_targ);
        }
    }

    g_rx_is_updating_matter = false;
    
    ESP_LOGI(TAG, "Matter UI Updated for MAX! %06X: Actual=%.1f C, Target=%.1f C, Mode=%d", (unsigned)address, actual_temp, setpoint_temp, system_mode);
}

extern "C" void update_matter_contact(uint32_t address, bool is_open)
{
    node_t *node = esp_matter::node::get();
    if (!node) return;

    max_device_t* dev = max_nvs_get_device(address);
    if (!dev || dev->endpoint_id == 0) return;

    endpoint_t *ep = endpoint::get(node, dev->endpoint_id);
    if (!ep) return;

    cluster_t *cluster = cluster::get(ep, chip::app::Clusters::BooleanState::Id);
    if (!cluster) return;

    // Update StateValue attribute (0x0000)
    // Matter Contact Sensor: TRUE = Open, FALSE = Closed.
    // However, the physical MAX! window sensor logic was observed to be inverted by the user
    // (it reports 0x02 when closed and 0x00 when open, or similar).
    // So we invert the logic here to match reality.
    esp_matter_attr_val_t state_val = esp_matter_invalid(NULL);
    state_val.type = ESP_MATTER_VAL_TYPE_BOOLEAN;
    state_val.val.b = !is_open; 
    g_rx_is_updating_matter = true;
    attribute::update(dev->endpoint_id, chip::app::Clusters::BooleanState::Id, chip::app::Clusters::BooleanState::Attributes::StateValue::Id, &state_val);
    g_rx_is_updating_matter = false;
    
    ESP_LOGI(TAG, "Matter UI Updated for MAX! Contact %06X: PhysicalOpenBit=%d -> MatterState(Open)=%d", (unsigned)address, is_open, state_val.val.b);
}

extern "C" void update_matter_plug(uint32_t address, bool is_on)
{
    node_t *node = esp_matter::node::get();
    if (!node) return;

    max_device_t* dev = max_nvs_get_device(address);
    if (!dev || dev->endpoint_id == 0) return;

    endpoint_t *ep = endpoint::get(node, dev->endpoint_id);
    if (!ep) return;

    cluster_t *cluster = cluster::get(ep, chip::app::Clusters::OnOff::Id);
    if (!cluster) return;

    esp_matter_attr_val_t state_val = esp_matter_bool(is_on);
    
    g_rx_is_updating_matter = true;
    attribute::update(dev->endpoint_id, chip::app::Clusters::OnOff::Id, chip::app::Clusters::OnOff::Attributes::OnOff::Id, &state_val);
    g_rx_is_updating_matter = false;
    
    ESP_LOGI(TAG, "Matter UI Updated for MAX! Plug %06X: OnOff=%d", (unsigned)address, is_on);
}

extern "C" bool add_max_device_to_matter(max_device_t *device)
{
    if (!device) return false;
    
    // Get the node object from somewhere (maybe save it in a global variable in matter_bridge_init?)
    // Wait, let's look at esp_matter::node::get()
    node_t *node = esp_matter::node::get();
    if (!node) {
        ESP_LOGE(TAG, "Cannot get matter node");
        return false;
    }

    if (device->type == MAX_DEV_TYPE_THERMOSTAT || 
        device->type == MAX_DEV_TYPE_THERMOSTAT_PLUS ||
        device->type == MAX_DEV_TYPE_WALL_THERMOSTAT ||
        device->type == MAX_DEV_TYPE_VIRTUAL_THERMOSTAT || device->type == MAX_DEV_TYPE_PUSH_BUTTON) {
        endpoint::bridged_node::config_t bridged_node_config;
        endpoint_t *ep = NULL;
        if (device->endpoint_id > 0) {
            ep = endpoint::bridged_node::resume(node, &bridged_node_config, ENDPOINT_FLAG_NONE, device->endpoint_id, NULL);
            if (!ep) ep = endpoint::bridged_node::create(node, &bridged_node_config, ENDPOINT_FLAG_NONE, NULL);
        } else {
            ep = endpoint::bridged_node::create(node, &bridged_node_config, ENDPOINT_FLAG_NONE, NULL);
        }
        if (!ep) return false;
        
        endpoint::thermostat::config_t thermo_config;
        // Only enable Heat feature (prevents Matter from forcing Cooling attributes via AutoMode)
        thermo_config.thermostat.feature_flags = cluster::thermostat::feature::heating::get_id();
        // HA matter integration often fails to init entities if local_temperature is null initially.
        thermo_config.thermostat.local_temperature = nullable<int16_t>(2000); 
        // 2 = Heating Only (Matter Spec).
        thermo_config.thermostat.control_sequence_of_operation = 2;
        thermo_config.thermostat.system_mode = 4; // Heat
        thermo_config.thermostat.features.heating.occupied_heating_setpoint = 2000;
        
        endpoint::thermostat::add(ep, &thermo_config);
        
        cluster_t *thermo_cluster = cluster::get(ep, chip::app::Clusters::Thermostat::Id);
        if (thermo_cluster) {
            cluster::thermostat::attribute::create_abs_min_heat_setpoint_limit(thermo_cluster, 450);
            cluster::thermostat::attribute::create_abs_max_heat_setpoint_limit(thermo_cluster, 3050);
            cluster::thermostat::attribute::create_min_heat_setpoint_limit(thermo_cluster, 450);
            cluster::thermostat::attribute::create_max_heat_setpoint_limit(thermo_cluster, 3050);
        }

        cluster_t *bdbi = cluster::get(ep, 0x0039);
        if (bdbi) {
            char name[32];
            snprintf(name, sizeof(name), "MAX! Thermo %06lX", (unsigned long)device->address);
            cluster::bridged_device_basic_information::attribute::create_node_label(bdbi, name, strlen(name));
            cluster::bridged_device_basic_information::attribute::create_product_name(bdbi, name, strlen(name));
            cluster::bridged_device_basic_information::attribute::create_vendor_name(bdbi, (char *)"busware.de", 10);

            char uid[32];
            snprintf(uid, sizeof(uid), "MAX-T-%06lX", (unsigned long)device->address);
            cluster::bridged_device_basic_information::attribute::create_unique_id(bdbi, uid, strlen(uid));
        }

        // Tie to Aggregator
        if (g_aggregator_endpoint) {
            endpoint::set_parent_endpoint(ep, g_aggregator_endpoint);
        }

        // Dynamically enable the endpoint since Matter stack is already started
        if (esp_matter::is_started()) { endpoint::enable(ep); }
        
        device->endpoint_id = endpoint::get_id(ep);
        ESP_LOGI(TAG, "Added Matter Thermostat for MAX! 0x%06x (Endpoint %d)", (unsigned)device->address, device->endpoint_id);


        esp_matter_attr_val_t seq_val = esp_matter_uint8(2);
        attribute::update(device->endpoint_id, chip::app::Clusters::Thermostat::Id, chip::app::Clusters::Thermostat::Attributes::ControlSequenceOfOperation::Id, &seq_val);
        esp_matter_attr_val_t temp_val = esp_matter_nullable_int16(nullable<int16_t>(2000));
        attribute::update(device->endpoint_id, chip::app::Clusters::Thermostat::Id, chip::app::Clusters::Thermostat::Attributes::LocalTemperature::Id, &temp_val);

        max_nvs_update_device(device); // Save updated endpoint IDs
        return true;
    } else if (device->type == MAX_DEV_TYPE_SHUTTER_CONTACT ||
               device->type == MAX_DEV_TYPE_VIRTUAL_SHUTTER) {
        endpoint::bridged_node::config_t bridged_node_config;
        endpoint_t *ep = NULL;
        if (device->endpoint_id > 0) {
            ep = endpoint::bridged_node::resume(node, &bridged_node_config, ENDPOINT_FLAG_NONE, device->endpoint_id, NULL);
            if (!ep) ep = endpoint::bridged_node::create(node, &bridged_node_config, ENDPOINT_FLAG_NONE, NULL);
        } else {
            ep = endpoint::bridged_node::create(node, &bridged_node_config, ENDPOINT_FLAG_NONE, NULL);
        }
        if (!ep) return false;
        
        endpoint::contact_sensor::config_t contact_config;
        // Matter Contact Sensor: FALSE = Closed, TRUE = Open.
        contact_config.boolean_state.state_value = false; // Initialize as Closed
        endpoint::contact_sensor::add(ep, &contact_config);
        
        cluster_t *bdbi = cluster::get(ep, 0x0039);
        if (bdbi) {
            char name[32];
            snprintf(name, sizeof(name), "MAX! Contact %06lX", (unsigned long)device->address);
            cluster::bridged_device_basic_information::attribute::create_node_label(bdbi, name, strlen(name));
            cluster::bridged_device_basic_information::attribute::create_product_name(bdbi, name, strlen(name));
            cluster::bridged_device_basic_information::attribute::create_vendor_name(bdbi, (char *)"busware.de", 10);

            char uid[32];
            snprintf(uid, sizeof(uid), "MAX-C-%06lX", (unsigned long)device->address);
            cluster::bridged_device_basic_information::attribute::create_unique_id(bdbi, uid, strlen(uid));
        }

        if (g_aggregator_endpoint) {
            endpoint::set_parent_endpoint(ep, g_aggregator_endpoint);
        }

        // Dynamically enable the endpoint since Matter stack is already started
        if (esp_matter::is_started()) { endpoint::enable(ep); }

        device->endpoint_id = endpoint::get_id(ep);
        ESP_LOGI(TAG, "Added Matter Contact for MAX! 0x%06x (Endpoint %d)", (unsigned)device->address, device->endpoint_id);
        return true;
    } else if (device->type == MAX_DEV_TYPE_PLUG_ADAPTER) {
        endpoint::bridged_node::config_t bridged_node_config;
        endpoint_t *ep = NULL;
        if (device->endpoint_id > 0) {
            ep = endpoint::bridged_node::resume(node, &bridged_node_config, ENDPOINT_FLAG_NONE, device->endpoint_id, NULL);
            if (!ep) ep = endpoint::bridged_node::create(node, &bridged_node_config, ENDPOINT_FLAG_NONE, NULL);
        } else {
            ep = endpoint::bridged_node::create(node, &bridged_node_config, ENDPOINT_FLAG_NONE, NULL);
        }
        if (!ep) return false;

        endpoint::on_off_plugin_unit::config_t plug_config;
        plug_config.on_off.on_off = false;
        endpoint::on_off_plugin_unit::add(ep, &plug_config);

        cluster_t *bdbi = cluster::get(ep, 0x0039);
        if (bdbi) {
            char name[32];
            snprintf(name, sizeof(name), "MAX! Plug %06lX", (unsigned long)device->address);
            cluster::bridged_device_basic_information::attribute::create_node_label(bdbi, name, strlen(name));
            cluster::bridged_device_basic_information::attribute::create_product_name(bdbi, name, strlen(name));
            cluster::bridged_device_basic_information::attribute::create_vendor_name(bdbi, (char *)"busware.de", 10);

            char uid[32];
            snprintf(uid, sizeof(uid), "MAX-P-%06lX", (unsigned long)device->address);
            cluster::bridged_device_basic_information::attribute::create_unique_id(bdbi, uid, strlen(uid));
        }

        if (g_aggregator_endpoint) {
            endpoint::set_parent_endpoint(ep, g_aggregator_endpoint);
        }

        // Dynamically enable the endpoint since Matter stack is already started
        if (esp_matter::is_started()) { endpoint::enable(ep); }

        device->endpoint_id = endpoint::get_id(ep);
        ESP_LOGI(TAG, "Added Matter Plug for MAX! 0x%06x (Endpoint %d)", (unsigned)device->address, device->endpoint_id);
        return true;
    }

    return false;
}
