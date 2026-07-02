// SettingsPage.xaml.h — settings page: watch/output folders + Save.
#pragma once

#include "SettingsPage.xaml.g.h"

namespace winrt::BackgroundRemover::implementation {

struct SettingsPage : SettingsPageT<SettingsPage> {
    SettingsPage();

    void SaveButton_Click(Windows::Foundation::IInspectable const&,
                          Microsoft::UI::Xaml::RoutedEventArgs const&);
};

} // namespace winrt::BackgroundRemover::implementation

namespace winrt::BackgroundRemover::factory_implementation {
struct SettingsPage : SettingsPageT<SettingsPage, implementation::SettingsPage> {};
} // namespace winrt::BackgroundRemover::factory_implementation
