#include <stdio.h>

#include "wlr-output-management-unstable-v1.h"

#include "listeners.h"
#include "types.h"

static void size(void *data,
		struct zwlr_output_mode_v1 *zwlr_output_mode_v1,
		int32_t width,
		int32_t height) {
	struct Mode *mode = data;

	mode->width = width;
	mode->height = height;
}

static void refresh(void *data,
		struct zwlr_output_mode_v1 *zwlr_output_mode_v1,
		int32_t refresh) {
	struct Mode *mode = data;

	mode->refresh = refresh;
}

static void preferred(void *data,
		struct zwlr_output_mode_v1 *zwlr_output_mode_v1) {
	struct Mode *mode = data;

	mode->preferred = 1;
}

static void finished(void *data,
		struct zwlr_output_mode_v1 *zwlr_output_mode_v1) {
	// todo: release
}

static const struct zwlr_output_mode_v1_listener listener = {
	.size = size,
	.refresh = refresh,
	.preferred = preferred,
	.finished = finished,
};

const struct zwlr_output_mode_v1_listener *mode_listener() {
	return &listener;
}

