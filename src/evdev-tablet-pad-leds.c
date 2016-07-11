/*
 * Copyright © 2016 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holders not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  The copyright holders make
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "config.h"

#include <assert.h>
#include <limits.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "evdev-tablet-pad.h"

#if HAVE_LIBWACOM
#include <libwacom/libwacom.h>
#endif

struct pad_led_group {
	struct libinput_tablet_pad_mode_group base;
	struct list led_list;
	struct list toggle_button_list;
};

struct pad_mode_toggle_button {
	struct list link;
	unsigned int button_index;
};

struct pad_mode_led {
	struct list link;
	/* /sys/devices/..../input1235/input1235::wacom-led_0.1/brightness */
	int brightness_fd;
	int mode_idx;
};

static inline struct pad_mode_toggle_button *
pad_mode_toggle_button_new(struct pad_dispatch *pad,
			   struct libinput_tablet_pad_mode_group *group,
			   unsigned int button_index)
{
	struct pad_mode_toggle_button *button;

	button = zalloc(sizeof *button);
	if (!button)
		return NULL;

	button->button_index = button_index;

	return button;
}

static inline void
pad_mode_toggle_button_destroy(struct pad_mode_toggle_button* button)
{
	list_remove(&button->link);
	free(button);
}

static inline int
pad_led_group_get_mode(struct pad_led_group *group)
{
	char buf[4] = {0};
	int rc;
	unsigned int brightness;
	struct pad_mode_led *led;

	list_for_each(led, &group->led_list, link) {
		rc = lseek(led->brightness_fd, 0, SEEK_SET);
		if (rc == -1)
			return -errno;

		rc = read(led->brightness_fd, buf, sizeof(buf) - 1);
		if (rc == -1)
			return -errno;

		rc = sscanf(buf, "%u\n", &brightness);
		if (rc != 1)
			return -EINVAL;

		/* Assumption: only one LED lit up at any time */
		if (brightness != 0)
			return led->mode_idx;
	}

	return -EINVAL;
}

static inline void
pad_led_destroy(struct libinput *libinput,
		struct pad_mode_led *led)
{
	list_remove(&led->link);
	if (led->brightness_fd != -1)
		close_restricted(libinput, led->brightness_fd);
	free(led);
}

static inline struct pad_mode_led *
pad_led_new(struct libinput *libinput, const char *prefix, int group, int mode)
{
	struct pad_mode_led *led;
	char path[PATH_MAX];
	int rc, fd;

	led = zalloc(sizeof *led);
	if (!led)
		return NULL;

	led->brightness_fd = -1;
	led->mode_idx = mode;
	list_init(&led->link);

	/* /sys/devices/..../input1235/input1235::wacom-led_0.1/brightness,
	 * where 0 and 1 are group and mode index. */
	rc = snprintf(path,
		      sizeof(path),
		      "%s%d.%d/brightness",
		      prefix,
		      group,
		      mode);
	if (rc == -1)
		goto error;

	fd = open_restricted(libinput, path, O_RDONLY);
	if (fd < 0) {
		errno = -fd;
		goto error;
	}

	led->brightness_fd = fd;

	return led;

error:
	pad_led_destroy(libinput, led);
	return NULL;
}

static void
pad_led_group_destroy(struct libinput_tablet_pad_mode_group *g)
{
	struct pad_led_group *group = (struct pad_led_group *)g;
	struct pad_mode_toggle_button *button, *tmp;
	struct pad_mode_led *led, *tmpled;

	list_for_each_safe(button, tmp, &group->toggle_button_list, link)
		pad_mode_toggle_button_destroy(button);

	list_for_each_safe(led, tmpled, &group->led_list, link)
		pad_led_destroy(g->device->seat->libinput, led);

	free(group);
}

static struct pad_led_group *
pad_group_new_basic(struct pad_dispatch *pad,
		    unsigned int group_index,
		    int nleds)
{
	struct pad_led_group *group;

	group = zalloc(sizeof *group);
	if (!group)
		return NULL;

	group->base.device = &pad->device->base;
	group->base.refcount = 1;
	group->base.index = group_index;
	group->base.current_mode = 0;
	group->base.num_modes = nleds;
	group->base.destroy = pad_led_group_destroy;
	list_init(&group->toggle_button_list);
	list_init(&group->led_list);

	return group;
}

