// Susie.hpp --- Susieプラグイン管理 by katahiromz
#pragma once

#ifndef _INC_WINDOWS
    #include <windows.h> // Windowsの標準ヘッダ。
#endif

#include <string> // std::string, std::wstring
#include <vector> // std::vector

class SusiePlugin;
class SusiePluginManager;

// Susieプラグインのクラス。
class SusiePlugin
{
public:
    // API関数型の定義。
    typedef int (PASCAL *FN_GetPluginInfo)(int infono, LPSTR buf, int buflen);
    typedef int (PASCAL *FN_IsSupported)(LPSTR filename, DWORD dw);
    typedef int (PASCAL *FN_GetPicture)(LPSTR buf, long len, unsigned int flag,
                                        HANDLE *pHBInfo, HANDLE *pHBm,
                                        FARPROC lpPrgressCallback, long lData);
    HINSTANCE m_hInst = NULL; // プラグインのインスタンス。

    // API関数のポインタ。
    FN_GetPluginInfo GetPluginInfo = NULL;
    FN_IsSupported IsSupported = NULL;
    FN_GetPicture GetPicture = NULL;

    std::string m_filename; // ファイル名。
    std::string m_filter; // フィルター文字列。

    // コンストラクタ。
    SusiePlugin()
    {
    }

    // SusiePluginクラスはコピー禁止。
    SusiePlugin(const SusiePlugin&) = delete;
    SusiePlugin& operator=(const SusiePlugin&) = delete;

    // デストラクタ。
    ~SusiePlugin()
    {
        // 自動的に破棄する。
        unload();
    }

    // 破棄または初期化する。
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

    // 拡張子がサポートされているか？
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

    // プラグインを読み込む。
    bool load(const char *filename)
    {
        // 最初に初期化する。
        unload();

        // プラグインをDLLファイルとして読み込む。
        m_hInst = LoadLibraryA(filename);
        if (m_hInst == NULL)
            return false;

        // DLLからAPI関数を取得する。
        GetPluginInfo = (FN_GetPluginInfo)GetProcAddress(m_hInst, "GetPluginInfo");
        IsSupported = (FN_IsSupported)GetProcAddress(m_hInst, "IsSupported");
        GetPicture = (FN_GetPicture)GetProcAddress(m_hInst, "GetPicture");
        if (!GetPluginInfo || !IsSupported || !GetPicture)
        {
            unload();
            return false;
        }

        // APIバージョン情報を取得する。
        CHAR buf[512];
        GetPluginInfo(0, buf, 512);
        std::string api_ver = buf;

        // 画像読み込み用のプラグインか？
        if (api_ver.size() == 4 && api_ver[2] == 'I')
        {
            // フィルターを取得する。
            for (int i = 2; i < 64; i += 2)
            {
                if (!GetPluginInfo(i + 0, buf, 512))
                    break;
                if (m_filter.size())
                    m_filter += ";";
                m_filter += buf;
            }
        }

        // フィルターがなければ破棄する。
        if (m_filter.empty())
        {
            unload();
            return false;
        }

        // ファイル名を格納する。
        m_filename = PathFindFileNameA(filename);
        return true;
    }

    // プラグインを使用して画像を読み込む。
    HBITMAP load_image(LPCSTR filename)
    {
        // API関数GetPictureを使って画像データを取得する。
        HLOCAL hBitmapInfo = NULL, hBits = NULL;
        if (GetPicture((LPSTR)filename, 0, 0, (HANDLE*)&hBitmapInfo, (HANDLE*)&hBits, NULL, 0) != 0)
            return NULL;

        // ハンドルをロックすることで、実際のポインタを取得できる。
        LPBITMAPINFO pbmi = (LPBITMAPINFO)LocalLock(hBitmapInfo);
        LPBYTE pbBits = (LPBYTE)LocalLock(hBits);

        // DIBのHBITMAPを作成。
        LPVOID pBits;
        HBITMAP hbm = CreateDIBSection(NULL, pbmi, DIB_RGB_COLORS, &pBits, NULL, 0);

        // ビット群をコピーする。
        BITMAP bm;
        if (hbm && GetObject(hbm, sizeof(bm), &bm))
            CopyMemory(pBits, pbBits, bm.bmWidthBytes * bm.bmHeight);

        // アンロック・破棄する。
        LocalUnlock(hBitmapInfo);
        LocalUnlock(hBits);
        LocalFree(hBitmapInfo);
        LocalFree(hBits);

        // ビットマップハンドルHBITMAPを返す。
        return hbm;
    }

protected:
    // 文字列を区切りで分割する関数。
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

// Susieプラグインの管理クラス。
class SusiePluginManager
{
public:
    // Susieプラグインのファイル名群。
    std::vector<std::string> m_plugin_filenames;
    // Susieプラグインのパスファイル名群。
    std::vector<std::string> m_plugin_pathnames;
    // Susieプラグイン群を保持する変数。
    std::vector<SusiePlugin*> m_plugins;

    // コンストラクタ。
    SusiePluginManager()
    {
    }

    // デストラクタ。
    ~SusiePluginManager()
    {
        // 自動的に破棄する。
        unload();
    }

    // プラグインが読み込まれたか？
    bool is_loaded() const
    {
        return m_plugins.size() > 0;
    }

    // プラグイン群を破棄して、初期化する。
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

    // ディレクトリ（フォルダ）からプラグイン群を読み込む。
    bool load(LPCSTR dir = ".")
    {
        // 最初に破棄・初期化する。
        unload();

        // ワイルドカードを含む文字列を構築する。
        CHAR path[MAX_PATH];
        GetFullPathNameA(dir, MAX_PATH, path, NULL);
        PathAppendA(path, "*.spi");

        // パターンマッチにマッチした文字列を列挙して、プラグインの一覧を取得する。
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

        // 実際にプラグインを読み込む。
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

        // １つでもプラグインを読み込めたら成功。
        return m_plugins.size() > 0;
    }

    // 指定された拡張子がサポートされているか？
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

    // プラグイン群を使用して画像ファイルをHBITMAPとして読み込む。
    HBITMAP load_image(LPCSTR filename)
    {
        // 拡張子を取得する。
        LPCSTR dotext = PathFindExtensionA(filename);

        // 拡張子をサポートしているプラグインで読み込みを試みる。
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

        return NULL; // 失敗。
    }

    // フィルター文字列を取得する。
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
