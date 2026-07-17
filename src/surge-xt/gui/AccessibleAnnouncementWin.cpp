/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2024, various authors, as described in the VCS
 * history and in the readme file in the root of this repository.
 *
 * Surge XT is released under the GNU General Public Licence v3
 * or later (GPL-3.0-or-later). The license is found in the "LICENSE"
 * file in the root of this repository, or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 * Surge was a commercial product from 2004-2018, copyright and ownership
 * held by Claes Johanson at Vember Audio during that period.
 * Claes made Surge open source in September 2018.
 *
 * All source for Surge XT is available at
 * https://github.com/surge-synthesizer/surge
 */

#include "AccessibleAnnouncementWin.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <uiautomation.h>

#include <atomic>

namespace Surge
{
namespace GUI
{

namespace
{
typedef LRESULT(WINAPI *UiaReturnRawElementProviderFn)(HWND, WPARAM, LPARAM,
                                                       IRawElementProviderSimple *);
typedef HRESULT(WINAPI *UiaRaiseNotificationEventFn)(IRawElementProviderSimple *, NotificationKind,
                                                     NotificationProcessing, BSTR, BSTR);
typedef HRESULT(WINAPI *UiaHostProviderFromHwndFn)(HWND, IRawElementProviderSimple **);
typedef BOOL(WINAPI *UiaClientsAreListeningFn)();
typedef HRESULT(WINAPI *UiaDisconnectProviderFn)(IRawElementProviderSimple *);

// UiaRaiseNotificationEvent only exists on Windows 10 1709 and later, so
// resolve everything at runtime rather than linking UIAutomationCore.lib
struct UiaApi
{
    UiaReturnRawElementProviderFn returnRawElementProvider{nullptr};
    UiaRaiseNotificationEventFn raiseNotificationEvent{nullptr};
    UiaHostProviderFromHwndFn hostProviderFromHwnd{nullptr};
    UiaClientsAreListeningFn clientsAreListening{nullptr};
    UiaDisconnectProviderFn disconnectProvider{nullptr};

    bool usable() const
    {
        return returnRawElementProvider && raiseNotificationEvent && hostProviderFromHwnd &&
               clientsAreListening;
    }

    static const UiaApi &get()
    {
        static const UiaApi api = [] {
            UiaApi res;

            if (auto mod = LoadLibraryW(L"UIAutomationCore.dll"))
            {
                res.returnRawElementProvider = reinterpret_cast<UiaReturnRawElementProviderFn>(
                    GetProcAddress(mod, "UiaReturnRawElementProvider"));
                res.raiseNotificationEvent = reinterpret_cast<UiaRaiseNotificationEventFn>(
                    GetProcAddress(mod, "UiaRaiseNotificationEvent"));
                res.hostProviderFromHwnd = reinterpret_cast<UiaHostProviderFromHwndFn>(
                    GetProcAddress(mod, "UiaHostProviderFromHwnd"));
                res.clientsAreListening = reinterpret_cast<UiaClientsAreListeningFn>(
                    GetProcAddress(mod, "UiaClientsAreListening"));
                res.disconnectProvider = reinterpret_cast<UiaDisconnectProviderFn>(
                    GetProcAddress(mod, "UiaDisconnectProvider"));
            }

            return res;
        }();

        return api;
    }
};

// A do-nothing UIA element which exists only so notification events have a
// source. It reports itself as neither a control nor a content element, so
// screen readers never surface it in object or keyboard navigation.
class AnnouncementProvider final : public IRawElementProviderSimple
{
  public:
    explicit AnnouncementProvider(HWND h) : hwnd(h) {}

