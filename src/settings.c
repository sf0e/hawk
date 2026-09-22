#include "hawk.h"

#include <stdio.h>

gchar *hawk_data_dir(void)
{
    gchar *base = g_build_filename(g_get_home_dir(), HAWK_DATA_DIR, NULL);
    g_mkdir_with_parents(base, 0700);
    return base;
}

gchar *hawk_data_file(const gchar *name)
{
    gchar *base = hawk_data_dir();
    gchar *path = g_build_filename(base, name, NULL);
    g_free(base);
    return path;
}

gchar *hawk_data_subdir(const gchar *sub)
{
    gchar *base = hawk_data_dir();
    gchar *path = g_build_filename(base, sub, NULL);
    g_mkdir_with_parents(path, 0700);
    g_free(base);
    return path;
}

static gchar *legacy_config_path(void)
{
    const gchar *dir = g_get_user_config_dir();
    gchar *base = g_build_filename(dir, "hawk", NULL);
    gchar *path = g_build_filename(base, "hawk.ini", NULL);
    g_free(base);
    return path;
}

static gchar *hawk_config_path(void)
{
    gchar *path = hawk_data_file("hawk.ini");

    gchar *legacy = legacy_config_path();
    if (!g_file_test(path, G_FILE_TEST_EXISTS) &&
        g_file_test(legacy, G_FILE_TEST_EXISTS))
        rename(legacy, path);
    g_free(legacy);
    return path;
}

void hawk_config_init(SearchConfig *cfg)
{
    cfg->local_search = TRUE;
    cfg->home = g_strdup(HAWK_BACKEND_HOME);
    cfg->block_3p = TRUE;
    cfg->ua_lock = TRUE;
    cfg->block_media = TRUE;
    cfg->dark = FALSE;
    cfg->restore_session = TRUE;
    cfg->max_history = 400;
}

void hawk_config_load(SearchConfig *cfg)
{
    hawk_config_init(cfg);

    gchar *path = hawk_config_path();
    GKeyFile *kf = g_key_file_new();
    GError *err = NULL;
    if (g_key_file_load_from_file(kf, path, G_KEY_FILE_NONE, &err)) {
        cfg->local_search =
            g_key_file_get_boolean(kf, "search", "local", NULL);
        gchar *home = g_key_file_get_string(kf, "search", "home", NULL);
        if (home) {
            g_free(cfg->home);
            cfg->home = home;
        }
        cfg->block_3p =
            g_key_file_get_boolean(kf, "privacy", "block_3p", NULL);
        cfg->ua_lock =
            g_key_file_get_boolean(kf, "privacy", "ua_lock", NULL);
        cfg->block_media =
            g_key_file_get_boolean(kf, "privacy", "block_media", NULL);
        cfg->dark =
            g_key_file_get_boolean(kf, "ui", "dark", NULL);
        cfg->restore_session =
            g_key_file_get_boolean(kf, "session", "restore", NULL);
        cfg->max_history =
            g_key_file_get_integer(kf, "session", "max_history", NULL);
        if (cfg->max_history < 10)
            cfg->max_history = 400;
    } else {
        g_clear_error(&err);
    }
    g_key_file_free(kf);
    g_free(path);
}

void hawk_config_save(const SearchConfig *cfg)
{
    gchar *path = hawk_config_path();
    GKeyFile *kf = g_key_file_new();

    g_key_file_set_boolean(kf, "search", "local", cfg->local_search);
    g_key_file_set_string(kf, "search", "home",
                          cfg->home ? cfg->home : HAWK_BACKEND_HOME);
    g_key_file_set_boolean(kf, "privacy", "block_3p", cfg->block_3p);
    g_key_file_set_boolean(kf, "privacy", "ua_lock", cfg->ua_lock);
    g_key_file_set_boolean(kf, "privacy", "block_media", cfg->block_media);
    g_key_file_set_boolean(kf, "ui", "dark", cfg->dark);
    g_key_file_set_boolean(kf, "session", "restore", cfg->restore_session);
    g_key_file_set_integer(kf, "session", "max_history", cfg->max_history);

    gchar *data = g_key_file_to_data(kf, NULL, NULL);
    GError *err = NULL;
    if (!g_file_set_contents(path, data, -1, &err)) {
        g_warning("hawk: could not save config: %s", err->message);
        g_clear_error(&err);
    }
    g_free(data);
    g_key_file_free(kf);
    g_free(path);
}

typedef struct {
    HawkWin *win;
    int id;
} RowCtx;

enum {
    ROW_LOCAL = 0,
    ROW_BLOCK_3P,
    ROW_UA_LOCK,
    ROW_BLOCK_MEDIA,
    ROW_DARK,
};

