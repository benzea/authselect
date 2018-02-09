/*
    Authors:
        Pavel Březina <pbrezina@redhat.com>

    Copyright (C) 2017 Red Hat

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "lib/authselect_private.h"
#include "lib/authselect_paths.h"
#include "lib/util/string_array.h"
#include "lib/util/string.h"

static errno_t
read_line(FILE *file, char **_line)
{
    char *line = NULL;
    size_t len = 0;
    char *trimmed;

    errno = 0;
    while (getline(&line, &len, file) != -1) {
        trimmed = string_trim_noempty(line);

        /* Reset values for next getline call. */
        free(line);
        line = NULL;
        len = 0;

        /* Skip empty line. */
        if (trimmed == NULL) {
            continue;
        }

        /* Skip comments. */
        if (trimmed[0] == '#') {
            free(trimmed);
            continue;
        }

        *_line = trimmed;
        return EOK;
    }

    if (errno != 0) {
        return errno;
    }

    /* End of file. */
    return ENOENT;
}

static errno_t
authselect_read_conf_features(FILE *file, char ***_features)
{
    char **features = NULL;
    char **reallocated;
    size_t count = 0;
    errno_t ret;

    do {
        count++;
        reallocated = realloc_array(features, char *, count + 1);
        if (reallocated == NULL) {
            string_array_free(features);
            return ENOMEM;
        }

        features = reallocated;
        features[count - 1] = NULL;
        features[count] = NULL;

        ret = read_line(file, &features[count - 1]);
    } while (ret == EOK);

    if (ret == ENOENT) {
        *_features = features;
        return EOK;
    }

    string_array_free(features);
    return ret;
}

errno_t
authselect_read_conf(char **_profile_id,
                     char ***_features)
{
    FILE *file;
    char *profile_id = NULL;
    char **features = NULL;
    errno_t ret;

    file = fopen(PATH_CONFIG_FILE, "r");
    if (file == NULL) {
        ret = errno;

        if (ret == ENOENT) {
            WARN("Configuration file [%s] is missing", PATH_CONFIG_FILE);
        } else {
            ERROR("Unable to read configuration file [%s] [%d]: %s",
                  PATH_CONFIG_FILE, ret, strerror(ret));
        }

        return ret;
    }

    /* First line is a profile name. */
    ret = read_line(file, &profile_id);
    if (ret != EOK) {
        ERROR("Unable to read profile id from configuration [%d]: %s",
              ret, strerror(ret));
        goto done;
    }

    /* Following lines are options that were used. */
    ret = authselect_read_conf_features(file, &features);
    if (ret != EOK) {
        ERROR("Unable to read profile parameters from configuration [%d]: %s",
              ret, strerror(ret));
        goto done;
    }

    if (_profile_id != NULL) {
        *_profile_id = profile_id;
        profile_id = NULL;
    }

    if (_features != NULL) {
        *_features = features;
        features = NULL;
    }

    ret = EOK;

done:
    fclose(file);

    if (profile_id != NULL) {
        free(profile_id);
    }

    string_array_free(features);

    return ret;
}