    void detachFromWindow() { hwnd = nullptr; }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) override
    {
        if (!ppv)
            return E_POINTER;

        if (riid == __uuidof(IUnknown) || riid == __uuidof(IRawElementProviderSimple))
        {
            *ppv = static_cast<IRawElementProviderSimple *>(this);
            AddRef();
            return S_OK;
        }

        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override { return ++refCount; }

    ULONG STDMETHODCALLTYPE Release() override
    {
        auto rc = --refCount;

        if (rc == 0)
            delete this;

        return rc;
    }

    HRESULT STDMETHODCALLTYPE get_ProviderOptions(ProviderOptions *pRetVal) override
    {
        *pRetVal = ProviderOptions_ServerSideProvider;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetPatternProvider(PATTERNID, IUnknown **pRetVal) override
    {
        *pRetVal = nullptr;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetPropertyValue(PROPERTYID propertyId, VARIANT *pRetVal) override
    {
        VariantInit(pRetVal);

        switch (propertyId)
        {
        case UIA_NamePropertyId:
            pRetVal->vt = VT_BSTR;
            pRetVal->bstrVal = SysAllocString(L"Surge XT announcements");
            break;
        case UIA_ControlTypePropertyId:
            pRetVal->vt = VT_I4;
            pRetVal->lVal = UIA_CustomControlTypeId;
            break;
        case UIA_IsControlElementPropertyId:
        case UIA_IsContentElementPropertyId:
        case UIA_IsKeyboardFocusablePropertyId:
            pRetVal->vt = VT_BOOL;
            pRetVal->boolVal = VARIANT_FALSE;
            break;
        default:
            break;
        }

        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE
    get_HostRawElementProvider(IRawElementProviderSimple **pRetVal) override
    {
        *pRetVal = nullptr;

        if (hwnd)
            return UiaApi::get().hostProviderFromHwnd(hwnd, pRetVal);

        return S_OK;
    }

  private:
    ~AnnouncementProvider() = default;

    std::atomic<ULONG> refCount{1};
    HWND hwnd;
};

constexpr wchar_t notifyWindowClass[] = L"SurgeXTUiaNotifyWindow";

HINSTANCE thisModule()
{
    HMODULE mod = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&thisModule), &mod);
    return mod;
}

LRESULT CALLBACK notifyWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_GETOBJECT && static_cast<LONG>(lParam) == UiaRootObjectId)
    {
        auto *provider =
            reinterpret_cast<AnnouncementProvider *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

        if (provider)
            return UiaApi::get().returnRawElementProvider(hwnd, wParam, lParam, provider);
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

bool registerNotifyWindowClass()
{
    static const bool registered = [] {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = notifyWndProc;
        wc.hInstance = thisModule();
        wc.lpszClassName = notifyWindowClass;

        return RegisterClassExW(&wc) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
    }();

    return registered;
}
} // namespace

struct UiaAnnouncer::Impl
{
    HWND notifyWindow{nullptr};
    AnnouncementProvider *provider{nullptr};
};

UiaAnnouncer::UiaAnnouncer(void *parentWindowHandle) : impl(std::make_unique<Impl>())
{
    auto parent = static_cast<HWND>(parentWindowHandle);

    if (!UiaApi::get().usable() || !parent || !registerNotifyWindowClass())
        return;

    // Hidden (no WS_VISIBLE), unfocusable child; OSARA found Narrator
    // misbehaves if the notification window is visible
    impl->notifyWindow = CreateWindowExW(0, notifyWindowClass, L"Surge XT announcements", WS_CHILD,
                                         0, 0, 1, 1, parent, nullptr, thisModule(), nullptr);

    if (!impl->notifyWindow)
        return;

    impl->provider = new AnnouncementProvider(impl->notifyWindow);
    SetWindowLongPtrW(impl->notifyWindow, GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(impl->provider));
}

UiaAnnouncer::~UiaAnnouncer()
{
    if (impl->notifyWindow)
    {
        SetWindowLongPtrW(impl->notifyWindow, GWLP_USERDATA, 0);
        DestroyWindow(impl->notifyWindow);
    }

    if (impl->provider)
    {
        impl->provider->detachFromWindow();

        if (auto disconnect = UiaApi::get().disconnectProvider)
            disconnect(impl->provider);

        impl->provider->Release();
    }
}

bool UiaAnnouncer::announce(const std::string &text, bool interrupt)
{
    const auto &api = UiaApi::get();

    if (!impl->provider || !api.usable() || !api.clientsAreListening())
        return false;

    std::wstring wide;
    auto wlen =
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);

    if (wlen > 0)
    {
        wide.resize(wlen);
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), wide.data(),
                            wlen);
    }

    auto message = SysAllocString(wide.c_str());
    auto activity = SysAllocString(L"SurgeXTAnnouncement");

    auto hr = api.raiseNotificationEvent(impl->provider, NotificationKind_Other,
                                         interrupt ? NotificationProcessing_MostRecent
                                                   : NotificationProcessing_All,
                                         message, activity);

    SysFreeString(message);
    SysFreeString(activity);

    return SUCCEEDED(hr);
}

} // namespace GUI
} // namespace Surge
