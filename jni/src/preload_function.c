/*
 * Copyright (C) 2025-2026 VelocityFox22
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <nusantara.h>
#include <dirent.h>

/***********************************************************************************
 * Function Name      : GamePreload
 * Inputs             : const char* package - target application package name
 * Returns            : void
 * Description        : Optimized preload dengan cache RAM
 ***********************************************************************************/
void GamePreload(const char* package) {
    if (!package || !*package) {
        log_nusantara(LOG_WARN, "Package is null or empty");
        return;
    }

    /* ================= DYNAMIC RAM BUDGET ================= */
    long mem_total_mb = 0, mem_avail_mb = 0;
    FILE* meminfo = fopen("/proc/meminfo", "r");
    if (meminfo) {
        char line[256];
        while (fgets(line, sizeof(line), meminfo)) {
            long v;
            if (sscanf(line, "MemTotal: %ld kB", &v) == 1)
                mem_total_mb = v / 1024;
            else if (sscanf(line, "MemAvailable: %ld kB", &v) == 1)
                mem_avail_mb = v / 1024;
        }
        fclose(meminfo);
    }

    if (mem_total_mb < 4000) {
        log_nusantara(LOG_INFO, "RAM %ldMB < 4GB, skipping preload", mem_total_mb);
        return;
    }

    const char* budget = "350M";

    if (mem_total_mb >= 12000)      budget = "900M";
    else if (mem_total_mb >= 8000)  budget = "700M";
    else if (mem_total_mb >= 6000)  budget = "500M";
    else                            budget = "350M"; 

    /* Soft clamp based on available RAM */
    if (mem_avail_mb < 800)
        budget = "300M";
    else if (mem_avail_mb < 1200)
        budget = "350M";

    log_nusantara(LOG_INFO,
        "GamePreload | RAM %ldMB | Avail %ldMB | Budget %s",
        mem_total_mb, mem_avail_mb, budget);

    log_preload(LOG_INFO,
        "Game preload %s | Budget %s",
        package, budget);

    /* ================= GET BASE APK PATH ================= */
    char apk_path[512] = {0};
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
        "cmd package path %s | grep base.apk | head -n1 | cut -d: -f2",
        package);

    FILE* apk = popen(cmd, "r");
    if (!apk || !fgets(apk_path, sizeof(apk_path), apk)) {
        log_nusantara(LOG_WARN, "Failed to get APK path");
        if (apk) pclose(apk);
        return;
    }
    pclose(apk);
    apk_path[strcspn(apk_path, "\n")] = 0;

    char* slash = strrchr(apk_path, '/');
    if (!slash) return;
    *slash = '\0';

    /* ================= ABI DETECTION ================= */
    char target_path[512] = {0};
    const char* abi_list[] = { "lib/arm64-v8a", "lib/arm64" };
    bool found = false;

    for (int i = 0; i < 2; i++) {
        char test[512];
        snprintf(test, sizeof(test), "%s/%s", apk_path, abi_list[i]);
        DIR* d = opendir(test);
        if (d) {
            struct dirent* e;
            while ((e = readdir(d))) {
                size_t l = strlen(e->d_name);
                if (l > 3 && !strcmp(e->d_name + l - 3, ".so")) {
                    strncpy(target_path, test, sizeof(target_path) - 1);
                    found = true;
                    break;
                }
            }
            closedir(d);
        }
        if (found) break;
    }

    if (!found)
        strncpy(target_path, apk_path, sizeof(target_path) - 1);

    /* ================= RUN PRELOADER ================= */
    char preload_cmd[600];
    snprintf(preload_cmd, sizeof(preload_cmd),
        "sys.npreloader -v -t -m %s \"%s\"", budget, target_path);

    FILE* fp = popen(preload_cmd, "r");
    if (!fp) {
        log_nusantara(LOG_WARN, "Failed to execute preloader");
        return;
    }

    /* ================= PARSE OUTPUT ================= */
    char line[1024];
    int total_pages = 0;

    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = 0;

        int pages = 0;
        if (sscanf(line, "Touched Pages: %d", &pages) == 1) {
            total_pages += pages;
        }

        if (strstr(line, ".so") || strstr(line, ".apk") || strstr(line, ".dm") ||
            strstr(line, ".odex") || strstr(line, ".vdex") || strstr(line, ".art")) {
            log_preload(LOG_DEBUG, "Touched: %s", line);
        }
    }
    pclose(fp);

    long total_mb = (total_pages * 4) / 1024;
    log_nusantara(LOG_INFO,
        "Game %s preloaded: %d pages (~%ldMB)",
        package, total_pages, total_mb);

    log_preload(LOG_INFO,
        "Preload success: %s | ~%ldMB",
        package, total_mb);
}
