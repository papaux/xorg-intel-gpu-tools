/*
 * Copyright © 2019 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */

#include "igt_device_scan.h"
#include "igt.h"
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <glib.h>

/**
 * SECTION:lsgpu
 * @short_description: lsgpu
 * @title: lsgpu
 * @include: lsgpu.c
 *
 * # lsgpu
 *
 * The devices can be scanned and displayed using 'lsgpu' tool. Tool also
 * displays properties and sysattrs (-p switch, means print detail) which
 * can be used during filter implementation.
 *
 * Tool can also be used to try out filters.
 * To select device use '-d' or '--device' argument like:
 *
 * |[<!-- language="plain" -->
 * ./lsgpu -d 'pci:vendor=Intel'
 * === Device filter list ===
 * [ 0]: pci:vendor=Intel

 * === Testing device open ===
 * subsystem   : pci
 * drm card    : /dev/dri/card0
 * drm render  : /dev/dri/renderD128
 * Device /dev/dri/card0 successfully opened
 * Device /dev/dri/renderD128 successfully opened
 * ]|
 *
 * NOTE: When using filters only the first matching device is printed.
 *
 * Additionally lsgpu tries to open the card and render nodes to verify
 * permissions. It also uses IGT variable search order:
 * - use --device first (it overrides IGT_DEVICE and .igtrc Common::Device
 *   settings)
 * - use IGT_DEVICE enviroment variable if no --device are passed
 * - use .igtrc Common::Device if no --device nor IGT_DEVICE are passed
 */

enum {
	OPT_PRINT_DETAIL   = 'p',
	OPT_LIST_VENDORS   = 'v',
	OPT_LIST_FILTERS   = 'l',
	OPT_DEVICE         = 'd',
	OPT_HELP           = 'h'
};

static bool g_show_vendors;
static bool g_list_filters;
static bool g_help;
static char *igt_device;

static const char *usage_str =
	"usage: lsgpu [options]\n\n"
	"Options:\n"
	"  -p, --print-details         Print devices with details\n"
	"  -v, --list-vendors          List recognized vendors\n"
	"  -l, --list-filter-types     List registered device filters types\n"
	"  -d, --device filter         Device filter, can be given multiple times\n"
	"  -h, --help                  Show this help message and exit\n";

static void test_device_open(struct igt_device_card *card)
{
	int fd;

	if (!card)
		return;

	fd = igt_open_card(card);
	if (fd >= 0) {
		printf("Device %s successfully opened\n", card->card);
		close(fd);
	} else {
		if (strlen(card->card))
			printf("Cannot open card %s device\n", card->card);
		else
			printf("Cannot open card device, empty name\n");
	}

	fd = igt_open_render(card);
	if (fd >= 0) {
		printf("Device %s successfully opened\n", card->render);
		close(fd);
	} else {
		if (strlen(card->render))
			printf("Cannot open render %s device\n", card->render);
		else
			printf("Cannot open render device, empty name\n");
	}
}

static void print_card(struct igt_device_card *card)
{
	if (!card)
		return;

	printf("subsystem   : %s\n", card->subsystem);
	printf("drm card    : %s\n", card->card);
	printf("drm render  : %s\n", card->render);
}

static char *get_device_from_rc(void)
{
	char *rc_device = NULL;
	GError *error = NULL;
	GKeyFile *key_file = igt_load_igtrc();

	if (key_file == NULL)
		return NULL;

	rc_device = g_key_file_get_string(key_file, "Common",
					  "Device", &error);

	g_clear_error(&error);

	return rc_device;
}

int main(int argc, char *argv[])
{
	static struct option long_options[] = {
		{"print-detail",      no_argument,       NULL, OPT_PRINT_DETAIL},
		{"list-vendors",      no_argument,       NULL, OPT_LIST_VENDORS},
		{"list-filter-types", no_argument,       NULL, OPT_LIST_FILTERS},
		{"device",            required_argument, NULL, OPT_DEVICE},
		{"help",              no_argument,       NULL, OPT_HELP},
		{0, 0, 0, 0}
	};
	int c, index = 0;
	char *env_device = NULL, *opt_device = NULL, *rc_device = NULL;
	enum igt_devices_print_type printtype = IGT_PRINT_SIMPLE;

	while ((c = getopt_long(argc, argv, "pvld:h",
				long_options, &index)) != -1) {
		switch(c) {

		case OPT_PRINT_DETAIL:
			printtype = IGT_PRINT_DETAIL;
			break;
		case OPT_LIST_VENDORS:
			g_show_vendors = true;
			break;
		case OPT_LIST_FILTERS:
			g_list_filters = true;
			break;
		case OPT_DEVICE:
			opt_device = strdup(optarg);
			break;
		case OPT_HELP:
			g_help = true;
			break;
		}
	}

	if (g_help) {
		printf("%s\n", usage_str);
		exit(0);
	}

	if (g_show_vendors) {
		igt_devices_print_vendors();
		return 0;
	}

	if (g_list_filters) {
		igt_device_print_filter_types();
		return 0;
	}

	env_device = getenv("IGT_DEVICE");
	rc_device = get_device_from_rc();

	if (opt_device != NULL) {
		igt_device = opt_device;
		printf("Notice: Using filter supplied via --device\n");
	}
	else if (env_device != NULL) {
		igt_device = env_device;
		printf("Notice: Using filter from IGT_DEVICE env variable\n");
	}
	else if (rc_device != NULL) {
		igt_device = rc_device;
		printf("Notice: Using filter from .igtrc\n");
	}

	igt_devices_scan(false);

	if (igt_device != NULL) {
		struct igt_device_card card;

		printf("=== Device filter ===\n");
		printf("%s\n\n", igt_device);

		printf("=== Testing device open ===\n");

		if (!igt_device_card_match(igt_device, &card)) {
			printf("No device found for the filter\n\n");
			return -1;
		}

		printf("Device detail:\n");
		print_card(&card);
		test_device_open(&card);
		if (printtype == IGT_PRINT_DETAIL) {
			printf("\n");
			igt_devices_print(printtype);
		}
		printf("-------------------------------------------\n");

	} else {
		igt_devices_print(printtype);
	}

	free(rc_device);
	free(opt_device);

	return 0;
}
