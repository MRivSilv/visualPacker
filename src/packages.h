#ifndef VP_PACKAGES_H
#define VP_PACKAGES_H

#include <gio/gio.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define VP_TYPE_PKG (vp_pkg_get_type())
G_DECLARE_FINAL_TYPE(VpPkg, vp_pkg, VP, PKG, GObject)

VpPkg *vp_pkg_new(const char *name, const char *version, guint64 bytes);

const char *vp_pkg_get_name(VpPkg *self);
const char *vp_pkg_get_version(VpPkg *self);
const char *vp_pkg_get_size(VpPkg *self);

guint vp_packages_load(GListStore *store, char **error);

G_END_DECLS

#endif /* VP_PACKAGES_H */
