SRC_URI += " \
        file://interfaces \
        "
# For 2021.2 and old releases
FILESEXTRAPATHS_prepend := "${THISDIR}/files:"
 
#For 2021.2 and old releases
do_install_append() {
     install -m 0644 ${WORKDIR}/interfaces ${D}${sysconfdir}/network/interfaces
}