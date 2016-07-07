/*
 * Copyright © 2014 Red Hat, Inc.
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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <config.h>

#include <check.h>
#include <errno.h>
#include <fcntl.h>
#include <libinput.h>
#include <unistd.h>

#include "libinput-util.h"
#include "litest.h"

START_TEST(touchpad_1fg_motion)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct libinput_event *event;
	struct libinput_event_pointer *ptrev;

	litest_disable_tap(dev->libinput_device);

	litest_drain_events(li);

	litest_touch_down(dev, 0, 50, 50);
	litest_touch_move_to(dev, 0, 50, 50, 80, 50, 20, 0);
	litest_touch_up(dev, 0);

	libinput_dispatch(li);

	event = libinput_get_event(li);
	ck_assert(event != NULL);

	while (event) {
		ck_assert_int_eq(libinput_event_get_type(event),
				 LIBINPUT_EVENT_POINTER_MOTION);

		ptrev = libinput_event_get_pointer_event(event);
		ck_assert_int_ge(libinput_event_pointer_get_dx(ptrev), 0);
		ck_assert_int_eq(libinput_event_pointer_get_dy(ptrev), 0);
		libinput_event_destroy(event);
		event = libinput_get_event(li);
	}
}
END_TEST

START_TEST(touchpad_2fg_no_motion)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct libinput_event *event;

	libinput_device_config_tap_set_enabled(dev->libinput_device,
					       LIBINPUT_CONFIG_TAP_DISABLED);

	litest_drain_events(li);

	litest_touch_down(dev, 0, 20, 20);
	litest_touch_down(dev, 1, 70, 20);
	litest_touch_move_to(dev, 0, 20, 20, 80, 80, 20, 0);
	litest_touch_move_to(dev, 1, 70, 20, 80, 50, 20, 0);
	litest_touch_up(dev, 1);
	litest_touch_up(dev, 0);

	libinput_dispatch(li);

	event = libinput_get_event(li);
	while (event) {
		ck_assert_int_ne(libinput_event_get_type(event),
				 LIBINPUT_EVENT_POINTER_MOTION);
		libinput_event_destroy(event);
		event = libinput_get_event(li);
	}
}
END_TEST

static int
touchpad_has_palm_detect_size(struct litest_device *dev)
{
	double width, height;
	unsigned int vendor;
	int rc;

	vendor = libinput_device_get_id_vendor(dev->libinput_device);
	if (vendor == VENDOR_ID_WACOM)
		return 0;
	if (vendor == VENDOR_ID_APPLE)
		return 1;

	rc = libinput_device_get_size(dev->libinput_device, &width, &height);

	return rc == 0 && width >= 70;
}

START_TEST(touchpad_palm_detect_at_edge)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;

	if (!touchpad_has_palm_detect_size(dev) ||
	    !litest_has_2fg_scroll(dev))
		return;

	litest_enable_2fg_scroll(dev);

	litest_disable_tap(dev->libinput_device);

	litest_drain_events(li);

	litest_touch_down(dev, 0, 99, 50);
	litest_touch_move_to(dev, 0, 99, 50, 99, 70, 5, 0);
	litest_touch_up(dev, 0);

	litest_assert_empty_queue(li);

	litest_touch_down(dev, 0, 5, 50);
	litest_touch_move_to(dev, 0, 5, 50, 5, 70, 5, 0);
	litest_touch_up(dev, 0);
}
END_TEST

START_TEST(touchpad_no_palm_detect_at_edge_for_edge_scrolling)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;

	if (!touchpad_has_palm_detect_size(dev))
		return;

	litest_enable_edge_scroll(dev);

	litest_drain_events(li);

	litest_touch_down(dev, 0, 99, 50);
	litest_touch_move_to(dev, 0, 99, 50, 99, 70, 5, 0);
	litest_touch_up(dev, 0);

	litest_assert_only_typed_events(li, LIBINPUT_EVENT_POINTER_AXIS);
}
END_TEST

START_TEST(touchpad_palm_detect_at_bottom_corners)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;

	if (!touchpad_has_palm_detect_size(dev) ||
	    !litest_has_2fg_scroll(dev))
		return;

	litest_enable_2fg_scroll(dev);

	litest_disable_tap(dev->libinput_device);

	/* Run for non-clickpads only: make sure the bottom corners trigger
	   palm detection too */
	litest_drain_events(li);

	litest_touch_down(dev, 0, 99, 95);
	litest_touch_move_to(dev, 0, 99, 95, 99, 99, 10, 0);
	litest_touch_up(dev, 0);

	litest_assert_empty_queue(li);

	litest_touch_down(dev, 0, 5, 95);
	litest_touch_move_to(dev, 0, 5, 95, 5, 99, 5, 0);
	litest_touch_up(dev, 0);
}
END_TEST

START_TEST(touchpad_palm_detect_at_top_corners)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;

	if (!touchpad_has_palm_detect_size(dev) ||
	    !litest_has_2fg_scroll(dev))
		return;

	litest_enable_2fg_scroll(dev);

	litest_disable_tap(dev->libinput_device);

	/* Run for non-clickpads only: make sure the bottom corners trigger
	   palm detection too */
	litest_drain_events(li);

	litest_touch_down(dev, 0, 99, 5);
	litest_touch_move_to(dev, 0, 99, 5, 99, 9, 10, 0);
	litest_touch_up(dev, 0);

	litest_assert_empty_queue(li);

	litest_touch_down(dev, 0, 5, 5);
	litest_touch_move_to(dev, 0, 5, 5, 5, 9, 5, 0);
	litest_touch_up(dev, 0);
}
END_TEST

START_TEST(touchpad_palm_detect_palm_stays_palm)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;

	if (!touchpad_has_palm_detect_size(dev) ||
	    !litest_has_2fg_scroll(dev))
		return;

	litest_enable_2fg_scroll(dev);

	litest_disable_tap(dev->libinput_device);

	litest_drain_events(li);

	litest_touch_down(dev, 0, 99, 20);
	litest_touch_move_to(dev, 0, 99, 20, 75, 99, 10, 0);
	litest_touch_up(dev, 0);
	litest_assert_empty_queue(li);
}
END_TEST

START_TEST(touchpad_palm_detect_palm_becomes_pointer)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;

	if (!touchpad_has_palm_detect_size(dev) ||
	    !litest_has_2fg_scroll(dev))
		return;

	litest_enable_2fg_scroll(dev);

	litest_disable_tap(dev->libinput_device);

	litest_drain_events(li);

	litest_touch_down(dev, 0, 99, 50);
	litest_touch_move_to(dev, 0, 99, 50, 0, 70, 20, 0);
	litest_touch_up(dev, 0);

	libinput_dispatch(li);

	litest_assert_only_typed_events(li, LIBINPUT_EVENT_POINTER_MOTION);

	litest_assert_empty_queue(li);
}
END_TEST

START_TEST(touchpad_palm_detect_no_palm_moving_into_edges)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;

	if (!touchpad_has_palm_detect_size(dev))
		return;

	litest_disable_tap(dev->libinput_device);

	/* moving non-palm into the edge does not label it as palm */
	litest_drain_events(li);

	litest_touch_down(dev, 0, 50, 50);
	litest_touch_move_to(dev, 0, 50, 50, 99, 50, 10, 0);

	litest_drain_events(li);

	litest_touch_move_to(dev, 0, 99, 50, 99, 90, 10, 0);
	libinput_dispatch(li);

	litest_assert_only_typed_events(li, LIBINPUT_EVENT_POINTER_MOTION);

	litest_touch_up(dev, 0);
	libinput_dispatch(li);
	litest_assert_empty_queue(li);
}
END_TEST

START_TEST(touchpad_palm_detect_tap_hardbuttons)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;

	if (!touchpad_has_palm_detect_size(dev))
		return;

	litest_enable_tap(dev->libinput_device);

	litest_drain_events(li);

	litest_touch_down(dev, 0, 95, 5);
	litest_touch_up(dev, 0);
	litest_assert_empty_queue(li);

	litest_touch_down(dev, 0, 5, 5);
	litest_touch_up(dev, 0);
	litest_assert_empty_queue(li);

	litest_touch_down(dev, 0, 5, 99);
	litest_touch_up(dev, 0);
	litest_assert_empty_queue(li);

	litest_touch_down(dev, 0, 95, 99);
	litest_touch_up(dev, 0);
	litest_assert_empty_queue(li);
}
END_TEST

START_TEST(touchpad_palm_detect_tap_softbuttons)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;

	if (!touchpad_has_palm_detect_size(dev))
		return;

	litest_enable_tap(dev->libinput_device);
	litest_enable_buttonareas(dev);

	litest_drain_events(li);

	litest_touch_down(dev, 0, 95, 5);
	litest_touch_up(dev, 0);
	litest_assert_empty_queue(li);

	litest_touch_down(dev, 0, 5, 5);
	litest_touch_up(dev, 0);
	litest_assert_empty_queue(li);

	litest_touch_down(dev, 0, 5, 99);
	litest_touch_up(dev, 0);
	litest_assert_button_event(li,
				   BTN_LEFT,
				   LIBINPUT_BUTTON_STATE_PRESSED);
	litest_assert_button_event(li,
				   BTN_LEFT,
				   LIBINPUT_BUTTON_STATE_RELEASED);
	litest_assert_empty_queue(li);

	litest_touch_down(dev, 0, 95, 99);
	litest_touch_up(dev, 0);
	litest_assert_button_event(li,
				   BTN_LEFT,
				   LIBINPUT_BUTTON_STATE_PRESSED);
	litest_assert_button_event(li,
				   BTN_LEFT,
				   LIBINPUT_BUTTON_STATE_RELEASED);
	litest_assert_empty_queue(li);
}
END_TEST

START_TEST(touchpad_palm_detect_tap_clickfinger)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;

	if (!touchpad_has_palm_detect_size(dev))
		return;

	litest_enable_tap(dev->libinput_device);
	litest_enable_clickfinger(dev);

	litest_drain_events(li);

	litest_touch_down(dev, 0, 95, 5);
	litest_touch_up(dev, 0);
	litest_assert_empty_queue(li);

	litest_touch_down(dev, 0, 5, 5);
	litest_touch_up(dev, 0);
	litest_assert_empty_queue(li);

	litest_touch_down(dev, 0, 5, 99);
	litest_touch_up(dev, 0);
	litest_assert_empty_queue(li);

	litest_touch_down(dev, 0, 95, 99);
	litest_touch_up(dev, 0);
	litest_assert_empty_queue(li);
}
END_TEST

