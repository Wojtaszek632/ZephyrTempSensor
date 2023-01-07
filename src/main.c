

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/printk.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/drivers/uart.h>

#include <stdbool.h>
#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <zephyr/sys/byteorder.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/services/bas.h>

#define SENSOR_1_NAME				"Temperature Sensor 1"

#define SENSOR_3_NAME				"Pressure Sensor"


/* ESS error definitions */
#define ESS_ERR_WRITE_REJECT			0x80 //error code that is returned when a write operation is rejected
#define ESS_ERR_COND_NOT_SUPP			0x81 //error code that is returned when a condition is not supported


BUILD_ASSERT(DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_console), zephyr_cdc_acm_uart),
	     "Console device is not ACM CDC UART device");

static const struct device *get_bme280_device(void)
{
	const struct device *const dev = DEVICE_DT_GET_ANY(bosch_bme280);

	if (dev == NULL) {
		/* No such node, or the node does not have status "okay". */
		printk("\nError: no device found.\n");
		return NULL;
	}

	if (!device_is_ready(dev)) {
		printk("\nError: Device \"%s\" is not ready; "
		       "check the driver initialization logs for errors.\n",
		       dev->name);
		return NULL;
	}

	printk("Found device \"%s\", getting sensor data\n", dev->name);
	return dev;
}

static ssize_t read_u16(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			void *buf, uint16_t len, uint16_t offset)
{
	const uint16_t *u16 = attr->user_data;
	uint16_t value = sys_cpu_to_le16(*u16);

	return bt_gatt_attr_read(conn, attr, buf, len, offset, &value,sizeof(value));
}

static ssize_t read_u32(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			void *buf, uint16_t len, uint16_t offset)
{
	const uint32_t *u32 = attr->user_data;
	uint32_t value = sys_cpu_to_le32(*u32);

	return bt_gatt_attr_read(conn, attr, buf, len, offset, &value,sizeof(value));
}

/* Environmental Sensing Service Declaration */

struct temperature_sensor {
	int16_t temp_value;	
	};

struct pressure_sensor {
	uint32_t press_value;
	
};

static struct temperature_sensor sensor_1 = {
		.temp_value = 2115,
};

static struct pressure_sensor sensor_3 = {
		.press_value = 6322,		
};

static void temp_ccc_cfg_changed(const struct bt_gatt_attr *attr,
				 uint16_t value)
{
	printk("temp_ccc_cfg_changed\n");
}

static void press_ccc_cfg_changed(const struct bt_gatt_attr *attr,
				 uint16_t value)
{
	printk("press_ccc_cfg_changed\n");
}


static void update_temperature(struct bt_conn *conn,
			       const struct bt_gatt_attr *chrc, int32_t value,
			       struct temperature_sensor *sensor)
{
	/* Update temperature value */
	sensor->temp_value = value;
		
	value = sys_cpu_to_le16(sensor->temp_value);

	bt_gatt_notify(conn, chrc, &value, sizeof(value));
}

static void update_preassure(struct bt_conn *conn,
			       const struct bt_gatt_attr *chrc, int32_t value,
			       struct pressure_sensor *sensor)
{
	/* Update temperature value */
	sensor->press_value = (uint32_t)value;
		
	value = sys_cpu_to_le32(sensor->press_value);

	bt_gatt_notify(conn, chrc, &value, sizeof(value));
}
BT_GATT_SERVICE_DEFINE(ess_svc,
	//gatt_attrb_indx = 0
	BT_GATT_PRIMARY_SERVICE(BT_UUID_ESS), 				

	/* Temperature Sensor */ //gatt_attrb_indx = 1 & 2
	BT_GATT_CHARACTERISTIC(BT_UUID_TEMPERATURE, 		
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_READ,
			       read_u16, NULL, &sensor_1.temp_value),
	//gatt_attrb_indx = 3
	BT_GATT_CCC(temp_ccc_cfg_changed, 					
		    	   BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
	//gatt_attrb_indx = 4
	BT_GATT_CUD(SENSOR_1_NAME, BT_GATT_PERM_READ), 		
		
	/* Preassure Sensor */	//gatt_attrb_indx = 5 & 6
	BT_GATT_CHARACTERISTIC(BT_UUID_PRESSURE, 			
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_READ,
			       read_u32, NULL, &sensor_3.press_value),
    //gatt_attrb_indx = 7
	BT_GATT_CCC(press_ccc_cfg_changed,					
		   		   BT_GATT_PERM_READ | BT_GATT_PERM_WRITE), 
	//gatt_attrb_indx = 8
	BT_GATT_CUD(SENSOR_3_NAME, BT_GATT_PERM_READ), 	  
);

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_GAP_APPEARANCE, 0x00, 0x03),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL,
		      BT_UUID_16_ENCODE(BT_UUID_ESS_VAL),),
};

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		printk("Connection failed (err 0x%02x)\n", err);
	} else {
		printk("Connected\n");
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	printk("Disconnected (reason 0x%02x)\n", reason);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

static void bt_ready(void)
{
	int err;

	printk("Bluetooth initialized\n");

	err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
		return;
	}

	printk("Advertising successfully started\n");
}

static void auth_passkey_display(struct bt_conn *conn, unsigned int passkey)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Passkey for %s: %06u\n", addr, passkey);
}

static void auth_cancel(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Pairing cancelled: %s\n", addr);
}

static struct bt_conn_auth_cb auth_cb_display = {
	.passkey_display = auth_passkey_display,
	.passkey_entry = NULL,
	.cancel = auth_cancel,
};

void main(void)
{
	/*Serial*/
	const struct device *const dev2 = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
	uint32_t dtr = 0;

	if (usb_enable(NULL)) {
		return;
	}

	//while (!dtr) {
		uart_line_ctrl_get(dev2, UART_LINE_CTRL_DTR, &dtr);
		/* Give CPU resources to low priority threads. */
	//	k_sleep(K_MSEC(100));
	//}

	/*BMP280*/
	const struct device *dev = get_bme280_device();
	if (dev == NULL) {
		printk("BMP init failed\n");
		return;
	}

	/*BLE*/
	int err;
	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	bt_ready();
	bt_conn_auth_cb_register(&auth_cb_display);
	struct sensor_value temp, press;

	while (1) {
		
		sensor_sample_fetch(dev);
		sensor_channel_get(dev, SENSOR_CHAN_AMBIENT_TEMP, &temp);
		sensor_channel_get(dev, SENSOR_CHAN_PRESS, &press);
		
		/* Temperature Sensor */ //gatt_attrb_indx = 1 & 2
		update_temperature(NULL, &ess_svc.attrs[2], 
						temp.val1 * 100 + temp.val2 / 10000, 
						&sensor_1);

		/* Preassure Sensor */	//gatt_attrb_indx = 5 & 6
		update_preassure(NULL, &ess_svc.attrs[6],
						press.val1 * 10000 + press.val2 / 100,
		 				&sensor_3);
		
		printk("temp: %d.%06d; press: %d.%06d;\n",temp.val1, temp.val2, press.val1, press.val2);
		
		k_sleep(K_MSEC(1000));
	}
}

