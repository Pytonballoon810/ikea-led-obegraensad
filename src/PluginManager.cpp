// Module: Plugin lifecycle management: register, activate, cycle, and run plugins.
#include "PluginManager.h"
#include "scheduler.h"

Plugin::Plugin() : id(-1) {}

void Plugin::setId(int id)
{
    this->id = id;
}

int Plugin::getId() const
{
    return id;
}

void Plugin::teardown() {}
void Plugin::loop() {}
void Plugin::websocketHook(JsonDocument &request) {}

PluginManager::PluginManager() : activePlugin(nullptr), nextPluginId(1) {}

void PluginManager::init()
{
    Screen.clear();
    std::vector<Plugin *> &allPlugins = pluginManager.getAllPlugins();

    activatePersistedPlugin();
}

void PluginManager::activatePersistedPlugin()
{
    std::vector<Plugin *> &allPlugins = pluginManager.getAllPlugins();
    if (allPlugins.empty())
        return; // no plugins registered yet

#ifdef ENABLE_STORAGE
    storage.begin("led-wall", true);
    persistedPluginId = storage.getInt("current-plugin", allPlugins.at(0)->getId());
    pluginManager.setActivePluginById(persistedPluginId);
    storage.end();
#endif
    if (!activePlugin)
    {
        pluginManager.setActivePluginById(allPlugins.at(0)->getId());
    }
}

void PluginManager::persistActivePlugin()
{
#ifdef ENABLE_STORAGE
    storage.begin("led-wall", false);
    if (activePlugin)
    {
        persistedPluginId = activePlugin->getId();
        storage.putInt("current-plugin", persistedPluginId);
    }
    storage.end();
#endif
}

int PluginManager::getPersistedPluginId()
{
    // Return the cached in-memory value – opening NVS storage on every
    // WebSocket info broadcast would be both slow and wear the flash.
#ifdef ENABLE_STORAGE
    return persistedPluginId;
#else
    return -1;
#endif
}


int PluginManager::addPlugin(Plugin *plugin)
{

    plugin->setId(nextPluginId++);
    plugins.push_back(plugin);
    return plugin->getId();
}

void PluginManager::setActivePlugin(const char *pluginName)
{
    if (activePlugin)
    {
        activePlugin->teardown();
        // Do NOT busy-wait here – setActivePlugin can be called from an
        // async WebSocket callback on the TCP task, and a blocking delay()
        // stalls that task and can trigger the watchdog.
        activePlugin = nullptr;
    }

    for (Plugin *plugin : plugins)
    {
        if (strcmp(plugin->getName(), pluginName) == 0)
        {
            Screen.clear();
            activePlugin = plugin;
            activePlugin->setup();
            break;
        }
    }
}

void PluginManager::setActivePluginById(int pluginId)
{
    for (Plugin *plugin : plugins)
    {
        if (plugin->getId() == pluginId)
        {
            setActivePlugin(plugin->getName());
        }
    }
}

void PluginManager::setupActivePlugin()
{
    if (activePlugin)
    {
        activePlugin->setup();
    }
}

void PluginManager::runActivePlugin()
{
    if (Screen.isPoweredOff())
    {
        // Brightness 0 is treated as software power-off.
        return;
    }

    if (activePlugin && currentStatus != UPDATE &&
        currentStatus != LOADING && currentStatus != WSBINARY)
    {
        activePlugin->loop();
    }
}

Plugin *PluginManager::getActivePlugin() const
{
    return activePlugin;
}

std::vector<Plugin *> &PluginManager::getAllPlugins()
{
    return plugins;
}

size_t PluginManager::getNumPlugins()
{
    return plugins.size();
}

void PluginManager::activateNextPlugin()
{
    if (!activePlugin || plugins.empty())
    {
        if (!plugins.empty())
            setActivePluginById(plugins.front()->getId());
        return;
    }

    // Find the position of the active plugin in the list and advance by one,
    // wrapping around at the end.  Using the plugin list index (not the plugin
    // id) avoids the size_t underflow that occurred when getNumPlugins()-1 was
    // computed on an empty vector.
    size_t numPlugins = plugins.size();
    for (size_t i = 0; i < numPlugins; i++)
    {
        if (plugins[i] == activePlugin)
        {
            size_t next = (i + 1) % numPlugins;
            setActivePluginById(plugins[next]->getId());
            break;
        }
    }
#ifdef ENABLE_SERVER
    sendInfo();
#endif
}