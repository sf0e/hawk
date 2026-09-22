#include "hawk.h"

#include <gdk/gdkkeysyms.h>
#include <string.h>

#define HAWK_UA "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/17.4 Safari/605.1.15 Hawk/0.1.0"

static void apply_css(HawkWin *self);
static void apply_view_privacy(WebKitWebView *view, const SearchConfig *cfg);
static void on_destroy(GtkWidget *win, HawkWin *self);
static void on_load_changed(WebKitWebView *view, WebKitLoadEvent event, HawkTab *tab);
static WebKitWebView *on_create(WebKitWebView *view, WebKitNavigationAction *action, HawkTab *tab);
static void on_entry_activate(GtkWidget *entry, HawkWin *self);
static void on_download_started(WebKitWebContext *ctx, WebKitDownload *dl, gpointer user_data);
static gboolean poll_backend(gpointer user_data);
static void refresh_nav(HawkWin *self);
static gboolean looks_like_url(const gchar *s);
static void load_url_or_search(HawkWin *self, const gchar *raw);
static HawkTab *tab_new(HawkWin *win, const gchar *url);
static void tab_close(HawkWin *win, int idx);
static void tab_activate(HawkWin *win, int idx);
static void on_tab_clicked(GtkWidget *btn, HawkWin *win);
static void on_close_pressed(GtkGestureClick *gesture, gint n_press, gdouble x, gdouble y, gpointer user_data);
static void on_close_released(GtkGestureClick *gesture, gint n_press, gdouble x, gdouble y, gpointer user_data);
static void on_plus_clicked(HawkWin *win);
static WebKitWebView *active_view(HawkWin *win);
static void refresh_tab_label(HawkTab *tab);
static void tab_update_uri(HawkTab *tab, const gchar *uri);
static void tab_update_title(HawkTab *tab, const gchar *title);
static const gchar *default_home(HawkWin *win);

static GtkWidget *icon_button(const gchar *icon_name)
{
    GtkWidget *btn = gtk_button_new();
    GtkWidget *img = gtk_image_new_from_icon_name(icon_name);
    gtk_button_set_child(GTK_BUTTON(btn), img);
    gtk_widget_add_css_class(btn, "flat");
    return btn;
}

static GtkWidget *logo_image(void)
{
    gchar *bin = g_file_read_link("/proc/self/exe", NULL);
    gchar *bindir = bin ? g_path_get_dirname(bin) : NULL;
    g_free(bin);
    if (!bindir)
        return NULL;

    gchar *path = g_build_filename(bindir, "..", "data", "hawk.png", NULL);
    if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
        g_free(path);
        path = g_build_filename(g_get_current_dir(), "data", "hawk.png", NULL);
    }
    if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
        g_free(path);
        gchar *prefix = g_path_get_dirname(bindir);
        path = g_build_filename(prefix, "share", "icons", "hicolor", "128x128",
                                "apps", "org.hawk.Hawk.png", NULL);
        g_free(prefix);
    }
    g_free(bindir);
    if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
        g_free(path);
        return NULL;
    }

    GtkWidget *img = gtk_image_new_from_file(path);
    g_free(path);
    if (!img)
        return NULL;
    gtk_image_set_pixel_size(GTK_IMAGE(img), 20);
    gtk_widget_add_css_class(img, "logo-mark");
    return img;
}