START_TEST(touchpad_no_palm_detect_2fg_scroll)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;

	if (!touchpad_has_palm_detect_size(dev) ||
	    !litest_has_2fg_scroll(dev))
		return;

	litest_enable_2fg_scroll(dev);

	litest_drain_events(li);

	/* first finger is palm, second finger isn't so we trigger 2fg
	 * scrolling */
	litest_touch_down(dev, 0, 99, 50);
	litest_touch_move_to(dev, 0, 99, 50, 99, 40, 10, 0);
	litest_touch_move_to(dev, 0, 99, 40, 99, 50, 10, 0);
	litest_assert_empty_queue(li);
	litest_touch_down(dev, 1, 50, 50);
	litest_assert_empty_queue(li);

	litest_touch_move_two_touches(dev, 99, 50, 50, 50, 0, -20, 10, 0);
	litest_assert_only_typed_events(li, LIBINPUT_EVENT_POINTER_AXIS);
}
END_TEST

START_TEST(touchpad_palm_detect_both_edges)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;

	if (!touchpad_has_palm_detect_size(dev) ||
	    !litest_has_2fg_scroll(dev))
		return;

	litest_enable_2fg_scroll(dev);

	litest_drain_events(li);

	/* two fingers moving up/down in the left/right palm zone must not
	 * generate events */
	litest_touch_down(dev, 0, 99, 50);
	litest_touch_move_to(dev, 0, 99, 50, 99, 40, 10, 0);
	litest_touch_move_to(dev, 0, 99, 40, 99, 50, 10, 0);
	litest_assert_empty_queue(li);
	litest_touch_down(dev, 1, 1, 50);
	litest_touch_move_to(dev, 1, 1, 50, 1, 40, 10, 0);
	litest_touch_move_to(dev, 1, 1, 40, 1, 50, 10, 0);
	litest_assert_empty_queue(li);

	litest_touch_move_two_touches(dev, 99, 50, 1, 50, 0, -20, 10, 0);
	litest_assert_empty_queue(li);
}
END_TEST

START_TEST(touchpad_left_handed)
{
	struct litest_device *dev = litest_current_device();
	struct libinput_device *d = dev->libinput_device;
	struct libinput *li = dev->libinput;
	enum libinput_config_status status;

	status = libinput_device_config_left_handed_set(d, 1);
	ck_assert_int_eq(status, LIBINPUT_CONFIG_STATUS_SUCCESS);

	litest_drain_events(li);
	litest_button_click(dev, BTN_LEFT, 1);
	litest_button_click(dev, BTN_LEFT, 0);

	litest_assert_button_event(li,
				   BTN_RIGHT,
				   LIBINPUT_BUTTON_STATE_PRESSED);
	litest_assert_button_event(li,
				   BTN_RIGHT,
				   LIBINPUT_BUTTON_STATE_RELEASED);

	litest_button_click(dev, BTN_RIGHT, 1);
	litest_button_click(dev, BTN_RIGHT, 0);
	litest_assert_button_event(li,
				   BTN_LEFT,
				   LIBINPUT_BUTTON_STATE_PRESSED);
	litest_assert_button_event(li,
				   BTN_LEFT,
				   LIBINPUT_BUTTON_STATE_RELEASED);

	if (libevdev_has_event_code(dev->evdev,
				    EV_KEY,
				    BTN_MIDDLE)) {
		litest_button_click(dev, BTN_MIDDLE, 1);
		litest_button_click(dev, BTN_MIDDLE, 0);
		litest_assert_button_event(li,
					   BTN_MIDDLE,
					   LIBINPUT_BUTTON_STATE_PRESSED);
		litest_assert_button_event(li,
					   BTN_MIDDLE,
					   LIBINPUT_BUTTON_STATE_RELEASED);
	}
}
END_TEST

START_TEST(touchpad_left_handed_clickpad)
{
	struct litest_device *dev = litest_current_device();
	struct libinput_device *d = dev->libinput_device;
	struct libinput *li = dev->libinput;
	enum libinput_config_status status;

	status = libinput_device_config_left_handed_set(d, 1);
	ck_assert_int_eq(status, LIBINPUT_CONFIG_STATUS_SUCCESS);

	litest_drain_events(li);
	litest_touch_down(dev, 0, 10, 90);
	litest_button_click(dev, BTN_LEFT, 1);
	litest_button_click(dev, BTN_LEFT, 0);
	litest_touch_up(dev, 0);

	litest_assert_button_event(li,
				   BTN_RIGHT,
				   LIBINPUT_BUTTON_STATE_PRESSED);
	litest_assert_button_event(li,
				   BTN_RIGHT,
				   LIBINPUT_BUTTON_STATE_RELEASED);

	litest_drain_events(li);
	litest_touch_down(dev, 0, 90, 90);
	litest_button_click(dev, BTN_LEFT, 1);
	litest_button_click(dev, BTN_LEFT, 0);
	litest_touch_up(dev, 0);

	litest_assert_button_event(li,
				   BTN_LEFT,
				   LIBINPUT_BUTTON_STATE_PRESSED);
	litest_assert_button_event(li,
				   BTN_LEFT,
				   LIBINPUT_BUTTON_STATE_RELEASED);

	litest_drain_events(li);
	litest_touch_down(dev, 0, 50, 50);
	litest_button_click(dev, BTN_LEFT, 1);
	litest_button_click(dev, BTN_LEFT, 0);
	litest_touch_up(dev, 0);

	litest_assert_button_event(li,
				   BTN_LEFT,
				   LIBINPUT_BUTTON_STATE_PRESSED);
	litest_assert_button_event(li,
				   BTN_LEFT,
				   LIBINPUT_BUTTON_STATE_RELEASED);
}
END_TEST

START_TEST(touchpad_left_handed_clickfinger)
{
	struct litest_device *dev = litest_current_device();
	struct libinput_device *d = dev->libinput_device;
	struct libinput *li = dev->libinput;
	enum libinput_config_status status;

	status = libinput_device_config_left_handed_set(d, 1);
	ck_assert_int_eq(status, LIBINPUT_CONFIG_STATUS_SUCCESS);

	litest_drain_events(li);
	litest_touch_down(dev, 0, 10, 90);
	litest_button_click(dev, BTN_LEFT, 1);
	litest_button_click(dev, BTN_LEFT, 0);
	litest_touch_up(dev, 0);

	/* Clickfinger is unaffected by left-handed setting */
	litest_assert_button_event(li,
				   BTN_LEFT,
				   LIBINPUT_BUTTON_STATE_PRESSED);
	litest_assert_button_event(li,
				   BTN_LEFT,
				   LIBINPUT_BUTTON_STATE_RELEASED);

	litest_drain_events(li);
	litest_touch_down(dev, 0, 10, 90);
	litest_touch_down(dev, 1, 30, 90);
	litest_button_click(dev, BTN_LEFT, 1);
	litest_button_click(dev, BTN_LEFT, 0);
	litest_touch_up(dev, 0);
	litest_touch_up(dev, 1);

	litest_assert_button_event(li,
				   BTN_RIGHT,
				   LIBINPUT_BUTTON_STATE_PRESSED);
	litest_assert_button_event(li,
				   BTN_RIGHT,
				   LIBINPUT_BUTTON_STATE_RELEASED);
}
END_TEST

START_TEST(touchpad_left_handed_tapping)
{
	struct litest_device *dev = litest_current_device();
	struct libinput_device *d = dev->libinput_device;
	struct libinput *li = dev->libinput;
	enum libinput_config_status status;

	litest_enable_tap(dev->libinput_device);

	status = libinput_device_config_left_handed_set(d, 1);
	ck_assert_int_eq(status, LIBINPUT_CONFIG_STATUS_SUCCESS);

	litest_drain_events(li);

	litest_touch_down(dev, 0, 50, 50);
	litest_touch_up(dev, 0);

	libinput_dispatch(li);
	litest_timeout_tap();
	libinput_dispatch(li);

	/* Tapping is unaffected by left-handed setting */
	litest_assert_button_event(li,
				   BTN_LEFT,
				   LIBINPUT_BUTTON_STATE_PRESSED);
	litest_assert_button_event(li,
				   BTN_LEFT,
				   LIBINPUT_BUTTON_STATE_RELEASED);
}
END_TEST

START_TEST(touchpad_left_handed_tapping_2fg)
{
	struct litest_device *dev = litest_current_device();
	struct libinput_device *d = dev->libinput_device;
	struct libinput *li = dev->libinput;
	enum libinput_config_status status;

	litest_enable_tap(dev->libinput_device);

	status = libinput_device_config_left_handed_set(d, 1);
	ck_assert_int_eq(status, LIBINPUT_CONFIG_STATUS_SUCCESS);

	litest_drain_events(li);

	litest_touch_down(dev, 0, 50, 50);
	litest_touch_down(dev, 1, 70, 50);
	litest_touch_up(dev, 1);
	litest_touch_up(dev, 0);

	libinput_dispatch(li);
	litest_timeout_tap();
	libinput_dispatch(li);

	/* Tapping is unaffected by left-handed setting */
	litest_assert_button_event(li,
				   BTN_RIGHT,
				   LIBINPUT_BUTTON_STATE_PRESSED);
	litest_assert_button_event(li,
				   BTN_RIGHT,
				   LIBINPUT_BUTTON_STATE_RELEASED);
}
END_TEST

START_TEST(touchpad_left_handed_delayed)
{
	struct litest_device *dev = litest_current_device();
	struct libinput_device *d = dev->libinput_device;
	struct libinput *li = dev->libinput;
	enum libinput_config_status status;

	litest_drain_events(li);
	litest_button_click(dev, BTN_LEFT, 1);
	libinput_dispatch(li);

	status = libinput_device_config_left_handed_set(d, 1);
	ck_assert_int_eq(status, LIBINPUT_CONFIG_STATUS_SUCCESS);

	litest_button_click(dev, BTN_LEFT, 0);

	litest_assert_button_event(li,
				   BTN_LEFT,
				   LIBINPUT_BUTTON_STATE_PRESSED);
	litest_assert_button_event(li,
				   BTN_LEFT,
				   LIBINPUT_BUTTON_STATE_RELEASED);

	/* left-handed takes effect now */
	litest_button_click(dev, BTN_RIGHT, 1);
	libinput_dispatch(li);
	litest_timeout_middlebutton();
	libinput_dispatch(li);
	litest_button_click(dev, BTN_LEFT, 1);
	libinput_dispatch(li);

	status = libinput_device_config_left_handed_set(d, 0);
	ck_assert_int_eq(status, LIBINPUT_CONFIG_STATUS_SUCCESS);

	litest_button_click(dev, BTN_RIGHT, 0);
	litest_button_click(dev, BTN_LEFT, 0);

	litest_assert_button_event(li,
				   BTN_LEFT,
				   LIBINPUT_BUTTON_STATE_PRESSED);
	litest_assert_button_event(li,
				   BTN_RIGHT,
				   LIBINPUT_BUTTON_STATE_PRESSED);
	litest_assert_button_event(li,
				   BTN_LEFT,
				   LIBINPUT_BUTTON_STATE_RELEASED);
	litest_assert_button_event(li,
				   BTN_RIGHT,
				   LIBINPUT_BUTTON_STATE_RELEASED);
}
END_TEST

