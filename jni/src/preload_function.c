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
 * Description        : Preloads all native libraries (.so) inside lib/arm64
 ***********************************************************************************/
void GamePreload(const char* package) {
    sleep(5);
    if (!package || strlen(package) == 0) {
        log_nusantara(LOG_WARN, "Package is null or empty");
        return;
    }

    //  Detect RAM Size for Dynamic Budget  
    long ram_mb = 0;
    FILE* meminfo = fopen("/proc/meminfo", "r");
    if (meminfo) {
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), meminfo)) {
            if (strstr(buffer, "MemTotal:")) {
                long mem_kb;
                if (sscanf(buffer, "MemTotal: %ld kB", &mem_kb) == 1) {
                    ram_mb = mem_kb / 1024;  // Convert KB to MB
                }
                break;
            }
        }
        fclose(meminfo);
    }

    //  DETERMINING A BUDGET BASED ON RAM 
    const char* budget;
    if (ram_mb >= 12000) {      // 12GB+ RAM (Flagship 2024+)
        budget = "800M";
    } else if (ram_mb >= 8000) { // 8GB RAM (Mid-high)
        budget = "600M";
    } else if (ram_mb >= 6000) { // 6GB RAM (Mid-range)
        budget = "450M";
    } else if (ram_mb >= 4000) { // 4GB RAM (Entry-level)
        budget = "300M";
    } else {                     // <4GB RAM (Low-end)
        budget = "200M";
    }
    
    log_nusantara(LOG_INFO, "Detected RAM: %ldMB, using budget: %s", ram_mb, budget);
    log_preload(LOG_INFO, "Preload for %s with RAM: %ldMB, budget: %s", package, ram_mb, budget);

    //  GET APK PATH 
    char apk_path[256] = {0};
    char cmd_apk[512];
    snprintf(cmd_apk, sizeof(cmd_apk), "cmd package path %s | head -n1 | cut -d: -f2", package);

    FILE* apk = popen(cmd_apk, "r");
    if (!apk || !fgets(apk_path, sizeof(apk_path), apk)) {
        log_nusantara(LOG_WARN, "Failed to get APK path for %s", package);
        log_preload(LOG_WARN, "Failed to get APK path for %s", package);
        if (apk)
            pclose(apk);
        return;
    }
    pclose(apk);
    apk_path[strcspn(apk_path, "\n")]  0;

    //  EXTRACT APK FOLDER PATH 
    char* last_slash = strrchr(apk_path, '/');
    if (!last_slash) {
        log_nusantara(LOG_WARN, "Failed to determine APK folder from path: %s", apk_path);
        log_preload(LOG_WARN, "Failed to determine APK folder from path: %s", apk_path);
        return;
    }
    *last_slash = '\0';

    //  CHECK LIB/ARM64 DIRECTORY 
    char lib_path[300];
    snprintf(lib_path, sizeof(lib_path), "%s/lib/arm64", apk_path);

    bool lib_exists = false;
    DIR* dir = opendir(lib_path);
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strstr(entry->d_name, ".so")) {
                lib_exists = true;
                break;
            }
        }
        closedir(dir);
    }

    //  EXECUTE PRELOAD 
    FILE* fp = NULL;
    char line[1024];
    int total_pages = 0;
    char total_size[32] = {0};

    if (lib_exists) {
        char preload_cmd[512];
        snprintf(preload_cmd, sizeof(preload_cmd), "sys.npreloader -v -t -m %s \"%s\"", budget, lib_path);

        fp = popen(preload_cmd, "r");
        if (!fp) {
            log_nusantara(LOG_WARN, "Failed to run preloader for %s", package);
            log_preload(LOG_WARN, "Failed to run preloader for %s", package);
            return;
        }

        log_nusantara(LOG_INFO, "Preloading game libs: %s", package);
        log_preload(LOG_INFO, "Preloading libs: %s with budget %s", lib_path, budget);

    } else {
        // Fallback to split APKs
        char preload_cmd[512];
        snprintf(preload_cmd, sizeof(preload_cmd), "sys.npreloader -v -t -m %s \"%s\"", budget, apk_path);

        fp = popen(preload_cmd, "r");
        if (!fp) {
            log_nusantara(LOG_WARN, "Failed to run preloader for %s", package);
            log_preload(LOG_WARN, "Failed to run preloader for %s", package);
            return;
        }

        log_nusantara(LOG_INFO, "Preloading split APKs: %s", package);
        log_preload(LOG_INFO, "Preloading split APKs: %s with budget %s", apk_path, budget);
    }

    //  PARSE PRELOAD OUTPUT 
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = 0;

        char* p_pages = strstr(line, "Touched Pages:");
        if (p_pages) {
            int pages = 0;
            char size[32] = {0};

            if (sscanf(p_pages, "Touched Pages: %d (%31[^)])", &pages, size) == 2) {
                total_pages += pages;
                strncpy(total_size, size, sizeof(total_size) - 1);
                log_nusantara(LOG_DEBUG, "Preloaded: %d pages (%s)", pages, size);
                log_preload(LOG_DEBUG, "Preloaded: %d pages (%s)", pages, size);
            } else {
                log_nusantara(LOG_WARN, "Failed to parse Touched Pages");
                log_preload(LOG_WARN, "Failed to parse Touched Pages");
            }
        }

        // Log detail file yang di-touch
        if (strstr(line, ".so") || strstr(line, ".apk") || strstr(line, ".dm") || 
            strstr(line, ".odex") || strstr(line, ".vdex") || strstr(line, ".art")) {
            log_preload(LOG_DEBUG, "Touched: %s", line);
        }
    }

    //  FINAL LOG 
    log_nusantara(LOG_INFO, "Game %s preloaded: %d pages (~%s)", 
                  package, total_pages, total_size);
    log_preload(LOG_INFO, "Game %s preloaded success: total %d pages touched (~%s)", 
                package, total_pages, total_size);

    pclose(fp);
}