static inline struct pad_led_group *
pad_group_new(struct pad_dispatch *pad,
	      unsigned int group_index,
	      int nleds,
	      const char *syspath)
{
	struct libinput *libinput = pad->device->base.seat->libinput;
	struct pad_led_group *group;
	int rc;

	group = pad_group_new_basic(pad, group_index, nleds);
	if (!group)
		return NULL;

	while (nleds--) {
		struct pad_mode_led *led;

		led = pad_led_new(libinput, syspath, group_index, nleds);
		if (!led)
			goto error;

		list_insert(&group->led_list, &led->link);
	}

	rc = pad_led_group_get_mode(group);
	if (rc < 0) {
		errno = -rc;
		goto error;
	}

	group->base.current_mode = rc;

	return group;

error:
	log_error(libinput, "Unable to init LED group: %s\n", strerror(errno));
	pad_led_group_destroy(&group->base);

	return NULL;
}

static inline bool
pad_led_get_sysfs_base_path(struct evdev_device *device,
			    char *path_out,
			    size_t path_out_sz)
{
	struct udev_device *parent, *udev_device;
	const char *test_path;
	int rc;

	udev_device = device->udev_device;

	/* For testing purposes only allow for a base path set through a
	 * udev rule. We still expect the normal directory hierarchy inside */
	test_path = udev_device_get_property_value(udev_device,
						   "LIBINPUT_TEST_TABLET_PAD_SYSFS_PATH");
	if (test_path) {
		rc = snprintf(path_out, path_out_sz, "%s", test_path);
		return rc != -1;
	}

	parent = udev_device_get_parent_with_subsystem_devtype(udev_device,
							       "input",
							       NULL);
	if (!parent)
		return false;

	rc = snprintf(path_out,
		      path_out_sz,
		      "%s/%s::wacom-led_",
		      udev_device_get_syspath(parent),
		      udev_device_get_sysname(parent));

	return rc != -1;
}

#if HAVE_LIBWACOM
static int
pad_init_led_groups(struct pad_dispatch *pad,
		    struct evdev_device *device,
		    WacomDevice *wacom)
{
	struct libinput *libinput = device->base.seat->libinput;
	const WacomStatusLEDs *leds;
	int nleds, nmodes;
	int i;
	struct pad_led_group *group;
	char syspath[PATH_MAX];

	leds = libwacom_get_status_leds(wacom, &nleds);
	if (nleds == 0)
		return 1;

	/* syspath is /sys/class/leds/input1234/input12345::wacom-led_" and
	   only needs the group + mode appended */
	if (!pad_led_get_sysfs_base_path(device, syspath, sizeof(syspath)))
		return 1;

	for (i = 0; i < nleds; i++) {
		switch(leds[i]) {
		case WACOM_STATUS_LED_UNAVAILABLE:
			log_bug_libinput(libinput,
					 "Invalid led type %d\n",
					 leds[i]);
			return 1;
		case WACOM_STATUS_LED_RING:
			nmodes = libwacom_get_ring_num_modes(wacom);
			group = pad_group_new(pad, i, nmodes, syspath);
			if (!group)
				return 1;
			list_insert(&pad->modes.mode_group_list, &group->base.link);
			break;
		case WACOM_STATUS_LED_RING2:
			nmodes = libwacom_get_ring2_num_modes(wacom);
			group = pad_group_new(pad, i, nmodes, syspath);
			if (!group)
				return 1;
			list_insert(&pad->modes.mode_group_list, &group->base.link);
			break;
		case WACOM_STATUS_LED_TOUCHSTRIP:
			nmodes = libwacom_get_strips_num_modes(wacom);
			group = pad_group_new(pad, i, nmodes, syspath);
			if (!group)
				return 1;
			list_insert(&pad->modes.mode_group_list, &group->base.link);
			break;
		case WACOM_STATUS_LED_TOUCHSTRIP2:
			/* there is no get_strips2_... */
			nmodes = libwacom_get_strips_num_modes(wacom);
			group = pad_group_new(pad, i, nmodes, syspath);
			if (!group)
				return 1;
			list_insert(&pad->modes.mode_group_list, &group->base.link);
			break;
		}
	}

	return 0;
}