static void apply_css(HawkWin *self)
{
    self->css = gtk_css_provider_new();
    gtk_css_provider_load_from_string(self->css,
        ".omnibar {"
        "  border-radius: 999px;"      
        "  min-height: 40px;"
        "  padding: 0 16px;"
        "  box-shadow: none;"
        "}"
        ".toolbar { padding: 6px 8px 4px 8px; }"
        ".tabstrip { padding: 0 8px 6px 8px; }"
        ".tab {"
        "  border-radius: 12px;"       
        "  min-width: 56px;"
        "  min-height: 26px;"
        "  padding: 0 4px;"
        "}"
        ".tab.active {"
        "  background-color: @theme_selected_bg_color;"
        "  color: @theme_selected_fg_color;"
        "}"
        ".tab-close {"
        "  min-width: 18px;"
        "  min-height: 18px;"
        "  padding: 0;"
        "  border-radius: 50%;"
        "}"
        ".tab-x { font-size: 11px; }"
        ".tab-close:hover { background-color: alpha(@theme_fg_color, 0.15); }"
        ".tab-label { font-size: 12px; }"
        ".plus-btn {"
        "  border-radius: 50%;"
        "  min-width: 26px;"
        "  min-height: 26px;"
        "  padding: 0;"
        "}");
    gtk_style_context_add_provider_for_display(
        gtk_widget_get_display(self->win),
        GTK_STYLE_PROVIDER(self->css),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

static WebKitWebView *active_view(HawkWin *win)
{
    if (win->active < 0 || win->active >= (int)win->tabs->len)
        return NULL;
    return ((HawkTab *)g_ptr_array_index(win->tabs, win->active))->view;
}

static void refresh_nav(HawkWin *self)
{
    WebKitWebView *view = active_view(self);
    if (!view)
        return;
    gtk_widget_set_sensitive(self->btn_back, webkit_web_view_can_go_back(view));
    gtk_widget_set_sensitive(self->btn_fwd, webkit_web_view_can_go_forward(view));
}

static void refresh_tab_label(HawkTab *tab)
{
    const gchar *text = (tab->title && *tab->title) ? tab->title
                        : (tab->uri ? tab->uri : "New Tab");
    gtk_label_set_text(GTK_LABEL(tab->label), text);
}

static void tab_update_uri(HawkTab *tab, const gchar *uri)
{
    g_free(tab->uri);
    tab->uri = g_strdup(uri);
}

static void tab_update_title(HawkTab *tab, const gchar *title)
{
    g_free(tab->title);
    tab->title = g_strdup(title ? title : "");
    refresh_tab_label(tab);
}

static void on_load_changed(WebKitWebView *view, WebKitLoadEvent event, HawkTab *tab)
{
    HawkWin *win = tab->win;
    gboolean is_active =
        win->active >= 0 && g_ptr_array_index(win->tabs, win->active) == tab;

    switch (event) {
    case WEBKIT_LOAD_COMMITTED: {
        const gchar *uri = webkit_web_view_get_uri(view);
        tab_update_uri(tab, uri);
        refresh_tab_label(tab);
        if (is_active) {
            gtk_editable_set_text(GTK_EDITABLE(win->entry), uri ? uri : "");
            gtk_window_set_title(GTK_WINDOW(win->win), uri ? uri : HAWK_NAME);
            refresh_nav(win);
        }
        break;
    }
    case WEBKIT_LOAD_FINISHED: {
        const gchar *title = webkit_web_view_get_title(view);
        tab_update_title(tab, title);
        if (is_active) {
            gtk_window_set_title(GTK_WINDOW(win->win),
                                 (title && *title) ? title : HAWK_NAME);
            refresh_nav(win);
        }
        break;
    }
    default:
        break;
    }
}

static WebKitWebView *on_create(WebKitWebView *view, WebKitNavigationAction *action, HawkTab *tab)
{
    (void)view;
    const gchar *uri = NULL;
    if (action) {
        WebKitURIRequest *req = webkit_navigation_action_get_request(action);
        if (req)
            uri = webkit_uri_request_get_uri(req);
    }
    
    return tab_new(tab->win, uri)->view;
}

static gboolean looks_like_url(const gchar *s)
{
    if (g_str_has_prefix(s, "http://") || g_str_has_prefix(s, "https://") ||
        g_str_has_prefix(s, "file://") || g_str_has_prefix(s, "about:"))
        return TRUE;
    if (strstr(s, "://"))
        return TRUE;
    
    if (strchr(s, ' ') == NULL && strchr(s, '.') != NULL)
        return TRUE;
    return FALSE;
}

void hawk_browser_search(HawkWin *self, const gchar *raw)
{
    gchar *q = g_uri_escape_string(raw, NULL, FALSE);
    gchar *url;
    if (self->cfg.local_search && hawk_backend_reachable())
        url = g_strdup_printf(HAWK_BACKEND_URL "/search?q=%s", q);
    else
        url = g_strdup_printf("https://html.duckduckgo.com/html/?q=%s", q);
    webkit_web_view_load_uri(active_view(self), url);
    g_free(url);
    g_free(q);
}

static void load_url_or_search(HawkWin *self, const gchar *raw)
{
    gchar *trimmed = g_strstrip(g_strdup(raw));
    if (!*trimmed)
        goto out;

    if (looks_like_url(trimmed)) {
        gchar *url = trimmed;
        if (!g_str_has_prefix(url, "http://") && !g_str_has_prefix(url, "https://") &&
            !g_str_has_prefix(url, "file://") && !g_str_has_prefix(url, "about:")) {
            url = g_strconcat("https://", trimmed, NULL);
            g_free(trimmed);
        }
        webkit_web_view_load_uri(active_view(self), url);
        g_free(url);
    } else {
        hawk_browser_search(self, trimmed);
        g_free(trimmed);
    }
    trimmed = NULL;
out:
    if (trimmed)
        g_free(trimmed);
}

static void on_entry_activate(GtkWidget *entry, HawkWin *self)
{
    load_url_or_search(self, gtk_editable_get_text(GTK_EDITABLE(entry)));
}

void hawk_browser_load_home(HawkWin *self)
{
    if (self->cfg.local_search && hawk_backend_reachable())
        webkit_web_view_load_uri(active_view(self), HAWK_BACKEND_HOME);
    else
        webkit_web_view_load_uri(active_view(self), "https://duckduckgo.com/");
}

static const gchar *default_home(HawkWin *win)
{
    return (win->cfg.local_search && win->backend_up)
               ? HAWK_BACKEND_HOME
               : "https://duckduckgo.com/";
}

static gboolean poll_backend(gpointer user_data)
{
    HawkWin *self = user_data;
    if (hawk_backend_reachable()) {
        self->backend_up = TRUE;
        self->pending_home = FALSE;
        if (self->cfg.local_search) {
            HawkTab *tab = g_ptr_array_index(self->tabs, self->active);
            

            if (tab && (!tab->uri || g_str_has_prefix(tab->uri, "https://duckduckgo.com/")))
                webkit_web_view_load_uri(tab->view, HAWK_BACKEND_HOME);
        }
        return G_SOURCE_REMOVE;
    }
    if (++self->polls >= 100) { 
        self->pending_home = FALSE;
        hawk_browser_load_home(self);
        return G_SOURCE_REMOVE;
    }
    return G_SOURCE_CONTINUE;
}

static void on_download_started(WebKitWebContext *ctx, WebKitDownload *dl, gpointer user_data)
{
    (void)ctx;
    (void)user_data;
    const gchar *dl_dir = g_get_user_special_dir(G_USER_DIRECTORY_DOWNLOAD);
    const gchar *base = dl_dir ? dl_dir : g_get_home_dir();
    WebKitURIResponse *resp = webkit_download_get_response(dl);
    const gchar *fn = resp ? webkit_uri_response_get_suggested_filename(resp) : NULL;
    gchar *dest = g_build_filename(base, fn ? fn : "hawk-download", NULL);
    webkit_download_set_destination(dl, dest);
    g_free(dest);
}

static void tab_activate(HawkWin *win, int idx)
{
    if (idx < 0 || idx >= (int)win->tabs->len)
        return;
    win->active = idx;
    for (guint i = 0; i < win->tabs->len; i++) {
        HawkTab *t = g_ptr_array_index(win->tabs, i);
        gtk_widget_set_visible(GTK_WIDGET(t->view), i == (guint)idx);
        if (i == (guint)idx)
            gtk_widget_add_css_class(t->btn, "active");
        else
            gtk_widget_remove_css_class(t->btn, "active");
    }
    HawkTab *t = g_ptr_array_index(win->tabs, idx);
    gtk_editable_set_text(GTK_EDITABLE(win->entry), t->uri ? t->uri : "");
    gtk_window_set_title(GTK_WINDOW(win->win),
                         (t->title && *t->title) ? t->title : HAWK_NAME);
    refresh_nav(win);
}

static HawkTab *tab_new(HawkWin *win, const gchar *url)
{
    HawkTab *tab = g_new0(HawkTab, 1);
    tab->win = win;

    WebKitSettings *ws = webkit_settings_new();
    tab->view = WEBKIT_WEB_VIEW(webkit_web_view_new());
    webkit_web_view_set_settings(tab->view, ws);
    apply_view_privacy(tab->view, &win->cfg);

    
    GtkWidget *btn = gtk_button_new();
    gtk_widget_add_css_class(btn, "tab");
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    tab->label = gtk_label_new("New Tab");
    gtk_widget_add_css_class(tab->label, "tab-label");
    gtk_widget_add_css_class(tab->label, "dim-label");
    gtk_label_set_ellipsize(GTK_LABEL(tab->label), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(tab->label), 12);
    GtkWidget *close = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(close, "tab-close");
    GtkWidget *x = gtk_label_new("\342\234\225");
    gtk_widget_add_css_class(x, "tab-x");
    gtk_widget_set_halign(x, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(x, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(close), x);
    gtk_widget_set_cursor_from_name(close, "pointer");
    GtkGesture *gesture = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(gesture), 0);
    g_signal_connect(gesture, "pressed", G_CALLBACK(on_close_pressed), win);
    g_signal_connect(gesture, "released", G_CALLBACK(on_close_released), win);
    gtk_widget_add_controller(close, GTK_EVENT_CONTROLLER(gesture));
    gtk_box_append(GTK_BOX(row), tab->label);
    gtk_box_append(GTK_BOX(row), close);
    gtk_button_set_child(GTK_BUTTON(btn), row);

    g_signal_connect(btn, "clicked", G_CALLBACK(on_tab_clicked), win);
    g_object_set_data(G_OBJECT(close), "tab", tab);
    tab->btn = btn;

    g_ptr_array_add(win->tabs, tab);
    gtk_box_append(GTK_BOX(win->tabbox), btn);

    gtk_widget_set_hexpand(GTK_WIDGET(tab->view), TRUE);
    gtk_widget_set_vexpand(GTK_WIDGET(tab->view), TRUE);
    gtk_box_append(GTK_BOX(win->root), GTK_WIDGET(tab->view));

    g_signal_connect(tab->view, "load-changed", G_CALLBACK(on_load_changed), tab);
    g_signal_connect(tab->view, "create", G_CALLBACK(on_create), tab);

    
    GtkAdjustment *adj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(win->tabbar));
    gtk_adjustment_set_value(adj, gtk_adjustment_get_upper(adj));

    tab_activate(win, (int)win->tabs->len - 1);
    webkit_web_view_load_uri(tab->view, url ? url : default_home(win));
    return tab;
}

static void tab_close(HawkWin *win, int idx)
{
    if (idx < 0 || idx >= (int)win->tabs->len || win->tabs->len <= 1)
        return; 
    HawkTab *tab = g_ptr_array_index(win->tabs, idx);
    gboolean was_active = (idx == win->active);

    g_ptr_array_remove_index(win->tabs, idx);
    gtk_box_remove(GTK_BOX(win->tabbox), tab->btn);
    gtk_box_remove(GTK_BOX(win->root), GTK_WIDGET(tab->view));

    if (was_active) {
        int next = idx < (int)win->tabs->len ? idx : idx - 1;
        tab_activate(win, next);
    } else if (idx < win->active) {
        win->active--;
    }

    g_free(tab->title);
    g_free(tab->uri);
    g_free(tab);
}

static void on_tab_clicked(GtkWidget *btn, HawkWin *win)
{
    for (guint i = 0; i < win->tabs->len; i++) {
        if (((HawkTab *)g_ptr_array_index(win->tabs, i))->btn == btn) {
            tab_activate(win, (int)i);
            return;
        }
    }
}

static void on_close_pressed(GtkGestureClick *gesture, gint n_press,
                             gdouble x, gdouble y, gpointer user_data)
{
    (void)n_press;
    (void)x;
    (void)y;
    (void)user_data;
    gtk_gesture_set_state(GTK_GESTURE(gesture), GTK_EVENT_SEQUENCE_CLAIMED);
}

static void on_close_released(GtkGestureClick *gesture, gint n_press,
                              gdouble x, gdouble y, gpointer user_data)
{
    (void)n_press;
    (void)x;
    (void)y;
    GtkWidget *close = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(gesture));
    HawkTab *tab = g_object_get_data(G_OBJECT(close), "tab");
    HawkWin *win = user_data;
    for (guint i = 0; i < win->tabs->len; i++) {
        if (g_ptr_array_index(win->tabs, i) == tab) {
            tab_close(win, (int)i);
            return;
        }
    }
}