START_TEST(touchpad_left_handed_clickpad_delayed)
{
	struct litest_device *dev = litest_current_device();
	struct libinput_device *d = dev->libinput_device;
	struct libinput *li = dev->libinput;
	enum libinput_config_status status;

	litest_drain_events(li);
	litest_touch_down(dev, 0, 10, 90);
	litest_button_click(dev, BTN_LEFT, 1);
	libinput_dispatch(li);

	status = libinput_device_config_left_handed_set(d, 1);
	ck_assert_int_eq(status, LIBINPUT_CONFIG_STATUS_SUCCESS);

	litest_button_click(dev, BTN_LEFT, 0);
	litest_touch_up(dev, 0);

	litest_assert_button_event(li,
				   BTN_LEFT,
				   LIBINPUT_BUTTON_STATE_PRESSED);
	litest_assert_button_event(li,
				   BTN_LEFT,
				   LIBINPUT_BUTTON_STATE_RELEASED);

	/* left-handed takes effect now */
	litest_drain_events(li);
	litest_touch_down(dev, 0, 90, 90);
	litest_button_click(dev, BTN_LEFT, 1);
	libinput_dispatch(li);

	status = libinput_device_config_left_handed_set(d, 0);
	ck_assert_int_eq(status, LIBINPUT_CONFIG_STATUS_SUCCESS);

	litest_button_click(dev, BTN_LEFT, 0);
	litest_touch_up(dev, 0);

	litest_assert_button_event(li,
				   BTN_LEFT,
				   LIBINPUT_BUTTON_STATE_PRESSED);
	litest_assert_button_event(li,
				   BTN_LEFT,
				   LIBINPUT_BUTTON_STATE_RELEASED);
}
END_TEST

static void
hover_continue(struct litest_device *dev, unsigned int slot,
	       int x, int y)
{
	litest_event(dev, EV_ABS, ABS_MT_SLOT, slot);
	litest_event(dev, EV_ABS, ABS_MT_POSITION_X, x);
	litest_event(dev, EV_ABS, ABS_MT_POSITION_Y, y);
	litest_event(dev, EV_ABS, ABS_X, x);
	litest_event(dev, EV_ABS, ABS_Y, y);
	litest_event(dev, EV_ABS, ABS_PRESSURE, 10);
	litest_event(dev, EV_ABS, ABS_TOOL_WIDTH, 6);
	/* WARNING: no SYN_REPORT! */
}

static void
hover_start(struct litest_device *dev, unsigned int slot,
	    int x, int y)
{
	static unsigned int tracking_id;

	litest_event(dev, EV_ABS, ABS_MT_SLOT, slot);
	litest_event(dev, EV_ABS, ABS_MT_TRACKING_ID, ++tracking_id);
	hover_continue(dev, slot, x, y);
	/* WARNING: no SYN_REPORT! */
}

START_TEST(touchpad_semi_mt_hover_noevent)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	int i;
	int x = 2400,
	    y = 2400;

	litest_drain_events(li);

	hover_start(dev, 0, x, y);
	litest_event(dev, EV_KEY, BTN_TOOL_FINGER, 1);
	litest_event(dev, EV_SYN, SYN_REPORT, 0);

	for (i = 0; i < 10; i++) {
		x += 200;
		y -= 200;
		litest_event(dev, EV_ABS, ABS_MT_POSITION_X, x);
		litest_event(dev, EV_ABS, ABS_MT_POSITION_Y, y);
		litest_event(dev, EV_ABS, ABS_X, x);
		litest_event(dev, EV_ABS, ABS_Y, y);
		litest_event(dev, EV_SYN, SYN_REPORT, 0);
	}

	litest_event(dev, EV_KEY, BTN_TOOL_FINGER, 0);
	litest_event(dev, EV_SYN, SYN_REPORT, 0);

	litest_assert_empty_queue(li);
}
END_TEST

START_TEST(touchpad_semi_mt_hover_down)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct libinput_event *event;
	int i;
	int x = 2400,
	    y = 2400;

	litest_drain_events(li);

	hover_start(dev, 0, x, y);
	litest_event(dev, EV_KEY, BTN_TOOL_FINGER, 1);
	litest_event(dev, EV_SYN, SYN_REPORT, 0);

	for (i = 0; i < 10; i++) {
		x += 200;
		y -= 200;
		litest_event(dev, EV_ABS, ABS_MT_POSITION_X, x);
		litest_event(dev, EV_ABS, ABS_MT_POSITION_Y, y);
		litest_event(dev, EV_ABS, ABS_X, x);
		litest_event(dev, EV_ABS, ABS_Y, y);
		litest_event(dev, EV_SYN, SYN_REPORT, 0);
	}

	litest_assert_empty_queue(li);

	litest_event(dev, EV_ABS, ABS_X, x + 100);
	litest_event(dev, EV_ABS, ABS_Y, y + 100);
	litest_event(dev, EV_KEY, BTN_TOUCH, 1);
	litest_event(dev, EV_SYN, SYN_REPORT, 0);
	libinput_dispatch(li);
	for (i = 0; i < 10; i++) {
		x -= 200;
		y += 200;
		litest_event(dev, EV_ABS, ABS_MT_POSITION_X, x);
		litest_event(dev, EV_ABS, ABS_MT_POSITION_Y, y);
		litest_event(dev, EV_ABS, ABS_X, x);
		litest_event(dev, EV_ABS, ABS_Y, y);
		litest_event(dev, EV_SYN, SYN_REPORT, 0);
	}

	libinput_dispatch(li);

	ck_assert_int_ne(libinput_next_event_type(li),
			 LIBINPUT_EVENT_NONE);
	while ((event = libinput_get_event(li)) != NULL) {
		ck_assert_int_eq(libinput_event_get_type(event),
				 LIBINPUT_EVENT_POINTER_MOTION);
		libinput_event_destroy(event);
		libinput_dispatch(li);
	}

	/* go back to hover */
	hover_continue(dev, 0, x, y);
	litest_event(dev, EV_KEY, BTN_TOUCH, 0);
	litest_event(dev, EV_SYN, SYN_REPORT, 0);

	for (i = 0; i < 10; i++) {
		x += 200;
		y -= 200;
		litest_event(dev, EV_ABS, ABS_MT_POSITION_X, x);
		litest_event(dev, EV_ABS, ABS_MT_POSITION_Y, y);
		litest_event(dev, EV_ABS, ABS_X, x);
		litest_event(dev, EV_ABS, ABS_Y, y);
		litest_event(dev, EV_SYN, SYN_REPORT, 0);
	}

	litest_event(dev, EV_KEY, BTN_TOOL_FINGER, 0);
	litest_event(dev, EV_SYN, SYN_REPORT, 0);

	litest_assert_empty_queue(li);
}
END_TEST

START_TEST(touchpad_semi_mt_hover_down_hover_down)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct libinput_event *event;
	int i, j;
	int x = 1400,
	    y = 1400;

	litest_drain_events(li);

	/* hover */
	hover_start(dev, 0, x, y);
	litest_event(dev, EV_KEY, BTN_TOOL_FINGER, 1);
	litest_event(dev, EV_SYN, SYN_REPORT, 0);
	litest_assert_empty_queue(li);

	for (i = 0; i < 3; i++) {
		/* touch */
		litest_event(dev, EV_ABS, ABS_X, x + 100);
		litest_event(dev, EV_ABS, ABS_Y, y + 100);
		litest_event(dev, EV_KEY, BTN_TOUCH, 1);
		litest_event(dev, EV_SYN, SYN_REPORT, 0);
		libinput_dispatch(li);

		for (j = 0; j < 5; j++) {
			x += 200;
			y += 200;
			litest_event(dev, EV_ABS, ABS_MT_POSITION_X, x);
			litest_event(dev, EV_ABS, ABS_MT_POSITION_Y, y);
			litest_event(dev, EV_ABS, ABS_X, x);
			litest_event(dev, EV_ABS, ABS_Y, y);
			litest_event(dev, EV_SYN, SYN_REPORT, 0);
		}

		libinput_dispatch(li);

		ck_assert_int_ne(libinput_next_event_type(li),
				 LIBINPUT_EVENT_NONE);
		while ((event = libinput_get_event(li)) != NULL) {
			ck_assert_int_eq(libinput_event_get_type(event),
					 LIBINPUT_EVENT_POINTER_MOTION);
			libinput_event_destroy(event);
			libinput_dispatch(li);
		}

		/* go back to hover */
		hover_continue(dev, 0, x, y);
		litest_event(dev, EV_KEY, BTN_TOUCH, 0);
		litest_event(dev, EV_SYN, SYN_REPORT, 0);

		for (j = 0; j < 5; j++) {
			x -= 200;
			y -= 200;
			litest_event(dev, EV_ABS, ABS_MT_POSITION_X, x);
			litest_event(dev, EV_ABS, ABS_MT_POSITION_Y, y);
			litest_event(dev, EV_ABS, ABS_X, x);
			litest_event(dev, EV_ABS, ABS_Y, y);
			litest_event(dev, EV_SYN, SYN_REPORT, 0);
		}

		litest_assert_empty_queue(li);
	}

	/* touch */
	litest_event(dev, EV_KEY, BTN_TOOL_FINGER, 0);
	litest_event(dev, EV_SYN, SYN_REPORT, 0);

	litest_assert_empty_queue(li);

	/* start a new touch to be sure */
	litest_touch_down(dev, 0, 50, 50);
	litest_touch_move_to(dev, 0, 50, 50, 70, 70, 10, 10);
	litest_touch_up(dev, 0);

	libinput_dispatch(li);
	ck_assert_int_ne(libinput_next_event_type(li),
			 LIBINPUT_EVENT_NONE);
	while ((event = libinput_get_event(li)) != NULL) {
		ck_assert_int_eq(libinput_event_get_type(event),
				 LIBINPUT_EVENT_POINTER_MOTION);
		libinput_event_destroy(event);
		libinput_dispatch(li);
	}
}
END_TEST

