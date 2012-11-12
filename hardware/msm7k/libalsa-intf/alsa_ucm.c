/*
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of Code Aurora Forum, Inc. nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#define LOG_TAG "alsa_ucm"
#define LOG_NDEBUG 0
#include <utils/Log.h>
#include <cutils/properties.h>

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <ctype.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/poll.h>

#include <linux/ioctl.h>
#include "msm8960_use_cases.h"

/**
 * Create an identifier
 * fmt - sprintf like format,
 * ... - Optional arguments
 * returns - string allocated or NULL on error
 */
char *snd_use_case_identifier(const char *fmt, ...)
{
    LOGE("API not implemented for now, to be updated if required");
    return NULL;
}

/**
 * Free a list
 * list - list to free
 * items -  Count of strings
 * Return Zero on success, otherwise a negative error code
 */
int snd_use_case_free_list(const char *list[], int items)
{
    /* list points to UCM internal static tables,
     * hence there is no need to do a free call
     * just set the list to NULL and return */
    list = NULL;
    return 0;
}

/**
 * Obtain a list of entries
 * uc_mgr - UCM structure pointer or  NULL for card list
 * identifier - NULL for card list
 * list - Returns allocated list
 * returns Number of list entries on success, otherwise a negative error code
 */
int snd_use_case_get_list(snd_use_case_mgr_t *uc_mgr,
                          const char *identifier,
                          const char **list[])
{
    int verb_index, list_size, index = 0;

    if (identifier == NULL) {
        *list = card_list;
        return ((int)MAX_NUM_CARDS);
    }

    if ((uc_mgr->snd_card_index >= (int)MAX_NUM_CARDS) ||
        (uc_mgr->snd_card_index < 0) || (uc_mgr->card_ctxt_ptr == NULL)) {
        LOGE("snd_use_case_get_list(): failed, invalid arguments");
        return -EINVAL;
    }

    if (!strcmp(identifier, "_verbs")) {
        while(strcmp(verb_list[index], SND_UCM_END_OF_LIST)) {
            LOGV("Index:%d Verb:%s", index, verb_list[index]);
            index++;
        }
        *list = verb_list;
        return index;
    } else  if (!strcmp(identifier, "_devices")) {
        if (uc_mgr->card_ctxt_ptr->current_verb == NULL ||
            (!strcmp(uc_mgr->card_ctxt_ptr->current_verb,
                        SND_USE_CASE_VERB_INACTIVE))) {
            LOGE("Use case verb name not set, invalid current verb");
            return -EINVAL;
        }
        while(strcmp(uc_mgr->card_ctxt_ptr->current_verb,
                        use_case_verb_list[index].use_case_name)) {
            index++;
        }
        verb_index = index;
        index = 0;
        while(strcmp(use_case_verb_list[verb_index].device_list[index],
                        SND_UCM_END_OF_LIST)) {
            LOGV("Index:%d Device:%s", index,
                        use_case_verb_list[verb_index].device_list[index]);
            index++;
        }
        *list = use_case_verb_list[verb_index].device_list;
        return index;
    } else  if (!strcmp(identifier, "_modifiers")) {
        if (uc_mgr->card_ctxt_ptr->current_verb == NULL ||
            (!strcmp(uc_mgr->card_ctxt_ptr->current_verb,
                        SND_USE_CASE_VERB_INACTIVE))) {
            LOGE("Use case verb name not set, invalid current verb");
            return -EINVAL;
        }
        while(strcmp(uc_mgr->card_ctxt_ptr->current_verb,
                        use_case_verb_list[index].use_case_name)) {
            index++;
        }
        verb_index = index;
        index = 0;
        while(strcmp(use_case_verb_list[verb_index].modifier_list[index],
                        SND_UCM_END_OF_LIST)) {
            LOGV("Index:%d Modifier:%s", index,
                        use_case_verb_list[verb_index].modifier_list[index]);
            index++;
        }
        *list = use_case_verb_list[verb_index].modifier_list;
        return index;
    } else  if (!strcmp(identifier, "_enadevs")) {
        pthread_mutex_lock(&uc_mgr->card_ctxt_ptr->card_lock);
        if (uc_mgr->device_list_count) {
            for (index = 0; index < uc_mgr->device_list_count; index++) {
                free(uc_mgr->current_device_list[index]);
                uc_mgr->current_device_list[index] = NULL;
            }
            free(uc_mgr->current_device_list);
            uc_mgr->current_device_list = NULL;
            uc_mgr->device_list_count = 0;
        }
        list_size = snd_ucm_get_size_of_list(uc_mgr->card_ctxt_ptr->dev_list_head);
        uc_mgr->device_list_count = list_size;
	if (list_size > 0) {
            uc_mgr->current_device_list = (char **)malloc(sizeof(char *)*list_size);
            for (index = 0; index < list_size; index++) {
                uc_mgr->current_device_list[index] =
                            snd_ucm_get_value_at_index(uc_mgr->card_ctxt_ptr->dev_list_head, index);
            }
        }
        *list = (const char **)uc_mgr->current_device_list;
        pthread_mutex_unlock(&uc_mgr->card_ctxt_ptr->card_lock);
        return (list_size);
    } else  if (!strcmp(identifier, "_enamods")) {
        pthread_mutex_lock(&uc_mgr->card_ctxt_ptr->card_lock);
        if (uc_mgr->modifier_list_count) {
            for (index = 0; index < uc_mgr->modifier_list_count; index++) {
                free(uc_mgr->current_modifier_list[index]);
                uc_mgr->current_modifier_list[index] = NULL;
            }
            free(uc_mgr->current_modifier_list);
            uc_mgr->current_modifier_list = NULL;
            uc_mgr->modifier_list_count = 0;
        }
        list_size = snd_ucm_get_size_of_list(uc_mgr->card_ctxt_ptr->mod_list_head);
        uc_mgr->modifier_list_count = list_size;
	if (list_size > 0) {
        uc_mgr->current_modifier_list = (char **)malloc(sizeof(char *) * list_size);
            for (index = 0; index < list_size; index++) {
                uc_mgr->current_modifier_list[index] =
                            snd_ucm_get_value_at_index(uc_mgr->card_ctxt_ptr->mod_list_head, index);
            }
        }
        *list = (const char **)uc_mgr->current_modifier_list;
        pthread_mutex_unlock(&uc_mgr->card_ctxt_ptr->card_lock);
        return (list_size);
    } else {
        LOGE("Invalid identifier");
        return -EINVAL;
    }
}


