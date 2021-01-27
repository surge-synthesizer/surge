/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2020 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#if BUILD_DEFERRED_ASSET_LOADER

#include <string>
#include <sstream>
#include <thread>
#include <fstream>

#include "DeferredAssetLoader.h"
#include "SurgeStorage.h"

#if MAC
#include "objc_utils.h"
#endif

#if WINDOWS
#include <windows.h>
#include <winhttp.h>
#endif

#if LINUX
#if !NOCURL
#define GET_WITH_CURL 1
#endif
#endif

#if GET_WITH_CURL
#include <curl/curl.h>
#endif

namespace Surge
{
namespace Storage
{

#if GET_WITH_CURL

struct curl_memory
{
    size_t size = 0;
    char *response = nullptr;
};

size_t curl_callback(void *data, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    auto *mem = static_cast<curl_memory *>(userp);

    char *ptr = (char *)realloc(mem->response, mem->size + realsize + 1);
    if (ptr == nullptr)
        return 0; /* out of memory! */

    mem->response = ptr;
    memcpy(&(mem->response[mem->size]), data, realsize);
    mem->size += realsize;
    mem->response[mem->size] = 0;

    return realsize;
}
#endif

#if WINDOWS
std::wstring s2ws(const std::string &s)
{
    int len;
    int slength = (int)s.length() + 1;
    len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0);
    wchar_t *buf = new wchar_t[len];
    MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, buf, len);
    std::wstring r(buf);
    delete[] buf;
    return r;
}
#endif

// Call this off the main thread
void loadUrlToPath(const std::string &url, const fs::path &path,
                   std::function<void(const std::string &url)> onDone,
                   std::function<void(const std::string &url, const std::string &msg)> onError)
{
#if MAC
    Surge::ObjCUtil::loadUrlToPath(url, path, onDone, onError);
    return;
#elif WINDOWS
    URL_COMPONENTS urlComp;

    ZeroMemory(&urlComp, sizeof(urlComp));
    urlComp.dwStructSize = sizeof(urlComp);

    // Set required component lengths to non-zero so that they are cracked.
    urlComp.dwSchemeLength = (DWORD)-1;
    urlComp.dwHostNameLength = (DWORD)-1;
    urlComp.dwUrlPathLength = (DWORD)-1;
    urlComp.dwExtraInfoLength = (DWORD)-1;

    auto wurl = s2ws(url);
    if (!WinHttpCrackUrl(wurl.c_str(), (DWORD)wcslen(wurl.c_str()), 0, &urlComp))
    {
        std::ostringstream oss;
        oss << "Error in WinHttpCrackUrl: " << GetLastError();
        onError(url, oss.str());
        return;
    }

    std::wstring hn = urlComp.lpszHostName;
    hn = hn.substr(0, urlComp.dwHostNameLength);

    std::wstring proto = urlComp.lpszScheme;
    proto = proto.substr(0, urlComp.dwSchemeLength);
    if (proto != L"https")
    {
        onError(url, "Not an HTTPS URL");
        return;
    }

    DWORD dwSize = 0;
    DWORD dwDownloaded = 0;
    LPSTR pszOutBuffer;
    BOOL bResults = FALSE;
    HINTERNET hSession = NULL, hConnect = NULL, hRequest = NULL;

    // Use WinHttpOpen to obtain a session handle.
    hSession = WinHttpOpen(L"Surge Synthesizer WinHTTP/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                           WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);

    // Specify an HTTP server.
    if (hSession)
        hConnect = WinHttpConnect(hSession, hn.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);

    // Create an HTTP request handle.
    if (hConnect)
        hRequest =
            WinHttpOpenRequest(hConnect, L"GET", urlComp.lpszUrlPath, NULL, WINHTTP_NO_REFERER,
                               WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);

    // Send a request.
    if (hRequest)
        bResults = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                      WINHTTP_NO_REQUEST_DATA, 0, 0, 0);

    // End the request.
    if (bResults)
        bResults = WinHttpReceiveResponse(hRequest, NULL);

    struct web_memory
    {
        size_t size = 0;
        char *response = nullptr;

        size_t push(size_t realsize, void *data)
        {
            char *ptr = (char *)realloc(response, size + realsize + 1);
            if (ptr == nullptr)
                return 0; /* out of memory! */

            response = ptr;
            memcpy(&(response[size]), data, realsize);
            size += realsize;
            response[size] = 0;

            return realsize;
        }
    } webm;

    std::ofstream outf;

    // Keep checking for data until there is nothing left.
    if (bResults)
    {
        do
        {
            // Check for available data.
            dwSize = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize))
            {
                onError(url, "Data Not Available");
                goto cleanup;
            }

            // Allocate space for the buffer.
            pszOutBuffer = new char[dwSize + 1];
            if (!pszOutBuffer)
            {
                onError(url, "Out of Memory");
                dwSize = 0;
                goto cleanup;
            }
            else
            {
                // Read the data.
                ZeroMemory(pszOutBuffer, dwSize + 1);

                if (!WinHttpReadData(hRequest, (LPVOID)pszOutBuffer, dwSize, &dwDownloaded))
                {
                    onError(url, "Unable to read data");
                    goto cleanup;
                }
                else
                {
                    webm.push(dwDownloaded, pszOutBuffer);
                }

                // Free the memory allocated to the buffer.
                delete[] pszOutBuffer;
            }
        } while (dwSize > 0);
    }

    if (hRequest)
    {
        DWORD dwStatusCode = 0;
        DWORD dwSize = sizeof(dwStatusCode);
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &dwStatusCode, &dwSize,
                            WINHTTP_NO_HEADER_INDEX);
        if (dwStatusCode >= 400)
        {
            onError(url, "HTTPS Status : " + std::to_string(dwStatusCode));
            goto cleanup;
        }
    }

    // Report any errors.
    if (!bResults)
    {
        onError(url, "Error number " + std::to_string(GetLastError()));
        goto cleanup;
    }

    outf = std::ofstream(path, std::ofstream::out | std::ofstream::binary);
    if (!outf.is_open())
    {
        onError(url, "Can't open output file " + path_to_string(path));
        goto cleanup;
    }
    outf.write(webm.response, webm.size);
    outf.close();

    onDone(url);

    // Close any open handles.
