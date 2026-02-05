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
#include <libgen.h>

char* gamestart = NULL;
pid_t game_pid = 0;

int is_file_empty(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("fopen failed");
        return -1;
    }

    int ch = fgetc(file);
    if (ch == EOF) {
        if (feof(file)) {
            fclose(file);
            return 1;
        } else {
            perror("fgetc failed");
            fclose(file);
            return -1;
        }
    }

    fclose(file);
    return 0;
}

int main(int argc, char* argv[]) {
    if (getuid() != 0) {
        fprintf(stderr, "\033[31mERROR:\033[0m Please run this program as root\n");
        exit(EXIT_FAILURE);
    }

    char* base_name = basename(argv[0]);
    if (strcmp(base_name, "nusantara_log") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: nusantara_log <TAG> <LEVEL> <MESSAGE>\n");
            fprintf(stderr, "Levels: 0=DEBUG, 1=INFO, 2=WARN, 3=ERROR, 4=FATAL\n");
            return EXIT_FAILURE;
        }

        int level = atoi(argv[2]);
        if (level < LOG_DEBUG || level > LOG_FATAL) {
            fprintf(stderr, "Invalid log level. Use 0-4.\n");
            return EXIT_FAILURE;
        }

        size_t message_len = 0;
        for (int i = 3; i < argc; i++)
            message_len += strlen(argv[i]) + 1;

        char message[message_len];
        message[0] = '\0';

        for (int i = 3; i < argc; i++) {
            strcat(message, argv[i]);
            if (i < argc - 1)
                strcat(message, " ");
        }

        external_log(level, argv[1], message);
        return EXIT_SUCCESS;
    }

    if (access("/system/bin/dumpsys", F_OK) != 0) {
        fprintf(stderr, "\033[31mFATAL ERROR:\033[0m /system/bin/dumpsys: inaccessible or not found\n");
        log_nusantara(LOG_FATAL, "/system/bin/dumpsys: inaccessible or not found");
        notify("Something wrong happening in the daemon, please check module log.");
        exit(EXIT_FAILURE);
    }

    if (is_file_empty("/system/bin/dumpsys") == 1) {
        fprintf(stderr, "\033[31mFATAL ERROR:\033[0m /system/bin/dumpsys was tampered by kill logger module.\n");
        log_nusantara(LOG_FATAL, "/system/bin/dumpsys was tampered by kill logger module");
        notify("Please remove your stupid kill logger module.");
        exit(EXIT_FAILURE);
    }

    if (create_lock_file() != 0) {
        fprintf(stderr, "\033[31mERROR:\033[0m Another instance of Nusantara Daemon is already running!\n");
        exit(EXIT_FAILURE);
    }

    is_kanged();

    if (access(GAMELIST, F_OK) != 0) {
        fprintf(stderr, "\033[31mFATAL ERROR:\033[0m Unable to access Gamelist, either has been removed or moved.\n");
        log_nusantara(LOG_FATAL, "Critical file not found (%s)", GAMELIST);
        exit(EXIT_FAILURE);
    }

    if (daemon(0, 0)) {
        log_nusantara(LOG_FATAL, "Unable to daemonize service");
        exit(EXIT_FAILURE);
    }

    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);
    bool need_profile_checkup = false;
    MLBBState mlbb_is_running = MLBB_NOT_RUNNING;
    ProfileMode cur_mode = PERFCOMMON;
    log_nusantara(LOG_INFO, "Daemon started as PID %d", getpid());
    run_profiler(PERFCOMMON);

    int gamestart_poll_tick = 0;

    while (1) {
        sleep(LOOP_INTERVAL);
        if (access(MODULE_UPDATE, F_OK) == 0) [[clang::unlikely]] {
            log_nusantara(LOG_INFO, "Module update detected, exiting.");
            notify("Please reboot your device to complete module update.");
            break;
        }

        /* GAME DETECTION LOGIC (DIPERBAIKI) */
        gamestart_poll_tick++;
        if (!gamestart || gamestart_poll_tick >= 2) {
            char* new_game = get_gamestart();
            gamestart_poll_tick = 0;
            if (new_game && (!gamestart || strcmp(new_game, gamestart) != 0)) {
                if (gamestart)
                    free(gamestart);
                gamestart = new_game;
                need_profile_checkup = true;
            } else if (new_game) {
                free(new_game);
            }
        }
        if (game_pid != 0 && kill(game_pid, 0) == -1) [[clang::unlikely]] {
            log_nusantara(LOG_INFO, "Game %s exited, resetting profile...", gamestart);
            game_pid = 0;
            free(gamestart);
            gamestart = NULL;
            need_profile_checkup = true;
        }
        if (gamestart)
            mlbb_is_running = handle_mlbb(gamestart);

        /* PROFILE HANDLING */
        if (gamestart && get_screenstate() && mlbb_is_running != MLBB_RUN_BG) {
            if (!need_profile_checkup && cur_mode == PERFORMANCE_PROFILE)
                continue;

            game_pid = (mlbb_is_running == MLBB_RUNNING) ? mlbb_pid : pidof(gamestart);
            if (game_pid == 0) [[clang::unlikely]] {
                log_nusantara(LOG_ERROR, "Unable to fetch PID of %s", gamestart);
                free(gamestart);
                gamestart = NULL;
                continue;
            }

            cur_mode = PERFORMANCE_PROFILE;
            need_profile_checkup = false;
            toast("Applying performance profile");
            run_profiler(PERFORMANCE_PROFILE);
            set_priority(game_pid);
            NusantaraPreload(gamestart);
            log_nusantara(LOG_INFO, "Applying performance profile for %s", gamestart);
        } else if (get_low_power_state()) {
            if (cur_mode == POWERSAVE_PROFILE)
                continue;

            cur_mode = POWERSAVE_PROFILE;
            need_profile_checkup = false;
            toast("Applying powersave profile");
            run_profiler(POWERSAVE_PROFILE);
            log_nusantara(LOG_INFO, "Applying powersave profile");
        } else {
            if (cur_mode == NORMAL_PROFILE)
                continue;

            cur_mode = NORMAL_PROFILE;
            need_profile_checkup = false;
            toast("Applying normal profile");
            run_profiler(NORMAL_PROFILE);
            log_nusantara(LOG_INFO, "Applying normal profile");
        }
    }

    return 0;
}
