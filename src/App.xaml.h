// App.xaml.h — application class for BackgroundRemover.
#pragma once

#include "App.xaml.g.h"

#include "AppState.h"
#include <memory>

namespace winrt::BackgroundRemover::implementation {

struct App : AppT<App> {
    App();

    void OnLaunched(Microsoft::UI::Xaml::LaunchActivatedEventArgs const&);

private:
    winrt::Microsoft::UI::Xaml::Window m_window{nullptr};
    // Owns the app-wide state; pages access it via rmbg::AppState::Instance().
    std::unique_ptr<rmbg::AppState> m_appState;
};

} // namespace winrt::BackgroundRemover::implementation
