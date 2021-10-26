#include <libgen.h>
#include <linux/limits.h>
#include <string.h>
#include <unistd.h>
#include <yaml-cpp/yaml.h>

extern "C" {
#include "cfg.h"
#include "log.h"
}

namespace { // }
using std::exception;
using std::invalid_argument;
using std::string;
using std::stringstream;
} // namespace

#define CFG_FILE_NAME "cfg.yaml"
#define DEFAULT_LAPTOP_OUTPUT_PREFIX "eDP"

struct Cfg *default_cfg() {
	struct Cfg *cfg = (struct Cfg*)calloc(1, sizeof(struct Cfg));

	cfg->dirty = true;

	cfg->laptop_display_prefix = strdup(DEFAULT_LAPTOP_OUTPUT_PREFIX);
	cfg->auto_scale = true;

	return cfg;
}

bool resolve(struct Cfg *cfg, const char *prefix, const char *suffix) {
	if (!cfg)
		return false;

	char path[PATH_MAX];
	snprintf(path, PATH_MAX, "%s%s/way-displays/%s", prefix, suffix, CFG_FILE_NAME);
	if (access(path, R_OK) != 0) {
		return false;
	}

	char *file_path = realpath(path, NULL);
	if (!file_path) {
		return false;
	}
	if (access(file_path, R_OK) != 0) {
		free(file_path);
		return false;
	}

	cfg->file_path = file_path;

	strcpy(path, file_path);
	cfg->dir_path = strdup(dirname(path));

	strcpy(path, file_path);
	cfg->file_name = strdup(basename(path));

	return true;
}

bool parse(struct Cfg *cfg) {
	if (!cfg || !cfg->file_path)
		return false;

	try {
		YAML::Node config = YAML::LoadFile(cfg->file_path);

		if (config["LOG_THRESHOLD"]) {
			const auto &level = config["LOG_THRESHOLD"].as<string>();
			if (level == "DEBUG") {
				log_threshold = LOG_LEVEL_DEBUG;
			} else if (level == "INFO") {
				log_threshold = LOG_LEVEL_INFO;
			} else if (level == "WARNING") {
				log_threshold = LOG_LEVEL_WARNING;
			} else if (level == "ERROR") {
				log_threshold = LOG_LEVEL_ERROR;
			} else {
				log_threshold = LOG_LEVEL_INFO;
				log_warn("\nIgnoring invalid LOG_THRESHOLD: '%s', using default 'INFO'", level.c_str());
			}
		}

		if (config["LAPTOP_DISPLAY_PREFIX"]) {
			free(cfg->laptop_display_prefix);
			cfg->laptop_display_prefix = strdup(config["LAPTOP_DISPLAY_PREFIX"].as<string>().c_str());
		}

		if (config["ORDER"]) {
			const auto &orders = config["ORDER"];
			for (const auto &order : orders) {
				slist_append(&cfg->order_name_desc, strdup(order.as<string>().c_str()));
			}
		}

		if (config["LEFT_TO_RIGHT_ALIGN"]) {
			const auto &ltr_align = config["LEFT_TO_RIGHT_ALIGN"].as<string>();
			if (ltr_align == "TOP") {
				cfg->ltr_align = TOP;
			} else if (ltr_align == "BOTTOM") {
				cfg->ltr_align = BOTTOM;
			} else {
				cfg->ltr_align = TOP;
				log_warn("\nIgnoring invalid LEFT_TO_RIGHT_ALIGN: '%s', using default 'TOP'", ltr_align.c_str());
			}
		}


		if (config["AUTO_SCALE"]) {
			const auto &orders = config["AUTO_SCALE"];
			cfg->auto_scale = orders.as<bool>();
		}

		if (config["SCALE"]) {
			const auto &display_scales = config["SCALE"];
			for (const auto &display_scale : display_scales) {
				if (display_scale["NAME_DESC"] && display_scale["SCALE"]) {
					struct UserScale *user_scale = NULL;
					try {
						user_scale = (struct UserScale*)calloc(1, sizeof(struct UserScale));
						user_scale->name_desc = strdup(display_scale["NAME_DESC"].as<string>().c_str());
						user_scale->scale = display_scale["SCALE"].as<float>();
						if (user_scale->scale <= 0) {
							log_warn("\nIgnoring invalid scale for %s: %.3f", user_scale->name_desc, user_scale->scale);
							free(user_scale);
						} else {
							slist_append(&cfg->user_scales, user_scale);
						}
					} catch (...) {
						if (user_scale) {
							free_user_scale(user_scale);
						}
						throw;
					}
				}
			}
		}

	} catch (const exception &e) {
		log_error("\ncannot parse '%s': %s", cfg->file_path, e.what());
		return false;
	}
	return true;
}

void print_cfg(struct Cfg *cfg) {
	if (!cfg)
		return;

	struct UserScale *user_scale;
	struct SList *i;

	if (cfg->order_name_desc) {
		log_info("  Order:");
		for (i = cfg->order_name_desc; i; i = i->nex) {
			log_info("    %s", (char*)i->val);
		}
	}

	switch (cfg->ltr_align) {
		case TOP:
			log_info("  LTR alignment: TOP");
			break;
		case BOTTOM:
			log_info("  LTR alignment: BOTTOM");
			break;
		default:
			break;
	}

	log_info("  Auto scale: %s", cfg->auto_scale ? "ON" : "OFF");

	if (cfg->user_scales) {
		log_info("  Scale:");
		for (i = cfg->user_scales; i; i = i->nex) {
			user_scale = (struct UserScale*)i->val;
			log_info("    %s: %.3f", user_scale->name_desc, user_scale->scale);
		}
	}

	log_info("  Laptop display prefix: '%s'", cfg->laptop_display_prefix);
}

struct Cfg *load_cfg() {
	bool found = false;

	struct Cfg *cfg = default_cfg();

	if (getenv("XDG_CONFIG_HOME"))
		found = resolve(cfg, getenv("XDG_CONFIG_HOME"), "");
	if (!found && getenv("HOME"))
		found = resolve(cfg, getenv("HOME"), "/.config");
	if (!found)
		found = resolve(cfg, "/usr/local/etc", "");
	if (!found)
		found = resolve(cfg, "/etc", "");

	if (found) {
		log_info("\nFound configuration file: %s", cfg->file_path);
		if (!parse(cfg)) {
			log_info("\nUsing default configuration:");
			struct Cfg *cfg_def = default_cfg();
			cfg_def->dir_path = strdup(cfg->dir_path);
			cfg_def->file_path = strdup(cfg->file_path);
			cfg_def->file_name = strdup(cfg->file_name);
			free_cfg(cfg);
			cfg = cfg_def;
		}
	} else {
		log_info("\nNo configuration file found, using defaults:");
	}
	print_cfg(cfg);

	return cfg;
}

struct Cfg *reload_cfg(struct Cfg *cfg) {
	if (!cfg || !cfg->file_path)
		return cfg;

	struct Cfg *cfg_new = default_cfg();
	cfg_new->dir_path = strdup(cfg->dir_path);
	cfg_new->file_path = strdup(cfg->file_path);
	cfg_new->file_name = strdup(cfg->file_name);

	log_info("\nReloading configuration file: %s", cfg->file_path);
	if (parse(cfg_new)) {
		print_cfg(cfg_new);
		free_cfg(cfg);
		return cfg_new;
	} else {
		log_info("\nConfiguration unchanged:");
		print_cfg(cfg);
		free_cfg(cfg_new);
		return cfg;
	}
}

