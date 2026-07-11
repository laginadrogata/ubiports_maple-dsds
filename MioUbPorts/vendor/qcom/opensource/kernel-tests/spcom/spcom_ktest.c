/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/*
 * Secure-Processor-Communication (SPCOM) Kernel Test.
 *
 * The spcom kernel driver provides interface to both User Space application
 * (via spcomlib) and to kernel drivers.
 *
 * This driver tests the Secure-Processor-Communication (SPCOM) Kernel API.
 *
 */

/*
 *-------------------------------------------------------------------------
 * Include Files
 *-------------------------------------------------------------------------
 */

#define pr_fmt(fmt)    "spcom_ktest [%s]: " fmt, __func__

#include <linux/module.h>      /* module_init() */
#include <linux/printk.h>      /* pr_debug() */
#include <linux/delay.h>       /* msleep() */

#include <soc/qcom/spcom.h>

extern void glink_ssr(const char* edge_name);

/*
 *-------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 *-------------------------------------------------------------------------
 */

/* SPCOM KTEST driver name */
#define DEVICE_NAME    "spcom_ktest"

/*
 *-------------------------------------------------------------------------
 * Global Variables
 *-------------------------------------------------------------------------
 */

/* SP main process (SKP) channel name */
static const char *skp_ch_name = "sp_kernel";