START_TEST(touchpad_semi_mt_hover_down_up)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	int i;
	int x = 1400,
	    y = 1400;

	litest_drain_events(li);

	/* hover two fingers, then touch */
	hover_start(dev, 0, x, y);
	litest_event(dev, EV_KEY, BTN_TOOL_FINGER, 1);
	litest_event(dev, EV_SYN, SYN_REPORT, 0);
	litest_assert_empty_queue(li);

	hover_start(dev, 1, x, y);
	litest_event(dev, EV_KEY, BTN_TOOL_FINGER, 0);
	litest_event(dev, EV_KEY, BTN_TOOL_DOUBLETAP, 1);
	litest_event(dev, EV_SYN, SYN_REPORT, 0);
	litest_assert_empty_queue(li);

	litest_event(dev, EV_KEY, BTN_TOOL_DOUBLETAP, 0);
	litest_event(dev, EV_KEY, BTN_TOOL_TRIPLETAP, 1);
	litest_event(dev, EV_KEY, BTN_TOUCH, 1);
	litest_event(dev, EV_SYN, SYN_REPORT, 0);

	litest_assert_empty_queue(li);

	/* hover first finger, end second in same frame */
	litest_event(dev, EV_ABS, ABS_MT_SLOT, 1);
	litest_event(dev, EV_ABS, ABS_MT_TRACKING_ID, -1);
	litest_event(dev, EV_KEY, BTN_TOOL_TRIPLETAP, 0);
	litest_event(dev, EV_KEY, BTN_TOOL_FINGER, 1);
	litest_event(dev, EV_KEY, BTN_TOUCH, 0);
	litest_event(dev, EV_SYN, SYN_REPORT, 0);

	litest_assert_empty_queue(li);

	litest_event(dev, EV_KEY, BTN_TOUCH, 1);
	litest_event(dev, EV_SYN, SYN_REPORT, 0);
	libinput_dispatch(li);

	/* now move the finger */
	for (i = 0; i < 10; i++) {
		litest_event(dev, EV_ABS, ABS_MT_SLOT, 0);
		litest_event(dev, EV_ABS, ABS_MT_POSITION_X, x);
		litest_event(dev, EV_ABS, ABS_MT_POSITION_Y, y);
		litest_event(dev, EV_ABS, ABS_X, x);
		litest_event(dev, EV_ABS, ABS_Y, y);
		litest_event(dev, EV_SYN, SYN_REPORT, 0);
		x -= 100;
		y -= 100;
	}

	litest_assert_only_typed_events(li, LIBINPUT_EVENT_POINTER_MOTION);

	litest_event(dev, EV_ABS, ABS_MT_SLOT, 0);
	litest_event(dev, EV_ABS, ABS_MT_TRACKING_ID, -1);
	litest_event(dev, EV_KEY, BTN_TOOL_FINGER, 0);
	litest_event(dev, EV_KEY, BTN_TOUCH, 0);
	litest_event(dev, EV_SYN, SYN_REPORT, 0);
	libinput_dispatch(li);
}
END_TEST

START_TEST(touchpad_semi_mt_hover_2fg_noevent)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	int i;
	int x = 2400,
	    y = 2400;

	litest_drain_events(li);

	hover_start(dev, 0, x, y);
	litest_event(dev, EV_KEY, BTN_TOOL_FINGER, 1);
	litest_event(dev, EV_SYN, SYN_REPORT, 0);

	hover_start(dev, 1, x + 500, y + 500);
	litest_event(dev, EV_KEY, BTN_TOOL_FINGER, 0);
	litest_event(dev, EV_KEY, BTN_TOOL_DOUBLETAP, 1);
	litest_event(dev, EV_SYN, SYN_REPORT, 0);

	for (i = 0; i < 10; i++) {
		x += 200;
		y -= 200;
		litest_event(dev, EV_ABS, ABS_MT_SLOT, 0);
		litest_event(dev, EV_ABS, ABS_MT_POSITION_X, x);
		litest_event(dev, EV_ABS, ABS_MT_POSITION_Y, y);
		litest_event(dev, EV_ABS, ABS_MT_SLOT, 1);
		litest_event(dev, EV_ABS, ABS_MT_POSITION_X, x + 500);
		litest_event(dev, EV_ABS, ABS_MT_POSITION_Y, y + 500);
		litest_event(dev, EV_ABS, ABS_X, x);
		litest_event(dev, EV_ABS, ABS_Y, y);
		litest_event(dev, EV_SYN, SYN_REPORT, 0);
	}

	litest_event(dev, EV_KEY, BTN_TOOL_DOUBLETAP, 0);
	litest_event(dev, EV_SYN, SYN_REPORT, 0);

	litest_assert_empty_queue(li);

	litest_event(dev, EV_KEY, BTN_TOOL_FINGER, 0);
	litest_event(dev, EV_SYN, SYN_REPORT, 0);

	litest_assert_empty_queue(li);
}
END_TEST

START_TEST(touchpad_semi_mt_hover_2fg_1fg_down)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct libinput_event *event;
	int i;
	int x = 2400,
	    y = 2400;

	litest_drain_events(li);

	/* two slots active, but BTN_TOOL_FINGER only */
	hover_start(dev, 0, x, y);
	hover_start(dev, 1, x + 500, y + 500);
	litest_event(dev, EV_KEY, BTN_TOUCH, 1);
	litest_event(dev, EV_KEY, BTN_TOOL_FINGER, 1);
	litest_event(dev, EV_SYN, SYN_REPORT, 0);

	for (i = 0; i < 10; i++) {
		x += 200;
		y -= 200;
		litest_event(dev, EV_ABS, ABS_MT_SLOT, 0);
		litest_event(dev, EV_ABS, ABS_MT_POSITION_X, x);
		litest_event(dev, EV_ABS, ABS_MT_POSITION_Y, y);
		litest_event(dev, EV_ABS, ABS_MT_SLOT, 1);
		litest_event(dev, EV_ABS, ABS_MT_POSITION_X, x + 500);
		litest_event(dev, EV_ABS, ABS_MT_POSITION_Y, y + 500);
		litest_event(dev, EV_ABS, ABS_X, x);
		litest_event(dev, EV_ABS, ABS_Y, y);
		litest_event(dev, EV_SYN, SYN_REPORT, 0);
	}

	litest_event(dev, EV_KEY, BTN_TOUCH, 0);
	litest_event(dev, EV_KEY, BTN_TOOL_FINGER, 0);
	litest_event(dev, EV_SYN, SYN_REPORT, 0);

	libinput_dispatch(li);

	ck_assert_int_ne(libinput_next_event_type(li),
			 LIBINPUT_EVENT_NONE);
	while ((event = libinput_get_event(li)) != NULL) {
		ck_assert_int_eq(libinput_event_get_type(event),
				 LIBINPUT_EVENT_POINTER_MOTION);
		libinput_event_destroy(event);
		libinput_dispatch(li);
	}
}
END_TEST

START_TEST(touchpad_semi_mt_hover_2fg_up)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;

	litest_touch_down(dev, 0, 70, 50);
	litest_touch_down(dev, 1, 50, 50);

	litest_push_event_frame(dev);
	litest_touch_move(dev, 0, 72, 50);
	litest_touch_move(dev, 1, 52, 50);
	litest_event(dev, EV_KEY, BTN_TOUCH, 0);
	litest_pop_event_frame(dev);

	litest_event(dev, EV_ABS, ABS_MT_SLOT, 0);
	litest_event(dev, EV_ABS, ABS_MT_TRACKING_ID, -1);
	litest_event(dev, EV_ABS, ABS_MT_SLOT, 1);
	litest_event(dev, EV_ABS, ABS_MT_TRACKING_ID, -1);
	litest_event(dev, EV_KEY, BTN_TOOL_DOUBLETAP, 0);
	litest_event(dev, EV_SYN, SYN_REPORT, 0);

	litest_drain_events(li);
}
END_TEST

START_TEST(touchpad_hover_noevent)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;

	litest_drain_events(li);

	litest_hover_start(dev, 0, 50, 50);
	litest_hover_move_to(dev, 0, 50, 50, 70, 70, 10, 10);
	litest_hover_end(dev, 0);

	litest_assert_empty_queue(li);
}
END_TEST

START_TEST(touchpad_hover_down)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;

	litest_drain_events(li);

	/* hover the finger */
	litest_hover_start(dev, 0, 50, 50);

	litest_hover_move_to(dev, 0, 50, 50, 70, 70, 10, 10);

	litest_assert_empty_queue(li);

	/* touch the finger on the sensor */
	litest_touch_move_to(dev, 0, 70, 70, 50, 50, 10, 10);

	libinput_dispatch(li);

	litest_assert_only_typed_events(li, LIBINPUT_EVENT_POINTER_MOTION);

	/* go back to hover */
	litest_hover_move_to(dev, 0, 50, 50, 70, 70, 10, 10);
	litest_hover_end(dev, 0);

	litest_assert_empty_queue(li);
}
END_TEST

START_TEST(touchpad_hover_down_hover_down)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	int i;

	litest_drain_events(li);

	litest_hover_start(dev, 0, 50, 50);

	for (i = 0; i < 3; i++) {

		/* hover the finger */
		litest_hover_move_to(dev, 0, 50, 50, 70, 70, 10, 10);

		litest_assert_empty_queue(li);

		/* touch the finger */
		litest_touch_move_to(dev, 0, 70, 70, 50, 50, 10, 10);

		libinput_dispatch(li);

		litest_assert_only_typed_events(li,
						LIBINPUT_EVENT_POINTER_MOTION);
	}

	litest_hover_end(dev, 0);

	/* start a new touch to be sure */
	litest_touch_down(dev, 0, 50, 50);
	litest_touch_move_to(dev, 0, 50, 50, 70, 70, 10, 10);
	litest_touch_up(dev, 0);

	litest_assert_only_typed_events(li, LIBINPUT_EVENT_POINTER_MOTION);
}
END_TEST

START_TEST(touchpad_hover_down_up)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;

	litest_drain_events(li);

	/* hover two fingers, and a touch */
	litest_push_event_frame(dev);
	litest_hover_start(dev, 0, 50, 50);
	litest_hover_start(dev, 1, 50, 50);
	litest_touch_down(dev, 2, 50, 50);
	litest_pop_event_frame(dev);;

	litest_assert_empty_queue(li);

	/* hover first finger, end second and third in same frame */
	litest_push_event_frame(dev);
	litest_hover_move(dev, 0, 55, 55);
	litest_hover_end(dev, 1);
	litest_touch_up(dev, 2);
	litest_pop_event_frame(dev);;

	litest_assert_empty_queue(li);

	/* now move the finger */
	litest_touch_move_to(dev, 0, 50, 50, 70, 70, 10, 10);

	litest_touch_up(dev, 0);

	litest_assert_only_typed_events(li, LIBINPUT_EVENT_POINTER_MOTION);
}
END_TEST

