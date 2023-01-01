#pragma

#ifndef _INC_WINDOWS
    #include <windows.h>
#endif

#include <string>
#include <vector>

class SusiePlugin;
class SusiePluginManager;

class SusiePlugin
{
public:
    typedef int (PASCAL *FN_GetPluginInfo)(int infono, LPSTR buf, int buflen);
    typedef int (PASCAL *FN_IsSupported)(LPSTR filename, DWORD dw);
    typedef int (PASCAL *FN_GetPicture)(LPSTR buf, long len, unsigned int flag,
                                        HANDLE *pHBInfo, HANDLE *pHBm,
                                        FARPROC lpPrgressCallback, long lData);
    HINSTANCE m_hInst = NULL;
    FN_GetPluginInfo GetPluginInfo = NULL;
    FN_IsSupported IsSupported = NULL;
    FN_GetPicture GetPicture = NULL;
    std::string m_filename;
    std::string m_filter;

    SusiePlugin()
    {
    }

    SusiePlugin(const SusiePlugin&) = delete;
    SusiePlugin& operator=(const SusiePlugin&) = delete;

    ~SusiePlugin()
    {
        unload();
    }

    void unload()
    {
        m_filename.clear();
        m_filter.clear();
        if (m_hInst)
        {
            FreeLibrary(m_hInst);
            m_hInst = NULL;
        }
        GetPluginInfo = NULL;
        IsSupported = NULL;
        GetPicture = NULL;
    }

    bool is_dotext_supported(LPCSTR dotext) const
    {
        std::vector<std::string> extensions;
        str_split(extensions, m_filter, std::string(";"));
        for (size_t i = 0; i < extensions.size(); ++i)
        {
            const char *extension = extensions[i].c_str();
            if (*extension == '*')
                ++extension;
            if (lstrcmpiA(extension, dotext) == 0)
                return true;
        }

        return false;
    }

    bool load(const char *filename)
    {
        unload();

        m_hInst = LoadLibraryA(filename);
        if (m_hInst == NULL)
            return false;

        GetPluginInfo = (FN_GetPluginInfo)GetProcAddress(m_hInst, "GetPluginInfo");
        IsSupported = (FN_IsSupported)GetProcAddress(m_hInst, "IsSupported");
        GetPicture = (FN_GetPicture)GetProcAddress(m_hInst, "GetPicture");
        if (!GetPluginInfo || !IsSupported || !GetPicture)
        {
            unload();
            return false;
        }

        CHAR buf[512];
        GetPluginInfo(0, buf, 512);
        std::string api_ver = buf;

        if (api_ver.size() == 4 && api_ver[2] == 'I')
        {
            for (int i = 2; i < 64; i += 2)
            {
                if (!GetPluginInfo(i + 0, buf, 512))
                    break;
                if (m_filter.size())
                    m_filter += ";";
                m_filter += buf;
            }
        }

        if (m_filter.empty())
        {
            unload();
            return false;
        }

        m_filename = PathFindFileNameA(filename);
        return true;
    }

    HBITMAP load_image(LPCSTR filename)
    {
        HLOCAL hBitmapInfo = NULL, hBits = NULL;
        if (GetPicture((LPSTR)filename, 0, 0, (HANDLE*)&hBitmapInfo, (HANDLE*)&hBits, NULL, 0) != 0)
            return NULL;

        LPBITMAPINFO pbmi = (LPBITMAPINFO)LocalLock(hBitmapInfo);
        LPBYTE pbBits = (LPBYTE)LocalLock(hBits);

        LPVOID pBits;
        HBITMAP hbm = CreateDIBSection(NULL, pbmi, DIB_RGB_COLORS, &pBits, NULL, 0);

        BITMAP bm;
        if (hbm && GetObject(hbm, sizeof(bm), &bm))
            CopyMemory(pBits, pbBits, bm.bmWidthBytes * bm.bmHeight);

        LocalUnlock(hBitmapInfo);
        LocalUnlock(hBits);
        LocalFree(hBitmapInfo);
        LocalFree(hBits);

        return hbm;
    }

protected:
    template <typename T_STR_CONTAINER>
    static void
    str_split(T_STR_CONTAINER& container,
              const typename T_STR_CONTAINER::value_type& str,
              const typename T_STR_CONTAINER::value_type& chars)
    {
        container.clear();
        size_t i = 0, k = str.find_first_of(chars);
        while (k != T_STR_CONTAINER::value_type::npos)
        {
            container.push_back(str.substr(i, k - i));
            i = k + 1;
            k = str.find_first_of(chars, i);
        }
        container.push_back(str.substr(i));
    }
};

class SusiePluginManager
{
public:
    std::vector<std::string> m_plugin_filenames;
    std::vector<std::string> m_plugin_pathnames;
    std::vector<SusiePlugin*> m_plugins;

    SusiePluginManager()
    {
    }

    ~SusiePluginManager()
    {
        unload();
    }

    bool is_loaded() const
    {
        return m_plugins.size() > 0;
    }

    void unload()
    {
        for (size_t i = 0; i < m_plugins.size(); ++i)
        {
            delete m_plugins[i];
            m_plugins[i] = NULL;
        }
        m_plugins.clear();

        m_plugin_filenames.clear();
        m_plugin_pathnames.clear();
    }

    bool load(LPCSTR dir = ".")
    {
        unload();

        CHAR path[MAX_PATH];
        GetFullPathNameA(dir, MAX_PATH, path, NULL);
        PathAppendA(path, "*.spi");

        WIN32_FIND_DATAA find;
        HANDLE hFind = FindFirstFileA(path, &find);
        PathRemoveFileSpecA(path);
        if (hFind != INVALID_HANDLE_VALUE)
        {
            do
            {
                m_plugin_filenames.push_back(find.cFileName);
                PathAppendA(path, find.cFileName);
                m_plugin_pathnames.push_back(path);
                PathRemoveFileSpecA(path);
            } while (FindNextFileA(hFind, &find));
            FindClose(hFind);
        }

        for (size_t i = 0; i < m_plugin_pathnames.size(); ++i)
        {
            SusiePlugin *plugin = new SusiePlugin();
            std::string& pathname = m_plugin_pathnames[i];
            if (!plugin->load(pathname.c_str()))
            {
                delete plugin;
                continue;
            }
            m_plugins.push_back(plugin);
        }

        return m_plugins.size() > 0;
    }

    bool is_dotext_supported(LPCSTR dotext) const
    {
        for (size_t i = 0; i < m_plugins.size(); ++i)
        {
            SusiePlugin *plugin = m_plugins[i];
            if (plugin)
            {
                if (plugin->is_dotext_supported(dotext))
                {
                    return true;
                }
            }
        }
        return false;
    }

    HBITMAP load_image(LPCSTR filename)
    {
        LPCSTR dotext = PathFindExtensionA(filename);

        for (size_t i = 0; i < m_plugins.size(); ++i)
        {
            SusiePlugin *plugin = m_plugins[i];
            if (plugin)
            {
                if (plugin->is_dotext_supported(dotext))
                {
                    HBITMAP hbm = plugin->load_image(filename);
                    if (hbm)
                        return hbm;
                }
            }
        }

        return NULL;
    }

    std::string get_filter() const
    {
        std::string ret;
        for (size_t i = 0; i < m_plugins.size(); ++i)
        {
            SusiePlugin *plugin = m_plugins[i];
            if (plugin && plugin->m_filter.size())
            {
                if (ret.size())
                    ret += ";";
                ret += plugin->m_filter;
            }
        }
        return ret;
    }
};