/**
 * Get current value of the identifier
 * identifier - NULL for current card
 *        _verb
 *        <Name>/<_device/_modifier>
 *    Name -    PlaybackPCM
 *        CapturePCM
 *        PlaybackCTL
 *        CaptureCTL
 * value - Value pointer
 * returns Zero if success, otherwise a negative error code
 */
int snd_use_case_get(snd_use_case_mgr_t *uc_mgr,
                     const char *identifier,
                     const char **value)
{
    char *ident, *ident1, *ident2;
    int index, ret = 0;

    if ((uc_mgr->snd_card_index >= (int)MAX_NUM_CARDS) ||
        (uc_mgr->snd_card_index < 0) || (uc_mgr->card_ctxt_ptr == NULL)) {
        LOGE("snd_use_case_get(): failed, invalid arguments");
        return -EINVAL;
    }

    if (identifier == NULL) {
        pthread_mutex_lock(&uc_mgr->card_ctxt_ptr->card_lock);
        if (uc_mgr->card_ctxt_ptr->card_name != NULL) {
            *value = strdup(uc_mgr->card_ctxt_ptr->card_name);
        } else {
            *value = NULL;
        }
        return 0;
        pthread_mutex_unlock(&uc_mgr->card_ctxt_ptr->card_lock);
    }

    if (!strcmp(identifier, "_verb")) {
        pthread_mutex_lock(&uc_mgr->card_ctxt_ptr->card_lock);
        if (uc_mgr->card_ctxt_ptr->current_verb != NULL) {
            *value = strdup(uc_mgr->card_ctxt_ptr->current_verb);
        } else {
            *value = NULL;
        }
        pthread_mutex_unlock(&uc_mgr->card_ctxt_ptr->card_lock);
        return 0;
    }

    ident = (char *)malloc((strlen(identifier)+1)*sizeof(char));
    strcpy(ident, identifier);
    if(!(ident1 = strtok(ident, "/"))) {
        LOGV("No multiple identifiers found in identifier value: %s", ident);
        ret = -EINVAL;
    } else {
        if ((!strcmp(ident1, "PlaybackPCM")) || (!strcmp(ident1, "CapturePCM"))) {
            ident2 = strtok(NULL, "/");
            index = 0;
            while(strcmp(uc_mgr->card_ctxt_ptr->card_ctrl[index].case_name, ident2)) {
                if (!strcmp(uc_mgr->card_ctxt_ptr->card_ctrl[index].case_name, SND_UCM_END_OF_LIST)){
                    ret = -EINVAL;
                    break;
                } else {
                    index++;
                }
            }
            if (ret < 0) {
                LOGE("No valid device/modifier found with given identifier: %s", ident2);
            } else {
                if(uc_mgr->card_ctxt_ptr->card_ctrl[index].dev_name != NULL) {
                    *value = strdup(uc_mgr->card_ctxt_ptr->card_ctrl[index].dev_name);
                } else {
                    LOGE("Invalid device name value for given identifier: %s", ident2);
                    ret = -ENODEV;
                }
            }
        } else if ((!strcmp(ident1, "PlaybackCTL")) || (!strcmp(ident1, "CaptureCTL"))) {
            if(uc_mgr->card_ctxt_ptr->control_device != NULL) {
                *value = strdup(uc_mgr->card_ctxt_ptr->control_device);
            } else {
                LOGE("No valid control device found");
                ret = -ENODEV;
            }
        } else {
            LOGE("Unsupported identifier value");
            ret = -EINVAL;
        }
    }
    free(ident);
    return ret;
}

/**
 * Get current status
 * uc_mgr - UCM structure
 * identifier - _devstatus/<device>,
        _modstatus/<modifier>
 * value - result
 * returns 0 on success, otherwise a negative error code
 */
