#!/bin/bash
# Adds the post-install script to an ipk where installers look for it:
# - beside control.tar.gz as pmPostInstall.script, executable, which the
#   webOS application installer runs (palm-install, App Catalog);
# - as postinst in control.tar.gz, as the 2.3.0 release had it (ipkg, Preware).
# palm-package puts it in neither place. Running it twice is harmless.
#   add-postinst.sh <ipk> <script>
set -e
ipk=$(readlink -f "$1")
script=$(readlink -f "$2")
work=$(mktemp -d)
cd "$work"
ar x "$ipk"
mkdir control
tar -xzf control.tar.gz -C control
tr -d '\r' < "$script" > control/postinst
chmod 755 control/postinst
tar -czf control.tar.gz -C control --owner=0 --group=0 ./control ./postinst
cp control/postinst pmPostInstall.script
rm -f "$ipk"
ar rc "$ipk" debian-binary control.tar.gz data.tar.gz pmPostInstall.script
cd /
rm -rf "$work"
