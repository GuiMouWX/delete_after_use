#include "statusbar.h"
#include <sysinfo/mac.h>
#include <log/liblog2.h> 
extern LibLogHandle LogHandle;

int showStatusBar() {
    int returnCode = vfimac::MAC_ERR_OK;
    returnCode = vfimac::sysStatusbar(true);
    if (returnCode == vfimac::MAC_ERR_OK) {
        LOGF_INFO(LogHandle, "show status bar");
    } else {
        LOGF_ERROR(LogHandle, "show status bar error = %d", returnCode);
    }
    return returnCode;
}

int hideStatusBar() {
    int returnCode = vfimac::MAC_ERR_OK;
    returnCode = vfimac::sysStatusbar(false);
    if (returnCode == vfimac::MAC_ERR_OK) {
        LOGF_INFO(LogHandle, "hide status bar");
    }
    else {
        LOGF_ERROR(LogHandle, "hide status bar error = %d", returnCode);
    }
    return returnCode;
}