#endif

static inline struct libinput_tablet_pad_mode_group *
pad_get_mode_group(struct pad_dispatch *pad, unsigned int index)
{
	struct libinput_tablet_pad_mode_group *group;

	list_for_each(group, &pad->modes.mode_group_list, link) {
		if (group->index == index)
			return group;
	}

	return NULL;
}

#if HAVE_LIBWACOM

static inline int
pad_find_button_group(WacomDevice *wacom,
		      int button_index,
		      WacomButtonFlags button_flags)
{
	int i;
	WacomButtonFlags flags;

	for (i = 0; i < libwacom_get_num_buttons(wacom); i++) {
		if (i == button_index)
			continue;

		flags = libwacom_get_button_flag(wacom, 'A' + i);
		if ((flags & WACOM_BUTTON_MODESWITCH) == 0)
			continue;

		if ((flags & WACOM_BUTTON_DIRECTION) ==
			(button_flags & WACOM_BUTTON_DIRECTION))
			return libwacom_get_button_led_group(wacom, 'A' + i);
	}

	return -1;
}

static int
pad_init_mode_buttons(struct pad_dispatch *pad,
		      WacomDevice *wacom)
{
	struct libinput *libinput = pad_libinput_context(pad);
	struct libinput_tablet_pad_mode_group *group;
	unsigned int group_idx;
	int i;
	WacomButtonFlags flags;

	/* libwacom numbers buttons as 'A', 'B', etc. We number them with 0,
	 * 1, ...
	 */
	for (i = 0; i < libwacom_get_num_buttons(wacom); i++) {
		group_idx = libwacom_get_button_led_group(wacom, 'A' + i);
		flags = libwacom_get_button_flag(wacom, 'A' + i);

		/* If this button is not a mode toggle button, find the mode
		 * toggle button with the same position flags and take that
		 * button's group idx */
		if ((int)group_idx == -1) {
			group_idx = pad_find_button_group(wacom, i, flags);
		}

		if ((int)group_idx == -1) {
			log_bug_libinput(libinput,
					 "%s: unhandled position for button %i\n",
					 pad->device->devname,
					 i);
			return 1;
		}

		group = pad_get_mode_group(pad, group_idx);
		if (!group) {
			log_bug_libinput(libinput,
					 "%s: Failed to find group %d for button %i\n",
					 pad->device->devname,
					 group_idx,
					 i);
			return 1;
		}

		group->button_mask |= 1 << i;

		if (flags & WACOM_BUTTON_MODESWITCH) {
			struct pad_mode_toggle_button *b;
			struct pad_led_group *g;

			b = pad_mode_toggle_button_new(pad, group, i);
			if (!b)
				return 1;
			g = (struct pad_led_group*)group;
			list_insert(&g->toggle_button_list, &b->link);
			group->toggle_button_mask |= 1 << i;
		}
	}

	return 0;
}

static void
pad_init_mode_rings(struct pad_dispatch *pad, WacomDevice *wacom)
{
	struct libinput_tablet_pad_mode_group *group;
	const WacomStatusLEDs *leds;
	int i, nleds;

	leds = libwacom_get_status_leds(wacom, &nleds);
	if (nleds == 0)
		return;

	for (i = 0; i < nleds; i++) {
		switch(leds[i]) {
		case WACOM_STATUS_LED_RING:
			group = pad_get_mode_group(pad, i);
			group->ring_mask |= 0x1;
			break;
		case WACOM_STATUS_LED_RING2:
			group = pad_get_mode_group(pad, i);
			group->ring_mask |= 0x2;
			break;
		default:
			break;
		}
	}
}

static void
pad_init_mode_strips(struct pad_dispatch *pad, WacomDevice *wacom)
{
	struct libinput_tablet_pad_mode_group *group;
	const WacomStatusLEDs *leds;
	int i, nleds;

	leds = libwacom_get_status_leds(wacom, &nleds);
	if (nleds == 0)
		return;

	for (i = 0; i < nleds; i++) {
		switch(leds[i]) {
		case WACOM_STATUS_LED_TOUCHSTRIP:
			group = pad_get_mode_group(pad, i);
			group->strip_mask |= 0x1;
			break;
		case WACOM_STATUS_LED_TOUCHSTRIP2:
			group = pad_get_mode_group(pad, i);
			group->strip_mask |= 0x2;
			break;
		default:
			break;
		}
	}
}

