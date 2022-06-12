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
#include <signal.h>

bool app_stopped = false;
bool force_exit = false;
int device = -1;

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

void sigint_handler(int sig)
{
	if (sig == SIGINT)
	{
		// ctrl+c退出时执行的代码
		printf("will exit\n");
		app_stopped = true;
		if (force_exit)
		{
			struct bt_hci_cmd_le_set_ext_scan_enable scan_cp;
			// disable scanning.
			memset(&scan_cp, 0, sizeof(scan_cp));
			scan_cp.enable = 0x00; // disable flag.
			int status;
			struct hci_request disable_adv_rq = ble_hci_request(BT_HCI_CMD_LE_SET_EXT_SCAN_ENABLE, sizeof(scan_cp), &status, &scan_cp);
			int ret = hci_send_req(device, &disable_adv_rq, 1000);
			if (ret < 0)
			{
				hci_close_dev(device);
				perror("failed to enable scan.");
			}
			exit(1);
		}

		force_exit = true;
	}
}

int main()
{
	signal(SIGINT, sigint_handler);
	int ret, status;

	// Get HCI device.

	int device_id = hci_get_route(NULL);
	// device_id = 0;
	printf("device id: %d\n", device_id);
	device = hci_open_dev(device_id);
	if (device < 0)
	{
		perror("Failed to open HCI device.");
		return 0;
	}

	// Set BLE scan parameters.

	struct bt_hci_cmd_le_set_ext_scan_params *scan_params_cp =
		(struct bt_hci_cmd_le_set_ext_scan_params *)malloc(sizeof(struct bt_hci_cmd_le_set_ext_scan_params) + 10);
	memset(scan_params_cp, 0, sizeof(struct bt_hci_cmd_le_set_ext_scan_params) + 10);
	scan_params_cp->own_addr_type = 0x00;
	scan_params_cp->filter_policy = 0x00;
	scan_params_cp->num_phys = 0x01 | 0x04;
	scan_params_cp->data[0] = 0x00;
	*((uint16_t *)&scan_params_cp->data[1]) = htobs(0x28);
	*((uint16_t *)&scan_params_cp->data[3]) = htobs(0x28);
	scan_params_cp->data[5] = 0x00;
	*((uint16_t *)&scan_params_cp->data[6]) = htobs(0x40);
	*((uint16_t *)&scan_params_cp->data[8]) = htobs(0x28);

	struct hci_request scan_params_rq = ble_hci_request(BT_HCI_CMD_LE_SET_EXT_SCAN_PARAMS, sizeof(struct bt_hci_cmd_le_set_ext_scan_params) + 10, &status, scan_params_cp);

	ret = hci_send_req(device, &scan_params_rq, 1000);
	if (ret < 0)
	{
		hci_close_dev(device);
		perror("Failed to set scan parameters data.");
		return 0;
	}
	printf("set param OK\n");

	// LE Set Scan Enable
	struct bt_hci_cmd_le_set_ext_scan_enable scan_cp;
	memset(&scan_cp, 0, sizeof(scan_cp));
	scan_cp.enable = 0x01;	   // Enable flag.
	scan_cp.filter_dup = 0x00; // Filtering disabled.
	struct hci_request enable_adv_rq = ble_hci_request(BT_HCI_CMD_LE_SET_EXT_SCAN_ENABLE, sizeof(scan_cp), &status, &scan_cp);
	ret = hci_send_req(device, &enable_adv_rq, 1000);
	if (ret < 0)
	{
		hci_close_dev(device);
		perror("Failed to enable scan.");
		return 0;
	}
	printf("enable scan OK\n");

	// Get Results.
	struct hci_filter nf;
	hci_filter_clear(&nf);
	hci_filter_all_ptypes(&nf);
	hci_filter_all_events(&nf);
	if (setsockopt(device, SOL_HCI, HCI_FILTER, &nf, sizeof(nf)) < 0)
	{
		hci_close_dev(device);
		perror("Could not set socket options\n");
		return 0;
	}

	printf("Scanning....\n");

	uint8_t buf[HCI_MAX_EVENT_SIZE];
	evt_le_meta_event *meta_event;
	int len;

	while (!app_stopped)
	{
		len = read(device, buf, sizeof(buf));
		if (len >= HCI_EVENT_HDR_SIZE)
		{
			meta_event = (evt_le_meta_event *)(buf + HCI_EVENT_HDR_SIZE + 1);
			if (meta_event->subevent == BT_HCI_EVT_LE_EXT_ADV_REPORT)
			{
				uint8_t reports_count = meta_event->data[0];
				void *offset = meta_event->data + 1;
				while (reports_count--)
				{
					struct bt_hci_le_ext_adv_report *info = (struct bt_hci_le_ext_adv_report *)offset;
					bdaddr_t *addr = (bdaddr_t *)info->addr;
					bdaddr_t leaddr;
					baswap(&leaddr, addr);

					printf("%s - RSSI %d len: %d, data[%.*s]]\n", batostr(&leaddr),
						   info->rssi, (int)info->data_len, info->data_len, info->data);

					offset = info->data + info->data_len;
				}
			}
		}
	}

	// Disable scanning.
	memset(&scan_cp, 0, sizeof(scan_cp));
	scan_cp.enable = 0x00; // Disable flag.
	struct hci_request disable_adv_rq = ble_hci_request(BT_HCI_CMD_LE_SET_EXT_SCAN_ENABLE, sizeof(scan_cp), &status, &scan_cp);
	ret = hci_send_req(device, &disable_adv_rq, 1000);
	if (ret < 0)
	{
		hci_close_dev(device);
		perror("Failed to enable scan.");
		return 0;
	}

	hci_close_dev(device);
	printf("close ok!!!\n");

	return 0;
}