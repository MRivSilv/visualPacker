#include "packages.h"
#include <alpm.h>
#include <alpm_list.h>

#define PACMAN_ROOT   "/"
#define PACMAN_DBPATH "/var/lib/pacman/"

struct _VpPkg {
    GObject parent_instance;
    char *name;
    char *version;
    char *size;
};

G_DEFINE_FINAL_TYPE(VpPkg, vp_pkg, G_TYPE_OBJECT)

static void vp_pkg_finalize(GObject *object){
    VpPkg *self = VP_PKG(object);
    g_free(self->name);
    g_free(self->version);
    g_free(self->size);
    G_OBJECT_CLASS(vp_pkg_parent_class)->finalize(object);
}

static void vp_pkg_class_init(VpPkgClass *klass){
    G_OBJECT_CLASS(klass)->finalize = vp_pkg_finalize;
}

static void vp_pkg_init(VpPkg *self) {}

VpPkg *vp_pkg_new(const char *name, const char *version, guint64 bytes){
    VpPkg *self = g_object_new(VP_TYPE_PKG, NULL);
    /* Copy the strings: libalpm owns its originals and frees them when we
     * close the database, which happens before this window is ever drawn. */
    self->name = g_strdup(name ? name : "");
    self->version = g_strdup(version ? version : "");
    self->size = g_format_size_full(bytes, G_FORMAT_SIZE_IEC_UNITS);
    
    return self;
}

const char *vp_pkg_get_name(VpPkg *self){
    g_return_val_if_fail(VP_IS_PKG(self), "");

    return self->name;
}

const char *vp_pkg_get_version(VpPkg *self){
    g_return_val_if_fail(VP_IS_PKG(self), "");

    return self->version;
}

const char *vp_pkg_get_size(VpPkg *self){
    g_return_val_if_fail(VP_IS_PKG(self), "");

    return self->size;
}

guint vp_packages_load(GListStore *store, char **error){
    alpm_errno_t err = 0;
    alpm_handle_t *handle = alpm_initialize(PACMAN_ROOT, PACMAN_DBPATH, &err);
    if (!handle) {
        *error = g_strdup_printf("Could not open the pacman database.\n\n%s",
                                 alpm_strerror(err));
	
        return 0;
    }
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

    alpm_release(handle);

    return count;
}
