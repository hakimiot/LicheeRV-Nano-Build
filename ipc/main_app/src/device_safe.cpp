#include <security/pam_appl.h>
#include <cstring>
#include <cstdlib>
#include "device_safe.h"

static int PamConversation(int numMsg, const struct pam_message** msg, struct pam_response** resp, void* appdataPtr)
{
    if (appdataPtr == nullptr) return PAM_AUTH_ERR;

    const char* password = static_cast<const char*>(appdataPtr);
    *resp = static_cast<pam_response*>(
        calloc(numMsg, sizeof(struct pam_response)));
    if (*resp == nullptr) return PAM_BUF_ERR;

    for (int i = 0; i < numMsg; ++i) {
        // 只处理密码提示（隐藏输入），其他消息忽略
        if (msg[i]->msg_style == PAM_PROMPT_ECHO_OFF) {
            (*resp)[i].resp = strdup(password);
        }
    }
    return PAM_SUCCESS;
}

bool DeviceSafe::Authenticate(const std::string& username, const std::string& password)
{
    struct pam_conv conv = { PamConversation, const_cast<char*>(password.c_str()) };
    pam_handle_t* pamh = nullptr;

    // 使用 "login" 服务，这是最常见的本地登录服务
    int retval = pam_start("login", username.c_str(), &conv, &pamh);
    if (retval != PAM_SUCCESS) return false;

    // 进行身份验证
    retval = pam_authenticate(pamh, PAM_SILENT | PAM_DISALLOW_NULL_AUTHTOK);
    if (retval != PAM_SUCCESS) {
        pam_end(pamh, retval);
        return false;
    }

    // 检查账户是否健康（未过期、未锁定等）
    retval = pam_acct_mgmt(pamh, PAM_SILENT | PAM_DISALLOW_NULL_AUTHTOK);
    pam_end(pamh, retval);

    return (retval == PAM_SUCCESS);
}
