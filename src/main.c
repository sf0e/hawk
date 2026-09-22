#include "hawk.h"

#include <glib-unix.h>
#include <signal.h>

static HawkWin *g_win = NULL;
static GApplication *g_app = NULL;

static void on_activate(GtkApplication *app, gpointer user_data)
{
    (void)user_data;
    g_win = hawk_browser_new(app);
    gtk_window_present(GTK_WINDOW(g_win->win));
}

static gboolean on_term_signal(gpointer user_data)
{
    (void)user_data;
    if (g_win)
        hawk_session_save(g_win);
    hawk_backend_stop();
    if (g_app)
        g_application_quit(g_app);
    return G_SOURCE_REMOVE;
}

int main(int argc, char **argv)
{
    GtkApplication *app =
        gtk_application_new(HAWK_APP_ID, G_APPLICATION_DEFAULT_FLAGS);
    g_app = G_APPLICATION(app);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);

    g_unix_signal_add(SIGTERM, on_term_signal, NULL);
    g_unix_signal_add(SIGINT, on_term_signal, NULL);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}