START_TEST(touchpad_hover_2fg_noevent)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;

	litest_drain_events(li);

	/* hover two fingers */
	litest_push_event_frame(dev);
	litest_hover_start(dev, 0, 25, 25);
	litest_hover_start(dev, 1, 50, 50);
	litest_pop_event_frame(dev);;

	litest_hover_move_two_touches(dev, 25, 25, 50, 50, 50, 50, 10, 0);

	litest_push_event_frame(dev);
	litest_hover_end(dev, 0);
	litest_hover_end(dev, 1);
	litest_pop_event_frame(dev);;

	litest_assert_empty_queue(li);
}
END_TEST

START_TEST(touchpad_hover_2fg_1fg_down)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	int i;

	litest_drain_events(li);

	/* hover two fingers */
	litest_push_event_frame(dev);
	litest_hover_start(dev, 0, 25, 25);
	litest_touch_down(dev, 1, 50, 50);
	litest_pop_event_frame(dev);;

	for (i = 0; i < 10; i++) {
		litest_push_event_frame(dev);
		litest_hover_move(dev, 0, 25 + 5 * i, 25 + 5 * i);
		litest_touch_move(dev, 1, 50 + 5 * i, 50 - 5 * i);
		litest_pop_event_frame(dev);;
	}

	litest_push_event_frame(dev);
	litest_hover_end(dev, 0);
	litest_touch_up(dev, 1);
	litest_pop_event_frame(dev);;

	litest_assert_only_typed_events(li, LIBINPUT_EVENT_POINTER_MOTION);
}
END_TEST

static void
assert_btnevent_from_device(struct litest_device *device,
			    unsigned int button,
			    enum libinput_button_state state)
{
	struct libinput *li = device->libinput;
	struct libinput_event *e;

	libinput_dispatch(li);
	e = libinput_get_event(li);
	litest_is_button_event(e, button, state);

	litest_assert_ptr_eq(libinput_event_get_device(e), device->libinput_device);
	libinput_event_destroy(e);
}

START_TEST(touchpad_trackpoint_buttons)
{
	struct litest_device *touchpad = litest_current_device();
	struct litest_device *trackpoint;
	struct libinput *li = touchpad->libinput;

	const struct buttons {
		unsigned int device_value;
		unsigned int real_value;
	} buttons[] = {
		{ BTN_0, BTN_LEFT },
		{ BTN_1, BTN_RIGHT },
		{ BTN_2, BTN_MIDDLE },
	};
	const struct buttons *b;

	trackpoint = litest_add_device(li,
				       LITEST_TRACKPOINT);
	libinput_device_config_scroll_set_method(trackpoint->libinput_device,
					 LIBINPUT_CONFIG_SCROLL_NO_SCROLL);

	litest_drain_events(li);

	ARRAY_FOR_EACH(buttons, b) {
		litest_button_click(touchpad, b->device_value, true);
		assert_btnevent_from_device(trackpoint,
					    b->real_value,
					    LIBINPUT_BUTTON_STATE_PRESSED);

		litest_button_click(touchpad, b->device_value, false);

		assert_btnevent_from_device(trackpoint,
					    b->real_value,
					    LIBINPUT_BUTTON_STATE_RELEASED);
	}

	litest_delete_device(trackpoint);
}
END_TEST

START_TEST(touchpad_trackpoint_mb_scroll)
{
	struct litest_device *touchpad = litest_current_device();
	struct litest_device *trackpoint;
	struct libinput *li = touchpad->libinput;

	trackpoint = litest_add_device(li,
				       LITEST_TRACKPOINT);

	litest_drain_events(li);
	litest_button_click(touchpad, BTN_2, true); /* middle */
	libinput_dispatch(li);
	litest_timeout_buttonscroll();
	libinput_dispatch(li);
	litest_event(trackpoint, EV_REL, REL_Y, -2);
	litest_event(trackpoint, EV_SYN, SYN_REPORT, 0);
	litest_event(trackpoint, EV_REL, REL_Y, -2);
	litest_event(trackpoint, EV_SYN, SYN_REPORT, 0);
	litest_event(trackpoint, EV_REL, REL_Y, -2);
	litest_event(trackpoint, EV_SYN, SYN_REPORT, 0);
	litest_event(trackpoint, EV_REL, REL_Y, -2);
	litest_event(trackpoint, EV_SYN, SYN_REPORT, 0);
	litest_button_click(touchpad, BTN_2, false);

	litest_assert_only_typed_events(li, LIBINPUT_EVENT_POINTER_AXIS);

	litest_delete_device(trackpoint);
}
END_TEST

START_TEST(touchpad_trackpoint_mb_click)
{
	struct litest_device *touchpad = litest_current_device();
	struct litest_device *trackpoint;
	struct libinput *li = touchpad->libinput;
	enum libinput_config_status status;

	trackpoint = litest_add_device(li,
				       LITEST_TRACKPOINT);
	status = libinput_device_config_scroll_set_method(
				  trackpoint->libinput_device,
				  LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN);
	ck_assert_int_eq(status, LIBINPUT_CONFIG_STATUS_SUCCESS);

	litest_drain_events(li);
	litest_button_click(touchpad, BTN_2, true); /* middle */
	litest_button_click(touchpad, BTN_2, false);

	assert_btnevent_from_device(trackpoint,
				    BTN_MIDDLE,
				    LIBINPUT_BUTTON_STATE_PRESSED);
	assert_btnevent_from_device(trackpoint,
				    BTN_MIDDLE,
				    LIBINPUT_BUTTON_STATE_RELEASED);
	litest_delete_device(trackpoint);
}
END_TEST

START_TEST(touchpad_trackpoint_buttons_softbuttons)
{
	struct litest_device *touchpad = litest_current_device();
	struct litest_device *trackpoint;
	struct libinput *li = touchpad->libinput;

	trackpoint = litest_add_device(li,
				       LITEST_TRACKPOINT);

	litest_drain_events(li);

	litest_touch_down(touchpad, 0, 95, 90);
	litest_button_click(touchpad, BTN_LEFT, true);
	litest_button_click(touchpad, BTN_1, true);
	litest_button_click(touchpad, BTN_LEFT, false);
	litest_touch_up(touchpad, 0);
	litest_button_click(touchpad, BTN_1, false);

	assert_btnevent_from_device(touchpad,
				    BTN_RIGHT,
				    LIBINPUT_BUTTON_STATE_PRESSED);
	assert_btnevent_from_device(trackpoint,
				    BTN_RIGHT,
				    LIBINPUT_BUTTON_STATE_PRESSED);
	assert_btnevent_from_device(touchpad,
				    BTN_RIGHT,
				    LIBINPUT_BUTTON_STATE_RELEASED);
	assert_btnevent_from_device(trackpoint,
				    BTN_RIGHT,
				    LIBINPUT_BUTTON_STATE_RELEASED);

	litest_touch_down(touchpad, 0, 95, 90);
	litest_button_click(touchpad, BTN_LEFT, true);
	litest_button_click(touchpad, BTN_1, true);
	litest_button_click(touchpad, BTN_1, false);
	litest_button_click(touchpad, BTN_LEFT, false);
	litest_touch_up(touchpad, 0);

	assert_btnevent_from_device(touchpad,
				    BTN_RIGHT,
				    LIBINPUT_BUTTON_STATE_PRESSED);
	assert_btnevent_from_device(trackpoint,
				    BTN_RIGHT,
				    LIBINPUT_BUTTON_STATE_PRESSED);
	assert_btnevent_from_device(trackpoint,
				    BTN_RIGHT,
				    LIBINPUT_BUTTON_STATE_RELEASED);
	assert_btnevent_from_device(touchpad,
				    BTN_RIGHT,
				    LIBINPUT_BUTTON_STATE_RELEASED);

	litest_delete_device(trackpoint);
}
END_TEST

START_TEST(touchpad_trackpoint_buttons_2fg_scroll)
{
	struct litest_device *touchpad = litest_current_device();
	struct litest_device *trackpoint;
	struct libinput *li = touchpad->libinput;
	struct libinput_event *e;
	struct libinput_event_pointer *pev;
	double val;

	trackpoint = litest_add_device(li,
				       LITEST_TRACKPOINT);

	litest_drain_events(li);

	litest_touch_down(touchpad, 0, 49, 70);
	litest_touch_down(touchpad, 1, 51, 70);
	litest_touch_move_two_touches(touchpad, 49, 70, 51, 70, 0, -40, 10, 0);

	libinput_dispatch(li);
	litest_wait_for_event(li);

	/* Make sure we get scroll events but _not_ the scroll release */
	while ((e = libinput_get_event(li))) {
		ck_assert_int_eq(libinput_event_get_type(e),
				 LIBINPUT_EVENT_POINTER_AXIS);
		pev = libinput_event_get_pointer_event(e);
		val = libinput_event_pointer_get_axis_value(pev,
				LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL);
		ck_assert(val != 0.0);
		libinput_event_destroy(e);
	}

	litest_button_click(touchpad, BTN_1, true);
	assert_btnevent_from_device(trackpoint,
				    BTN_RIGHT,
				    LIBINPUT_BUTTON_STATE_PRESSED);

	litest_touch_move_to(touchpad, 0, 40, 30, 40, 70, 10, 0);
	litest_touch_move_to(touchpad, 1, 60, 30, 60, 70, 10, 0);

	litest_assert_only_typed_events(li, LIBINPUT_EVENT_POINTER_AXIS);

	while ((e = libinput_get_event(li))) {
		ck_assert_int_eq(libinput_event_get_type(e),
				 LIBINPUT_EVENT_POINTER_AXIS);
		pev = libinput_event_get_pointer_event(e);
		val = libinput_event_pointer_get_axis_value(pev,
				LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL);
		ck_assert(val != 0.0);
		libinput_event_destroy(e);
	}

	litest_button_click(touchpad, BTN_1, false);
	assert_btnevent_from_device(trackpoint,
				    BTN_RIGHT,
				    LIBINPUT_BUTTON_STATE_RELEASED);

	/* the movement lags behind the touch movement, so the first couple
	   events can be downwards even though we started scrolling up. do a
	   short scroll up, drain those events, then we can use
	   litest_assert_scroll() which tests for the trailing 0/0 scroll
	   for us.
	   */
	litest_touch_move_to(touchpad, 0, 40, 70, 40, 60, 10, 0);
	litest_touch_move_to(touchpad, 1, 60, 70, 60, 60, 10, 0);
	litest_assert_only_typed_events(li, LIBINPUT_EVENT_POINTER_AXIS);
	litest_touch_move_to(touchpad, 0, 40, 60, 40, 30, 10, 0);
	litest_touch_move_to(touchpad, 1, 60, 60, 60, 30, 10, 0);

	litest_touch_up(touchpad, 0);
	litest_touch_up(touchpad, 1);

	libinput_dispatch(li);

	litest_assert_scroll(li,
			     LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL,
			     -1);

	litest_delete_device(trackpoint);
}
END_TEST

