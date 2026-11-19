#include "gui.h"
#include "packages.h"

static void setup_label(GtkListItemFactory *factory, GtkListItem *item, gpointer user_data){
    GtkWidget *label = gtk_label_new(NULL);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_widget_set_margin_end(label, 10);
    gtk_list_item_set_child(item, label);
}

static void setup_label_right(GtkListItemFactory *factory, GtkListItem *item, gpointer user_data){
    setup_label(factory, item, user_data);
    gtk_label_set_xalign(GTK_LABEL(gtk_list_item_get_child(item)), 1.0f);
}

static void bind_name(GtkListItemFactory *factory, GtkListItem *item, gpointer user_data){
    VpPkg *pkg = gtk_list_item_get_item(item);
    gtk_label_set_text(GTK_LABEL(gtk_list_item_get_child(item)),
                       vp_pkg_get_name(pkg));
}

static void bind_version(GtkListItemFactory *factory, GtkListItem *item, gpointer user_data){
    VpPkg *pkg = gtk_list_item_get_item(item);
    gtk_label_set_text(GTK_LABEL(gtk_list_item_get_child(item)),
                       vp_pkg_get_version(pkg));
}

static void bind_size(GtkListItemFactory *factory, GtkListItem *item, gpointer user_data){
    VpPkg *pkg = gtk_list_item_get_item(item);
    gtk_label_set_text(GTK_LABEL(gtk_list_item_get_child(item)),
                       vp_pkg_get_size(pkg));
}

static void add_column(GtkColumnView *view, const char *title, GCallback setup, GCallback bind, gboolean expand, int width){
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

static GtkWidget *message_page(const char *title, const char *detail){
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

void vp_gui_activate(GtkApplication *app, gpointer user_data){
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "VisualPacker");
    gtk_window_set_default_size(GTK_WINDOW(window), 760, 720);

    GListStore *store = g_list_store_new(VP_TYPE_PKG);
    char *error = NULL;
    guint count = vp_packages_load(store, &error);

    if (error) {
        gtk_window_set_child(GTK_WINDOW(window),
                             message_page("Could not read your packages", error));
        g_free(error);
        g_object_unref(store);
        gtk_window_present(GTK_WINDOW(window));
        return;
    }

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
