/*
 * Implementation for USBRH(Strawberry Linux Co.,Ltd. Hygrometer/Thermometer) with HIDAPI
 *
 * Copyright (c) 2021 stzlab
 *
 * This software is released under the MIT License, see LICENSE.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>
#include <hidapi/hidapi.h>

#define USBRH_VENDOR_ID			0x1774
#define USBRH_PRODUCT_ID		0x1001

#define USBRH_BUFFER_SIZE		7

static int debug = 0;

struct usbrh_sensor_value {
	unsigned char humiMSB;
	unsigned char humiLSB;
	unsigned char tempMSB;
	unsigned char tempLSB;
	unsigned char reserved[3];
};

struct usbrh_firmware_version {
	unsigned char year;
	unsigned char month;
	unsigned char date;
	unsigned char reserved[4];
};

void hex_dump(const void* buf, int size)
{
	int i;
	unsigned char* ptr;

	ptr = (unsigned char*)buf;
	for (i = 0; i < size; i++) {
		fprintf(stderr, "%02hhx ", *ptr++);
	}
	fprintf(stderr, "\n");
}

int usbrh_read_sensor(hid_device* dev, struct usbrh_sensor_value* value)
{
	int result;
	int read_size;
	unsigned char buffer[USBRH_BUFFER_SIZE];

	memset(buffer, 0, sizeof(buffer));
	buffer[0] = 0x00; // Report ID

	result = hid_write(dev, buffer, sizeof(buffer));
	if (debug) {
		fprintf(stderr, "debug: hid_write: ");
		hex_dump(buffer, sizeof(buffer));
	}
	if (result < 0) {
		fprintf(stderr, "error: hid_write: %ls\n", hid_error(dev));
		return -1;
	}

	read_size = hid_read_timeout(dev, (unsigned char*)value, sizeof(*value), 5000);
	if (read_size < 0) {
		fprintf(stderr, "error: hid_read_timeout: %ls\n", hid_error(dev));
		return -1;
	}
	if (debug) {
		fprintf(stderr, "debug: usbrh_read_sensor: ");
		hex_dump(value, read_size);
	}

	return (read_size == sizeof(*value)) ? 0 : -1;
}

int usbrh_ctrl_led(hid_device* dev, unsigned char led_index, unsigned char is_on)
{
	int result;
	unsigned char buffer[USBRH_BUFFER_SIZE + 1];

	memset(buffer, 0, sizeof(buffer));
	buffer[0] = 0x00;			// Report ID
	buffer[1] = 3 + led_index;	// 0x03:Green, 0x04:Red
	buffer[2] = is_on;			// 0x00:Off, 0x01:On

	result = hid_send_feature_report(dev, buffer, sizeof(buffer));
	if (debug) {
		fprintf(stderr, "debug: hid_send_feature_report: ");
		hex_dump(buffer, sizeof(buffer));
	}
	if (result < 0) {
		fprintf(stderr, "error: hid_send_feature_report: %ls", hid_error(dev));
		return -1;
	}

	return 0;
}

int usbrh_ctrl_heater(hid_device* dev, unsigned char is_on)
{
	int result;
	unsigned char buffer[USBRH_BUFFER_SIZE + 1];

	memset(buffer, 0, sizeof(buffer));
	buffer[0] = 0x00;		// Report ID
	buffer[1] = 1;			// 0x01:Heater
	buffer[2] = is_on << 2;	// 0x00:Off, 0x02:On

	result = hid_send_feature_report(dev, buffer, sizeof(buffer));
	if (debug) {
		fprintf(stderr, "debug: hid_send_feature_report: ");
		hex_dump(buffer, sizeof(buffer));
	}
	if (result < 0) {
		fprintf(stderr, "error: hid_send_feature_report: %ls", hid_error(dev));
		return -1;
	}

	return 0;
}

int usbrh_get_version(hid_device* dev, struct usbrh_firmware_version* version)
{
	int read_size;

	read_size = hid_get_feature_report(dev, (unsigned char*)version, sizeof(*version));
	if (debug) {
		fprintf(stderr, "debug: hid_get_feature_report: ");
		hex_dump(version, read_size);
	}
	if (read_size < 0) {
		fprintf(stderr, "error: hid_get_feature_report: %ls", hid_error(dev));
		return -1;
	}
	return 0;
}

/*
 * SHT1x
 * https://www.sensirion.com/fileadmin/user_upload/customers/sensirion/Dokumente/2_Humidity_Sensors/Sensirion_Humidity_Sensors_SHT1x_Datasheet_V5.pdf
 * https://www.sensirion.com/fileadmin/user_upload/customers/sensirion/Dokumente/1_Subsidiaries_Documents/1_Japan/1_Humidity_Sensors/Sensirion_Humidity_Sensors_SHT1x_Datasheet_V5b_J.pdf
 */
