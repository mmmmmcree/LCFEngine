#pragma once

#include <QGuiApplication>
#include <QScreen>

namespace lcf {
    class ScreenManager
    {
    public:
        ScreenManager() = default;
        static ScreenManager * getInstance();
        QScreen * getPrimaryScreen() const;
        QList<QScreen *> getScreens() const;
    private:
    };
}