static void on_plus_clicked(HawkWin *win)
{
    tab_new(win, NULL);
}

static void on_back_clicked(HawkWin *win)     { webkit_web_view_go_back(active_view(win)); }
static void on_fwd_clicked(HawkWin *win)      { webkit_web_view_go_forward(active_view(win)); }
static void on_reload_clicked(HawkWin *win)   { webkit_web_view_reload(active_view(win)); }

static gboolean on_key_pressed(GtkEventControllerKey *ctrl, guint keyval,
                               guint keycode, GdkModifierType state, HawkWin *win)
{
    (void)ctrl;
    (void)keycode;
    const guint ctrl_down = state & GDK_CONTROL_MASK;
    const guint alt_down  = state & GDK_ALT_MASK;
    const guint super_down = state & GDK_SUPER_MASK;

    if (super_down)
        return GDK_EVENT_PROPAGATE;

    if (ctrl_down && !alt_down) {
        switch (keyval) {
        case GDK_KEY_t:
            tab_new(win, NULL);
            return GDK_EVENT_STOP;
        case GDK_KEY_w:            
            tab_close(win, win->active);
            return GDK_EVENT_STOP;
        case GDK_KEY_l:
            gtk_widget_grab_focus(win->entry);
            gtk_editable_select_region(GTK_EDITABLE(win->entry), 0, -1);
            return GDK_EVENT_STOP;
        case GDK_KEY_r:
            webkit_web_view_reload(active_view(win));
            return GDK_EVENT_STOP;
        default:
            return GDK_EVENT_PROPAGATE;
        }
    }

    if (alt_down && !ctrl_down) {
        switch (keyval) {
        case GDK_KEY_w:            
            tab_close(win, win->active);
            return GDK_EVENT_STOP;
        default:
            return GDK_EVENT_PROPAGATE;
        }
    }

    return GDK_EVENT_PROPAGATE;
}