float usbrh_calc_temp(struct usbrh_sensor_value* value)
{
	const float D1 = -40.1;		// for 5V Ticks
	const float D2 = +0.01;		// for 14bit T_C

	int so_t;
	float t_c;

	so_t = (value->tempMSB) << 8 | value->tempLSB;
	t_c = D1 + D2 * so_t;

	return t_c;
}

float usbrh_calc_humi(struct usbrh_sensor_value* value)
{
	const float C1 = -2.0468;		// for 12bit SO_RH
	const float C2 = +0.0367;		// for 12bit SO_RH
	const float C3 = -0.0000015955;	// for 12bit SO_RH
	const float T1 = +0.01;			// for 12bit SO_RH
	const float T2 = +0.00008;		// for 12bit SO_RH

	int so_rh;
	float rh_linear;
	float t_c;
	float rh_true;

	so_rh = (value->humiMSB) << 8 | value->humiLSB;

	// Calculate humidity from ticks to [%RH]
	rh_linear = C1  + C2 * so_rh + C3 * so_rh * so_rh;
	// Calculate temperature compensated humidity [%RH]
	t_c = usbrh_calc_temp(value);
	rh_true = (t_c - 25) * (T1 + T2 * so_rh) + rh_linear;	

	// Cut if the value is outside of
	if (rh_true > 100) {
		rh_true = 100;
	}
	// The physical possible range
	if (rh_true < 0.1) {
		rh_true = 0.1;
	}
	return rh_linear;
}

void usage()
{
	printf("USBRH with HDAPI 1.0\n");
	printf("Usage: usbrh [-dlfVRGH]\n");
	printf("  -d  : Enable debugging\n");
	printf("  -h  : Show usage\n");
	printf("  -l  : Show device list\n");
	printf("  -sn : Specify device number (n=0:all)\n");
	printf("  -V  : Show firmware version\n");
	printf("  -Rn : Control Red LED   (0:off, 1:on)\n");
	printf("  -Gn : Control Green LED (0:off, 1:on)\n");
	printf("  -Hn : Control Heater    (0:off, 1:on)\n");
}

