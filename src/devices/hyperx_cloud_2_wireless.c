#include "../device.h"
#include "../utility.h"
#include <hidapi.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define VENDOR_HYPERX             0x03F0
#define ID_HYPERX_CLOUD2_WIRELESS 0x0696

#define WRITE_PACKET_SIZE 52
#define WRITE_TIMEOUT     50
#define READ_PACKET_SIZE  20
#define READ_TIMEOUT      1000

#define CMD_GET_BATTERY_LEVEL    0x02
#define CMD_GET_BATTERY_CHARGING 0x03

#define STATUS_SUCCESS   0
#define ERR_HID_IO       -1
#define ERR_TIMEOUT      -2
#define ERR_INVALID_RESP -3
#define ERR_BAD_INDEX    -4

static struct device device_hyperx_cloud2_wireless;

static const uint16_t PRODUCT_IDS[] = { ID_HYPERX_CLOUD2_WIRELESS };

static BatteryInfo hyperx_cloud2_wireless_request_battery(hid_device* device_handle);

void hyperx_cloud_2_wireless_init(struct device** device)
{
    device_hyperx_cloud2_wireless.idVendor            = VENDOR_HYPERX;
    device_hyperx_cloud2_wireless.idProductsSupported = PRODUCT_IDS;
    device_hyperx_cloud2_wireless.numIdProducts       = sizeof(PRODUCT_IDS) / sizeof(PRODUCT_IDS[0]);

    strncpy(device_hyperx_cloud2_wireless.device_name, "HyperX Cloud II Wireless", sizeof(device_hyperx_cloud2_wireless.device_name));

    device_hyperx_cloud2_wireless.capabilities                           = B(CAP_BATTERY_STATUS);
    device_hyperx_cloud2_wireless.capability_details[CAP_BATTERY_STATUS] = (struct capability_detail) { .usagepage = 0xFF90, .usageid = 0x0303, .interface = 0x0000 };

    device_hyperx_cloud2_wireless.request_battery = &hyperx_cloud2_wireless_request_battery;

    *device = &device_hyperx_cloud2_wireless;
}

int hyperx_cloud2_wireless_get_raw_status(hid_device* device_handle, unsigned char cmd_id, int result_index)
{
    if (!device_handle)
        return ERR_HID_IO;
    if (result_index < 0 || result_index >= READ_PACKET_SIZE)
        return ERR_BAD_INDEX;

    unsigned char write_buf[WRITE_PACKET_SIZE] = { 0x06, 0xFF, 0xBB, cmd_id, 0x00 };
    int write_res                              = hid_write(device_handle, write_buf, WRITE_PACKET_SIZE);
    usleep(WRITE_TIMEOUT);
    if (write_res < 0)
        return ERR_HID_IO;

    unsigned char read_buf[READ_PACKET_SIZE] = { 0x00 };
    int read_res                             = hid_read_timeout(device_handle, read_buf, READ_PACKET_SIZE, READ_TIMEOUT);
    if (read_res < 0)
        return ERR_HID_IO;
    else if (read_res == 0)
        return ERR_TIMEOUT;
    else if (read_res == READ_PACKET_SIZE && read_buf[0] == 0x06 && read_buf[1] == 0xFF && read_buf[2] == 0xBB && read_buf[3] == cmd_id) {
        return (int)read_buf[result_index];
    }

    return ERR_INVALID_RESP;
}

BatteryInfo hyperx_cloud2_wireless_request_battery(hid_device* device_handle)
{
    BatteryInfo info = { .status = BATTERY_UNAVAILABLE, .level = -1 };
    if (!device_handle) {
        info.status = BATTERY_HIDERROR;
        return info;
    }

    int raw_level = hyperx_cloud2_wireless_get_raw_status(device_handle, CMD_GET_BATTERY_LEVEL, 7);
    if (raw_level < 0) {
        if (raw_level == ERR_HID_IO)
            info.status = BATTERY_HIDERROR;
        else if (raw_level == ERR_TIMEOUT)
            info.status = BATTERY_TIMEOUT;
        else
            info.status = BATTERY_UNAVAILABLE;
        return info;
    }
    info.level = (raw_level > 100) ? 100 : raw_level;

    int raw_charging = hyperx_cloud2_wireless_get_raw_status(device_handle, CMD_GET_BATTERY_CHARGING, 4);
    if (raw_charging < 0) {
        if (raw_charging == ERR_HID_IO)
            info.status = BATTERY_HIDERROR;
        else if (raw_charging == ERR_TIMEOUT)
            info.status = BATTERY_TIMEOUT;
        else
            info.status = BATTERY_UNAVAILABLE;
        return info;
    }
    info.status = raw_charging == 1 ? BATTERY_CHARGING : BATTERY_AVAILABLE;

    return info;
}