int snd_use_case_geti(snd_use_case_mgr_t *uc_mgr,
              const char *identifier,
              long *value)
{
    char *ident, *ident1, *ident2, *ident_value;
    int index, list_size, ret = -EINVAL;

    if ((uc_mgr->snd_card_index >= (int)MAX_NUM_CARDS) ||
        (uc_mgr->snd_card_index < 0) || (uc_mgr->card_ctxt_ptr == NULL)) {
        LOGE("snd_use_case_geti(): failed, invalid arguments");
        return -EINVAL;
    }

    *value = 0;
    ident = (char *)malloc((strlen(identifier)+1)*sizeof(char));
    strcpy(ident, identifier);
    if(!(ident1 = strtok(ident, "/"))) {
        LOGE("No multiple identifiers found in identifier value: %s", ident);
        ret = -EINVAL;
    } else {
        if (!strcmp(ident1, "_devstatus")) {
            ident2 = strtok(NULL, "/");
            pthread_mutex_lock(&uc_mgr->card_ctxt_ptr->card_lock);
            list_size = snd_ucm_get_size_of_list(uc_mgr->card_ctxt_ptr->dev_list_head);
            for (index = 0; index < list_size; index++) {
                ident_value = snd_ucm_get_value_at_index(uc_mgr->card_ctxt_ptr->dev_list_head, index);
                if (!strcmp(ident2, ident_value)) {
                    *value = 1;
                    free(ident_value);
                    ident_value = NULL;
                    break;
                } else {
                    free(ident_value);
                    ident_value = NULL;
                }
            }
            pthread_mutex_unlock(&uc_mgr->card_ctxt_ptr->card_lock);
            ret = 0;
        } else if (!strcmp(ident1, "_modstatus")) {
            ident2 = strtok(NULL, "/");
            pthread_mutex_lock(&uc_mgr->card_ctxt_ptr->card_lock);
            list_size = snd_ucm_get_size_of_list(uc_mgr->card_ctxt_ptr->mod_list_head);
            for (index = 0; index < list_size; index++) {
                ident_value = snd_ucm_get_value_at_index(uc_mgr->card_ctxt_ptr->mod_list_head, index);
                if (!strcmp(ident2, ident_value)) {
                    *value = 1;
                    free(ident_value);
                    ident_value = NULL;
                    break;
                } else {
                    free(ident_value);
                    ident_value = NULL;
                }
            }
            pthread_mutex_unlock(&uc_mgr->card_ctxt_ptr->card_lock);
            ret = 0;
        } else {
            LOGE("Unknown identifier name: %s", ident1);
        }
    }
    return ret;
}

/* Apply the required mixer controls for specific use case
 * uc_mgr - UCM structure pointer
 * use_case - use case name
 * return 0 on sucess, otherwise a negative error code
 */
int snd_use_case_apply_mixer_controls(snd_use_case_mgr_t *uc_mgr,
                const char *use_case, int enable)
{
    struct mixer_ctl *ctl;
    int i, ret = 0, index = 0, use_case_index;

    while(strcmp(uc_mgr->card_ctxt_ptr->card_ctrl[index].case_name,
                use_case)) {
        if (!strcmp(uc_mgr->card_ctxt_ptr->card_ctrl[index].case_name,
                SND_UCM_END_OF_LIST)) {
            ret = -EINVAL;
            break;
        }
        index++;
    }

    if (ret < 0) {
        LOGE("No valid use case found with the use case: %s", use_case);
        ret = -ENODEV;
    } else {
        if (!uc_mgr->card_ctxt_ptr->mixer_handle) {
            LOGE("Control device not initialized");
            ret = -ENODEV;
        } else {
            use_case_index = index; index = 0;
            for(index = 0; index < uc_mgr->card_ctxt_ptr->card_ctrl[use_case_index].mixer_control_count; index++) {
                if (uc_mgr->card_ctxt_ptr->card_ctrl[use_case_index].mixer_list == NULL) {
                    LOGE("No valid controls available for this case: %s", use_case);
                    break;
                }
                ctl = mixer_get_control(uc_mgr->card_ctxt_ptr->mixer_handle,
                        uc_mgr->card_ctxt_ptr->card_ctrl[use_case_index].mixer_list[index].control_name, 0);
                if (ctl) {
                if (enable) {
                    if ((uc_mgr->card_ctxt_ptr->card_ctrl[use_case_index].mixer_list[index].type == 0) ||
                        (uc_mgr->card_ctxt_ptr->card_ctrl[use_case_index].mixer_list[index].type == 2)) {
                        LOGV("Setting mixer control: %s, value: %d",
                                  uc_mgr->card_ctxt_ptr->card_ctrl[use_case_index].mixer_list[index].control_name,
                                  uc_mgr->card_ctxt_ptr->card_ctrl[use_case_index].mixer_list[index].ena_value);
                        ret = mixer_ctl_set(ctl, uc_mgr->card_ctxt_ptr->card_ctrl[use_case_index].mixer_list[index].ena_value);
                    } else {
                        LOGV("Setting mixer control: %s, value: %s",
                            uc_mgr->card_ctxt_ptr->card_ctrl[use_case_index].mixer_list[index].control_name,
                            uc_mgr->card_ctxt_ptr->card_ctrl[use_case_index].mixer_list[index].ena_string);
                        ret = mixer_ctl_select(ctl, uc_mgr->card_ctxt_ptr->card_ctrl[use_case_index].mixer_list[index].ena_string);
                    }
                    if (ret != 0) {
                        /* Disable all the mixer controls which are already enabled before failure */
                        for(i = index; i >= 0; i--) {
                                ctl = mixer_get_control(uc_mgr->card_ctxt_ptr->mixer_handle,
                                    uc_mgr->card_ctxt_ptr->card_ctrl[use_case_index].mixer_list[i].control_name, 0);
                                if (uc_mgr->card_ctxt_ptr->card_ctrl[use_case_index].mixer_list[index].type == 0) {
                                    ret = mixer_ctl_set(ctl, uc_mgr->card_ctxt_ptr->card_ctrl[use_case_index].mixer_list[index].dis_value);
                                } else {
                                    ret = mixer_ctl_select(ctl, uc_mgr->card_ctxt_ptr->card_ctrl[use_case_index].mixer_list[index].dis_string);
                                }
                        }
                        LOGE("Failed to enable the mixer controls for %s", use_case);
                        break;
                    }
                } else {
                    if (uc_mgr->card_ctxt_ptr->card_ctrl[use_case_index].mixer_list[index].type == 0) {
                        LOGV("Setting mixer control: %s, value: %d",
                              uc_mgr->card_ctxt_ptr->card_ctrl[use_case_index].mixer_list[index].control_name,
                              uc_mgr->card_ctxt_ptr->card_ctrl[use_case_index].mixer_list[index].dis_value);
                        ret = mixer_ctl_set(ctl, uc_mgr->card_ctxt_ptr->card_ctrl[use_case_index].mixer_list[index].dis_value);
                    } else if (uc_mgr->card_ctxt_ptr->card_ctrl[use_case_index].mixer_list[index].type == 1) {
                        LOGV("Setting mixer control: %s, value: %s",
                              uc_mgr->card_ctxt_ptr->card_ctrl[use_case_index].mixer_list[index].control_name,
                              uc_mgr->card_ctxt_ptr->card_ctrl[use_case_index].mixer_list[index].dis_string);
                        ret = mixer_ctl_select(ctl, uc_mgr->card_ctxt_ptr->card_ctrl[use_case_index].mixer_list[index].dis_string);
                    }
                    if (ret != 0) {
                        LOGE("Failed to disable the mixer controls for %s", use_case);
                        break;
                    }
                }
                }
            }
        }
    }
    return ret;
}

