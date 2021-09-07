#include <string.h>

#include "util.h"

struct StateOptimalMode {
	struct SList *modes;
	struct Mode *mode12at3;
	struct Mode *mode34at1;
	struct Mode *mode34at2;
};

static int optimal_mode_setup(void **state) {
	struct StateOptimalMode *s = calloc(1, sizeof(struct StateOptimalMode));

	struct Mode *mode;

	mode = calloc(1, sizeof(struct Mode));
	mode->width = 1;
	mode->height = 2;
	mode->refresh_mHz = 3;
	s->mode12at3 = mode;
	slist_append(&s->modes, mode);

	slist_append(&s->modes, NULL);

	mode = calloc(1, sizeof(struct Mode));
	s->mode34at1 = mode;
	mode->width = 3;
	mode->height = 4;
	mode->refresh_mHz = 1;
	slist_append(&s->modes, mode);

	mode = calloc(1, sizeof(struct Mode));
	mode->width = 3;
	mode->height = 4;
	mode->refresh_mHz = 2;
	s->mode34at2 = mode;
	slist_append(&s->modes, mode);

	*state = s;

	return 0;
}

static int optimal_mode_teardown(void **state) {
	struct StateOptimalMode *s = *state;

	for (struct SList *i = s->modes; i; i = i->nex) {
		free(i->val);
	}
	slist_free(&s->modes);
	free(s);

	return 0;
}

static void optimal_mode_preferred(void **state) {
	struct StateOptimalMode *s = *state;

	s->mode12at3->preferred = true;

	struct Mode *mode = optimal_mode(s->modes);
	assert_ptr_equal(mode, s->mode12at3);
}

static void optimal_mode_highest(void **state) {
	struct StateOptimalMode *s = *state;

	struct Mode *mode = optimal_mode(s->modes);
	assert_ptr_equal(mode, s->mode34at2);
}

struct StateAutoScale {
	struct Head *head;
};

static int auto_scale_setup(void **state) {
	struct StateAutoScale *s = calloc(1, sizeof(struct StateAutoScale));

	s->head = calloc(1, sizeof(struct Head));
	s->head->desired.mode = calloc(1, sizeof(struct Mode));

	*state = s;

	return 0;
}

static int auto_scale_teardown(void **state) {
	struct StateAutoScale *s = *state;

	free(s->head->desired.mode);
	free(s->head);

	free(s);

	return 0;
}

static void auto_scale_missing(void **state) {
	struct StateAutoScale *s = *state;

	// null head
	wl_fixed_t scale = auto_scale(NULL);
	assert_int_equal(scale, wl_fixed_from_int(0));

	// null desired.mode
	struct Mode *mode = s->head->desired.mode;
	s->head->desired.mode = NULL;
	scale = auto_scale(s->head);
	assert_int_equal(scale, wl_fixed_from_int(0));
	s->head->desired.mode = mode;

	// zero width_mm
	scale = auto_scale(s->head);
	assert_int_equal(scale, wl_fixed_from_int(0));

	// zero height_mm
	s->head->width_mm = 1;
	scale = auto_scale(s->head);
	assert_int_equal(scale, wl_fixed_from_int(0));

	// zero desired width
	s->head->height_mm = 1;
	scale = auto_scale(s->head);
	assert_int_equal(scale, wl_fixed_from_int(0));

	// zero desired height
	s->head->desired.mode->width = 1;
	scale = auto_scale(s->head);
	assert_int_equal(scale, wl_fixed_from_int(0));
}

static void auto_scale_valid(void **state) {
	struct StateAutoScale *s = *state;

	// dpi 93.338022 which rounds to 96, a scale of 1
	s->head->width_mm = 700;
	s->head->height_mm = 390;
	s->head->desired.mode->width = 2560;
	s->head->desired.mode->height = 1440;
	wl_fixed_t scale = auto_scale(s->head);
	assert_int_equal(scale, wl_fixed_from_int(1));

	// dpi 287.814241 which rounds to 288, a scale of 3
	s->head->width_mm = 340;
	s->head->height_mm = 190;
	s->head->desired.mode->width = 3840;
	s->head->desired.mode->height = 2160;
	scale = auto_scale(s->head);
	assert_int_equal(scale, wl_fixed_from_int(3));

	// dpi 212.453890 which rounds to 216, a scale of 2.25
	s->head->width_mm = 310;
	s->head->height_mm = 170;
	s->head->desired.mode->width = 2560;
	s->head->desired.mode->height = 1440;
	scale = auto_scale(s->head);
	assert_int_equal(scale, wl_fixed_from_double(2.25));
}