static void on_toggle(GObject *sw, GParamSpec *ps, gpointer data)
{
    (void)ps;
    RowCtx *ctx = data;
    HawkWin *win = ctx->win;
    gboolean on = gtk_switch_get_active(GTK_SWITCH(sw));

    switch (ctx->id) {
    case ROW_LOCAL:
        win->cfg.local_search = on;
        break;
    case ROW_BLOCK_3P:
        win->cfg.block_3p = on;
        break;
    case ROW_UA_LOCK:
        win->cfg.ua_lock = on;
        break;
    case ROW_BLOCK_MEDIA:
        win->cfg.block_media = on;
        break;
    case ROW_DARK:
        win->cfg.dark = on;
        break;
    }

    hawk_config_save(&win->cfg);
    hawk_browser_apply_privacy(win);

    if (ctx->id == ROW_LOCAL && on && !win->backend_up)
        hawk_backend_start(win);
}

static void add_row(GtkGrid *grid, const gchar *label, GtkSwitch *sw,
                    int id, HawkWin *win, int *row)
{
    GtkWidget *l = gtk_label_new(label);
    gtk_widget_set_halign(l, GTK_ALIGN_START);
    gtk_widget_set_hexpand(l, TRUE);
    gtk_grid_attach(grid, l, 0, *row, 1, 1);

    gtk_widget_set_halign(GTK_WIDGET(sw), GTK_ALIGN_END);
    gtk_grid_attach(grid, GTK_WIDGET(sw), 1, *row, 1, 1);

    RowCtx *ctx = g_new0(RowCtx, 1);
    ctx->win = win;
    ctx->id = id;
    g_signal_connect_data(sw, "notify::active", G_CALLBACK(on_toggle), ctx,
                          (GClosureNotify)g_free, 0);
    (*row)++;
}

void hawk_settings_open(HawkWin *win)
{
    GtkWidget *d = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(d), "Hawk Settings");
    gtk_window_set_transient_for(GTK_WINDOW(d), GTK_WINDOW(win->win));
    gtk_window_set_modal(GTK_WINDOW(d), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(d), 440, -1);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_set_margin_top(box, 18);
    gtk_widget_set_margin_bottom(box, 18);
    gtk_widget_set_margin_start(box, 22);
    gtk_widget_set_margin_end(box, 22);
    gtk_window_set_child(GTK_WINDOW(d), box);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 14);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 18);
    gtk_box_append(GTK_BOX(box), grid);

    int row = 0;

    GtkWidget *sw_local = gtk_switch_new();
    gtk_switch_set_active(GTK_SWITCH(sw_local), win->cfg.local_search);
    add_row(GTK_GRID(grid), "Local Hawk search (SearXNG)", GTK_SWITCH(sw_local), ROW_LOCAL, win, &row);

    GtkWidget *sw_3p = gtk_switch_new();
    gtk_switch_set_active(GTK_SWITCH(sw_3p), win->cfg.block_3p);
    add_row(GTK_GRID(grid), "Block third-party cookies", GTK_SWITCH(sw_3p), ROW_BLOCK_3P, win, &row);

    GtkWidget *sw_ua = gtk_switch_new();
    gtk_switch_set_active(GTK_SWITCH(sw_ua), win->cfg.ua_lock);
    add_row(GTK_GRID(grid), "Lock user agent", GTK_SWITCH(sw_ua), ROW_UA_LOCK, win, &row);

    GtkWidget *sw_media = gtk_switch_new();
    gtk_switch_set_active(GTK_SWITCH(sw_media), win->cfg.block_media);
    add_row(GTK_GRID(grid), "Block camera & microphone", GTK_SWITCH(sw_media), ROW_BLOCK_MEDIA, win, &row);

    GtkWidget *sw_dark = gtk_switch_new();
    gtk_switch_set_active(GTK_SWITCH(sw_dark), win->cfg.dark);
    add_row(GTK_GRID(grid), "Dark mode", GTK_SWITCH(sw_dark), ROW_DARK, win, &row);

    GtkWidget *about = gtk_label_new(
        "Hawk " HAWK_VERSION " \xe2\x80\x94 lightweight C/WebKit browser.\n"
        "No telemetry. No logs. Cookies persist between sessions.\n"
        "When enabled, search runs through your local SearXNG.");
    gtk_label_set_xalign(GTK_LABEL(about), 0.0);
    gtk_label_set_wrap(GTK_LABEL(about), TRUE);
    gtk_box_append(GTK_BOX(box), about);

    GtkWidget *close = gtk_button_new_with_label("Close");
    gtk_widget_set_halign(close, GTK_ALIGN_END);
    g_signal_connect_swapped(close, "clicked", G_CALLBACK(gtk_window_destroy), d);
    gtk_box_append(GTK_BOX(box), close);

    gtk_window_present(GTK_WINDOW(d));
}