static char *test_name = "link_up_test"; /* default basic test */
module_param(test_name, charp, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(test_name, "Test Name");

static char *ch_name = "sp_kernel"; /* default channel name */
module_param(ch_name, charp, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(ch_name, "Channel Name");

static int test_loops = 1;
module_param(test_loops, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(test_loops, "Test Loop Number ");

static int check_loopback_data = 1;
module_param(check_loopback_data, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(check_loopback_data, "Check Loopback data");

/*
 *-------------------------------------------------------------------------
 * Function Implementations
 *-------------------------------------------------------------------------
 */
static inline void spcom_fake_ssr_cmd(void)
{
	 glink_ssr("spss"); /* FAKE SSR from GLINK layer */
}

volatile bool ssr_notified = false;

void spcom_test_notify_ssr_cb(void)
{
    ssr_notified = true;
    pr_debug("Notify SSR CALLBACK.\n");
}

/**
 * link_up_test() - check LINK between SP and HLOS.
 *
 * This is the very basic test, verify glink connection on SP
 * and HLOS.
 *
 * Return: 0 on success, negative value on failure
 */
static int link_up_test(void)
{
	int ret = 0;
	bool link_is_up = false;

	pr_debug("----- LINK-UP TEST START ----\n");

	link_is_up = spcom_is_sp_subsystem_link_up();


	if (link_is_up) {
		pr_debug("SP-HLOS link is UP\n");
		ret = 0;
	} else {
		pr_debug("SP-HLOS link is DOWN\n");
		ret = -EFAULT;
	}

	pr_debug("----- LINK-UP TEST END ----\n");

	return ret;
}

/**
 * connect_test() - Test connection with SP main process SKP.
 *
 * The SKP main process should open "sp_kernel" channel once
 * SPSS is up.
 *
 * Return: 0 on success, negative value on failure
 */
static int connection_test(void)
{
	int ret = 0;
	struct spcom_client *client;
	struct spcom_client_info client_info;
	int connect_timeout_msec = 100; /* connection timeout */
	bool remote_service_connected = false;

	pr_debug("----- CONNECTION TEST START ----\n");

	/* Register Client */
	memset(&client_info, 0, sizeof(client_info));
	client_info.ch_name = skp_ch_name;
	client_info.notify_ssr_cb = NULL;

	client = spcom_register_client(&client_info);
	if (client == NULL) {
		pr_debug("client is NULL\n");
		return -ENODEV;
	}

	pr_debug("Wait server connected\n");
	while(!remote_service_connected) {
		remote_service_connected = spcom_client_is_server_connected(client);
		msleep(1);
		connect_timeout_msec--;
		if (connect_timeout_msec == 0) {
		       pr_err("Timeout wait for connection.\n");
		       ret = -ETIMEDOUT;
		       goto exit_err;
	       }
	 }
	 pr_debug("ch [%s] connected!\n", skp_ch_name);

	/* Unregister Client */
exit_err:
	spcom_unregister_client(client);
	pr_debug("----- CONNECTION TEST END ----\n");

	return ret;
}

/*
 *-------------------------------------------------------------------------
 * Client Tests
 *-------------------------------------------------------------------------
 */

/**
 * client_test() - Test spcom Client API
 *
 * The client test expecting the server to LOOPBACK the data.
 * i.e. The Rx data equals the Tx data.
 *
 * @ch_name: channel name
 *
 * Return: 0 on success, negative value on failure
 */
static int client_test(const char *ch_name)
{
	int ret = 0;
	struct spcom_client *client;
	struct spcom_client_info client_info;
	char tx_buf[SPCOM_MAX_REQUEST_SIZE];
	char rx_buf[SPCOM_MAX_RESPONSE_SIZE];
	int connect_timeout_msec = 100; /* connection timeout */
	int response_timeout_msec = 3*1000; /* response timeout */
	bool remote_service_connected = false;
	int i, j;

	pr_debug("----- CLIENT TEST START ----\n");

	/* Register Client */
	memset(&client_info, 0, sizeof(client_info));
	client_info.ch_name = ch_name;
	client_info.notify_ssr_cb = NULL;

	client = spcom_register_client(&client_info);
	if (client == NULL) {
		pr_debug("client is NULL\n");
		return -ENODEV;
	}

	pr_debug("Wait server connected\n");
	while(!remote_service_connected) {
		remote_service_connected = spcom_client_is_server_connected(client);
		msleep(1);
		connect_timeout_msec--;
		if (connect_timeout_msec == 0) {
		       pr_err("Timeout wait for server connection.\n");
		       ret = -ETIMEDOUT;
		       goto exit_err;
	       }
	 }
	 pr_debug("Server connected!\n");

	 pr_debug("Delay for server to prepare rx-buffer.\n");
	 msleep(100);

	 pr_debug("test_loops [%d]\n", test_loops);

	/* Client Data Transfer */
	for (j = 0x10; j < test_loops+0x10 ; j++) {
		int delay_msec;
		/* incremental size 1..to..SPCOM_MAX_REQUEST_SIZE */
		int tx_size = j % sizeof(tx_buf);


		pr_debug("loop # [%d]\n", j);

		if (tx_size == 0) /* Zero is not a valid size */
			tx_size = 1;

		/* set some dummy value in the TX buffer */
		for (i = 0 ; i < tx_size ; i++)
			tx_buf[i]=0xA0+i;

		memset(rx_buf, 0, sizeof(rx_buf));

		ret = spcom_client_send_message_sync(client,
					   tx_buf, tx_size,
					   rx_buf, sizeof(rx_buf),
					   response_timeout_msec);

		pr_debug("spcom_client_send_message_sync() loop [%d] ch [%s] ret [%d].\n", j, ch_name, ret);

		if (ret < 0) {
		       pr_err("send request err [%d].\n", ret);
		       goto exit_err;
		}

		if (check_loopback_data) {
			if (ret != tx_size) {
				pr_debug("response size [%d] not equal to request size [%d]\n", ret , tx_size);
				goto exit_err;
			}

			for (i = 0 ; i < tx_size ; i++) {
				if (rx_buf[i] != tx_buf[i]) {
					pr_debug("ERROR! index [%d],  rx_buf [0x%x] != tx_buf [0x%x] \n", i, (int) rx_buf[i], (int) tx_buf[i]);
					goto exit_err;
				}
			}
		}

		delay_msec = 10 + j;
		msleep(delay_msec);
	}

	/* Unregister Client */
	spcom_unregister_client(client);
	pr_debug("----- CLIENT TEST PASS ----\n");

	return 0;
exit_err:
	spcom_unregister_client(client);
	pr_debug("----- CLIENT TEST FAIL ----\n");

	return ret;
}

/**
 * client_ssr_test() - Test Client SSR notification
 *
 * @ch_name: channel name
 *
 * Return: 0 on success, negative value on failure
 */
static int client_ssr_test(const char *ch_name)
{
	int ret = 0;
	struct spcom_client *client;
	struct spcom_client_info client_info;
	int timeout_sec = 60; /* connection timeout */
	bool remote_service_connected = false;

	pr_debug("----- SSR TEST START ----\n");

	/* Register Client */

	memset(&client_info, 0, sizeof(client_info));
	client_info.ch_name = ch_name;
	client_info.notify_ssr_cb = spcom_test_notify_ssr_cb;

	client = spcom_register_client(&client_info);
	if (client == NULL) {
		pr_debug("client is NULL\n");
		return -ENODEV;
	}

	while(!remote_service_connected) {
		 remote_service_connected = spcom_client_is_server_connected(client);
		 msleep(1000);
		 timeout_sec--;
		 if (timeout_sec == 0) {
			 pr_err("Timeout wait for server connection.\n");
			 ret = -ETIMEDOUT;
			 goto exit_err;
		 }
	}
	pr_debug("Server is connected.\n");

	/* FAKE SSR */
	pr_debug("Fake SSR.\n");
	spcom_fake_ssr_cmd();
	pr_debug("Wait SSR - ssr_notified flag.\n");
	timeout_sec = 60; /* SSR timeout */
	while (!ssr_notified) {
		msleep(1000);
		timeout_sec--;
		if (timeout_sec == 0) {
			pr_err("Timeout wait for SSR.\n");
			ret = -ETIMEDOUT;
			goto exit_err;
		}
	}
	pr_debug("SSR notify test pass.\n");

	/* Unregister Client */
exit_err:
	spcom_unregister_client(client);
	pr_debug("----- SSR TEST END ----\n");

	return ret;
}

/**
 * client_L2_test() - Level 2 Client Test - test API input parameters
 */
static int client_L2_test(const char *ch_name)
{
	int ret = 0;
	struct spcom_client *client;
	struct spcom_client *handle;
	struct spcom_client_info client_info;
	char tx_buf[SPCOM_MAX_REQUEST_SIZE];
	char rx_buf[SPCOM_MAX_RESPONSE_SIZE];
	uint32_t timeout_msec = 1001;
	bool remote_service_connected = false;
	unsigned int i;
	char bad_name[100];
	int timeout_sec = 60; /* connection timeout */

	pr_debug("----- CLIENT L2 TEST START ----\n");

	/*-----------------*/
	/* Register Client */
	/*-----------------*/

	/* default */
	memset(&client_info, 0, sizeof(client_info));
	client_info.ch_name = ch_name;
	client_info.notify_ssr_cb = spcom_test_notify_ssr_cb;

	/* bad name register */
	memset(bad_name, 'X', sizeof(bad_name));
	client_info.ch_name = bad_name;
	handle = spcom_register_client(&client_info);
	if (handle != NULL) {
		pr_debug("client-register-bad-name test fail.\n");
		return -ENODEV;
	} else {
		pr_debug("client-register-bad-name test ok.\n");
	}

	/*
	 * previously,  spcom_register_client() was returning error if connection was not completed within timeout.
	 * So providing an "unknown" wrong channel name returned an error.
	 * However, a new requirement asked that spcom will allow to register a client even before the server side app on SP was loaded.
	 * Hence, this test case was disabled.
	 */

	/* NULL register */
	client_info.ch_name = ch_name;
	handle = spcom_register_client(NULL);
	if (handle != NULL) {
		pr_debug("client-register-NULL test fail.\n");
		return -ENODEV;
	} else {
		pr_debug("client-register-NULL test OK.\n");
	}

	/* good register */
	client_info.ch_name = ch_name;
	client = spcom_register_client(&client_info);
	if (client == NULL) {
		pr_debug("register-client fail\n");
		return -ENODEV;
	} else {
		pr_debug("register-client ok\n");
	}

	handle = spcom_register_client(&client_info);
	if (handle != NULL) {
		pr_debug("client-register-twice test fail.\n");
		return -ENODEV;
	} else {
		pr_debug("client-register-twice ok.\n");
	}

	/* good un-register */
	ret = spcom_unregister_client(client);
	if (ret != 0) {
		pr_debug("unregister-client test fail.\n");
		return -ENODEV;
	} else {
		pr_debug("unregister-client test ok.\n");
	}

	ret = spcom_unregister_client(client);
	if (ret == 0) {
		pr_debug("unregister-client-twice test fail.\n");
		return -ENODEV;
	} else {
		pr_debug("unregister-client-twice test ok.\n");
	}

	/* good re-register */
	client_info.ch_name = ch_name;
	client = spcom_register_client(&client_info);
	if (client == NULL) {
		pr_debug("register-client fail\n");
		return -ENODEV;
	} else {
		pr_debug("re-register-client ok\n");
	}

	/* NULL - verify no crash */
	pr_debug("Check spcom_client_is_server_connected - NULL.\n");
	spcom_client_is_server_connected(NULL);

	pr_debug("wait for remote-service-connection.\n");
	while(!remote_service_connected) {
		remote_service_connected = spcom_client_is_server_connected(client);
		msleep(1000);
		timeout_sec--;
		if (timeout_sec == 0) {
		       pr_err("Timeout wait for server connection.\n");
		       ret = -ETIMEDOUT;
		       goto exit_err;
	       }
	}
	pr_debug("remote-service-connected-ok.\n");

	/* Client Data Transfer */

	/* set some dummy value in the buffer */
	for (i = 0 ; i < sizeof(tx_buf); i++)
		tx_buf[i]=0xA0+i;

	/* NULL */
	ret = spcom_client_send_message_sync(NULL,
				   tx_buf, sizeof(tx_buf),
				   rx_buf, sizeof(rx_buf),
				   timeout_msec);
	if (ret >= 0) {
		pr_err("client send with NULL handle fail.\n");
		return -EFAULT;
	} else {
		pr_debug("client send with NULL handle OK.\n");
	}

	ret = spcom_client_send_message_sync(client,
				   NULL, sizeof(tx_buf),
				   rx_buf, sizeof(rx_buf),
				   timeout_msec);
	if (ret >= 0) {
		pr_err("client send with NULL req_ptr fail.\n");
		return -EFAULT;
	} else {
		pr_debug("client send with NULL req_ptr OK.\n");
	}

	ret = spcom_client_send_message_sync(client,
				   tx_buf, sizeof(tx_buf),
				   NULL, sizeof(rx_buf),
				   timeout_msec);
	if (ret >= 0) {
		pr_err("client send with NULL resp_ptr fail.\n");
		return -EFAULT;
	} else {
		pr_debug("client send with NULL resp_ptr OK.\n");
	}

	/* big-tx-size */
	ret = spcom_client_send_message_sync(client,
				   tx_buf, SPCOM_MAX_REQUEST_SIZE+1,
				   rx_buf, sizeof(rx_buf),
				   timeout_msec);
	if (ret >= 0) {
		pr_err("client-send-big-tx-size fail\n");
		return -EFAULT;
	} else {
		pr_debug("client-send-big-tx-size ok.\n");
	}

	/* zero-tx-size */
	ret = spcom_client_send_message_sync(client,
				   tx_buf, 0,
				   rx_buf, sizeof(rx_buf),
				   timeout_msec);
	if (ret >= 0) {
		pr_err("client-send-zero-tx-size fail\n");
		return -EFAULT;
	} else {
		pr_debug("client-send-zero-tx-size ok.\n");
	}

	/* Good */
	ret = spcom_client_send_message_sync(client,
				   tx_buf, sizeof(tx_buf),
				   rx_buf, sizeof(rx_buf),
				   timeout_msec);
	if (ret <= 0) {
		pr_err("client-send fail\n");
		return -EFAULT;
	} else {
		pr_debug("client-send ok\n");
	}

	pr_debug("spcom_client_send_message_sync() ch [%s] ret [%d].\n", ch_name, ret);

	/* Unregister Client */
exit_err:
	spcom_unregister_client(client);
	pr_debug("----- CLIENT L2 TEST END ----\n");

	return ret;
}

/*
 *-------------------------------------------------------------------------
 * Server Tests
 *-------------------------------------------------------------------------
 */

/**
 * server_test() - Test spcom Server API
 *
 * The server performs a LOOPBACK.
 * The response data = the request data.
 *
 * @ch_name
 *
 * Return: 0 on success, negative value on failure
 */
static int server_test(const char *ch_name)
{
	int ret = 0;
	int size = 0;
	struct spcom_server *server;
	struct spcom_service_info service_info;
	char tx_buf[SPCOM_MAX_REQUEST_SIZE];
	char rx_buf[SPCOM_MAX_RESPONSE_SIZE];
	int j;

	pr_debug("----- SERVER TEST START ----\n");

	/* Register Service */
	memset(&service_info, 0, sizeof(service_info));
	service_info.ch_name = ch_name;
	service_info.notify_ssr_cb = spcom_test_notify_ssr_cb;

	server = spcom_register_service(&service_info);
	if (server == NULL) {
		pr_debug("server is NULL\n");
		return -ENODEV;
	}

	/* Service Data Transfer */
	for (j = 0; j < test_loops ; j++) {
		size = spcom_server_get_next_request_size(server);

		if ((size > (int) sizeof(rx_buf)) || (size > (int) sizeof(tx_buf))) {
			pr_debug("size [%d] too big, max size [%d].\n", size, (int) sizeof(rx_buf));
			return -EFAULT;
		}

		ret = spcom_server_wait_for_request(server, rx_buf, size);
		if (ret >= 0) {
			memcpy(tx_buf, rx_buf, size);
		} else {
			pr_debug("spcom_server_wait_for_request() error [%d].\n", ret);
			return -EFAULT;
		}
		pr_debug("spcom_server_send_response() loop [%d] ch [%s] size [%d].\n", j, ch_name , size);

		ret = spcom_server_send_response(server, tx_buf, size);
	}

	/* Unregister Server */
	spcom_unregister_service(server);

	pr_debug("----- SERVER TEST END ----\n");

	return ret;

}

static int server_L2_test(const char *ch_name)
{
	int ret;
	struct spcom_server *server;
	struct spcom_server *handle;
	struct spcom_service_info service_info;
	char tx_buf[SPCOM_MAX_REQUEST_SIZE];
	char rx_buf[SPCOM_MAX_RESPONSE_SIZE];
	char bad_name[100];
	uint32_t next_req_size;

	pr_debug("----- SERVER L2 TEST START ----\n");

	/* Register Service */

	/* default service_info */
	memset(&service_info, 0, sizeof(service_info));
	service_info.ch_name = ch_name;
	service_info.notify_ssr_cb = spcom_test_notify_ssr_cb;

	/* bad register tests */
	memset(bad_name, 'X', sizeof(bad_name));
	service_info.ch_name = bad_name;
	handle = spcom_register_service(&service_info);
	if (handle != NULL) {
		pr_err("bad name test fail\n");
		return -ENODEV;
	}
	handle = spcom_register_service(NULL);
	if (handle != NULL) {
		pr_err("NULL pointer test fail\n");
		return -ENODEV;
	}
	handle = spcom_register_service(NULL);
	service_info.ch_name = NULL;
	if (handle != NULL) {
		pr_err("NULL name test fail\n");
		return -ENODEV;
	}

	/* good register */
	service_info.ch_name = ch_name;
	service_info.notify_ssr_cb = spcom_test_notify_ssr_cb;
	server = spcom_register_service(&service_info);
	if (server == NULL) {
		pr_err("server is NULL\n");
		return -ENODEV;
	}
	/* bad register */
	handle = spcom_register_service(&service_info);
	if (handle != NULL) {
		pr_err("register twice should fail\n");
		return -ENODEV;
	} else {
		pr_debug("register twice failed as expected.\n");
	}

	/* good un-register */
	ret = spcom_unregister_service(server);
	if (ret == 0) {
		pr_debug("unregister ok\n");
	} else {
		pr_err("unregistered failed\n");
		return -ENODEV;
	}

	/* bad un-register */
	ret = spcom_unregister_service(server);
	if (ret == 0) {
		pr_err("unregister twice test failed\n");
		return -ENODEV;
	} else {
		pr_debug("unregister twice failed as expected\n");
	}

	ret = spcom_server_get_next_request_size(server);
	if (ret >= 0) {
		pr_err("get-size unregister service test failed.\n");
		return -ENODEV;
	} else {
		pr_debug("get-size unregister service failed as expected.\n");
	}

	/* good re-register (after un-register) */
	service_info.ch_name = ch_name; /* fix back to normal */
	server = spcom_register_service(&service_info);
	if (server == NULL) {
		pr_err("re-register failed.\n");
		return -ENODEV;
	} else {
		pr_debug("re-register ok.\n");
	}

	/* Service Data Transfer */
	ret = spcom_server_get_next_request_size(server);
	next_req_size = ret;
	if (ret <= 0) {
		pr_err("spcom_server_get_next_request_size() failed.\n");
		return -ENODEV;
	} else {
		pr_debug("spcom_server_get_next_request_size() ok.\n");
	}

	pr_debug("server next_request_size=%d.\n", next_req_size);

	ret = spcom_server_send_response(server, tx_buf, sizeof(tx_buf));
	if (ret >= 0) {
		pr_err("send-response-without-request test failed.\n");
		return -ENODEV;
	} else {
		pr_debug("send-response-without-request failed as expected.\n");
	}

	ret = spcom_server_get_next_request_size(server);
	if (ret >= 0) {
		pr_err("get-size twice test failed.\n");
		return -ENODEV;
	} else {
		pr_debug("get-size twice failed as expected.\n");
	}
	ret = spcom_server_get_next_request_size(NULL);
	if (ret >= 0) {
		pr_err("get-size with NULL test failed.\n");
		return -ENODEV;
	} else {
		pr_debug("get-size with NULL failed as expected.\n");
	}

	ret = spcom_server_wait_for_request(NULL, rx_buf, sizeof(rx_buf));
	if (ret >= 0) {
		pr_err("wait-for-request-with-NULL-handle test failed.\n");
		return -ENODEV;
	} else {
		pr_debug("wait-for-request-with-NULL-handle test OK.\n");
	}
	ret = spcom_server_wait_for_request(server, NULL, sizeof(rx_buf));
	if (ret >= 0) {
		pr_err("wait-for-request-with-NULL-req_ptr test failed.\n");
		return -ENODEV;
	} else {
		pr_debug("wait-for-request-with-NULL-req_ptr test OK.\n");
	}
	ret = spcom_server_wait_for_request(server, rx_buf, 0);
	if (ret >= 0) {
		pr_err("wait-for-request-with-size-zero test failed.\n");
		return -ENODEV;
	} else {
		pr_debug("wait-for-request-with-size-zero failed as expected.\n");
	}
	ret = spcom_server_wait_for_request(server, rx_buf, next_req_size-1);
	if (ret >= 0) {
		pr_err("wait-for-request-with-size-small test failed.\n");
		return -ENODEV;
	} else {
		pr_debug("wait-for-request-with-size-small failed as expected.\n");
	}

	ret = spcom_server_wait_for_request(server, rx_buf, sizeof(rx_buf));
	if (ret < 0) {
		pr_err("spcom_server_wait_for_request() failed.\n");
		return -ENODEV;
	} else {
		pr_debug("spcom_server_wait_for_request() ok.\n");
	}

	ret = spcom_server_send_response(server, tx_buf, sizeof(tx_buf));
	if (ret < 0) {
		pr_err("spcom_server_send_response() failed.\n");
		return -ENODEV;
	} else {
		pr_debug("spcom_server_send_response() ok.\n");
	}

	ret = spcom_server_send_response(server, tx_buf, sizeof(tx_buf));
	if (ret >= 0) {
		pr_err("send-response-twice test failed.\n");
		return -ENODEV;
	} else {
		pr_debug("send-response-twice failed as expected.\n");
	}

	ret = spcom_server_send_response(server, tx_buf, SPCOM_MAX_RESPONSE_SIZE+1);
	if (ret >= 0) {
		pr_err("send-response-size-too-big test failed.\n");
		return -ENODEV;
	} else {
		pr_debug("send-response-size-too-big failed as expected.\n");
	}
	ret = spcom_server_send_response(server, tx_buf, 0);
	if (ret >= 0) {
		pr_err("send-response-size-zero test failed.\n");
		return -ENODEV;
	} else {
		pr_debug("send-response-size-zero failed as expected.\n");
	}

	/* Unregister Server */
	ret = spcom_unregister_service(server);
	if (ret == 0) {
		pr_debug("unregistered ok\n");
	} else {
		pr_err("unregistered failed\n");
		return -ENODEV;
	}

	pr_debug("----- SERVER L2 TEST END ----\n");

	return ret;
}

/*
 *-------------------------------------------------------------------------
 * Main
 *-------------------------------------------------------------------------
 */

static int __init spcom_ktest_init(void)
{
	int ret=-1;

	pr_info("version 1.0 16-June-2016.\n");

	if (!strcmp("link_up_test",test_name)) {
	       ret = link_up_test();
	} else if (!strcmp("connection_test",test_name)) {
		ret = connection_test();
	} else if (!strcmp("client_ssr",test_name)) {
	       ret = client_ssr_test(ch_name);
	} else if (!strcmp("client_test",test_name)) {
		ret = client_test(ch_name);
	} else if (!strcmp("server_test",test_name)) {
		ret = server_test(ch_name);
	} else if (!strcmp("server_L2_test",test_name)) {
		ret = server_L2_test(ch_name);
	} else if (!strcmp("client_L2_test",test_name)) {
		ret = client_L2_test(ch_name);
	} else {
		pr_info("USAGE:\n");
		pr_info("  insmod spcom_ktest.ko test_name=<string> ch_name=<string> test_loops=<number>\n");
	}

	return ret;
}

static void __exit spcom_ktest_exit(void)
{
	 pr_info("Exit spcom Kernel Test\n");
}

module_init(spcom_ktest_init);
module_exit(spcom_ktest_exit);

MODULE_DESCRIPTION("Secure Processor Communication Kernel Unit Test");
MODULE_LICENSE("GPL v2");

