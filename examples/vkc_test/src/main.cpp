#include "vk_core/context/Context.h"
#include "log.h"


using namespace lcf;

int main()
{
    log::init();
    vk::ApplicationInfo app_info;
    app_info.setPApplicationName("LCFEngine")
        .setPEngineName("LCFEngine")
        .setApplicationVersion(vk::makeVersion(1, 0, 0))
        .setEngineVersion(vk::makeVersion(1, 0, 0))
        .setApiVersion(vk::HeaderVersionComplete);
    vkc::ContextCreateInfo context_info;
    context_info.setApplicationInfo(app_info)
        .addRequiredInstanceLayer("VK_LAYER_KHRONOS_validation")
        .addRequiredInstanceExtension(vk::EXTDebugUtilsExtensionName);
    vkc::Context context;
    if (auto ec = context.create(context_info)) {
        lcf_log_error(ec.message());
    } else {
        lcf_log_info("Context created successfully");
    }
    
    return 0;
}