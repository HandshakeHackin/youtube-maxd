/* Stand-in for the one EVO function its link tail refers to weakly
 * (tools/native-app/stubs/evo_dns.c logs through it). Only the stand-alone
 * probe links this; in the app the real one is there.
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later */
void evo_boot_log(const char *format, ...) { (void)format; }