START_TEST(touchpad_trackpoint_no_trackpoint)
{
	struct litest_device *touchpad = litest_current_device();
	struct libinput *li = touchpad->libinput;

	litest_drain_events(li);
	litest_button_click(touchpad, BTN_0, true); /* left */
	litest_button_click(touchpad, BTN_0, false);
	litest_assert_empty_queue(li);

	litest_button_click(touchpad, BTN_1, true); /* right */
	litest_button_click(touchpad, BTN_1, false);
	litest_assert_empty_queue(li);

	litest_button_click(touchpad, BTN_2, true); /* middle */
	litest_button_click(touchpad, BTN_2, false);
	litest_assert_empty_queue(li);
}
END_TEST

START_TEST(touchpad_initial_state)
{
	struct litest_device *dev;
	struct libinput *libinput1, *libinput2;
	struct libinput_event *ev1, *ev2;
	struct libinput_event_pointer *p1, *p2;
	int axis = _i; /* looped test */
	int x = 40, y = 60;

	dev = litest_current_device();
	libinput1 = dev->libinput;

	litest_disable_tap(dev->libinput_device);

	litest_touch_down(dev, 0, x, y);
	litest_touch_up(dev, 0);

	/* device is now on some x/y value */
	litest_drain_events(libinput1);

	libinput2 = litest_create_context();
	libinput_path_add_device(libinput2,
				 libevdev_uinput_get_devnode(dev->uinput));
	litest_drain_events(libinput2);

	if (axis == ABS_X)
		x = 30;
	else
		y = 30;
	litest_touch_down(dev, 0, x, y);
	litest_touch_move_to(dev, 0, x, y, 80, 80, 10, 1);
	litest_touch_up(dev, 0);
	libinput_dispatch(libinput1);
	libinput_dispatch(libinput2);

	litest_wait_for_event(libinput1);
	litest_wait_for_event(libinput2);

	while (libinput_next_event_type(libinput1)) {
		ev1 = libinput_get_event(libinput1);
		ev2 = libinput_get_event(libinput2);

		p1 = litest_is_motion_event(ev1);
		p2 = litest_is_motion_event(ev2);

		ck_assert_int_eq(libinput_event_get_type(ev1),
				 libinput_event_get_type(ev2));

		ck_assert_int_eq(libinput_event_pointer_get_dx(p1),
				 libinput_event_pointer_get_dx(p2));
		ck_assert_int_eq(libinput_event_pointer_get_dy(p1),
				 libinput_event_pointer_get_dy(p2));
		libinput_event_destroy(ev1);
		libinput_event_destroy(ev2);
	}

	libinput_unref(libinput2);
}
END_TEST

static int
has_thumb_detect(struct litest_device *dev)
{
	double w, h;

	if (!libevdev_has_event_code(dev->evdev, EV_ABS, ABS_MT_PRESSURE))
		return 0;

	if (libinput_device_get_size(dev->libinput_device, &w, &h) != 0)
		return 0;

	return h >= 50.0;
}

START_TEST(touchpad_thumb_begin_no_motion)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct axis_replacement axes[] = {
		{ ABS_MT_PRESSURE, 75 },
		{ -1, 0 }
	};

	if (!has_thumb_detect(dev))
		return;

	litest_disable_tap(dev->libinput_device);

	litest_drain_events(li);

	litest_touch_down_extended(dev, 0, 50, 99, axes);
	litest_touch_move_to(dev, 0, 50, 99, 80, 99, 10, 0);
	litest_touch_up(dev, 0);

	litest_assert_empty_queue(li);
}
END_TEST

START_TEST(touchpad_thumb_update_no_motion)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct axis_replacement axes[] = {
		{ ABS_MT_PRESSURE, 75 },
		{ -1, 0 }
	};

	litest_disable_tap(dev->libinput_device);
	litest_enable_clickfinger(dev);

	if (!has_thumb_detect(dev))
		return;

	litest_drain_events(li);

	litest_touch_down(dev, 0, 59, 99);
	litest_touch_move_extended(dev, 0, 59, 99, axes);
	litest_touch_move_to(dev, 0, 60, 99, 80, 99, 10, 0);
	litest_touch_up(dev, 0);

	litest_assert_empty_queue(li);
}
END_TEST

START_TEST(touchpad_thumb_moving)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct axis_replacement axes[] = {
		{ ABS_MT_PRESSURE, 75 },
		{ -1, 0 }
	};

	litest_disable_tap(dev->libinput_device);
	litest_enable_clickfinger(dev);

	if (!has_thumb_detect(dev))
		return;

	litest_drain_events(li);

	litest_touch_down(dev, 0, 50, 99);
	litest_touch_move_to(dev, 0, 50, 99, 60, 99, 10, 0);
	litest_touch_move_extended(dev, 0, 65, 99, axes);
	litest_touch_move_to(dev, 0, 65, 99, 80, 99, 10, 0);
	litest_touch_up(dev, 0);

	litest_assert_only_typed_events(li, LIBINPUT_EVENT_POINTER_MOTION);
}
END_TEST

START_TEST(touchpad_thumb_clickfinger)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct libinput_event *event;
	struct libinput_event_pointer *ptrev;
	struct axis_replacement axes[] = {
		{ ABS_MT_PRESSURE, 75 },
		{ -1, 0 }
	};

	if (!has_thumb_detect(dev))
		return;

	litest_disable_tap(dev->libinput_device);

	libinput_device_config_click_set_method(dev->libinput_device,
						LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER);

	litest_drain_events(li);

	litest_touch_down(dev, 0, 50, 99);
	litest_touch_down(dev, 1, 60, 99);
	litest_touch_move_extended(dev, 0, 55, 99, axes);
	litest_button_click(dev, BTN_LEFT, true);

	libinput_dispatch(li);
	event = libinput_get_event(li);
	ptrev = litest_is_button_event(event,
				       BTN_LEFT,
				       LIBINPUT_BUTTON_STATE_PRESSED);
	libinput_event_destroy(libinput_event_pointer_get_base_event(ptrev));

	litest_assert_empty_queue(li);

	litest_button_click(dev, BTN_LEFT, false);
	litest_touch_up(dev, 0);
	litest_touch_up(dev, 1);

	litest_drain_events(li);

	litest_touch_down(dev, 0, 50, 99);
	litest_touch_down(dev, 1, 60, 99);
	litest_touch_move_extended(dev, 1, 65, 99, axes);
	litest_button_click(dev, BTN_LEFT, true);

	libinput_dispatch(li);
	event = libinput_get_event(li);
	ptrev = litest_is_button_event(event,
				       BTN_LEFT,
				       LIBINPUT_BUTTON_STATE_PRESSED);
	libinput_event_destroy(libinput_event_pointer_get_base_event(ptrev));

	litest_assert_empty_queue(li);
}
END_TEST

START_TEST(touchpad_thumb_btnarea)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct libinput_event *event;
	struct libinput_event_pointer *ptrev;
	struct axis_replacement axes[] = {
		{ ABS_MT_PRESSURE, 75 },
		{ -1, 0 }
	};

	if (!has_thumb_detect(dev))
		return;

	litest_disable_tap(dev->libinput_device);

	libinput_device_config_click_set_method(dev->libinput_device,
						LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS);

	litest_drain_events(li);

	litest_touch_down(dev, 0, 90, 99);
	litest_touch_move_extended(dev, 0, 95, 99, axes);
	litest_button_click(dev, BTN_LEFT, true);

	/* button areas work as usual with a thumb */

	libinput_dispatch(li);
	event = libinput_get_event(li);
	ptrev = litest_is_button_event(event,
				       BTN_RIGHT,
				       LIBINPUT_BUTTON_STATE_PRESSED);
	libinput_event_destroy(libinput_event_pointer_get_base_event(ptrev));

	litest_assert_empty_queue(li);
}
END_TEST

START_TEST(touchpad_thumb_edgescroll)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct axis_replacement axes[] = {
		{ ABS_MT_PRESSURE, 75 },
		{ -1, 0 }
	};

	if (!has_thumb_detect(dev))
		return;

	litest_enable_edge_scroll(dev);
	litest_disable_tap(dev->libinput_device);

	litest_drain_events(li);

	litest_touch_down(dev, 0, 99, 30);
	litest_touch_move_to(dev, 0, 99, 30, 99, 50, 10, 0);
	litest_drain_events(li);

	litest_touch_move_extended(dev, 0, 99, 55, axes);
	libinput_dispatch(li);
	litest_assert_only_typed_events(li, LIBINPUT_EVENT_POINTER_AXIS);

	litest_touch_move_to(dev, 0, 99, 55, 99, 70, 10, 0);

	litest_assert_only_typed_events(li, LIBINPUT_EVENT_POINTER_AXIS);
}
END_TEST

START_TEST(touchpad_thumb_tap_begin)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct axis_replacement axes[] = {
		{ ABS_MT_PRESSURE, 75 },
		{ -1, 0 }
	};

	if (!has_thumb_detect(dev))
		return;

	litest_enable_tap(dev->libinput_device);
	litest_enable_clickfinger(dev);
	litest_drain_events(li);

	/* touch down is a thumb */
	litest_touch_down_extended(dev, 0, 50, 99, axes);
	litest_touch_up(dev, 0);
	libinput_dispatch(li);
	litest_timeout_tap();

	litest_assert_empty_queue(li);

	/* make sure normal tap still works */
	litest_touch_down(dev, 0, 50, 99);
	litest_touch_up(dev, 0);
	libinput_dispatch(li);
	litest_timeout_tap();
	litest_assert_only_typed_events(li, LIBINPUT_EVENT_POINTER_BUTTON);
}
END_TEST

START_TEST(touchpad_thumb_tap_touch)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct axis_replacement axes[] = {
		{ ABS_MT_PRESSURE, 75 },
		{ -1, 0 }
	};

	if (!has_thumb_detect(dev))
		return;

	litest_enable_tap(dev->libinput_device);
	litest_enable_clickfinger(dev);
	litest_drain_events(li);

	/* event after touch down is thumb */
	litest_touch_down(dev, 0, 50, 80);
	litest_touch_move_extended(dev, 0, 51, 99, axes);
	litest_touch_up(dev, 0);
	libinput_dispatch(li);
	litest_timeout_tap();
	litest_assert_empty_queue(li);

	/* make sure normal tap still works */
	litest_touch_down(dev, 0, 50, 99);
	litest_touch_up(dev, 0);
	libinput_dispatch(li);
	litest_timeout_tap();
	litest_assert_only_typed_events(li, LIBINPUT_EVENT_POINTER_BUTTON);
}
END_TEST