int main(int argc, char* argv[])
{
	unsigned int opt;

	int result;

	int show_devlist;
	int show_version;

	int dev_number;
	int dev_count;
	int proc_count;

	int ctrl_led_red;
	int ctrl_led_green;
	int ctrl_heater;
	int parm_led_red;
	int parm_led_green;
	int parm_heater;

	time_t now;
	struct tm *tm_now;

	hid_device* dev;
	struct hid_device_info* devinfo;
	struct hid_device_info* current;

	struct usbrh_sensor_value value;
	struct usbrh_firmware_version version;

	show_devlist   = 0;
	show_version   = 0;
	dev_number     = 0;
	ctrl_led_red   = 0;
	ctrl_led_green = 0;
	ctrl_heater    = 0;
	parm_led_red   = 0;
	parm_led_green = 0;
	parm_heater    = 0;

	while((opt = getopt(argc, argv,"ls:VhdR:G:H:")) != -1){
		switch(opt){
			case 'l':
				show_devlist = 1;
				break;
			case 's':
				dev_number = atoi(optarg);
				break;
			case 'V':
				show_version = 1;
				break;
			case 'h':
				usage();
				exit(0);
				break;
			case 'd':
				debug = 1;
				break;
			case 'R':
				ctrl_led_red = 1;
				parm_led_red = atoi(optarg);
				break;
			case 'G':
				ctrl_led_green = 1;
				parm_led_green = atoi(optarg);
				break;
			case 'H':
				ctrl_heater = 1;
				parm_heater = atoi(optarg);
				break;
			default:
				fprintf(stderr, "error: invalid option\n");
				exit(1);
				break;
		}
	}

	result = hid_init();
	if (result != 0) {
		fprintf(stderr, "error: hid_init: %d\n", result);
 		exit(1);
	}

	result = 0;
	dev_count = 0;
	proc_count = 0;
	devinfo = hid_enumerate(USBRH_VENDOR_ID, USBRH_PRODUCT_ID);
	current = devinfo;
	while (current) {
		dev_count++;
		if (!show_devlist) {
			if (debug) {
				fprintf(stderr, "debug: devicenumber: %d\n", dev_count);
				fprintf(stderr, "debug: path: %s\n", current->path);
			}
			if (dev_number == 0 || dev_count == dev_number) {
				if (proc_count == 0) {
					now = time(NULL);
					tm_now = localtime(&now);
					printf("tm:%04d/%02d/%02d-%02d:%02d:%02d ", tm_now->tm_year + 1900, tm_now->tm_mon + 1, tm_now->tm_mday,
								    tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec);
				}

				dev = hid_open_path(current->path);
				proc_count++;
				if (!dev) {
					fprintf(stderr, "error: hid_open_path: %ls\n", hid_error(dev));
					result = 1;
				}

				if(show_version){
					if(usbrh_get_version(dev, &version) != 0) {
						fprintf(stderr, "error: usbrh_get_version\n");
						result = 1;
					} else {
						printf("v%d:%02d/%02d/%02d ", dev_count, version.year, version.month, version.date);
					}
				}

				if(ctrl_led_red){
					if(usbrh_ctrl_led(dev, 1, parm_led_red) != 0) {
						fprintf(stderr, "error: usbrh_ctrl_led\n");
						result = 1;
					}
				}

				if(ctrl_led_green){
					if(usbrh_ctrl_led(dev, 0, parm_led_green) != 0) {
						fprintf(stderr, "error: usbrh_ctrl_led\n");
						result = 1;
					}
				}

				if(ctrl_heater){
					if(usbrh_ctrl_heater(dev, parm_heater) != 0) {;
						fprintf(stderr, "error: usbrh_ctrl_heater\n");
						result = 1;
					}
				}

				if (usbrh_read_sensor(dev, &value) != 0) {
					fprintf(stderr, "error: usbrh_read_sensor\n");
					result = 1;
				} else {
					printf("tc%d:%.2f rh%d:%.2f ", dev_count, usbrh_calc_temp(&value), dev_count, usbrh_calc_humi(&value));
				}

				hid_close(dev);
			}
		} else {
			if (debug) {
				fprintf(stderr, "debug: DeviceNumber      : %d\n",    dev_count);
				fprintf(stderr, "debug: Path              : %s\n",    current->path);
				fprintf(stderr, "debug: VendorID          : %04hx\n", current->vendor_id);
				fprintf(stderr, "debug: ProductID         : %04hx\n", current->product_id);
				fprintf(stderr, "debug: SerialNumber      : %ls\n",   current->serial_number);
				fprintf(stderr, "debug: ReleaseNumber     : %hx\n",   current->release_number);
				fprintf(stderr, "debug: ManufacturerString: %ls\n",   current->manufacturer_string);
				fprintf(stderr, "debug: ProductString     : %ls\n",   current->product_string);
				fprintf(stderr, "debug: InterfaceNumber   : %d\n",    current->interface_number);
			}
			printf("%d:%s\n", dev_count, current->path);
		}
		current = current->next;
	}

	if (!show_devlist) {
		if (proc_count == 0) {
			fprintf(stderr, "error: device not found\n");
			result = 1;
		} else {
			printf("\n");
		}
	} else {
		printf("%d device(s) found\n", dev_count);
	}

	hid_free_enumeration(devinfo);
	result = hid_exit();
	if (result != 0) {
		fprintf(stderr, "error: hid_exit: %d\n", result);
	}

	exit(result);
}