static void order_heads_valid(void **state) {
	struct Head *head;

	struct SList *order_name_desc = NULL;
	slist_append(&order_name_desc, strdup("e"));
	slist_append(&order_name_desc, strdup("d"));
	slist_append(&order_name_desc, NULL);
	slist_append(&order_name_desc, strdup("cdesc"));

	struct SList *heads = NULL;
	head = calloc(1, sizeof(struct Head));
	head->name = strdup("a");
	head->desired.enabled = false;
	slist_append(&heads, head);

	slist_append(&heads, NULL);

	head = calloc(1, sizeof(struct Head));
	head->name = strdup("b");
	head->desired.enabled = true;
	slist_append(&heads, head);

	head = calloc(1, sizeof(struct Head));
	head->name = strdup("c");
	head->description = strdup("cdesc");
	head->desired.enabled = true;
	slist_append(&heads, head);

	head = calloc(1, sizeof(struct Head));
	head->name = strdup("d");
	head->desired.enabled = false;
	slist_append(&heads, head);

	head = calloc(1, sizeof(struct Head));
	head->name = strdup("e");
	head->desired.enabled = true;
	slist_append(&heads, head);


	struct SList *heads_ordered = NULL;
	heads_ordered = order_heads(order_name_desc, heads);

	head = heads_ordered->val;
	assert_string_equal(head->name, "e");
	slist_remove(&heads_ordered, head);

	head = heads_ordered->val;
	assert_string_equal(head->name, "d");
	slist_remove(&heads_ordered, head);

	head = heads_ordered->val;
	assert_string_equal(head->name, "c");
	assert_string_equal(head->description, "cdesc");
	slist_remove(&heads_ordered, head);

	head = heads_ordered->val;
	assert_string_equal(head->name, "a");
	slist_remove(&heads_ordered, head);

	head = heads_ordered->val;
	assert_string_equal(head->name, "b");
	slist_remove(&heads_ordered, head);

	assert_null(heads_ordered);


	struct SList *i = order_name_desc;
	char *name_desc;
	while (i) {
		name_desc = i->val;
		i = i->nex;
		slist_remove(&order_name_desc, name_desc);
		free(name_desc);
	}
	slist_free(&order_name_desc);

	i = heads;
	while (i) {
		head = i->val;
		i = i->nex;
		slist_remove(&heads, head);
		if (head) {
			free(head->name);
			free(head);
		}
	}
	slist_free(&heads);
}

static int ltr_heads_setup(void **state) {
	struct SList *heads = NULL;
	struct Head *head = NULL;
	struct Mode *mode = NULL;

	mode = calloc(1, sizeof(struct Mode));
	mode->width = 10;
	head = calloc(1, sizeof(struct Head));
	head->desired.mode = mode;
	head->name = strdup("1");
	head->enabled = true;
	head->desired.scale = wl_fixed_from_int(2);
	slist_append(&heads, head);

	mode = calloc(1, sizeof(struct Mode));
	mode->width = 10;
	head = calloc(1, sizeof(struct Head));
	head->desired.mode = mode;
	head->name = strdup("2");
	head->enabled = false;
	head->desired.scale = wl_fixed_from_int(1);
	slist_append(&heads, head);

	head = calloc(1, sizeof(struct Head));
	head->name = strdup("3");
	head->enabled = false;
	slist_append(&heads, head);

	mode = calloc(1, sizeof(struct Mode));
	mode->width = 11;
	head = calloc(1, sizeof(struct Head));
	head->name = strdup("4");
	head->desired.mode = mode;
	head->enabled = true;
	head->desired.scale = wl_fixed_from_int(4);
	slist_append(&heads, head);

	mode = calloc(1, sizeof(struct Mode));
	mode->width = 1;
	head = calloc(1, sizeof(struct Head));
	head->name = strdup("5");
	head->desired.mode = mode;
	head->enabled = true;
	head->desired.scale = wl_fixed_from_int(1);
	slist_append(&heads, head);

	*state = heads;

	return 0;
}

static int ltr_heads_teardown(void **state) {
	struct SList *heads = *state;
	struct Head *head = NULL;

	for (struct SList *i = heads; i; i = i->nex) {
		head = i->val;
		free(head->name);
		free(head->desired.mode);
		free(head);
	}
	slist_free(&heads);

	return 0;
}

static void ltr_heads_valid(void **state) {
	struct SList *heads = slist_shallow_clone(*state);
	struct Head *head = NULL;

	ltr_heads(heads);

	head = heads->val;
	assert_string_equal(head->name, "1");
	assert_int_equal(head->desired.x, 0);
	assert_int_equal(head->desired.y, 0);
	slist_remove(&heads, head);

	head = heads->val;
	assert_string_equal(head->name, "2");
	assert_int_equal(head->desired.x, 0);
	assert_int_equal(head->desired.y, 0);
	slist_remove(&heads, head);

	head = heads->val;
	assert_string_equal(head->name, "3");
	assert_int_equal(head->desired.x, 0);
	assert_int_equal(head->desired.y, 0);
	slist_remove(&heads, head);

	head = heads->val;
	assert_string_equal(head->name, "4");
	assert_int_equal(head->desired.x, 5);
	assert_int_equal(head->desired.y, 0);
	slist_remove(&heads, head);

	head = heads->val;
	assert_string_equal(head->name, "5");
	// head 4 rounds it up to 3
	assert_int_equal(head->desired.x, 5 + 3);
	assert_int_equal(head->desired.y, 0);
	slist_remove(&heads, head);

	assert_null(heads);
}

#define util_tests \
	cmocka_unit_test_setup_teardown(optimal_mode_highest, optimal_mode_setup, optimal_mode_teardown), \
	cmocka_unit_test_setup_teardown(optimal_mode_preferred, optimal_mode_setup, optimal_mode_teardown), \
	cmocka_unit_test_setup_teardown(auto_scale_missing, auto_scale_setup, auto_scale_teardown), \
	cmocka_unit_test_setup_teardown(auto_scale_valid, auto_scale_setup, auto_scale_teardown), \
	cmocka_unit_test(order_heads_valid), \
	cmocka_unit_test_setup_teardown(ltr_heads_valid, ltr_heads_setup, ltr_heads_teardown)

