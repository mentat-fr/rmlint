/*
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
 * Authors:
 *
 *  - Christopher <sahib> Pahl 2010-2014 (https://github.com/sahib)
 *  - Daniel <SeeSpotRun> T.   2014-2014 (https://github.com/SeeSpotRun)
 *
 * Hosted on http://github.com/sahib/rmlint
 *
 */

#include "../formats.h"

#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <sys/ioctl.h>

typedef struct RmFmtHandlerProgress {
    /* must be first */
    RmFmtHandler parent;

    /* user data */
    gdouble percent;
    gdouble last_unknown_pos;

    char text_buf[1024];
    guint32 text_len;
    guint32 update_counter;
    guint32 update_interval;

    RmFmtProgressState last_state;
    struct winsize terminal;
} RmFmtHandlerProgress;

static void rm_fmt_progress_format_text(RmSession *session, RmFmtHandlerProgress *self, int max_len) {
    char num_buf[32] = {0};
    memset(num_buf, 0, sizeof(num_buf));

    switch(self->last_state) {
    case RM_PROGRESS_STATE_TRAVERSE:
        self->percent = 2.0;
        self->text_len = g_snprintf(
                             self->text_buf, sizeof(self->text_buf),
                             "%s (%s%"LLU"%s %s / %s%"LLU"%s + %s%"LLU"%s %s)",
                             _("Traversing"),
                             MAYBE_GREEN(session), session->total_files, MAYBE_RESET(session),
                             _("usable files"),
                             MAYBE_RED(session), session->ignored_files, MAYBE_RESET(session),
                             MAYBE_RED(session), session->ignored_folders, MAYBE_RESET(session),
                             _("ignored files / folders")
                         );
        break;
    case RM_PROGRESS_STATE_PREPROCESS:
        self->percent = 2.0;
        self->text_len = g_snprintf(
                             self->text_buf, sizeof(self->text_buf),
                             "%s (%s %s%"LLU"%s / %s %s%"LLU"%s %s)",
                             _("Preprocessing"),
                             _("reduces files to"),
                             MAYBE_GREEN(session), session->total_filtered_files, MAYBE_RESET(session),
                             _("found"),
                             MAYBE_RED(session), session->other_lint_cnt, MAYBE_RESET(session),
                             _("other lint")
                         );
        break;
    case RM_PROGRESS_STATE_SHREDDER:
        self->percent = 1.0 - ((gdouble)session->shred_files_remaining) / ((gdouble)session->total_filtered_files);
        rm_util_size_to_human_readable(session->shred_bytes_remaining, num_buf, sizeof(num_buf));
        self->text_len = g_snprintf(
                             self->text_buf, sizeof(self->text_buf),
                             "%s (%s%"LLU"%s %s %s%"LLU"%s %s; %s%s%s %s %s%"LLU"%s %s)",
                             _("Matching files"),
                             MAYBE_RED(session), session->dup_counter, MAYBE_RESET(session),
                             _("dupes of"),
                             MAYBE_YELLOW(session), session->dup_group_counter, MAYBE_RESET(session),
                             _("originals"),
                             MAYBE_GREEN(session), num_buf, MAYBE_RESET(session),
                             _("to scan in "),
                             MAYBE_GREEN(session), session->shred_files_remaining, MAYBE_RESET(session),
                             _("files")
                         );
        break;
    case RM_PROGRESS_STATE_MERGE:
        self->percent = 2.0;
        self->text_len = g_snprintf(self->text_buf, sizeof(self->text_buf),
                                    _("Merging files into directories (stand by...)"));
        break;
    case RM_PROGRESS_STATE_INIT:
    case RM_PROGRESS_STATE_SUMMARY:
    default:
        self->percent = 0;
        memset(self->text_buf, 0, sizeof(self->text_buf));
        break;
    }

    /* Get rid of colors */
    int text_iter = 0;
    for(char *iter = &self->text_buf[0]; *iter; iter++) {
        if(*iter == '\x1b') {
            char *jump = strchr(iter, 'm');
            if(jump != NULL) {
                self->text_len -= jump - iter + 1;
                iter = jump;
                continue;
            }
        }

        if(text_iter >= max_len) {
            *iter = 0;
            self->text_len = text_iter;
            break;
        }

        text_iter++;
    }
}

