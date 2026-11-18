/* VisualPacker — a window listing every package installed on this machine.
 *
 * This is the humble starting point: read the package database, show it.
 * No dependency tree, no buttons, nothing that can modify your system.
 *
 * Two libraries do the work:
 *   libalpm  the C library pacman itself is built from, so we read the real
 *            database directly instead of running pacman and parsing text
 *   GTK4     the window, in plain C, as a normal desktop app
 */
#include <alpm.h>
#include <alpm_list.h>
#include <gtk/gtk.h>

#define PACMAN_ROOT   "/"
#define PACMAN_DBPATH "/var/lib/pacman/"

/* ------------------------------------------------------------------ *
 * One row in the list.
 *
 * GTK's list widgets show a list of GObjects, so each package becomes a
 * small object. The size is formatted once, here, rather than every time
 * the row is drawn -- scrolling should not do arithmetic.
 * ------------------------------------------------------------------ */

#define VP_TYPE_PKG (vp_pkg_get_type())
G_DECLARE_FINAL_TYPE(VpPkg, vp_pkg, VP, PKG, GObject)

struct _VpPkg {
    GObject parent_instance;
    char *name;
    char *version;
    char *size;
};

G_DEFINE_FINAL_TYPE(VpPkg, vp_pkg, G_TYPE_OBJECT)

static void vp_pkg_finalize(GObject *object)
{
    VpPkg *self = VP_PKG(object);
    g_free(self->name);
    g_free(self->version);
    g_free(self->size);
    G_OBJECT_CLASS(vp_pkg_parent_class)->finalize(object);
}

static void vp_pkg_class_init(VpPkgClass *klass)
{
    G_OBJECT_CLASS(klass)->finalize = vp_pkg_finalize;
}

static void vp_pkg_init(VpPkg *self) {}

static VpPkg *vp_pkg_new(const char *name, const char *version, guint64 bytes)
{
    VpPkg *self = g_object_new(VP_TYPE_PKG, NULL);
    /* Copy the strings: libalpm owns its originals and frees them when we
     * close the database, which happens before this window is ever drawn. */
    self->name = g_strdup(name ? name : "");
    self->version = g_strdup(version ? version : "");
    self->size = g_format_size_full(bytes, G_FORMAT_SIZE_IEC_UNITS);
    return self;
}

/* ------------------------------------------------------------------ *
 * Reading the package database
 * ------------------------------------------------------------------ */

/* Fills `store` with every installed package. Returns the count, or 0 and
 * sets *error (which the caller must free) if the database cannot be read. */
static guint load_packages(GListStore *store, char **error)
{
    alpm_errno_t err = 0;
    alpm_handle_t *handle = alpm_initialize(PACMAN_ROOT, PACMAN_DBPATH, &err);
    if (!handle) {
        *error = g_strdup_printf("Could not open the pacman database.\n\n%s",
                                 alpm_strerror(err));
        return 0;
    }

    /* The "local" database is the set of installed packages. Asking for its
     * cache hands us the whole list, already sorted by name. */
    alpm_db_t *local = alpm_get_localdb(handle);
    alpm_list_t *packages = local ? alpm_db_get_pkgcache(local) : NULL;
    if (!packages) {
        *error = g_strdup("The local package database is empty or unreadable.");
        alpm_release(handle);
        return 0;
    }

    guint count = 0;
    for (alpm_list_t *i = packages; i; i = alpm_list_next(i)) {
        alpm_pkg_t *pkg = i->data;
        off_t size = alpm_pkg_get_isize(pkg);

        VpPkg *row = vp_pkg_new(alpm_pkg_get_name(pkg),
                                alpm_pkg_get_version(pkg),
                                size > 0 ? (guint64)size : 0);
        g_list_store_append(store, row);
        g_object_unref(row);        /* the store holds the only reference now */
        count++;
    }

    /* Everything we keep was copied above, so the database can be closed
     * immediately -- no file handles or caches stay open while you browse. */
    alpm_release(handle);
    return count;
}

/* ------------------------------------------------------------------ *
 * The list widget
 *
 * GTK recycles a small pool of row widgets as you scroll: `setup` builds a
 * blank one, `bind` points it at whichever package is currently visible.
 * That is why a list of thousands of packages scrolls without allocating.
 * ------------------------------------------------------------------ */

static void setup_label(GtkListItemFactory *factory, GtkListItem *item,
                        gpointer user_data)
{
    GtkWidget *label = gtk_label_new(NULL);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_widget_set_margin_end(label, 10);
    gtk_list_item_set_child(item, label);
}

static void setup_label_right(GtkListItemFactory *factory, GtkListItem *item,
                              gpointer user_data)
{
    setup_label(factory, item, user_data);
    gtk_label_set_xalign(GTK_LABEL(gtk_list_item_get_child(item)), 1.0f);
}

