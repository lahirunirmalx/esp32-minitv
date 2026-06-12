/**
 * @file apps.cpp
 * @brief See apps.h.
 *
 * The `<<APP_*>>` markers below are used by scripts/new_app.py to register
 * generated apps automatically — keep them in place.
 */
#include "apps.h"
#include "app_registry.h"
#include <mooncake.h>
#include <memory>

#include "app_demo/app_demo.h"
#include "app_info/app_info.h"
#include "app_weather/app_weather.h"
#include "app_messages/app_messages.h"
// <<APP_INCLUDES>> (new_app.py inserts includes above this line)

namespace apps {

static void install(const char* name, std::unique_ptr<mooncake::AppAbility> app)
{
    const int id = mooncake::GetMooncake().installApp(std::move(app));
    app_registry::entries().push_back({name, id});
}

void install_all()
{
    install("demo", std::make_unique<AppDemo>());
    install("info", std::make_unique<AppInfo>());
    install("weather", std::make_unique<AppWeather>());
    install("messages", std::make_unique<AppMessages>());
    // <<APP_INSTALL>> (new_app.py inserts install lines above this line)
}

} // namespace apps
