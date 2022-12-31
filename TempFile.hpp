#pragma once

#include <strsafe.h>
#include <cassert>

class TempFile
{
protected:
    DWORD m_old_value;
    TCHAR m_tempfile[MAX_PATH];
    TCHAR m_temppath[MAX_PATH];
    TCHAR m_prefix[4];
    TCHAR m_dot_ext[8];

    DWORD get_salt()
    {
        SYSTEMTIME st;
        ::GetLocalTime(&st);

        DWORD dw = m_old_value;
        dw += st.wYear;
        dw += st.wMonth;
        dw += st.wDay;
        dw += st.wHour;
        dw += st.wMinute;
        dw += st.wSecond;
        dw += st.wMilliseconds;
        m_old_value = dw;
        return LOWORD(dw) ^ HIWORD(dw);
    }

public:
    TempFile() : m_old_value(::GetTickCount())
    {
        m_tempfile[0] = m_temppath[0] = m_prefix[0] = m_dot_ext[0] = 0;
    }

    TempFile(LPCTSTR prefix, LPCTSTR dot_ext) : m_old_value(::GetTickCount())
    {
        init(prefix, dot_ext);
    }

    ~TempFile()
    {
        erase();
    }

    void init(LPCTSTR prefix, LPCTSTR dot_ext)
    {
        m_tempfile[0] = 0;
        ::GetTempPath(_countof(m_temppath), m_temppath);
        StringCchCopy(m_prefix, _countof(m_prefix), prefix);
        StringCchCopy(m_dot_ext, _countof(m_dot_ext), dot_ext);
        assert(dot_ext[0] == TEXT('.') || dot_ext[0] == 0);
    }

    LPCTSTR make()
    {
        for (UINT i = 0; i < 1000; ++i)
        {
            StringCchPrintf(m_tempfile, _countof(m_tempfile),
                            TEXT("%s%s-%04lX%s"),
                            m_temppath, m_prefix, get_salt(), m_dot_ext);
            HANDLE hFile = ::CreateFile(m_tempfile, GENERIC_WRITE, FILE_SHARE_READ,
                                        NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile != INVALID_HANDLE_VALUE)
            {
                ::CloseHandle(hFile);
                return m_tempfile;
            }
        }

        assert(0);
        m_tempfile[0] = 0;
        return NULL;
    }

    LPCTSTR get() const
    {
        assert(m_tempfile[0] != 0); // make first!

        if (m_tempfile[0] != 0)
            return m_tempfile;

        return NULL;
    }

    void erase()
    {
        if (m_tempfile[0])
            ::DeleteFile(m_tempfile);
    }

    void clear()
    {
        m_tempfile[0] = 0;
    }
};