static void on_destroy(GtkWidget *win, HawkWin *self)
{
    (void)win;
    if (self->poll_id)
        g_source_remove(self->poll_id);
    for (guint i = 0; i < self->tabs->len; i++) {
        HawkTab *t = g_ptr_array_index(self->tabs, i);
        g_free(t->title);
        g_free(t->uri);
        g_free(t);
    }
    g_ptr_array_free(self->tabs, TRUE);
    g_object_unref(self->css);
    hawk_backend_stop(); 
    g_free(self->cfg.home);
    g_free(self->start_home);
    g_free(self);
}

HawkWin *hawk_browser_new(GtkApplication *app)
{
    HawkWin *self = g_new0(HawkWin, 1);
    self->active = -1;

    hawk_config_load(&self->cfg);
    self->backend_up = hawk_backend_reachable();
    self->tabs = g_ptr_array_new();

    self->win = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(self->win), HAWK_NAME);
    gtk_window_set_icon_name(GTK_WINDOW(self->win), HAWK_APP_ID);
    gtk_window_set_default_size(GTK_WINDOW(self->win), 1200, 800);
    apply_css(self);

    self->root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(self->win), self->root);

    
    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_add_css_class(toolbar, "toolbar");
    gtk_box_append(GTK_BOX(self->root), toolbar);

    GtkWidget *logo = logo_image();
    if (logo)
        gtk_box_append(GTK_BOX(toolbar), logo);

    self->btn_back = icon_button("go-previous-symbolic");
    self->btn_fwd = icon_button("go-next-symbolic");
    self->btn_reload = icon_button("view-refresh-symbolic");
    self->btn_home = icon_button("go-home-symbolic");
    GtkWidget *btn_settings = icon_button("preferences-system-symbolic");

    gtk_box_append(GTK_BOX(toolbar), self->btn_back);
    gtk_box_append(GTK_BOX(toolbar), self->btn_fwd);
    gtk_box_append(GTK_BOX(toolbar), self->btn_reload);

    self->entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(self->entry), "Search or enter an address");
    gtk_widget_add_css_class(self->entry, "omnibar");
    gtk_widget_set_hexpand(self->entry, TRUE);
    gtk_box_append(GTK_BOX(toolbar), self->entry);

    gtk_box_append(GTK_BOX(toolbar), self->btn_home);
    gtk_box_append(GTK_BOX(toolbar), btn_settings);

    g_signal_connect_swapped(self->btn_back, "clicked", G_CALLBACK(on_back_clicked), self);
    g_signal_connect_swapped(self->btn_fwd, "clicked", G_CALLBACK(on_fwd_clicked), self);
    g_signal_connect_swapped(self->btn_reload, "clicked", G_CALLBACK(on_reload_clicked), self);
    g_signal_connect_swapped(self->btn_home, "clicked", G_CALLBACK(hawk_browser_load_home), self);
    g_signal_connect_swapped(btn_settings, "clicked", G_CALLBACK(hawk_settings_open), self);
    g_signal_connect(self->entry, "activate", G_CALLBACK(on_entry_activate), self);

    
    GtkWidget *strip = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_add_css_class(strip, "tabstrip");
    gtk_box_append(GTK_BOX(self->root), strip);

    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
    gtk_widget_set_hexpand(scroll, TRUE);
    GtkWidget *viewport = gtk_viewport_new(NULL, NULL);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), viewport);

    
    self->tabbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_set_homogeneous(GTK_BOX(self->tabbox), TRUE);
    gtk_viewport_set_child(GTK_VIEWPORT(viewport), self->tabbox);
    gtk_box_append(GTK_BOX(strip), scroll);
    self->tabbar = scroll;

    GtkWidget *plus = gtk_button_new_with_label("+");
    gtk_widget_add_css_class(plus, "plus-btn");
    g_signal_connect_swapped(plus, "clicked", G_CALLBACK(on_plus_clicked), self);
    gtk_box_append(GTK_BOX(strip), plus);

    g_signal_connect(self->win, "destroy", G_CALLBACK(on_destroy), self);

    
    GtkEventController *keys = gtk_event_controller_key_new();
    gtk_widget_add_controller(self->win, keys);
    g_signal_connect(keys, "key-pressed", G_CALLBACK(on_key_pressed), self);

    WebKitNetworkSession *net = webkit_network_session_get_default();
    g_signal_connect(net, "download-started", G_CALLBACK(on_download_started), self);

    hawk_browser_apply_privacy(self);

    
    tab_new(self, default_home(self));

    if (self->cfg.local_search && !self->backend_up) {
        
        self->pending_home = TRUE;
        self->polls = 0;
        hawk_backend_start(self);
        self->poll_id = g_timeout_add(400, poll_backend, self);
    }

    return self;
}