/* Set/Reset mixer controls of specific use case for all current devices
 * uc_mgr - UCM structure pointer
 * ident  - use case name (verb or modifier)
 * enable - 1 for enable and 0 for disable
 * return 0 on sucess, otherwise a negative error code
 */
int snd_use_case_ident_set_controls_for_all_devices(snd_use_case_mgr_t *uc_mgr,
    const char *ident, int enable)
{
    char *current_device, *use_case;
    int list_size, index, ret = 0;

    LOGV("set_use_case_ident_for_all_devices(): %s", ident);
    list_size = snd_ucm_get_size_of_list(uc_mgr->card_ctxt_ptr->dev_list_head);
    for (index = 0; index < list_size; index++) {
        current_device = snd_ucm_get_value_at_index(uc_mgr->card_ctxt_ptr->dev_list_head, index);
        if (current_device != NULL) {
            use_case = (char *)malloc((strlen(ident)+strlen(current_device)+2)*sizeof(char));
            if (use_case != NULL) {
                strcpy(use_case, ident);
                strcat(use_case, current_device);
                LOGV("Applying mixer controls for use case: %s", use_case);
                ret = snd_use_case_apply_mixer_controls(uc_mgr, use_case, enable);
                free(use_case);
            }
            if (enable && !ret) {
                ret = snd_use_case_apply_mixer_controls(uc_mgr, current_device, enable);
            }
            free(current_device);
        }
    }
    return ret;
}

/* Set/Reset mixer controls of specific device for all use cases
 * uc_mgr - UCM structure pointer
 * device - device name
 * enable - 1 for enable and 0 for disable
 * return 0 on sucess, otherwise a negative error code
 */
int snd_use_case_set_device_for_all_ident(snd_use_case_mgr_t *uc_mgr,
    const char *device, int enable)
{
    char *ident_value, *use_case;
    int list_size, index = 0, ret = -ENODEV, flag = 1;

    LOGV("set_device_for_all_ident(): %s", device);
    if ((uc_mgr->card_ctxt_ptr->current_verb != NULL) &&
        (strcmp(uc_mgr->card_ctxt_ptr->current_verb, SND_USE_CASE_VERB_INACTIVE))) {
        use_case = (char *)malloc((strlen(uc_mgr->card_ctxt_ptr->current_verb)+strlen(device)+2)*sizeof(char));
        strcpy(use_case, uc_mgr->card_ctxt_ptr->current_verb);
        strcat(use_case, device);
        LOGV("set %d for use case value: %s", enable, use_case);
        ret = snd_use_case_apply_mixer_controls(uc_mgr, use_case, enable);
        if (ret != 0) {
            LOGE("Setting mixer controls for usecase %s and device %s failed, enable: %d", use_case, device, enable);
        } else {
            flag = 0;
        }
        free(use_case);
    }
    snd_ucm_print_list(uc_mgr->card_ctxt_ptr->mod_list_head);
    list_size = snd_ucm_get_size_of_list(uc_mgr->card_ctxt_ptr->mod_list_head);
    for (index = 0; index < list_size; index++) {
        ident_value = snd_ucm_get_value_at_index(uc_mgr->card_ctxt_ptr->mod_list_head, index);
        use_case = (char *)malloc((strlen(ident_value)+strlen(device)+2)*sizeof(char));
        strcpy(use_case, ident_value);
        strcat(use_case, device);
        LOGV("set %d for use case value: %s", enable, use_case);
        ret = snd_use_case_apply_mixer_controls(uc_mgr, use_case, enable);
        if (ret != 0) {
            LOGE("Setting mixer controls for usecase %s and device %s failed, enable: %d", use_case, device, enable);
        } else {
            flag = 0;
        }
        free(use_case);
        free(ident_value);
    }
    if (!enable || (enable && !flag))
        ret = snd_use_case_apply_mixer_controls(uc_mgr, device, enable);
    return ret;
}

/* Check if a device is valid for existing use cases to avoid disabling
 * uc_mgr - UCM structure pointer
 * device - device name
 * return 1 if valid and 0 if device can be disabled
 */
