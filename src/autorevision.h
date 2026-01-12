#ifndef AUTOREVISION_H
#define AUTOREVISION_H

/* SVN/Git revision information for Warzone 2100 v2.3 webOS port */

/* Revision numbers */
#define SVN_REV 0
#define SVN_LOW_REV 0

/* String versions of revision numbers */
#define SVN_REV_STR "0"
#define SVN_LOW_REV_STR "0"

/* Hash information */
#define SVN_SHORT_HASH "webos"

/* Working copy state */
#define SVN_WC_MODIFIED 0
#define SVN_WC_SWITCHED 0

/* URI/Branch information */
#define SVN_URI "tags/2.3"

/* Date information */
#define SVN_DATE "2011-01-01 00:00:00"

/* Static string variables (SVN_AUTOREVISION_STATIC is defined before include) */
#ifndef SVN_AUTOREVISION_STATIC
# define SVN_AUTOREVISION_STATIC
#endif

SVN_AUTOREVISION_STATIC const char svn_uri_cstr[] = "tags/2.3";
SVN_AUTOREVISION_STATIC const char svn_date_cstr[] = "2011-01-01 00:00:00";

#endif /* AUTOREVISION_H */
