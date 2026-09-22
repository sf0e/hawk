#include "hawk.h"

static void on_activate(GtkApplication *app, gpointer user_data)
{
    (void)user_data;
    HawkWin *win = hawk_browser_new(app);
    gtk_window_present(GTK_WINDOW(win->win));
}

int main(int argc, char **argv)
{
    GtkApplication *app =
        gtk_application_new(HAWK_APP_ID, G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}