int snd_use_case_check_device_for_disable(snd_use_case_mgr_t *uc_mgr, const char *device)
{
    char *ident_value, *use_case;
    int list_size, index = 0, case_index = 0, ret = 0;

    if ((uc_mgr->card_ctxt_ptr->current_verb != NULL) &&
        (strcmp(uc_mgr->card_ctxt_ptr->current_verb, SND_USE_CASE_VERB_INACTIVE))) {
        use_case = (char *)malloc((strlen(uc_mgr->card_ctxt_ptr->current_verb)+strlen(device)+2)*sizeof(char));
        strcpy(use_case, uc_mgr->card_ctxt_ptr->current_verb);
        strcat(use_case, device);
        while(strcmp(uc_mgr->card_ctxt_ptr->card_ctrl[index].case_name,
                     SND_UCM_END_OF_LIST)) {
            if (!strcmp(uc_mgr->card_ctxt_ptr->card_ctrl[index].case_name, use_case)) {
                ret = 1;
                break;
            }
            index++;
        }
        free(use_case);
    }
    if (ret == 0) {
        index = 0;
        list_size = snd_ucm_get_size_of_list(uc_mgr->card_ctxt_ptr->mod_list_head);
        for (index = 0; index < list_size; index++) {
            ident_value = snd_ucm_get_value_at_index(uc_mgr->card_ctxt_ptr->mod_list_head, index);
            use_case = (char *)malloc((strlen(ident_value)+strlen(device)+2)*sizeof(char));
            strcpy(use_case, ident_value);
            strcat(use_case, device);
            case_index = 0;
            while(strcmp(uc_mgr->card_ctxt_ptr->card_ctrl[case_index].case_name,
                         SND_UCM_END_OF_LIST)) {
                if (!strcmp(uc_mgr->card_ctxt_ptr->card_ctrl[case_index].case_name, use_case)) {
                    ret = 1;
                    break;
                }
                case_index++;
            }
            free(use_case);
            free(ident_value);
            if (ret == 1) {
                break;
            }
        }
    }
    return ret;
}

/**
 * Set new value for an identifier
 * uc_mgr - UCM structure
 * identifier - _verb, _enadev, _disdev, _enamod, _dismod
 *        _swdev, _swmod
 * value - Value to be set
 * returns 0 on success, otherwise a negative error code
 */
