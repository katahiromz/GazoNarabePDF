// Susie.hpp --- Susie�v���O�C���Ǘ� by katahiromz
#pragma once

#ifndef _INC_WINDOWS
    #include <windows.h>
#endif

#include <string>
#include <vector>

class SusiePlugin;
class SusiePluginManager;

// Susie�v���O�C���̃N���X�B
class SusiePlugin
{
public:
    // API�֐��^�̒�`�B
    typedef int (PASCAL *FN_GetPluginInfo)(int infono, LPSTR buf, int buflen);
    typedef int (PASCAL *FN_IsSupported)(LPSTR filename, DWORD dw);
    typedef int (PASCAL *FN_GetPicture)(LPSTR buf, long len, unsigned int flag,
                                        HANDLE *pHBInfo, HANDLE *pHBm,
                                        FARPROC lpPrgressCallback, long lData);
    HINSTANCE m_hInst = NULL; // �v���O�C���̃C���X�^���X�B

    // API�֐��̃|�C���^�B
    FN_GetPluginInfo GetPluginInfo = NULL;
    FN_IsSupported IsSupported = NULL;
    FN_GetPicture GetPicture = NULL;

    std::string m_filename; // �t�@�C�����B
    std::string m_filter; // �t�B���^�[������B

    // �R���X�g���N�^�B
    SusiePlugin()
    {
    }

    // SusiePlugin�N���X�̓R�s�[�֎~�B
    SusiePlugin(const SusiePlugin&) = delete;
    SusiePlugin& operator=(const SusiePlugin&) = delete;

    // �f�X�g���N�^�B
    ~SusiePlugin()
    {
        // �����I�ɔj������B
        unload();
    }

    // �j���܂��͏���������B
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

    // �g���q���T�|�[�g����Ă��邩�H
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

    // �v���O�C����ǂݍ��ށB
    bool load(const char *filename)
    {
        // �ŏ��ɏ���������B
        unload();

        // �v���O�C����DLL�t�@�C���Ƃ��ēǂݍ��ށB
        m_hInst = LoadLibraryA(filename);
        if (m_hInst == NULL)
            return false;

        // DLL����API�֐����擾����B
        GetPluginInfo = (FN_GetPluginInfo)GetProcAddress(m_hInst, "GetPluginInfo");
        IsSupported = (FN_IsSupported)GetProcAddress(m_hInst, "IsSupported");
        GetPicture = (FN_GetPicture)GetProcAddress(m_hInst, "GetPicture");
        if (!GetPluginInfo || !IsSupported || !GetPicture)
        {
            unload();
            return false;
        }

        // API�o�[�W���������擾����B
        CHAR buf[512];
        GetPluginInfo(0, buf, 512);
        std::string api_ver = buf;

        // �摜�ǂݍ��ݗp�̃v���O�C�����H
        if (api_ver.size() == 4 && api_ver[2] == 'I')
        {
            // �t�B���^�[���擾����B
            for (int i = 2; i < 64; i += 2)
            {
                if (!GetPluginInfo(i + 0, buf, 512))
                    break;
                if (m_filter.size())
                    m_filter += ";";
                m_filter += buf;
            }
        }

        // �t�B���^�[���Ȃ���Δj������B
        if (m_filter.empty())
        {
            unload();
            return false;
        }

        // �t�@�C�������i�[����B
        m_filename = PathFindFileNameA(filename);
        return true;
    }

    // �v���O�C�����g�p���ĉ摜��ǂݍ��ށB
    HBITMAP load_image(LPCSTR filename)
    {
        // API�֐�GetPicture���g���ĉ摜�f�[�^���擾����B
        HLOCAL hBitmapInfo = NULL, hBits = NULL;
        if (GetPicture((LPSTR)filename, 0, 0, (HANDLE*)&hBitmapInfo, (HANDLE*)&hBits, NULL, 0) != 0)
            return NULL;

        // �n���h�������b�N���邱�ƂŁA���ۂ̃|�C���^���擾�ł���B
        LPBITMAPINFO pbmi = (LPBITMAPINFO)LocalLock(hBitmapInfo);
        LPBYTE pbBits = (LPBYTE)LocalLock(hBits);

        // DIB��HBITMAP���쐬�B
        LPVOID pBits;
        HBITMAP hbm = CreateDIBSection(NULL, pbmi, DIB_RGB_COLORS, &pBits, NULL, 0);

        // �r�b�g�Q���R�s�[����B
        BITMAP bm;
        if (hbm && GetObject(hbm, sizeof(bm), &bm))
            CopyMemory(pBits, pbBits, bm.bmWidthBytes * bm.bmHeight);

        // �A�����b�N�E�j������B
        LocalUnlock(hBitmapInfo);
        LocalUnlock(hBits);
        LocalFree(hBitmapInfo);
        LocalFree(hBits);

        // �r�b�g�}�b�v�n���h��HBITMAP��Ԃ��B
        return hbm;
    }

protected:
    // ���������؂�ŕ�������֐��B
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

// Susie�v���O�C���̊Ǘ��N���X�B
class SusiePluginManager
{
public:
    // Susie�v���O�C���̃t�@�C�����Q�B
    std::vector<std::string> m_plugin_filenames;
    // Susie�v���O�C���̃p�X�t�@�C�����Q�B
    std::vector<std::string> m_plugin_pathnames;
    // Susie�v���O�C���Q��ێ�����ϐ��B
    std::vector<SusiePlugin*> m_plugins;

    // �R���X�g���N�^�B
    SusiePluginManager()
    {
    }

    // �f�X�g���N�^�B
    ~SusiePluginManager()
    {
        // �����I�ɔj������B
        unload();
    }

    // �v���O�C�����ǂݍ��܂ꂽ���H
    bool is_loaded() const
    {
        return m_plugins.size() > 0;
    }

    // �v���O�C���Q��j�����āA����������B
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

    // �f�B���N�g���i�t�H���_�j����v���O�C���Q��ǂݍ��ށB
    bool load(LPCSTR dir = ".")
    {
        // �ŏ��ɔj���E����������B
        unload();

        // ���C���h�J�[�h���܂ޕ�������\�z����B
        CHAR path[MAX_PATH];
        GetFullPathNameA(dir, MAX_PATH, path, NULL);
        PathAppendA(path, "*.spi");

        // �p�^�[���}�b�`�Ƀ}�b�`�����������񋓂��āA�v���O�C���̈ꗗ���擾����B
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

        // ���ۂɃv���O�C����ǂݍ��ށB
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

        // �P�ł��v���O�C����ǂݍ��߂��琬���B
        return m_plugins.size() > 0;
    }

    // �w�肳�ꂽ�g���q���T�|�[�g����Ă��邩�H
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

    // �v���O�C���Q���g�p���ĉ摜�t�@�C����HBITMAP�Ƃ��ēǂݍ��ށB
    HBITMAP load_image(LPCSTR filename)
    {
        // �g���q���擾����B
        LPCSTR dotext = PathFindExtensionA(filename);

        // �g���q���T�|�[�g���Ă���v���O�C���œǂݍ��݂����݂�B
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

        return NULL; // ���s�B
    }

    // �t�B���^�[��������擾����B
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