static void bind_name(GtkListItemFactory *factory, GtkListItem *item,
                      gpointer user_data)
{
    VpPkg *pkg = gtk_list_item_get_item(item);
    gtk_label_set_text(GTK_LABEL(gtk_list_item_get_child(item)), pkg->name);
}

static void bind_version(GtkListItemFactory *factory, GtkListItem *item,
                         gpointer user_data)
{
    VpPkg *pkg = gtk_list_item_get_item(item);
    gtk_label_set_text(GTK_LABEL(gtk_list_item_get_child(item)), pkg->version);
}

static void bind_size(GtkListItemFactory *factory, GtkListItem *item,
                      gpointer user_data)
{
    VpPkg *pkg = gtk_list_item_get_item(item);
    gtk_label_set_text(GTK_LABEL(gtk_list_item_get_child(item)), pkg->size);
}

static void add_column(GtkColumnView *view, const char *title,
                       GCallback setup, GCallback bind,
                       gboolean expand, int width)
{
    GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
    g_signal_connect(factory, "setup", setup, NULL);
    g_signal_connect(factory, "bind", bind, NULL);

    GtkColumnViewColumn *column = gtk_column_view_column_new(title, factory);
    gtk_column_view_column_set_expand(column, expand);
    if (width > 0)
        gtk_column_view_column_set_fixed_width(column, width);
    gtk_column_view_append_column(view, column);
    g_object_unref(column);
}

/* ------------------------------------------------------------------ *
 * The window
 * ------------------------------------------------------------------ */

static GtkWidget *message_page(const char *title, const char *detail)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(box, 32);
    gtk_widget_set_margin_end(box, 32);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);

    GtkWidget *heading = gtk_label_new(title);
    gtk_widget_add_css_class(heading, "title-2");
    gtk_label_set_wrap(GTK_LABEL(heading), TRUE);

    GtkWidget *body = gtk_label_new(detail);
    gtk_label_set_wrap(GTK_LABEL(body), TRUE);
    gtk_label_set_justify(GTK_LABEL(body), GTK_JUSTIFY_CENTER);
    gtk_label_set_selectable(GTK_LABEL(body), TRUE);

    gtk_box_append(GTK_BOX(box), heading);
    gtk_box_append(GTK_BOX(box), body);
    return box;
}

static void activate(GtkApplication *app, gpointer user_data)
{
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "VisualPacker");
    gtk_window_set_default_size(GTK_WINDOW(window), 760, 720);

    GListStore *store = g_list_store_new(VP_TYPE_PKG);
    char *error = NULL;
    guint count = load_packages(store, &error);

    if (error) {
        gtk_window_set_child(GTK_WINDOW(window),
                             message_page("Could not read your packages", error));
        g_free(error);
        g_object_unref(store);
        gtk_window_present(GTK_WINDOW(window));
        return;
    }

    /* No selection model behaviour is needed yet, but GtkColumnView requires
     * one; NoSelection simply means rows are not selectable for now. */
    GtkNoSelection *selection = gtk_no_selection_new(G_LIST_MODEL(store));
    GtkWidget *view = gtk_column_view_new(GTK_SELECTION_MODEL(selection));
    gtk_column_view_set_show_row_separators(GTK_COLUMN_VIEW(view), TRUE);

    add_column(GTK_COLUMN_VIEW(view), "Package",
               G_CALLBACK(setup_label), G_CALLBACK(bind_name), TRUE, 0);
    add_column(GTK_COLUMN_VIEW(view), "Version",
               G_CALLBACK(setup_label), G_CALLBACK(bind_version), FALSE, 190);
    add_column(GTK_COLUMN_VIEW(view), "Size",
               G_CALLBACK(setup_label_right), G_CALLBACK(bind_size), FALSE, 110);

    GtkWidget *scroller = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), view);
    gtk_widget_set_vexpand(scroller, TRUE);

    char *summary = g_strdup_printf("%u packages installed", count);
    GtkWidget *status = gtk_label_new(summary);
    g_free(summary);
    gtk_label_set_xalign(GTK_LABEL(status), 0.0f);
    gtk_widget_add_css_class(status, "dim-label");
    gtk_widget_set_margin_start(status, 12);
    gtk_widget_set_margin_top(status, 6);
    gtk_widget_set_margin_bottom(status, 6);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(box), scroller);
    gtk_box_append(GTK_BOX(box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(box), status);

    gtk_window_set_child(GTK_WINDOW(window), box);
    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv)
{
    GtkApplication *app = gtk_application_new("org.visualpacker.VisualPacker",
                                              G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