cleanup:
    if (hRequest)
        WinHttpCloseHandle(hRequest);
    if (hConnect)
        WinHttpCloseHandle(hConnect);
    if (hSession)
        WinHttpCloseHandle(hSession);

    return;
#elif GET_WITH_CURL
    auto *curl = curl_easy_init();
    if (curl)
    {
        CURLcode res;

        char ebuf[CURL_ERROR_SIZE];

        curl_memory c;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&c);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback);
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, ebuf);
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);

        res = curl_easy_perform(curl);

        curl_easy_cleanup(curl);

        if (res != CURLE_OK)
        {
            onError(url, ebuf);
            return;
        }

        auto outf = std::ofstream(path, std::ofstream::out | std::ofstream::binary);
        if (!outf.is_open())
        {
            onError(url, "Can't open output file");
            return;
        }

        outf.write(c.response, c.size);
        outf.close();

        onDone(url);
    }
    else
    {
        onError(url, "Could not initialize curl");
    }
    return;
#else
    onError(url, "DeferredAssetLoader not implemented for your platform");
#endif
}

std::string urlToCacheName(const std::string &s)
{
    std::ostringstream oss;
    oss << "chc_";
    for (auto c : s)
    {
        bool isAlphaNum =
            (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        if (isAlphaNum)
            oss << c;
        else
            oss << "_";
    }
    return oss.str();
}

DeferredAssetLoader::DeferredAssetLoader(SurgeStorage *s)
{
    cacheDir = string_to_path(s->userDataPath) / string_to_path(".deferred_cache");
    fs::create_directories(cacheDir);
}

fs::path DeferredAssetLoader::pathOfCachedCopy(const url_t &url)
{
    auto cn = urlToCacheName(url);
    auto p = cacheDir / string_to_path(cn);
    return p;
}

bool DeferredAssetLoader::hasCachedCopyOf(const url_t &url)
{
    return fs::exists(pathOfCachedCopy(url));
}

void DeferredAssetLoader::retrieveSingleAsset(
    const url_t &url, std::function<void(const url_t &)> onRetrieved,
    std::function<void(const url_t &, const std::string &msg)> onError)
{
    std::thread t([url, onRetrieved, onError, this]() {
        loadUrlToPath(url, pathOfCachedCopy(url), onRetrieved, onError);
    });
    t.detach();
}

} // namespace Storage
} // namespace Surge

#endif