int snd_use_case_set(snd_use_case_mgr_t *uc_mgr,
                     const char *identifier,
                     const char *value)
{
    char *ident, *ident1, *ident2;
    int verb_index, list_size, index = 0, ret = -EINVAL;

    if ((uc_mgr->snd_card_index >= (int)MAX_NUM_CARDS) || (value == NULL) ||
        (uc_mgr->snd_card_index < 0) || (uc_mgr->card_ctxt_ptr == NULL) ||
        (identifier == NULL)) {
        LOGE("snd_use_case_set(): failed, invalid arguments");
        return -EINVAL;
    }

    LOGV("snd_use_case_set(): uc_mgr %p identifier %s value %s", uc_mgr, identifier, value);
    ident = (char *)malloc((strlen(identifier)+1)*sizeof(char));
    strcpy(ident, identifier);
    if(!(ident1 = strtok(ident, "/"))) {
        LOGE("No multiple identifiers found in identifier value");
        free(ident);
    } else {
        if (!strcmp(ident1, "_swdev")) {
            if(!(ident2 = strtok(NULL, "/"))) {
                LOGE("Invalid disable device value: %s, but enabling new device", ident2);
            } else {
                pthread_mutex_lock(&uc_mgr->card_ctxt_ptr->card_lock);
                ret = snd_ucm_del_ident_from_list(&uc_mgr->card_ctxt_ptr->dev_list_head, ident2);
                if (ret < 0) {
                    LOGE("Invalid device: Device not part of enabled device list");
                } else {
                    LOGV("swdev: device value to be disabled: %s", ident2);
                    /* Disable mixer controls for corresponding use cases and device */
                    ret = snd_use_case_set_device_for_all_ident(uc_mgr, ident2, 0);
                    if (ret < 0) {
                        LOGE("Disabling old device %s failed: %d", ident2, errno);
                    }
                }
                pthread_mutex_unlock(&uc_mgr->card_ctxt_ptr->card_lock);
            }
            ret = snd_use_case_set(uc_mgr, "_enadev", value);
            if (ret < 0) {
                LOGE("Enabling device %s failed", value);
            }
        } else if (!strcmp(ident1, "_swmod")) {
            if(!(ident2 = strtok(NULL, "/"))) {
                LOGE("Invalid modifier value: %s, but enabling new modifier", ident2);
            } else {
                ret = snd_use_case_set(uc_mgr, "_dismod", ident2);
                if (ret < 0) {
                    LOGE("Disabling old modifier %s failed", ident2);
                }
            }
            ret = snd_use_case_set(uc_mgr, "_enamod", value);
            if (ret < 0) {
                LOGE("Enabling modifier %s failed", value);
            }
            return ret;
        } else {
            LOGE("No switch device/modifier option found: %s", ident1);
        }
        free(ident);
    }

    pthread_mutex_lock(&uc_mgr->card_ctxt_ptr->card_lock);
    if (!strcmp(identifier, "_verb")) {
        /* Check if value is valid verb */
        while (strcmp(verb_list[index], SND_UCM_END_OF_LIST)) {
            if (!strcmp(verb_list[index], value)) {
                ret = 0;
                break;
            }
            index++;
        }
        if (ret < 0) {
            LOGE("Invalid verb identifier value");
        } else {
            LOGV("Index:%d Verb:%s", index, verb_list[index]);
            /* Disable the mixer controls for current use case
             * for all the enabled devices */
            if ((uc_mgr->card_ctxt_ptr->current_verb != NULL) &&
                (strcmp(uc_mgr->card_ctxt_ptr->current_verb, SND_USE_CASE_VERB_INACTIVE))) {
                ret = snd_use_case_ident_set_controls_for_all_devices(uc_mgr, uc_mgr->card_ctxt_ptr->current_verb, 0);
            }
            uc_mgr->card_ctxt_ptr->current_verb = (char *)realloc(uc_mgr->card_ctxt_ptr->current_verb, (strlen(value)+1)*sizeof(char));
            if (uc_mgr->card_ctxt_ptr->current_verb == NULL) {
                LOGE("Failed to allocate memory");
                ret = -ENOMEM;
            } else {
                strcpy(uc_mgr->card_ctxt_ptr->current_verb, value);
                ret = 0;
            }
            /* Enable the mixer controls for the new use case
             * for all the enabled devices */
            if ((ret == 0) && (strcmp(uc_mgr->card_ctxt_ptr->current_verb, SND_USE_CASE_VERB_INACTIVE))) {
               ret = snd_use_case_ident_set_controls_for_all_devices(uc_mgr, uc_mgr->card_ctxt_ptr->current_verb, 1);
            }
        }
    } else if (!strcmp(identifier, "_enadev")) {
        index = 0; ret = 0;
        list_size = snd_ucm_get_size_of_list(uc_mgr->card_ctxt_ptr->dev_list_head);
        for (index = 0; index < list_size; index++) {
            ident = snd_ucm_get_value_at_index(uc_mgr->card_ctxt_ptr->dev_list_head, index);
            if (!strcmp(ident, value)) {
                LOGV("Device already part of enabled list: %s", value);
                free(ident);
                break;
            }
            free(ident);
        }
        if (index == list_size) {
            LOGV("enadev: device value to be enabled: %s", value);
            snd_ucm_add_ident_to_list(&uc_mgr->card_ctxt_ptr->dev_list_head, value);
        }
        snd_ucm_print_list(uc_mgr->card_ctxt_ptr->dev_list_head);
        /* Apply Mixer controls of all verb and modifiers for this device*/
        ret = snd_use_case_set_device_for_all_ident(uc_mgr, value, 1);
    } else if (!strcmp(identifier, "_disdev")) {
        if (!strcmp(value, SND_USE_CASE_DEV_ALL)) {
            list_size = snd_ucm_get_size_of_list(uc_mgr->card_ctxt_ptr->mod_list_head);
            if ((list_size != 0) || ((uc_mgr->card_ctxt_ptr->current_verb != NULL) &&
                (strcmp(uc_mgr->card_ctxt_ptr->current_verb, SND_USE_CASE_VERB_INACTIVE)))) {
                LOGV("Ignoring disable device %s: Valid use case or modifier exists", value);
            } else {
                LOGV("No valid use case/modifier, Disabling all devices");
                list_size = snd_ucm_get_size_of_list(uc_mgr->card_ctxt_ptr->dev_list_head);
                for (index = (list_size-1); index >= 0; index--) {
                    ident = snd_ucm_get_value_at_index(uc_mgr->card_ctxt_ptr->dev_list_head, index);
                    if (ident != NULL) {
                        ret = snd_ucm_del_ident_from_list(&uc_mgr->card_ctxt_ptr->dev_list_head, ident);
                        if(!ret) {
                            ret = snd_use_case_apply_mixer_controls(uc_mgr, ident, 0);
                        }
                        free(ident);
                    }
                }
                ret = 0;
            }
        } else {
            ret = snd_use_case_check_device_for_disable(uc_mgr, value);
            if (ret == 0) {
                ret = snd_ucm_del_ident_from_list(&uc_mgr->card_ctxt_ptr->dev_list_head, value);
                if (ret < 0) {
                    LOGE("Invalid device: Device not part of enabled device list");
                } else {
                    LOGV("disdev: device value to be disabled: %s", value);
                    /* Apply Mixer controls for corresponding device and modifier */
                    ret = snd_use_case_apply_mixer_controls(uc_mgr, value, 0);
                }
            } else {
                LOGV("Valid use cases exists for device %s, ignoring disable command", value);
            }
        }
    } else if (!strcmp(identifier, "_enamod")) {
        if (!strcmp(uc_mgr->card_ctxt_ptr->current_verb, SND_USE_CASE_VERB_INACTIVE)) {
            LOGE("Invalid use case verb value");
            ret = -EINVAL;
        } else {
            ret = 0;
            while(strcmp(verb_list[index], uc_mgr->card_ctxt_ptr->current_verb)) {
                if (!strcmp(verb_list[index], SND_UCM_END_OF_LIST)){
                    ret = -EINVAL;
                    break;
                }
                index++;
            }
        }
        if (ret < 0) {
            LOGE("Invalid verb identifier value");
        } else {
            verb_index = index; index = 0; ret = 0;
            LOGV("Index:%d Verb:%s", verb_index, verb_list[verb_index]);
            while(strcmp(use_case_verb_list[verb_index].modifier_list[index], value)) {
                if (!strcmp(use_case_verb_list[verb_index].modifier_list[index], SND_UCM_END_OF_LIST)){
                    ret = -EINVAL;
                    break;
                }
                index++;
            }
            if (ret < 0) {
                LOGE("Invalid modifier identifier value");
            } else {
                snd_ucm_add_ident_to_list(&uc_mgr->card_ctxt_ptr->mod_list_head, value);
                /* Enable the mixer controls for the new use case
                 * for all the enabled devices */
                ret = snd_use_case_ident_set_controls_for_all_devices(uc_mgr, value, 1);
            }
        }
    } else if (!strcmp(identifier, "_dismod")) {
        ret = snd_ucm_del_ident_from_list(&uc_mgr->card_ctxt_ptr->mod_list_head, value);
        if (ret < 0) {
            LOGE("Modifier not enabled currently, invalid modifier");
        } else {
            LOGV("dismod: modifier value to be disabled: %s", value);
            /* Enable the mixer controls for the new use case
             * for all the enabled devices */
            ret = snd_use_case_ident_set_controls_for_all_devices(uc_mgr, value, 0);
        }
    } else {
        LOGE("Unknown identifier value: %s", identifier);
    }
    pthread_mutex_unlock(&uc_mgr->card_ctxt_ptr->card_lock);
    return ret;
}

/**
 * Open and initialise use case core for sound card
 * uc_mgr - Returned use case manager pointer
 * card_name - Sound card name.
 * returns 0 on success, otherwise a negative error code
 */
