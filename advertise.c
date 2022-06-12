//
//  Intel Edison Playground
//  Copyright (c) 2015 Damian Kołakowski. All rights reserved.
//

#include <stdlib.h>
#include <errno.h>
#include <curses.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include "bt.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>

bool app_stopped = false;

void sigint_handler(int sig)
{
	if (sig == SIGINT)
	{
		// ctrl+c退出时执行的代码
		printf("will exit\n");
		app_stopped = true;
	}
}

struct hci_request ble_hci_request(uint16_t ocf, int clen, void *status, void *cparam)
{
	struct hci_request rq;
	memset(&rq, 0, sizeof(rq));
	rq.ogf = OGF_LE_CTL;
	rq.ocf = ocf;
	rq.cparam = cparam;
	rq.clen = clen;
	rq.rparam = status;
	rq.rlen = 1;
	return rq;
}

struct bt_hci_cmd_le_set_ext_adv_data *ble_hci_params_for_set_adv_data(char *name)
{
	int name_len = strlen(name);
	int data_len = 2 + name_len + 1;
	struct bt_hci_cmd_le_set_ext_adv_data *adv_data_cp =
		(struct bt_hci_cmd_le_set_ext_adv_data *)malloc(sizeof(struct bt_hci_cmd_le_set_ext_adv_data) + data_len);
	memset(adv_data_cp, 0, sizeof(struct bt_hci_cmd_le_set_ext_adv_data) + data_len);
	adv_data_cp->data_len = data_len;
	adv_data_cp->handle = 0;
	adv_data_cp->operation = 0x03;
	adv_data_cp->fragment_preference = 0x01;
	// Build simple advertisement data bundle according to:
	// - ​"Core Specification Supplement (CSS) v5"
	// ( https://www.bluetooth.org/en-us/specification/adopted-specifications )

	adv_data_cp->data[0] = 0x37;
	adv_data_cp->data[1] = 0x09;
	memcpy(adv_data_cp->data + 2, name, name_len);
	adv_data_cp->data[2 + name_len] = 0x00; // end flag

	return adv_data_cp;
}

int main()
{
	signal(SIGINT, sigint_handler);
	int ret, status;

	// Get HCI device.

	const int device = hci_open_dev(hci_get_route(NULL));
	if (device < 0)
	{
		perror("Failed to open HC device.");
		return 0;
	}
	printf("open device succuss.\n");

	// Set BLE advertisement parameters.

	struct bt_hci_cmd_le_set_ext_adv_params ext_adv_params_cp;
	memset(&ext_adv_params_cp, 0, sizeof(ext_adv_params_cp));
	ext_adv_params_cp.handle = 0x00;
	ext_adv_params_cp.min_interval[0] = 0x40;
	ext_adv_params_cp.max_interval[0] = 0x40;
	ext_adv_params_cp.evt_properties = 0x00;
	ext_adv_params_cp.channel_map = 0x07; // all
	ext_adv_params_cp.own_addr_type = 0x00;
	ext_adv_params_cp.filter_policy = 0x00; // all accept
	ext_adv_params_cp.primary_phy = 0x01;
	ext_adv_params_cp.secondary_max_skip = 0;
	ext_adv_params_cp.secondary_phy = 0x03;
	ext_adv_params_cp.sid = 1;
	ext_adv_params_cp.notif_enable = 0x00;

	struct hci_request adv_params_rq = ble_hci_request(
		BT_HCI_CMD_LE_SET_EXT_ADV_PARAMS,
		sizeof(struct bt_hci_cmd_le_set_ext_adv_params), &status, &ext_adv_params_cp);

	ret = hci_send_req(device, &adv_params_rq, 1000);
	if (ret < 0)
	{
		hci_close_dev(device);
		perror("Failed to set advertisement parameters data.");
		return 0;
	}
	printf("set ext adv params succuss.\n");

	// Set BLE advertisement data.

	struct bt_hci_cmd_le_set_ext_adv_data *adv_data_cp = ble_hci_params_for_set_adv_data("Intel Edisonxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx11x1x2x3x4x5x6x7sedsagh");

	struct hci_request adv_data_rq = ble_hci_request(
		BT_HCI_CMD_LE_SET_EXT_ADV_DATA,
		sizeof(struct bt_hci_cmd_le_set_ext_adv_data) + adv_data_cp->data_len, &status, adv_data_cp);
	ret = hci_send_req(device, &adv_data_rq, 1000);
	if (ret < 0)
	{
		hci_close_dev(device);
		perror("Failed to set advertising data.");
		return 0;
	}
	printf("set ext adv data succuss.\n");

	// Enable advertising.

	struct bt_hci_cmd_le_set_ext_adv_enable *advertise_cp = (struct bt_hci_cmd_le_set_ext_adv_enable *)
		malloc(sizeof(struct bt_hci_cmd_le_set_ext_adv_enable) + sizeof(struct bt_hci_cmd_ext_adv_set) * 1);
	memset(advertise_cp, 0, sizeof(struct bt_hci_cmd_le_set_ext_adv_enable) + sizeof(struct bt_hci_cmd_ext_adv_set) * 1);
	advertise_cp->enable = 0x01;
	advertise_cp->num_of_sets = 1;
	struct bt_hci_cmd_ext_adv_set *sets = (struct bt_hci_cmd_ext_adv_set *)advertise_cp->data;
	sets[0].duration = 0;
	sets[0].max_events = 0;
	sets[0].handle = 0;

	struct hci_request enable_adv_rq = ble_hci_request(
		BT_HCI_CMD_LE_SET_EXT_ADV_ENABLE,
		sizeof(struct bt_hci_cmd_le_set_ext_adv_enable) + sizeof(struct bt_hci_cmd_ext_adv_set) * 1, &status, advertise_cp);

	ret = hci_send_req(device, &enable_adv_rq, 1000);
	if (ret < 0)
	{
		hci_close_dev(device);
		perror("Failed to enable advertising.");
		return 0;
	}
	printf("enable adv data...\n");

	struct timeval time;
	char str[30];
	while (!app_stopped)
	{
		gettimeofday(&time, NULL);
		sprintf(str, "%ld.%ld", time.tv_sec, (time.tv_sec * 1000 + time.tv_usec / 1000));
		memcpy(adv_data_cp->data, str, sizeof(str));
		hci_send_req(device, &adv_data_rq, 1000);
		printf("update: %s\n", str);
	}

	advertise_cp->enable = 0x00;
	struct hci_request disable_adv_rq = ble_hci_request(
		BT_HCI_CMD_LE_SET_EXT_ADV_ENABLE,
		sizeof(struct bt_hci_cmd_le_set_ext_adv_enable), &status, advertise_cp);
	ret = hci_send_req(device, &disable_adv_rq, 1000);
	if (ret < 0)
	{
		hci_close_dev(device);
		perror("Failed to disable advertising.");
		return 0;
	}
	printf("disable adv data...\n");

	hci_close_dev(device);

	return 0;
}