static void apply_color_scheme(HawkWin *self)
{
    GtkInterfaceColorScheme scheme =
        self->cfg.dark ? GTK_INTERFACE_COLOR_SCHEME_DARK
                       : GTK_INTERFACE_COLOR_SCHEME_DEFAULT;

    

    g_object_set(gtk_settings_get_default(),
                 "gtk-interface-color-scheme", scheme, NULL);
    g_object_set(self->css,
                 "prefers-color-scheme", scheme, NULL);
}

static void apply_view_privacy(WebKitWebView *view, const SearchConfig *cfg)
{
    WebKitSettings *s = webkit_web_view_get_settings(view);

    
    webkit_settings_set_user_agent(s, cfg->ua_lock ? HAWK_UA : NULL);

    
    webkit_settings_set_enable_media_stream(s, !cfg->block_media);
}

void hawk_browser_apply_privacy(HawkWin *self)
{
    for (guint i = 0; i < self->tabs->len; i++)
        apply_view_privacy(((HawkTab *)g_ptr_array_index(self->tabs, i))->view, &self->cfg);

    
    WebKitNetworkSession *session = webkit_network_session_get_default();
    WebKitCookieManager *cookies = webkit_network_session_get_cookie_manager(session);
    webkit_cookie_manager_set_accept_policy(
        cookies,
        self->cfg.block_3p ? WEBKIT_COOKIE_POLICY_ACCEPT_NO_THIRD_PARTY
                           : WEBKIT_COOKIE_POLICY_ACCEPT_ALWAYS);

    apply_color_scheme(self);
}