int snd_use_case_mgr_open(snd_use_case_mgr_t **uc_mgr, const char *card_name)
{
    snd_use_case_mgr_t *uc_mgr_ptr = NULL;
    int index, ret = -EINVAL;

    LOGV("snd_use_case_open(): card_name %s", card_name);

    if (card_name == NULL) {
        LOGE("snd_use_case_mgr_open: failed, invalid arguments");
        return ret;
    }

    for (index = 0; index < (int)MAX_NUM_CARDS; index++) {
        if(!strcmp(card_name, card_ctxt_list[index].card_name)) {
            ret = 0;
            break;
        }
    }

    if (ret < 0) {
        LOGE("Card %s not found", card_name);
    } else {
        uc_mgr_ptr = (snd_use_case_mgr_t *)malloc(sizeof(snd_use_case_mgr_t));
        uc_mgr_ptr->snd_card_index = index;
        uc_mgr_ptr->card_ctxt_ptr = &card_ctxt_list[index];
        uc_mgr_ptr->device_list_count = 0;
        uc_mgr_ptr->modifier_list_count = 0;
        uc_mgr_ptr->current_device_list = NULL;
        uc_mgr_ptr->current_modifier_list = NULL;
        pthread_mutexattr_init(&uc_mgr_ptr->card_ctxt_ptr->card_lock_attr);
        pthread_mutex_init(&uc_mgr_ptr->card_ctxt_ptr->card_lock,
            &uc_mgr_ptr->card_ctxt_ptr->card_lock_attr);
        /* Reset all mixer controls if any applied previously for the same card */
	snd_use_case_mgr_reset(uc_mgr_ptr);
        uc_mgr_ptr->card_ctxt_ptr->current_verb = NULL;
        LOGV("Open mixer device: %s", uc_mgr_ptr->card_ctxt_ptr->control_device);
        uc_mgr_ptr->card_ctxt_ptr->mixer_handle = mixer_open(uc_mgr_ptr->card_ctxt_ptr->control_device);
        LOGV("Mixer handle %p", uc_mgr_ptr->card_ctxt_ptr->mixer_handle);
        *uc_mgr = uc_mgr_ptr;
    }
    LOGV("snd_use_case_open(): returning instance %p", uc_mgr_ptr);
    return ret;
}


/**
 * \brief Reload and re-parse use case configuration files for sound card.
 * \param uc_mgr Use case manager
 * \return zero if success, otherwise a negative error code
 */
int snd_use_case_mgr_reload(snd_use_case_mgr_t *uc_mgr) {
    LOGE("Reload is not implemented for now as there is no use case currently");
    return 0;
}

/**
 * \brief Close use case manager
 * \param uc_mgr Use case manager
 * \return zero if success, otherwise a negative error code
 */
int snd_use_case_mgr_close(snd_use_case_mgr_t *uc_mgr)
{
    int ret = 0;

    if ((uc_mgr->snd_card_index >= (int)MAX_NUM_CARDS) ||
        (uc_mgr->snd_card_index < 0) || (uc_mgr->card_ctxt_ptr == NULL)) {
        LOGE("snd_use_case_mgr_close(): failed, invalid arguments");
        return -EINVAL;
    }

    LOGV("snd_use_case_close(): instance %p", uc_mgr);
    ret = snd_use_case_mgr_reset(uc_mgr);
    if (ret < 0)
        LOGE("Failed to reset ucm session");
    pthread_mutexattr_destroy(&uc_mgr->card_ctxt_ptr->card_lock_attr);
    pthread_mutex_destroy(&uc_mgr->card_ctxt_ptr->card_lock);
    if (uc_mgr->card_ctxt_ptr->mixer_handle) {
        mixer_close(uc_mgr->card_ctxt_ptr->mixer_handle);
        uc_mgr->card_ctxt_ptr->mixer_handle = NULL;
    }
    uc_mgr->snd_card_index = -1;
    uc_mgr->card_ctxt_ptr = NULL;
    free(uc_mgr);
    uc_mgr = NULL;
    LOGV("snd_use_case_mgr_close(): card instace closed successfully");
    return ret;
}

/**
 * \brief Reset use case manager verb, device, modifier to deafult settings.
 * \param uc_mgr Use case manager
 * \return zero if success, otherwise a negative error code
 */
