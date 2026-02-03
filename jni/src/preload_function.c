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

/***********************************************************************************
 * Function Name : GamePreload
 * Inputs        : const char* package - target application package name
 * Returns       : void
 * Description   : Dynamically preloads native libraries or split APK contents
 ***********************************************************************************/
void NusantaraPreload(const char* package) {
    if (!package || package[0] == '\0') {
        log_nusantara(LOG_WARN, "Package is null or empty");
        return;
    }

    /* ================= DYNAMIC RAM BUDGET ================= */
    long mem_total_mb = 0;
    long mem_avail_mb = 0;
    FILE* meminfo = fopen("/proc/meminfo", "r");
    if (meminfo) {
        char line[256];
        long v;
        while (fgets(line, sizeof(line), meminfo)) {
            if (sscanf(line, "MemTotal: %ld kB", &v) == 1)
                mem_total_mb = v / 1024;
            else if (sscanf(line, "MemAvailable: %ld kB", &v) == 1)
                mem_avail_mb = v / 1024;
        }
        fclose(meminfo);
    }
    if (mem_total_mb < 4000) {
        log_nusantara(LOG_INFO,
            "GamePreload | RAM %ldMB < 4GB, skipping preload",
            mem_total_mb);
        return;
    }
    const char* budget = "350M";
    if (mem_total_mb >= 12000)      budget = "900M";
    else if (mem_total_mb >= 8000)  budget = "700M";
    else if (mem_total_mb >= 6000)  budget = "500M";
    if (mem_avail_mb > 0) {
        if (mem_avail_mb < 800)
            budget = "300M";
        else if (mem_avail_mb < 1200)
            budget = "350M";
    }
    log_nusantara(LOG_INFO,
        "Nusantara Preload | Budget %s",
        mem_total_mb, mem_avail_mb, budget);

    /* ================= GET APK PATH ================= */
    char apk_path[256] = {0};
    char cmd_apk[512];
    snprintf(cmd_apk, sizeof(cmd_apk),
             "cmd package path %s | head -n1 | cut -d: -f2",
             package);

    FILE* apk = popen(cmd_apk, "r");
    if (!apk || !fgets(apk_path, sizeof(apk_path), apk)) {
        log_nusantara(LOG_WARN,
            "Failed to get APK path for %s", package);
        if (apk)
            pclose(apk);
        return;
    }
    pclose(apk);
    apk_path[strcspn(apk_path, "\n")] = 0;

    /* ================= EXTRACT APK DIR ================= */
    char* last_slash = strrchr(apk_path, '/');
    if (!last_slash) {
        log_nusantara(LOG_WARN,
            "Invalid APK path: %s", apk_path);
        return;
    }
    *last_slash = '\0';

    /* ================= ABI LIB DETECTION ================= */
    const char* abi_list[] = {
        "lib/arm64-v8a",
        "lib/arm64"
    };
    char lib_path[300] = {0};
    bool lib_exists = false;
    for (int i = 0; i < 2 && !lib_exists; i++) {
        snprintf(lib_path, sizeof(lib_path),
                 "%s/%s", apk_path, abi_list[i]);
        DIR* d = opendir(lib_path);
        if (d) {
            struct dirent* e;
            while ((e = readdir(d))) {
                if (strstr(e->d_name, ".so")) {
                    lib_exists = true;
                    break;
                }
            }
            closedir(d);
        }
    }

    /* ================= EXECUTE PRELOAD ================= */
    FILE* fp = NULL;
    char line[1024];
    int total_pages = 0;
    char last_size[32] = {0};
    char preload_cmd[512];
    if (lib_exists) {
        snprintf(preload_cmd, sizeof(preload_cmd),
                 "sys.npreloader -v -t -m %s \"%s\"",
                 budget, lib_path);

        log_nusantara(LOG_INFO,
            "Preloading native libs: %s", lib_path);
    } else {
        snprintf(preload_cmd, sizeof(preload_cmd),
                 "sys.npreloader -v -t -m %s \"%s\"",
                 budget, apk_path);

        log_nusantara(LOG_INFO,
            "Preloading split APKs: %s", apk_path);
    }
    fp = popen(preload_cmd, "r");
    if (!fp) {
        log_nusantara(LOG_WARN,
            "Failed to execute preloader for %s", package);
        return;
    }

    /* ================= PARSE OUTPUT ================= */
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = 0;
        char* p = strstr(line, "Touched Pages:");
        if (p) {
            int pages = 0;
            char size[32] = {0};
            if (sscanf(p,
                "Touched Pages: %d (%31[^)])",
                &pages, size) == 2) {
                total_pages += pages;
                strncpy(last_size, size,
                        sizeof(last_size) - 1);

                log_nusantara(LOG_DEBUG,
                    "Preloaded: %d pages (%s)",
                    pages, size);
            }
        }
        if (strstr(line, ".so") || strstr(line, ".apk") ||
            strstr(line, ".odex") || strstr(line, ".vdex") ||
            strstr(line, ".art") || strstr(line, ".dm")) {
            log_nusantara(LOG_DEBUG,
                "Touched: %s", line);
        }
    }

    pclose(fp);

    /* ================= FINAL LOG ================= */
    log_nusantara(LOG_INFO,
        "Game %s preloaded: %d pages (~%s)",
        package, total_pages, last_size);
}
