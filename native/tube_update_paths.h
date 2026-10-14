/* Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later */
const char *tube_update_helper_path(void);
const char *tube_update_param_path(void);
#define SELF_UPDATE_HELPER_PATH tube_update_helper_path()
#define SELF_UPDATE_PARAM_PATH tube_update_param_path()
#define SELF_UPDATE_SEQUENCE_PATH "/data/prosperotube/update-sequence"