static void rm_fmt_progress_print_text(RmFmtHandlerProgress *self, int width, FILE *out) {
    if(self->text_len < (unsigned)width) {
        for(guint32 i = 0; i < width - self->text_len; ++i) {
            fprintf(out, " ");
        }
    }

    fprintf(out, "%s", self->text_buf);
}

static void rm_fmt_progress_print_bar(RmSession *session, RmFmtHandlerProgress *self, int width, FILE *out) {
    int cells = width * self->percent;

    /* true when we do not know when 100% is reached.
     * Show a moving something in this case.
     * */
    bool is_unknown = self->percent > 1.1;


    fprintf(out, "[");
    for(int i = 0; i < width - 2; ++i) {
        if(i < cells) {
            if(is_unknown) {
                if((int)self->last_unknown_pos % 4 == i % 4) {
                    fprintf(out, "%so%s", MAYBE_BLUE(session), MAYBE_RESET(session));
                } else if((int)self->last_unknown_pos % 2 == i % 2) {
                    fprintf(out, "%sO%s", MAYBE_YELLOW(session), MAYBE_RESET(session));
                } else {
                    fprintf(out, " ");
                }
            } else {
                fprintf(out, "#");
            }
        } else if(i == cells) {
            fprintf(out, "%s>%s", MAYBE_YELLOW(session), MAYBE_RESET(session));
        } else {
            fprintf(out, "%s-%s", MAYBE_BLUE(session), MAYBE_RESET(session));
        }
    }
    fprintf(out, "]");

    self->last_unknown_pos = fmod(self->last_unknown_pos + 0.005, width - 2);
}

static void rm_fmt_prog(
    RmSession *session,
    RmFmtHandler *parent,
    FILE *out,
    RmFmtProgressState state
) {
    RmFmtHandlerProgress *self = (RmFmtHandlerProgress *) parent;

    if(state == RM_PROGRESS_STATE_INIT) {
        /* Do initializiation here */
        const char *update_interval_str = rm_fmt_get_config_value(
                                              session->formats, "progressbar", "update_interval"
                                          );

        if(update_interval_str) {
            self->update_interval = g_ascii_strtoull(update_interval_str, NULL, 10);
        }

        if(self->update_interval == 0) {
            self->update_interval = 50;
        }

        self->last_unknown_pos = 0;

        fprintf(out, "\e[?25l"); /* Hide the cursor */
        fflush(out);
        return;
    }

    if(state == RM_PROGRESS_STATE_SUMMARY || rm_session_was_aborted(session)) {
        fprintf(out, "\e[?25h"); /* show the cursor */
        fflush(out);
    }

    if(self->last_state != state && self->last_state != RM_PROGRESS_STATE_INIT) {
        self->percent = 1.0;
        rm_fmt_progress_print_bar(session, self, self->terminal.ws_col * 0.3, out);
        fprintf(out, "\n");
    } else if((self->update_counter++ % self->update_interval) > 0) {
        return;
    }

    if(ioctl(0, TIOCGWINSZ, &self->terminal) != 0) {
        rm_log_warning_line(_("Cannot figure out terminal width."));
    }

    self->last_state = state;

    int text_width = self->terminal.ws_col * 0.7 - 1;
    rm_fmt_progress_format_text(session, self, text_width);
    rm_fmt_progress_print_bar(session, self, self->terminal.ws_col * 0.3, out);
    rm_fmt_progress_print_text(self, text_width, out);
    fprintf(out, "%s\r", MAYBE_RESET(session));
}

static RmFmtHandlerProgress PROGRESS_HANDLER_IMPL = {
    /* Initialize parent */
    .parent = {
        .size = sizeof(PROGRESS_HANDLER_IMPL),
        .name = "progressbar",
        .head = NULL,
        .elem = NULL,
        .prog = rm_fmt_prog,
        .foot = NULL
    },

    /* Initialize own stuff */
    .percent = 0.0f,
    .text_len = 0,
    .text_buf = {0},
    .update_counter = 0,
    .last_state = RM_PROGRESS_STATE_INIT
};

RmFmtHandler *PROGRESS_HANDLER = (RmFmtHandler *) &PROGRESS_HANDLER_IMPL;
