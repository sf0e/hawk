#include "hawk.h"

#include <signal.h>
#include <unistd.h>

#define HAWK_PIDFILE "searchd.pid"

static gchar *find_searchd(void)
{
    const gchar *env = g_getenv("HAWK_SEARCHD");
    if (env && g_file_test(env, G_FILE_TEST_IS_EXECUTABLE))
        return g_strdup(env);

    gchar *bin = g_file_read_link("/proc/self/exe", NULL);
    gchar *bindir = bin ? g_path_get_dirname(bin) : NULL;
    g_free(bin);
    if (!bindir)
        return NULL;

    gchar *dev = g_build_filename(bindir, "..", "..", "scripts", "hawk-searchd", NULL);
    if (g_file_test(dev, G_FILE_TEST_IS_EXECUTABLE)) {
        g_free(bindir);
        return dev;
    }
    g_free(dev);

    gchar *beside = g_build_filename(bindir, "hawk-searchd", NULL);
    if (g_file_test(beside, G_FILE_TEST_IS_EXECUTABLE)) {
        g_free(bindir);
        return beside;
    }
    g_free(beside);

    gchar *cwd = g_build_filename(g_get_current_dir(), "scripts", "hawk-searchd", NULL);
    if (g_file_test(cwd, G_FILE_TEST_IS_EXECUTABLE)) {
        g_free(bindir);
        return cwd;
    }
    g_free(cwd);

    gchar *prefix = g_path_get_dirname(bindir);
    gchar *inst = g_build_filename(prefix, "libexec", "hawk", "hawk-searchd", NULL);
    if (g_file_test(inst, G_FILE_TEST_IS_EXECUTABLE)) {
        g_free(prefix);
        g_free(bindir);
        return inst;
    }
    g_free(inst);
    g_free(prefix);
    g_free(bindir);
    return NULL;
}

gboolean hawk_backend_reachable(void)
{
    GSocketClient *client = g_socket_client_new();
    g_socket_client_set_timeout(client, 300);
    GError *err = NULL;
    gboolean ok = FALSE;
    GSocketConnection *conn = g_socket_client_connect_to_host(
        client, HAWK_BACKEND_HOST, HAWK_BACKEND_PORT, NULL, &err);
    if (conn) {
        ok = TRUE;
        g_object_unref(conn);
    }
    if (err)
        g_clear_error(&err);
    g_object_unref(client);
    return ok;
}

void hawk_backend_start(HawkWin *self)
{
    (void)self;
    if (hawk_backend_reachable())
        return; 

    gchar *path = find_searchd();
    if (!path) {
        g_warning("hawk: could not find hawk-searchd script");
        return;
    }

    gchar *argv[] = { path, "start", NULL };
    GError *err = NULL;
    GPid pid;
    if (g_spawn_async(NULL, argv, NULL,
                      G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
                      NULL, NULL, &pid, &err)) {
        g_child_watch_add(pid, (GChildWatchFunc)g_spawn_close_pid, NULL);
    } else {
        g_warning("hawk: could not start search backend: %s", err->message);
        g_clear_error(&err);
    }
    g_free(path);
}

void hawk_backend_stop(void)
{
    gchar *path = find_searchd();
    if (path) {
        gchar *argv[] = { path, "stop", NULL };
        GError *err = NULL;
        GPid pid;
        if (g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH,
                          NULL, NULL, &pid, &err)) {
            g_child_watch_add(pid, (GChildWatchFunc)g_spawn_close_pid, NULL);
        } else {
            g_clear_error(&err);
        }
        g_free(path);
        return;
    }

    gchar *base = hawk_data_subdir(HAWK_SEARCHD_SUBDIR);
    gchar *pidfile = g_build_filename(base, HAWK_PIDFILE, NULL);

    gchar *contents = NULL;
    GError *err = NULL;
    if (g_file_get_contents(pidfile, &contents, NULL, &err)) {
        if (contents) {
            pid_t pid = (pid_t)g_ascii_strtoll(contents, NULL, 10);
            if (pid > 0)
                kill(pid, SIGTERM);
            g_free(contents);
        }
    } else {
        g_clear_error(&err);
    }
    
    unlink(pidfile);

    g_free(pidfile);
    g_free(base);
}