static int
pad_init_leds_from_libwacom(struct pad_dispatch *pad,
			    struct evdev_device *device)
{
	struct libinput *libinput = device->base.seat->libinput;
	WacomDeviceDatabase *db = NULL;
	WacomDevice *wacom = NULL;
	int rc = 1;

	db = libwacom_database_new();
	if (!db) {
		log_info(libinput,
			 "Failed to initialize libwacom context.\n");
		goto out;
	}

	wacom = libwacom_new_from_path(db,
				       udev_device_get_devnode(device->udev_device),
				       WFALLBACK_NONE,
				       NULL);
	if (!wacom)
		goto out;

	rc = pad_init_led_groups(pad, device, wacom);
	if (rc != 0)
		goto out;

	if ((rc = pad_init_mode_buttons(pad, wacom)) != 0)
		goto out;

	pad_init_mode_rings(pad, wacom);
	pad_init_mode_strips(pad, wacom);

out:
	if (wacom)
		libwacom_destroy(wacom);
	if (db)
		libwacom_database_destroy(db);

	if (rc != 0)
		pad_destroy_leds(pad);

	return rc;
}
#endif /* HAVE_LIBWACOM */

static int
pad_init_fallback_group(struct pad_dispatch *pad)
{
	struct pad_led_group *group;

	group = pad_group_new_basic(pad, 0, 1);
	if (!group)
		return 1;

	/* If we only have one group, all buttons/strips/rings are part of
	 * that group. We rely on the other layers to filter out invalid
	 * indices */
	group->base.button_mask = -1;
	group->base.strip_mask = -1;
	group->base.ring_mask = -1;
	group->base.toggle_button_mask = 0;

	list_insert(&pad->modes.mode_group_list, &group->base.link);

	return 0;
}

int
pad_init_leds(struct pad_dispatch *pad,
	      struct evdev_device *device)
{
	int rc = 1;

	list_init(&pad->modes.mode_group_list);

	if (pad->nbuttons > 32) {
		log_bug_libinput(device->base.seat->libinput,
				 "Too many pad buttons for modes %d\n",
				 pad->nbuttons);
		return rc;
	}

	/* If libwacom fails, we init one fallback group anyway */
#if HAVE_LIBWACOM
	rc = pad_init_leds_from_libwacom(pad, device);
#endif
	if (rc != 0)
		rc = pad_init_fallback_group(pad);

	return rc;
}

void
pad_destroy_leds(struct pad_dispatch *pad)
{
	struct libinput_tablet_pad_mode_group *group, *tmpgrp;

	list_for_each_safe(group, tmpgrp, &pad->modes.mode_group_list, link)
		libinput_tablet_pad_mode_group_unref(group);
}

void
pad_button_update_mode(struct libinput_tablet_pad_mode_group *g,
		       unsigned int button_index,
		       enum libinput_button_state state)
{
	struct pad_led_group *group = (struct pad_led_group*)g;
	int rc;

	if (state != LIBINPUT_BUTTON_STATE_PRESSED)
		return;

	if (!libinput_tablet_pad_mode_group_button_is_toggle(g, button_index))
		return;

	rc = pad_led_group_get_mode(group);
	if (rc >= 0)
		group->base.current_mode = rc;
}

int
evdev_device_tablet_pad_get_num_mode_groups(struct evdev_device *device)
{
	struct pad_dispatch *pad = (struct pad_dispatch*)device->dispatch;
	struct libinput_tablet_pad_mode_group *group;
	int num_groups = 0;

	if (!(device->seat_caps & EVDEV_DEVICE_TABLET_PAD))
		return -1;

	list_for_each(group, &pad->modes.mode_group_list, link)
		num_groups++;

	return num_groups;
}

struct libinput_tablet_pad_mode_group *
evdev_device_tablet_pad_get_mode_group(struct evdev_device *device,
				       unsigned int index)
{
	struct pad_dispatch *pad = (struct pad_dispatch*)device->dispatch;

	if (!(device->seat_caps & EVDEV_DEVICE_TABLET_PAD))
		return NULL;

	return pad_get_mode_group(pad, index);
}
