/**
*  This file is part of rmlint.
*
*  rmlint is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, either version 3 of the License, or
*  (at your option) any later version.
*
*  rmlint is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with rmlint.  If not, see <http://www.gnu.org/licenses/>.
*
** Authors:
 *
 *  - Christopher <sahib> Pahl 2010-2014 (https://github.com/sahib)
 *  - Daniel <SeeSpotRun> T.   2014-2014 (https://github.com/SeeSpotRun)
 *
** Hosted on http://github.com/sahib/rmlint
*
**/

#ifndef RM_SESSION_H
#define RM_SESSION_H

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <glib.h>

#include "treemerge.h"
#include "utilities.h"
#include "checksum.h"
#include "file.h"


typedef struct RmFileTables {
    struct RmMountTable *mounts;
    GHashTable *dev_table;
    GHashTable *size_groups;
    GHashTable *node_table;
    GHashTable *mtime_filter;
    GHashTable *basename_filter;
    GQueue *file_queue;
    GList *other_lint[RM_LINT_TYPE_DUPE_CANDIDATE];
    GRecMutex lock;
} RmFileTables;

struct RmFmtTable;
struct RmTreeMerger;

typedef struct RmSession {
    RmSettings *settings;

    /* Stores for RmFile during traversal, preproces and shredder */
    struct RmFileTables *tables;

    /* Table of mountpoints used in the system */
    struct RmMountTable *mounts;

    /* Output formatting control */
    struct RmFmtTable *formats;

    /* Treemerging for -D */
    struct RmTreeMerger *dir_merger;

    /* Counters for printing useful statistics */
    RmOff total_files;
    RmOff total_filtered_files;
    RmOff total_lint_size;
    RmOff shred_bytes_remaining;
    RmOff shred_files_remaining;
    RmOff dup_counter;
    RmOff dup_group_counter;
    RmOff ignored_files;
    RmOff ignored_folders;
    RmOff other_lint_cnt;

    /* flag indicating if rmlint was aborted early */
    volatile bool aborted;

    GTimer *timer;

    /* Debugging counters */
    RmOff offset_fragments;
    RmOff offsets_read;
    RmOff offset_fails;

    /* Daniels paranoia */
    RmOff hash_seed1;
    RmOff hash_seed2;
} RmSession;

/**
 * @brief Initialize session according to settings.
 */
void rm_session_init(RmSession *session, RmSettings *settings);

/**
 * @brief Clear all memory allocated by rm_session_init.
 */
void rm_session_clear(RmSession *session);

/**
 * @brief Set the abort flag of RmSession.
 *
 * This flag is checked periodically on strategic points,
 * leading to an early but planned exit.
 *
 * Threadsafe.
 */
void rm_session_abort(RmSession *session);

/**
 * @brief Check if rmlint was aborted early.
 *
 * Threadsafe.
 */
bool rm_session_was_aborted(RmSession *session);

/* Maybe colors, for use outside of the rm_log macros,
 * in order to work with the --with-no-color option
 * */
#define MAYBE_RED(s)    ((s->settings->color) ? RED : "")
#define MAYBE_YELLOW(s) ((s->settings->color) ? YELLOW : "")
#define MAYBE_RESET(s)  ((s->settings->color) ? RESET : "")
#define MAYBE_GREEN(s)  ((s->settings->color) ? GREEN : "")
#define MAYBE_BLUE(s)   ((s->settings->color) ? BLUE : "")

#endif /* end of include guard */

