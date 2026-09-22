#pragma once

#include <gtk/gtk.h>
#include <webkit/webkit.h>

#define HAWK_APP_ID  "org.hawk.Hawk"
#define HAWK_NAME    "Hawk"
#define HAWK_VERSION "0.1.0"

#define HAWK_BACKEND_HOST "127.0.0.1"
#define HAWK_BACKEND_PORT 8080
#define HAWK_BACKEND_URL  "http://127.0.0.1:8080"
#define HAWK_BACKEND_HOME HAWK_BACKEND_URL

typedef struct {
  gboolean local_search; 
  gchar   *home;         
  gboolean block_3p;     
  gboolean ua_lock;      
  gboolean block_media;  
  gboolean dark;         
} SearchConfig;

typedef struct HawkWin HawkWin;

typedef struct {
  HawkWin      *win;      
  WebKitWebView *view;     
  GtkWidget     *btn;      
  GtkWidget     *label;    
  gchar         *title;    
  gchar         *uri;      
} HawkTab;

struct HawkWin {
  GtkWidget      *win;
  GtkWidget      *root;       
  GtkWidget      *entry;
  GtkWidget      *btn_back;
  GtkWidget      *btn_fwd;
  GtkWidget      *btn_reload;
  GtkWidget      *btn_home;
  GtkWidget      *tabbar;     
  GtkWidget      *tabbox;     
  GPtrArray      *tabs;       
  int             active;     
  SearchConfig    cfg;
  GtkCssProvider *css;        
  gboolean        backend_up;
  gboolean        pending_home; 
  gchar          *start_home;   
  int             polls;        
  guint           poll_id;      
};

HawkWin *hawk_browser_new(GtkApplication *app);
void      hawk_browser_apply_privacy(HawkWin *win);
void      hawk_browser_load_home(HawkWin *win);
void      hawk_browser_search(HawkWin *win, const gchar *raw);

void     hawk_backend_start(HawkWin *win);
void     hawk_backend_stop(void);
gboolean hawk_backend_reachable(void);

void hawk_config_init(SearchConfig *cfg);
void hawk_config_load(SearchConfig *cfg);
void hawk_config_save(const SearchConfig *cfg);
void hawk_settings_open(HawkWin *win);