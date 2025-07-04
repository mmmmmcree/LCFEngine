#include "ScreenManager.h"

lcf::ScreenManager * lcf::ScreenManager::getInstance()
{
    static lcf::ScreenManager s_instance;
    return &s_instance;
}

QScreen *lcf::ScreenManager::getPrimaryScreen() const
{
    return QGuiApplication::primaryScreen();
}

QList<QScreen *> lcf::ScreenManager::getScreens() const
{
    return QGuiApplication::screens();
}