START_TEST(touchpad_thumb_tap_hold)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct axis_replacement axes[] = {
		{ ABS_MT_PRESSURE, 75 },
		{ -1, 0 }
	};

	if (!has_thumb_detect(dev))
		return;

	litest_enable_tap(dev->libinput_device);
	litest_enable_clickfinger(dev);
	litest_drain_events(li);

	/* event in state HOLD is thumb */
	litest_touch_down(dev, 0, 50, 99);
	libinput_dispatch(li);
	litest_timeout_tap();
	libinput_dispatch(li);
	litest_touch_move_extended(dev, 0, 51, 99, axes);
	litest_touch_up(dev, 0);
	litest_assert_empty_queue(li);

	/* make sure normal tap still works */
	litest_touch_down(dev, 0, 50, 99);
	litest_touch_up(dev, 0);
	libinput_dispatch(li);
	litest_timeout_tap();
	litest_assert_only_typed_events(li, LIBINPUT_EVENT_POINTER_BUTTON);
}
END_TEST

START_TEST(touchpad_thumb_tap_hold_2ndfg)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct axis_replacement axes[] = {
		{ ABS_MT_PRESSURE, 75 },
		{ -1, 0 }
	};

	if (!has_thumb_detect(dev))
		return;

	litest_enable_tap(dev->libinput_device);
	litest_enable_clickfinger(dev);
	litest_drain_events(li);

	/* event in state HOLD is thumb */
	litest_touch_down(dev, 0, 50, 99);
	libinput_dispatch(li);
	litest_timeout_tap();
	libinput_dispatch(li);
	litest_touch_move_extended(dev, 0, 51, 99, axes);

	litest_assert_empty_queue(li);

	/* one finger is a thumb, now get second finger down */
	litest_touch_down(dev, 1, 60, 50);
	litest_assert_empty_queue(li);

	/* release thumb */
	litest_touch_up(dev, 0);
	litest_assert_empty_queue(li);

	/* timeout -> into HOLD, no event on release */
	libinput_dispatch(li);
	litest_timeout_tap();
	libinput_dispatch(li);
	litest_touch_up(dev, 1);
	litest_assert_empty_queue(li);

	/* make sure normal tap still works */
	litest_touch_down(dev, 0, 50, 99);
	litest_touch_up(dev, 0);
	libinput_dispatch(li);
	litest_timeout_tap();
	litest_assert_only_typed_events(li, LIBINPUT_EVENT_POINTER_BUTTON);
}
END_TEST

START_TEST(touchpad_thumb_tap_hold_2ndfg_tap)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct libinput_event *event;
	struct libinput_event_pointer *ptrev;
	struct axis_replacement axes[] = {
		{ ABS_MT_PRESSURE, 75 },
		{ -1, 0 }
	};

	if (!has_thumb_detect(dev))
		return;

	litest_enable_tap(dev->libinput_device);
	litest_drain_events(li);

	/* event in state HOLD is thumb */
	litest_touch_down(dev, 0, 50, 99);
	libinput_dispatch(li);
	litest_timeout_tap();
	libinput_dispatch(li);
	litest_touch_move_extended(dev, 0, 51, 99, axes);

	litest_assert_empty_queue(li);

	/* one finger is a thumb, now get second finger down */
	litest_touch_down(dev, 1, 60, 50);
	litest_assert_empty_queue(li);

	/* release thumb */
	litest_touch_up(dev, 0);
	litest_assert_empty_queue(li);

	/* release second finger, within timeout, ergo event */
	litest_touch_up(dev, 1);
	libinput_dispatch(li);
	event = libinput_get_event(li);
	ptrev = litest_is_button_event(event,
				       BTN_LEFT,
				       LIBINPUT_BUTTON_STATE_PRESSED);
	libinput_event_destroy(libinput_event_pointer_get_base_event(ptrev));

	libinput_dispatch(li);
	litest_timeout_tap();
	libinput_dispatch(li);
	event = libinput_get_event(li);
	ptrev = litest_is_button_event(event,
				       BTN_LEFT,
				       LIBINPUT_BUTTON_STATE_RELEASED);
	libinput_event_destroy(libinput_event_pointer_get_base_event(ptrev));

	/* make sure normal tap still works */
	litest_touch_down(dev, 0, 50, 99);
	litest_touch_up(dev, 0);
	libinput_dispatch(li);
	litest_timeout_tap();
	litest_assert_only_typed_events(li, LIBINPUT_EVENT_POINTER_BUTTON);
}
END_TEST

START_TEST(touchpad_tool_tripletap_touch_count)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct libinput_event *event;
	struct libinput_event_pointer *ptrev;

	/* Synaptics touchpads sometimes end one touch point while
	 * simultaneously setting BTN_TOOL_TRIPLETAP.
	 * https://bugs.freedesktop.org/show_bug.cgi?id=91352
	 */
	litest_drain_events(li);
	litest_enable_clickfinger(dev);

	/* touch 1 down */
	litest_event(dev, EV_ABS, ABS_MT_SLOT, 0);
	litest_event(dev, EV_ABS, ABS_MT_TRACKING_ID, 1);
	litest_event(dev, EV_ABS, ABS_MT_POSITION_X, 1200);
	litest_event(dev, EV_ABS, ABS_MT_POSITION_Y, 3200);
	litest_event(dev, EV_ABS, ABS_MT_PRESSURE, 78);
	litest_event(dev, EV_ABS, ABS_X, 1200);
	litest_event(dev, EV_ABS, ABS_Y, 3200);
	litest_event(dev, EV_ABS, ABS_PRESSURE, 78);
	litest_event(dev, EV_KEY, BTN_TOOL_FINGER, 1);
	litest_event(dev, EV_KEY, BTN_TOUCH, 1);
	litest_event(dev, EV_SYN, SYN_REPORT, 0);
	libinput_dispatch(li);
	msleep(2);

	/* touch 2 down */
	litest_event(dev, EV_ABS, ABS_MT_SLOT, 1);
	litest_event(dev, EV_ABS, ABS_MT_TRACKING_ID, 1);
	litest_event(dev, EV_ABS, ABS_MT_POSITION_X, 2200);
	litest_event(dev, EV_ABS, ABS_MT_POSITION_Y, 3200);
	litest_event(dev, EV_ABS, ABS_MT_PRESSURE, 73);
	litest_event(dev, EV_KEY, BTN_TOOL_FINGER, 0);
	litest_event(dev, EV_KEY, BTN_TOOL_DOUBLETAP, 1);
	litest_event(dev, EV_SYN, SYN_REPORT, 0);
	libinput_dispatch(li);
	msleep(2);

	/* touch 3 down, coordinate jump + ends slot 1 */
	litest_event(dev, EV_ABS, ABS_MT_SLOT, 0);
	litest_event(dev, EV_ABS, ABS_MT_POSITION_X, 4000);
	litest_event(dev, EV_ABS, ABS_MT_POSITION_Y, 4000);
	litest_event(dev, EV_ABS, ABS_MT_PRESSURE, 78);
	litest_event(dev, EV_ABS, ABS_MT_SLOT, 1);
	litest_event(dev, EV_ABS, ABS_MT_TRACKING_ID, -1);
	litest_event(dev, EV_ABS, ABS_X, 4000);
	litest_event(dev, EV_ABS, ABS_Y, 4000);
	litest_event(dev, EV_ABS, ABS_PRESSURE, 78);
	litest_event(dev, EV_KEY, BTN_TOOL_DOUBLETAP, 0);
	litest_event(dev, EV_KEY, BTN_TOOL_TRIPLETAP, 1);
	litest_event(dev, EV_SYN, SYN_REPORT, 0);
	libinput_dispatch(li);
	msleep(2);

	/* slot 2 reactivated:
	 * Note, slot is activated close enough that we don't accidentally
	 * trigger the clickfinger distance check, remains to be seen if
	 * that is true for real-world interaction.
	 */
	litest_event(dev, EV_ABS, ABS_MT_SLOT, 0);
	litest_event(dev, EV_ABS, ABS_MT_POSITION_X, 4000);
	litest_event(dev, EV_ABS, ABS_MT_POSITION_Y, 4000);
	litest_event(dev, EV_ABS, ABS_MT_PRESSURE, 78);
	litest_event(dev, EV_ABS, ABS_MT_SLOT, 1);
	litest_event(dev, EV_ABS, ABS_MT_TRACKING_ID, 3);
	litest_event(dev, EV_ABS, ABS_MT_POSITION_X, 3500);
	litest_event(dev, EV_ABS, ABS_MT_POSITION_Y, 3500);
	litest_event(dev, EV_ABS, ABS_MT_PRESSURE, 73);
	litest_event(dev, EV_ABS, ABS_X, 4000);
	litest_event(dev, EV_ABS, ABS_Y, 4000);
	litest_event(dev, EV_ABS, ABS_PRESSURE, 78);
	litest_event(dev, EV_SYN, SYN_REPORT, 0);
	libinput_dispatch(li);
	msleep(2);

	/* now a click should trigger middle click */
	litest_event(dev, EV_KEY, BTN_LEFT, 1);
	litest_event(dev, EV_SYN, SYN_REPORT, 0);
	libinput_dispatch(li);
	litest_event(dev, EV_KEY, BTN_LEFT, 0);
	litest_event(dev, EV_SYN, SYN_REPORT, 0);
	libinput_dispatch(li);

	litest_wait_for_event(li);
	event = libinput_get_event(li);
	ptrev = litest_is_button_event(event,
				       BTN_MIDDLE,
				       LIBINPUT_BUTTON_STATE_PRESSED);
	libinput_event_destroy(event);
	event = libinput_get_event(li);
	ptrev = litest_is_button_event(event,
				       BTN_MIDDLE,
				       LIBINPUT_BUTTON_STATE_RELEASED);
	/* silence gcc set-but-not-used warning, litest_is_button_event
	 * checks what we care about */
	event = libinput_event_pointer_get_base_event(ptrev);
	libinput_event_destroy(event);

	/* release everything */
	litest_event(dev, EV_ABS, ABS_MT_SLOT, 0);
	litest_event(dev, EV_ABS, ABS_MT_TRACKING_ID, -1);
	litest_event(dev, EV_ABS, ABS_MT_SLOT, 0);
	litest_event(dev, EV_ABS, ABS_MT_TRACKING_ID, -1);
	litest_event(dev, EV_KEY, BTN_TOOL_FINGER, 0);
	litest_event(dev, EV_KEY, BTN_TOOL_DOUBLETAP, 0);
	litest_event(dev, EV_KEY, BTN_TOOL_TRIPLETAP, 0);
	litest_event(dev, EV_KEY, BTN_TOUCH, 0);
	litest_event(dev, EV_SYN, SYN_REPORT, 0);
}
END_TEST