int snd_use_case_mgr_reset(snd_use_case_mgr_t *uc_mgr)
{
    char *ident_value;
    int index, list_size, ret = 0;

    if ((uc_mgr->snd_card_index >= (int)MAX_NUM_CARDS) ||
        (uc_mgr->snd_card_index < 0) || (uc_mgr->card_ctxt_ptr == NULL)) {
        LOGE("snd_use_case_mgr_reset(): failed, invalid arguments");
        return -EINVAL;
    }

    LOGV("snd_use_case_reset(): instance %p", uc_mgr);
    pthread_mutex_lock(&uc_mgr->card_ctxt_ptr->card_lock);
    /* Disable mixer controls of all the enabled modifiers */
    list_size = snd_ucm_get_size_of_list(uc_mgr->card_ctxt_ptr->mod_list_head);
    for (index = (list_size-1); index >= 0; index--) {
        ident_value = snd_ucm_get_value_at_index(uc_mgr->card_ctxt_ptr->mod_list_head, index);
        snd_ucm_del_ident_from_list(&uc_mgr->card_ctxt_ptr->mod_list_head, ident_value);
        ret = snd_use_case_ident_set_controls_for_all_devices(uc_mgr, ident_value, 0);
	if (ret != 0)
		LOGE("Failed to disable mixer controls for %s", ident_value);
	free(ident_value);
    }
    /* Clear the enabled modifiers list */
    if (uc_mgr->modifier_list_count) {
        for (index = 0; index < uc_mgr->modifier_list_count; index++) {
            free(uc_mgr->current_modifier_list[index]);
            uc_mgr->current_modifier_list[index] = NULL;
        }
        free(uc_mgr->current_modifier_list);
        uc_mgr->current_modifier_list = NULL;
        uc_mgr->modifier_list_count = 0;
    }
    /* Disable mixer controls of current use case verb */
    if (uc_mgr->card_ctxt_ptr->current_verb != NULL) {
        if(strcmp(uc_mgr->card_ctxt_ptr->current_verb, SND_USE_CASE_VERB_INACTIVE)) {
            ret = snd_use_case_ident_set_controls_for_all_devices(uc_mgr, uc_mgr->card_ctxt_ptr->current_verb, 0);
            if (ret != 0)
                LOGE("Failed to disable mixer controls for %s", uc_mgr->card_ctxt_ptr->current_verb);
        }
        free(uc_mgr->card_ctxt_ptr->current_verb);
        uc_mgr->card_ctxt_ptr->current_verb = NULL;
    }
    /* Disable mixer controls of all the enabled devices */
    list_size = snd_ucm_get_size_of_list(uc_mgr->card_ctxt_ptr->dev_list_head);
    for (index = (list_size-1); index >= 0; index--) {
        ident_value = snd_ucm_get_value_at_index(uc_mgr->card_ctxt_ptr->dev_list_head, index);
        snd_ucm_del_ident_from_list(&uc_mgr->card_ctxt_ptr->dev_list_head, ident_value);
        ret = snd_use_case_set_device_for_all_ident(uc_mgr, ident_value, 0);
	if (ret != 0)
            LOGE("Failed to disable or no mixer controls set for %s", ident_value);
	free(ident_value);
    }
    /* Clear the enabled devices list */
    if (uc_mgr->device_list_count) {
        for (index = 0; index < uc_mgr->device_list_count; index++) {
            free(uc_mgr->current_device_list[index]);
            uc_mgr->current_device_list[index] = NULL;
        }
        free(uc_mgr->current_device_list);
        uc_mgr->current_device_list = NULL;
        uc_mgr->device_list_count = 0;
    }
    pthread_mutex_unlock(&uc_mgr->card_ctxt_ptr->card_lock);
    return ret;
}

/* Add an identifier to the respective list
 * head - list head
 * value - node value that needs to be added
 * Returns 0 on sucess, negative error code otherwise
 */
int snd_ucm_add_ident_to_list(struct snd_ucm_ident_node **head, const char *value)
{
    struct snd_ucm_ident_node *temp, *node;

    node = (struct snd_ucm_ident_node *)malloc(sizeof(struct snd_ucm_ident_node));
    if (node == NULL) {
        LOGE("Failed to allocate memory for new node");
        return -ENOMEM;
    } else {
        node->next = NULL;
        node->ident = (char *)malloc((strlen(value) + 1)*sizeof(char));
        if (node->ident == NULL) {
            LOGE("Failed to allocate memory for identifier of new node");
            free(node);
            return -ENOMEM;
        } else {
            strcpy(node->ident, value);
        }
    }
    if (*head == NULL) {
        *head = node;
    } else {
        temp = *head;
        while (temp->next != NULL) {
            temp = temp->next;
        }
        temp->next = node;
    }
    LOGV("add_to_list: head %p, value %s", *head, node->ident);
    return 0;
}

/* Get the identifier value at particulare index of the list
 * head - list head
 * index - node index value
 * Returns node idetifier value at index on sucess, NULL otherwise
 */
char *snd_ucm_get_value_at_index(struct snd_ucm_ident_node *head, int index)
{
    if (head == NULL) {
        LOGV("Empty list");
        return NULL;
    }

    if ((index < 0) || (index >= (snd_ucm_get_size_of_list(head)))) {
        LOGE("Element with given index %d doesn't exist in the list", index);
        return NULL;
    }

    while (index) {
        head = head->next;
        index--;
    }

    return (strdup(head->ident));
}

/* Get the size of the list
 * head - list head
 * Returns size of list on sucess, negative error code otherwise
 */
int snd_ucm_get_size_of_list(struct snd_ucm_ident_node *head)
{
    int index = 0;

    if (head == NULL) {
        LOGV("Empty list");
        return 0;
    }

    while (head->next != NULL) {
        index++;
        head = head->next;
    }

    return (index+1);
}

void snd_ucm_print_list(struct snd_ucm_ident_node *head)
{
    int index = 0;

    LOGV("print_list: head %p", head);
    if (head == NULL) {
        LOGV("Empty list");
        return;
    }

    while (head->next != NULL) {
        LOGV("index: %d, value: %s", index, head->ident);
        index++;
        head = head->next;
    }
    LOGV("index: %d, value: %s", index, head->ident);
}

/* Delete an identifier from respective list
 * head - list head
 * value - node value that needs to be deleted
 * Returns 0 on sucess, negative error code otherwise
 *
 */
int snd_ucm_del_ident_from_list(struct snd_ucm_ident_node **head, const char *value)
{
    struct snd_ucm_ident_node *temp1, *temp2;
    int ret = -EINVAL;

    if (*head == NULL) {
        LOGE("Error: Empty list");
        return -EINVAL;
    } else if (!strcmp((*head)->ident, value)) {
            temp2 = *head;
            *head = temp2->next;
            ret = 0;
    } else {
        temp1 = *head;
        temp2 = temp1->next;
        while (temp2 != NULL) {
            if (!strcmp(temp2->ident, value)) {
                temp1->next = temp2->next;
                ret = 0;
                break;
            }
            temp1 = temp1->next;
            temp2 = temp1->next;
        }
    }
    if (ret < 0) {
        LOGE("Element not found in enabled list");
    } else {
        temp2->next = NULL;
        free(temp2->ident);
        temp2->ident = NULL;
        free(temp2);
        temp2 = NULL;
    }
    return ret;
}
