#include <windows.h>
#include <setupapi.h>
#include <newdev.h>
#include <devguid.h>

#include <cwchar>
#include <iostream>
#include <string>
#include <vector>

#pragma comment(lib, "Setupapi.lib")
#pragma comment(lib, "Newdev.lib")

namespace {

constexpr wchar_t kHardwareId[] = L"ROOT\\PuffyVirtualAudio";
constexpr wchar_t kDeviceName[] = L"PuffyVirtualAudio";
constexpr wchar_t kDeviceDescription[] = L"Puffy Virtual Audio";

class DevInfoSet {
public:
    explicit DevInfoSet(HDEVINFO value = INVALID_HANDLE_VALUE) noexcept : value_(value) {}
    ~DevInfoSet() {
        if (value_ != INVALID_HANDLE_VALUE) SetupDiDestroyDeviceInfoList(value_);
    }
    DevInfoSet(const DevInfoSet&) = delete;
    DevInfoSet& operator=(const DevInfoSet&) = delete;
    HDEVINFO get() const noexcept { return value_; }
    bool valid() const noexcept { return value_ != INVALID_HANDLE_VALUE; }
private:
    HDEVINFO value_;
};

std::wstring formatWin32Error(DWORD code) {
    wchar_t* buffer = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD length = FormatMessageW(flags, nullptr, code, 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    if (length == 0 || buffer == nullptr) return L"Win32 error " + std::to_wstring(code);

    std::wstring message(buffer, length);
    LocalFree(buffer);
    while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n' || message.back() == L' ')) {
        message.pop_back();
    }
    return message + L" (" + std::to_wstring(code) + L")";
}

bool isAdministrator() {
    BOOL member = FALSE;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    PSID administrators = nullptr;
    if (!AllocateAndInitializeSid(
            &ntAuthority,
            2,
            SECURITY_BUILTIN_DOMAIN_RID,
            DOMAIN_ALIAS_RID_ADMINS,
            0, 0, 0, 0, 0, 0,
            &administrators)) {
        return false;
    }
    const BOOL ok = CheckTokenMembership(nullptr, administrators, &member);
    FreeSid(administrators);
    return ok && member;
}

bool multiSzContains(const wchar_t* multiSz, size_t charCount, const wchar_t* wanted) {
    if (multiSz == nullptr || wanted == nullptr) return false;
    size_t offset = 0;
    while (offset < charCount && multiSz[offset] != L'\0') {
        const wchar_t* current = multiSz + offset;
        const size_t remaining = charCount - offset;
        const size_t length = wcsnlen_s(current, remaining);
        if (length == remaining) break;
        if (_wcsicmp(current, wanted) == 0) return true;
        offset += length + 1;
    }
    return false;
}

bool deviceHasHardwareId(HDEVINFO infoSet, SP_DEVINFO_DATA& data, const wchar_t* hardwareId) {
    DWORD required = 0;
    DWORD type = 0;
    SetupDiGetDeviceRegistryPropertyW(infoSet, &data, SPDRP_HARDWAREID, &type, nullptr, 0, &required);
    if (required == 0 || GetLastError() != ERROR_INSUFFICIENT_BUFFER) return false;

    std::vector<BYTE> buffer(required + sizeof(wchar_t), 0);
    if (!SetupDiGetDeviceRegistryPropertyW(
            infoSet,
            &data,
            SPDRP_HARDWAREID,
            &type,
            buffer.data(),
            static_cast<DWORD>(buffer.size()),
            &required)) {
        return false;
    }

    if (type != REG_MULTI_SZ && type != REG_SZ) return false;
    const auto* strings = reinterpret_cast<const wchar_t*>(buffer.data());
    return multiSzContains(strings, buffer.size() / sizeof(wchar_t), hardwareId);
}

bool findDevice(HDEVINFO infoSet, SP_DEVINFO_DATA& result) {
    for (DWORD index = 0;; ++index) {
        SP_DEVINFO_DATA data{};
        data.cbSize = sizeof(data);
        if (!SetupDiEnumDeviceInfo(infoSet, index, &data)) {
            if (GetLastError() == ERROR_NO_MORE_ITEMS) return false;
            return false;
        }
        if (deviceHasHardwareId(infoSet, data, kHardwareId)) {
            result = data;
            return true;
        }
    }
}

bool removeDevice(HDEVINFO infoSet, SP_DEVINFO_DATA& data, bool* rebootRequired) {
    SP_REMOVEDEVICE_PARAMS params{};
    params.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
    params.ClassInstallHeader.InstallFunction = DIF_REMOVE;
    params.Scope = DI_REMOVEDEVICE_GLOBAL;
    params.HwProfile = 0;

    if (!SetupDiSetClassInstallParamsW(
            infoSet,
            &data,
            &params.ClassInstallHeader,
            sizeof(params))) {
        return false;
    }

    if (!SetupDiCallClassInstaller(DIF_REMOVE, infoSet, &data)) return false;

    SP_DEVINSTALL_PARAMS_W installParams{};
    installParams.cbSize = sizeof(installParams);
    if (SetupDiGetDeviceInstallParamsW(infoSet, &data, &installParams) && rebootRequired != nullptr) {
        *rebootRequired = (installParams.Flags & (DI_NEEDREBOOT | DI_NEEDRESTART)) != 0;
    }
    return true;
}

int installDevice(const std::wstring& infArgument) {
    wchar_t fullInfPath[MAX_PATH]{};
    const DWORD pathLength = GetFullPathNameW(infArgument.c_str(), ARRAYSIZE(fullInfPath), fullInfPath, nullptr);
    if (pathLength == 0 || pathLength >= ARRAYSIZE(fullInfPath)) {
        std::wcerr << L"Unable to resolve INF path: " << formatWin32Error(GetLastError()) << L"\n";
        return 2;
    }
    if (GetFileAttributesW(fullInfPath) == INVALID_FILE_ATTRIBUTES) {
        std::wcerr << L"INF does not exist: " << fullInfPath << L"\n";
        return 2;
    }

    DevInfoSet existing(SetupDiGetClassDevsW(&GUID_DEVCLASS_MEDIA, nullptr, nullptr, 0));
    if (!existing.valid()) {
        std::wcerr << L"SetupDiGetClassDevs failed: " << formatWin32Error(GetLastError()) << L"\n";
        return 3;
    }

    SP_DEVINFO_DATA existingData{};
    existingData.cbSize = sizeof(existingData);
    bool createdNow = false;

    if (!findDevice(existing.get(), existingData)) {
        DevInfoSet created(SetupDiCreateDeviceInfoList(&GUID_DEVCLASS_MEDIA, nullptr));
        if (!created.valid()) {
            std::wcerr << L"SetupDiCreateDeviceInfoList failed: " << formatWin32Error(GetLastError()) << L"\n";
            return 4;
        }

        SP_DEVINFO_DATA data{};
        data.cbSize = sizeof(data);
        if (!SetupDiCreateDeviceInfoW(
                created.get(),
                kDeviceName,
                &GUID_DEVCLASS_MEDIA,
                kDeviceDescription,
                nullptr,
                DICD_GENERATE_ID,
                &data)) {
            std::wcerr << L"SetupDiCreateDeviceInfo failed: " << formatWin32Error(GetLastError()) << L"\n";
            return 4;
        }

        // SPDRP_HARDWAREID is REG_MULTI_SZ, so include an explicit second NUL.
        const wchar_t hardwareIds[] = L"ROOT\\PuffyVirtualAudio\0";
        if (!SetupDiSetDeviceRegistryPropertyW(
                created.get(),
                &data,
                SPDRP_HARDWAREID,
                reinterpret_cast<const BYTE*>(hardwareIds),
                sizeof(hardwareIds))) {
            std::wcerr << L"Setting SPDRP_HARDWAREID failed: " << formatWin32Error(GetLastError()) << L"\n";
            return 4;
        }

        if (!SetupDiCallClassInstaller(DIF_REGISTERDEVICE, created.get(), &data)) {
            std::wcerr << L"Registering the root device failed: " << formatWin32Error(GetLastError()) << L"\n";
            return 4;
        }
        createdNow = true;
    }

    BOOL reboot = FALSE;
    if (!UpdateDriverForPlugAndPlayDevicesW(
            nullptr,
            kHardwareId,
            fullInfPath,
            INSTALLFLAG_FORCE,
            &reboot)) {
        const DWORD error = GetLastError();

        // If this invocation created the devnode, remove the orphan on failure.
        if (createdNow) {
            DevInfoSet cleanup(SetupDiGetClassDevsW(&GUID_DEVCLASS_MEDIA, nullptr, nullptr, 0));
            if (cleanup.valid()) {
                SP_DEVINFO_DATA cleanupData{};
                cleanupData.cbSize = sizeof(cleanupData);
                if (findDevice(cleanup.get(), cleanupData)) {
                    bool ignoredReboot = false;
                    (void)removeDevice(cleanup.get(), cleanupData, &ignoredReboot);
                }
            }
        }

        std::wcerr << L"UpdateDriverForPlugAndPlayDevices failed: " << formatWin32Error(error) << L"\n";
        return 5;
    }

    std::wcout << L"Puffy Virtual Audio device installed successfully.\n";
    if (reboot) std::wcout << L"Windows reports that a reboot is required.\n";
    return reboot ? 10 : 0;
}

int removeInstalledDevice() {
    bool foundAny = false;
    bool reboot = false;

    for (;;) {
        DevInfoSet devices(SetupDiGetClassDevsW(&GUID_DEVCLASS_MEDIA, nullptr, nullptr, 0));
        if (!devices.valid()) {
            std::wcerr << L"SetupDiGetClassDevs failed: " << formatWin32Error(GetLastError()) << L"\n";
            return 3;
        }

        SP_DEVINFO_DATA data{};
        data.cbSize = sizeof(data);
        if (!findDevice(devices.get(), data)) break;

        foundAny = true;
        bool oneReboot = false;
        if (!removeDevice(devices.get(), data, &oneReboot)) {
            std::wcerr << L"Removing the Puffy device failed: " << formatWin32Error(GetLastError()) << L"\n";
            return 6;
        }
        reboot = reboot || oneReboot;
        // The next iteration re-opens the device information set after removal.
    }

    if (!foundAny) {
        std::wcout << L"Puffy Virtual Audio device is not installed.\n";
        return 0;
    }
    std::wcout << L"Puffy Virtual Audio device removed. The driver package may remain in Driver Store.\n";
    if (reboot) std::wcout << L"Windows reports that a reboot is required.\n";
    return reboot ? 10 : 0;
}

void printUsage() {
    std::wcerr << L"Usage:\n"
               << L"  PuffyVirtualAudioDevice.exe install <full-or-relative-path-to-INF>\n"
               << L"  PuffyVirtualAudioDevice.exe remove\n";
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    if (!isAdministrator()) {
        std::wcerr << L"Administrator privileges are required.\n";
        return 1;
    }

    if (argc == 3 && _wcsicmp(argv[1], L"install") == 0) {
        return installDevice(argv[2]);
    }
    if (argc == 2 && _wcsicmp(argv[1], L"remove") == 0) {
        return removeInstalledDevice();
    }

    printUsage();
    return 2;
}