START_TEST(touchpad_time_usec)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct libinput_event *event;
	struct libinput_event_pointer *ptrev;

	litest_disable_tap(dev->libinput_device);

	litest_drain_events(li);

	litest_touch_down(dev, 0, 50, 50);
	litest_touch_move_to(dev, 0, 50, 50, 80, 50, 5, 0);
	litest_touch_up(dev, 0);

	libinput_dispatch(li);

	event = libinput_get_event(li);
	ck_assert_notnull(event);

	while (event) {
		uint64_t utime;

		ptrev = litest_is_motion_event(event);
		utime = libinput_event_pointer_get_time_usec(ptrev);

		ck_assert_int_eq(libinput_event_pointer_get_time(ptrev),
				 (uint32_t) (utime / 1000));
		libinput_event_destroy(event);
		event = libinput_get_event(li);
	}
}
END_TEST

START_TEST(touchpad_jump_finger_motion)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct libinput_event *event;
	struct libinput_event_pointer *ptrev;

	litest_touch_down(dev, 0, 20, 30);
	litest_touch_move_to(dev, 0, 20, 30, 90, 30, 10, 0);
	litest_drain_events(li);

	litest_disable_log_handler(li);
	litest_touch_move_to(dev, 0, 90, 30, 20, 80, 1, 0);
	litest_assert_empty_queue(li);
	litest_restore_log_handler(li);

	litest_touch_move_to(dev, 0, 20, 80, 21, 81, 10, 0);
	litest_touch_up(dev, 0);

	/* expect lots of little events, no big jump */
	libinput_dispatch(li);
	event = libinput_get_event(li);
	do {
		double dx, dy;

		ptrev = litest_is_motion_event(event);
		dx = libinput_event_pointer_get_dx(ptrev);
		dy = libinput_event_pointer_get_dy(ptrev);
		ck_assert_int_lt(abs(dx), 20);
		ck_assert_int_lt(abs(dy), 20);

		libinput_event_destroy(event);
		event = libinput_get_event(li);
	} while (event != NULL);
}
END_TEST

void
litest_setup_tests(void)
{
	struct range axis_range = {ABS_X, ABS_Y + 1};

	litest_add("touchpad:motion", touchpad_1fg_motion, LITEST_TOUCHPAD, LITEST_ANY);
	litest_add("touchpad:motion", touchpad_2fg_no_motion, LITEST_TOUCHPAD, LITEST_SINGLE_TOUCH);

	litest_add("touchpad:palm", touchpad_palm_detect_at_edge, LITEST_TOUCHPAD, LITEST_ANY);
	litest_add("touchpad:palm", touchpad_palm_detect_at_bottom_corners, LITEST_TOUCHPAD, LITEST_CLICKPAD);
	litest_add("touchpad:palm", touchpad_palm_detect_at_top_corners, LITEST_TOUCHPAD, LITEST_TOPBUTTONPAD);
	litest_add("touchpad:palm", touchpad_palm_detect_palm_becomes_pointer, LITEST_TOUCHPAD, LITEST_ANY);
	litest_add("touchpad:palm", touchpad_palm_detect_palm_stays_palm, LITEST_TOUCHPAD, LITEST_ANY);
	litest_add("touchpad:palm", touchpad_palm_detect_no_palm_moving_into_edges, LITEST_TOUCHPAD, LITEST_ANY);
	litest_add("touchpad:palm", touchpad_palm_detect_tap_hardbuttons, LITEST_TOUCHPAD, LITEST_CLICKPAD);
	litest_add("touchpad:palm", touchpad_palm_detect_tap_softbuttons, LITEST_CLICKPAD, LITEST_ANY);
	litest_add("touchpad:palm", touchpad_palm_detect_tap_clickfinger, LITEST_CLICKPAD, LITEST_ANY);
	litest_add("touchpad:palm", touchpad_no_palm_detect_at_edge_for_edge_scrolling, LITEST_TOUCHPAD, LITEST_CLICKPAD);
	litest_add("touchpad:palm", touchpad_no_palm_detect_2fg_scroll, LITEST_TOUCHPAD, LITEST_SINGLE_TOUCH);
	litest_add("touchpad:palm", touchpad_palm_detect_both_edges, LITEST_TOUCHPAD, LITEST_SINGLE_TOUCH);

	litest_add("touchpad:left-handed", touchpad_left_handed, LITEST_TOUCHPAD|LITEST_BUTTON, LITEST_CLICKPAD);
	litest_add("touchpad:left-handed", touchpad_left_handed_clickpad, LITEST_CLICKPAD, LITEST_APPLE_CLICKPAD);
	litest_add("touchpad:left-handed", touchpad_left_handed_clickfinger, LITEST_APPLE_CLICKPAD, LITEST_ANY);
	litest_add("touchpad:left-handed", touchpad_left_handed_tapping, LITEST_TOUCHPAD, LITEST_ANY);
	litest_add("touchpad:left-handed", touchpad_left_handed_tapping_2fg, LITEST_TOUCHPAD, LITEST_SINGLE_TOUCH);
	litest_add("touchpad:left-handed", touchpad_left_handed_delayed, LITEST_TOUCHPAD|LITEST_BUTTON, LITEST_CLICKPAD);
	litest_add("touchpad:left-handed", touchpad_left_handed_clickpad_delayed, LITEST_CLICKPAD, LITEST_APPLE_CLICKPAD);

	/* Semi-MT hover tests aren't generic, they only work on this device and
	 * ignore the semi-mt capability (it doesn't matter for the tests) */
	litest_add_for_device("touchpad:semi-mt-hover", touchpad_semi_mt_hover_noevent, LITEST_SYNAPTICS_HOVER_SEMI_MT);
	litest_add_for_device("touchpad:semi-mt-hover", touchpad_semi_mt_hover_down, LITEST_SYNAPTICS_HOVER_SEMI_MT);
	litest_add_for_device("touchpad:semi-mt-hover", touchpad_semi_mt_hover_down_up, LITEST_SYNAPTICS_HOVER_SEMI_MT);
	litest_add_for_device("touchpad:semi-mt-hover", touchpad_semi_mt_hover_down_hover_down, LITEST_SYNAPTICS_HOVER_SEMI_MT);
	litest_add_for_device("touchpad:semi-mt-hover", touchpad_semi_mt_hover_2fg_noevent, LITEST_SYNAPTICS_HOVER_SEMI_MT);
	litest_add_for_device("touchpad:semi-mt-hover", touchpad_semi_mt_hover_2fg_1fg_down, LITEST_SYNAPTICS_HOVER_SEMI_MT);
	litest_add_for_device("touchpad:semi-mt-hover", touchpad_semi_mt_hover_2fg_up, LITEST_SYNAPTICS_HOVER_SEMI_MT);

	litest_add("touchpad:hover", touchpad_hover_noevent, LITEST_TOUCHPAD|LITEST_HOVER, LITEST_ANY);
	litest_add("touchpad:hover", touchpad_hover_down, LITEST_TOUCHPAD|LITEST_HOVER, LITEST_ANY);
	litest_add("touchpad:hover", touchpad_hover_down_up, LITEST_TOUCHPAD|LITEST_HOVER, LITEST_ANY);
	litest_add("touchpad:hover", touchpad_hover_down_hover_down, LITEST_TOUCHPAD|LITEST_HOVER, LITEST_ANY);
	litest_add("touchpad:hover", touchpad_hover_2fg_noevent, LITEST_TOUCHPAD|LITEST_HOVER, LITEST_ANY);
	litest_add("touchpad:hover", touchpad_hover_2fg_1fg_down, LITEST_TOUCHPAD|LITEST_HOVER, LITEST_ANY);

	litest_add_for_device("touchpad:trackpoint", touchpad_trackpoint_buttons, LITEST_SYNAPTICS_TRACKPOINT_BUTTONS);
	litest_add_for_device("touchpad:trackpoint", touchpad_trackpoint_mb_scroll, LITEST_SYNAPTICS_TRACKPOINT_BUTTONS);
	litest_add_for_device("touchpad:trackpoint", touchpad_trackpoint_mb_click, LITEST_SYNAPTICS_TRACKPOINT_BUTTONS);
	litest_add_for_device("touchpad:trackpoint", touchpad_trackpoint_buttons_softbuttons, LITEST_SYNAPTICS_TRACKPOINT_BUTTONS);
	litest_add_for_device("touchpad:trackpoint", touchpad_trackpoint_buttons_2fg_scroll, LITEST_SYNAPTICS_TRACKPOINT_BUTTONS);
	litest_add_for_device("touchpad:trackpoint", touchpad_trackpoint_no_trackpoint, LITEST_SYNAPTICS_TRACKPOINT_BUTTONS);

	litest_add_ranged("touchpad:state", touchpad_initial_state, LITEST_TOUCHPAD, LITEST_ANY, &axis_range);

	litest_add("touchpad:thumb", touchpad_thumb_begin_no_motion, LITEST_CLICKPAD, LITEST_ANY);
	litest_add("touchpad:thumb", touchpad_thumb_update_no_motion, LITEST_CLICKPAD, LITEST_ANY);
	litest_add("touchpad:thumb", touchpad_thumb_moving, LITEST_CLICKPAD, LITEST_ANY);
	litest_add("touchpad:thumb", touchpad_thumb_clickfinger, LITEST_CLICKPAD, LITEST_ANY);
	litest_add("touchpad:thumb", touchpad_thumb_btnarea, LITEST_CLICKPAD, LITEST_ANY);
	litest_add("touchpad:thumb", touchpad_thumb_edgescroll, LITEST_CLICKPAD, LITEST_ANY);
	litest_add("touchpad:thumb", touchpad_thumb_tap_begin, LITEST_CLICKPAD, LITEST_ANY);
	litest_add("touchpad:thumb", touchpad_thumb_tap_touch, LITEST_CLICKPAD, LITEST_ANY);
	litest_add("touchpad:thumb", touchpad_thumb_tap_hold, LITEST_CLICKPAD, LITEST_ANY);
	litest_add("touchpad:thumb", touchpad_thumb_tap_hold_2ndfg, LITEST_CLICKPAD, LITEST_SINGLE_TOUCH);
	litest_add("touchpad:thumb", touchpad_thumb_tap_hold_2ndfg_tap, LITEST_CLICKPAD, LITEST_SINGLE_TOUCH);

	litest_add_for_device("touchpad:bugs", touchpad_tool_tripletap_touch_count, LITEST_SYNAPTICS_TOPBUTTONPAD);

	litest_add("touchpad:time", touchpad_time_usec, LITEST_TOUCHPAD, LITEST_ANY);

	litest_add_for_device("touchpad:jumps", touchpad_jump_finger_motion, LITEST_SYNAPTICS_CLICKPAD);
}
