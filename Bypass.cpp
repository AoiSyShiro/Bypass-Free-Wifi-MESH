#include <windows.h>
#include <wlanapi.h>
#include <curl/curl.h>
#include <iostream>
#include <string>

#pragma comment(lib, "wlanapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "libcurl.lib")

std::string FetchUrl(const std::string& url) {
    CURL* curl;
    CURLcode res;
    std::string response;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](void* contents, size_t size, size_t nmemb, std::string* s) {
            size_t newLength = size * nmemb;
            size_t oldLength = s->size();
            try {
                s->resize(oldLength + newLength);
            }
            catch(std::bad_alloc &e) {
                return size_t(0);
            }
            std::copy((char*)contents, (char*)contents + newLength, s->begin() + oldLength);
            return newLength;
        });
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        }
        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();
    return response;
}

void ConnectToWifi(HANDLE hClient, GUID interfaceGuid, const std::wstring& ssid) {
    WLAN_CONNECTION_PARAMETERS connParams;
    memset(&connParams, 0, sizeof(connParams));

    connParams.wlanConnectionMode = wlan_connection_mode_profile;
    connParams.strProfile = ssid.c_str();
    connParams.dot11BssType = dot11_BSS_type_any;
    connParams.dwFlags = 0;

    DWORD result = WlanConnect(hClient, &interfaceGuid, &connParams, NULL);
    if (result == ERROR_SUCCESS) {
        std::wcout << L"Successfully connected to " << ssid << std::endl;
    } else {
        std::wcerr << L"Failed to connect to " << ssid << L" with error: " << result << std::endl;
    }
}

bool IsConnectedToWifi(HANDLE hClient, GUID interfaceGuid) {
    PWLAN_CONNECTION_ATTRIBUTES pConnectInfo = NULL;
    DWORD connectInfoSize = sizeof(WLAN_CONNECTION_ATTRIBUTES);
    WLAN_OPCODE_VALUE_TYPE opCode = wlan_opcode_value_type_invalid;
    DWORD result = WlanQueryInterface(hClient, &interfaceGuid, wlan_intf_opcode_current_connection, NULL, &connectInfoSize, (PVOID*)&pConnectInfo, &opCode);

    if (result == ERROR_SUCCESS && pConnectInfo->isState == wlan_interface_state_connected) {
        WlanFreeMemory(pConnectInfo);
        return true;
    }

    if (pConnectInfo != NULL) {
        WlanFreeMemory(pConnectInfo);
    }
    return false;
}

int main() {
    HANDLE hClient = NULL;
    DWORD version;
    DWORD result = WlanOpenHandle(2, NULL, &version, &hClient);

    if (result != ERROR_SUCCESS) {
        std::cerr << "WlanOpenHandle failed with error: " << result << std::endl;
        return 1;
    }

    PWLAN_INTERFACE_INFO_LIST pIfList = NULL;
    result = WlanEnumInterfaces(hClient, NULL, &pIfList);

    if (result != ERROR_SUCCESS) {
        std::cerr << "WlanEnumInterfaces failed with error: " << result << std::endl;
        WlanCloseHandle(hClient, NULL);
        return 1;
    }

    std::wstring ssid = L"free.wi-mesh.vn";

    while (true) {
        std::string statusPage = FetchUrl("http://free.wi-mesh.vn/status");
        // Phân tích statusPage để xác định thời gian kết nối còn lại và logic xử lý cần thiết

        // Giả sử logic là: nếu thời gian còn lại < 5 giây, thì thực hiện skip quảng cáo và kết nối lại
        if (!statusPage.empty()) {
            // Bỏ qua quảng cáo 5 giây
            FetchUrl("http://free.wi-mesh.vn/skip"); // URL giả định để skip quảng cáo

            for (int i = 0; i < (int)pIfList->dwNumberOfItems; i++) {
                WLAN_INTERFACE_INFO interfaceInfo = pIfList->InterfaceInfo[i];
                if (!IsConnectedToWifi(hClient, interfaceInfo.InterfaceGuid)) {
                    std::wcout << L"Disconnected from Wi-Fi, reconnecting to " << ssid << std::endl;
                    ConnectToWifi(hClient, interfaceInfo.InterfaceGuid, ssid);
                } else {
                    std::wcout << L"Wi-Fi is connected." << std::endl;
                }
            }
        }

        Sleep(10000); // Kiểm tra lại sau 10 giây
    }

    if (pIfList != NULL) {
        WlanFreeMemory(pIfList);
    }
    WlanCloseHandle(hClient, NULL);

    return 0;
}
