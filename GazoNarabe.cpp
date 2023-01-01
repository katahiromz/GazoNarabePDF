// ガゾーナラベPDF by katahiromz
// Copyright (C) 2022-2023 片山博文MZ. All Rights Reserved.
// See README.txt and LICENSE.txt.
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <tchar.h>
#include <strsafe.h>
#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <cassert>
#include <hpdf.h>
#include "TempFile.hpp"
#include "gpimage.hpp"
#include "Susie.hpp"
#include "resource.h"

#ifndef NO_SHAREWARE
    #include "Shareware.hpp"
#endif

// 文字列クラス。
#ifdef UNICODE
    typedef std::wstring string_t;
#else
    typedef std::string string_t;
#endif

// シフトJIS コードページ（Shift_JIS）。
#define CP932  932

// わかりやすい項目名を使用する。
enum
{
    IDC_PAGE_SIZE = cmb1,
    IDC_PAGE_DIRECTION = cmb2,
    IDC_MARGIN = cmb3,
    IDC_ROWS = cmb4,
    IDC_COLUMNS = cmb5,
    IDC_FONT_NAME = cmb6,
    IDC_FONT_SIZE = cmb7,
    IDC_IMAGE_DATA_SIZE = cmb8,
    IDC_IMAGE_TITLE = cmb9,
    IDC_OUTPUT_NAME = cmb10,
    IDC_DRAW_BORDER = chx1,
    IDC_PAGE_NUMBERS = chx2,
    IDC_DONT_RESIZE_SMALL = chx3,
    IDC_GENERATE = IDOK,
    IDC_EXIT = IDCANCEL,
    IDC_ADD = psh1,
    IDC_UP = psh2,
    IDC_DOWN = psh3,
    IDC_DELETE = psh4,
    IDC_ERASESETTINGS = psh5,
};

// Susieプラグイン マネジャー。
SusiePluginManager g_susie;

// ガゾーナラベPDFのメインクラス。
class GazoNarabe
{
public:
    HINSTANCE m_hInstance;
    INT m_argc;
    LPTSTR *m_argv;
    HBITMAP m_hbmPreview;
    std::map<string_t, string_t> m_settings;
    std::vector<string_t> m_list;

    // コンストラクタ。
    GazoNarabe(HINSTANCE hInstance, INT argc, LPTSTR *argv);

    // デストラクタ。
    ~GazoNarabe()
    {
        ::DeleteObject(m_hbmPreview);
    }

    // データをリセットする。
    void Reset();
    // ダイアログを初期化する。
    void InitDialog(HWND hwnd);
    // ダイアログからデータへ。
    BOOL DataFromDialog(HWND hwnd, BOOL bList);
    // データからダイアログへ。
    BOOL DialogFromData(HWND hwnd, BOOL bList);
    // レジストリからデータへ。
    BOOL DataFromReg(HWND hwnd);
    // データからレジストリへ。
    BOOL RegFromData(HWND hwnd);

    // メインディッシュ処理。
    string_t JustDoIt(HWND hwnd);
};

// グローバル変数。
HINSTANCE g_hInstance = NULL; // インスタンス。
TCHAR g_szAppName[256] = TEXT(""); // アプリ名。
HICON g_hIcon = NULL; // アイコン（大）。
HICON g_hIconSm = NULL; // アイコン（小）。

SW_Shareware g_shareware(
    /* company registry key */      TEXT("Katayama Hirofumi MZ"),
    /* application registry key */  TEXT("GazoNarabePDF"),
    /* password hash */
    "e218f83f070a186f886c6dc82bd7ecf3d6c3ea4224fd7d213aa06e9c9713b395",
    /* trial days */                10,
    /* salt string */               "mJpDxx2D",
    /* version string */            "0.9.5");

// リソース文字列を読み込む。
LPTSTR doLoadString(INT nID)
{
    static TCHAR s_szText[1024];
    s_szText[0] = 0;
    LoadString(NULL, nID, s_szText, _countof(s_szText));
    return s_szText;
}

// ローカルのファイルのパス名を取得する。
LPCTSTR findLocalFile(LPCTSTR filename)
{
    // 現在のプログラムのパスファイル名を取得する。
    static TCHAR szPath[MAX_PATH];
    GetModuleFileName(NULL, szPath, _countof(szPath));

    // ファイルタイトルをfilenameで置き換える。
    PathRemoveFileSpec(szPath);
    PathAppend(szPath, filename);
    if (PathFileExists(szPath))
        return szPath;

    // 一つ上のフォルダへ。
    PathRemoveFileSpec(szPath);
    PathRemoveFileSpec(szPath);
    PathAppend(szPath, filename);
    if (PathFileExists(szPath))
        return szPath;

    // さらに一つ上のフォルダへ。
    PathRemoveFileSpec(szPath);
    PathRemoveFileSpec(szPath);
    PathRemoveFileSpec(szPath);
    PathAppend(szPath, filename);
    if (PathFileExists(szPath))
        return szPath;

    return NULL; // 見つからなかった。
}

// 不正な文字列が入力された。
void OnInvalidString(HWND hwnd, INT nItemID, INT nFieldId, INT nReasonId)
{
    SetFocus(GetDlgItem(hwnd, nItemID));
    string_t field = doLoadString(nFieldId);
    string_t reason = doLoadString(nReasonId);
    TCHAR szText[256];
    StringCchPrintf(szText, _countof(szText), doLoadString(IDS_INVALIDSTRING), field.c_str(), reason.c_str());
    MessageBox(hwnd, szText, g_szAppName, MB_ICONERROR);
}

// コンボボックスのテキストを取得する。
BOOL getComboText(HWND hwnd, INT id, LPTSTR text, INT cchMax)
{
    text[0] = 0;

    HWND hCombo = GetDlgItem(hwnd, id);
    INT iSel = ComboBox_GetCurSel(hCombo);
    if (iSel == CB_ERR) // コンボボックスに選択項目がなければ
    {
        // そのままテキストを取得する。
        ComboBox_GetText(hCombo, text, cchMax);
    }
    else
    {
        // リストからテキストを取得する。長さのチェックあり。
        if (ComboBox_GetLBTextLen(hCombo, iSel) >= cchMax)
        {
            StringCchCopy(text, cchMax, doLoadString(IDS_TEXTTOOLONG));
            return FALSE;
        }
        else
        {
            ComboBox_GetLBText(hCombo, iSel, text);
        }
    }

    return TRUE;
}

// コンボボックスのテキストを設定する。
BOOL setComboText(HWND hwnd, INT id, LPCTSTR text)
{
    // テキストに一致する項目を取得する。
    HWND hCombo = GetDlgItem(hwnd, id);
    INT iItem = ComboBox_FindStringExact(hCombo, -1, text);
    if (iItem == CB_ERR) // 一致する項目がなければ
        ComboBox_SetText(hCombo, text); // そのままテキストを設定。
    else
        ComboBox_SetCurSel(hCombo, iItem); // 一致する項目を選択。
    return TRUE;
}

// ワイド文字列をANSI文字列に変換する。
LPSTR ansi_from_wide(UINT codepage, LPCWSTR wide)
{
    static CHAR s_ansi[1024];
    WideCharToMultiByte(codepage, 0, wide, -1, s_ansi, _countof(s_ansi), NULL, NULL);
    return s_ansi;
}

// ANSI文字列をワイド文字列に変換する。
LPWSTR wide_from_ansi(UINT codepage, LPCSTR ansi)
{
    static WCHAR s_wide[1024];
    MultiByteToWideChar(codepage, 0, ansi, -1, s_wide, _countof(s_wide));
    return s_wide;
}

// mm単位からピクセル単位への変換。
double pixels_from_mm(double mm, double dpi = 72)
{
    return dpi * mm / 25.4;
}

// ピクセル単位からmm単位への変換。
double mm_from_pixels(double pixels, double dpi = 72)
{
    return 25.4 * pixels / dpi;
}

// ファイル名に使えない文字を下線文字に置き換える。
void validate_filename(string_t& filename)
{
    for (auto& ch : filename)
    {
        if (wcschr(L"\\/:*?\"<>|", ch) != NULL)
            ch = L'_';
    }
}

// 有効な画像ファイルかを確認する。
bool isValidImageFile(const string_t& item)
{
    // 有効なファイルか？
    LPCTSTR filename = item.c_str();
    if (!PathFileExists(filename) || PathIsDirectory(filename))
        return false;

    // GDI+などで読み込める画像ファイルか？
    if (gpimage_is_valid_extension(filename))
        return true;

    // Susie プラグインで読み込める画像ファイルか？
    if (g_susie.is_loaded())
    {
        auto ansi = ansi_from_wide(CP_ACP, item.c_str());
        auto dotext = PathFindExtensionA(ansi);
        if (g_susie.is_dotext_supported(dotext))
            return true;
    }

    return false; // 読み込めない。
}

// 画像を読み込む。
HBITMAP
doLoadPic(LPCWSTR filename, int* width = NULL, int* height = NULL,
          FILETIME* pftCreated = NULL, FILETIME* pftModified = NULL)
{
    // GDI+などで読み込みを試みる。
    HBITMAP hbm = gpimage_load(filename, width, height, pftCreated, pftModified);
    if (hbm)
        return hbm;

    // Susieプラグインを試す。SusieではANSI文字列を使用。
    auto ansi = ansi_from_wide(CP_ACP, filename);
    hbm = g_susie.load_image(ansi);
    if (hbm == NULL)
    {
        // ANSI文字列では表現できないパスファイル名かもしれない。
        // 一時ファイルを使用して再度挑戦。
        LPCWSTR dotext = PathFindExtension(filename);
        TempFile temp_file;
        temp_file.init(L"GN2", dotext);
        if (CopyFile(filename, temp_file.make(), FALSE))
        {
            ansi = ansi_from_wide(CP_ACP, temp_file.get());
            hbm = g_susie.load_image(ansi);
        }
    }
    if (hbm)
    {
        // 必要な情報を取得する。
        BITMAP bm;
        GetObject(hbm, sizeof(bm), &bm);
        if (width)
            *width = bm.bmWidth;
        if (height)
            *height = bm.bmHeight;
        gpimage_load_datetime(filename, pftCreated, pftModified);
        return hbm;
    }

    return NULL; // 失敗。
}

// コンストラクタ。
GazoNarabe::GazoNarabe(HINSTANCE hInstance, INT argc, LPTSTR *argv)
    : m_hInstance(hInstance)
    , m_argc(argc)
    , m_argv(argv)
    , m_hbmPreview(NULL)
{
    // データをリセットする。
    Reset();

    // コマンドライン引数をリストに追加。
    for (INT i = 1; i < m_argc; ++i)
    {
        // 有効な画像ファイルかを確認して追加。
        if (isValidImageFile(m_argv[i]))
            m_list.push_back(m_argv[i]);
    }
}

// データをリセットする。
void GazoNarabe::Reset()
{
#define SETTING(id) m_settings[TEXT(#id)]
    SETTING(IDC_PAGE_SIZE) = doLoadString(IDS_A4);
    SETTING(IDC_PAGE_DIRECTION) = doLoadString(IDS_LANDSCAPE);
    SETTING(IDC_MARGIN) = TEXT("8");
    SETTING(IDC_ROWS) = TEXT("2");
    SETTING(IDC_COLUMNS) = TEXT("2");
    SETTING(IDC_FONT_NAME) = doLoadString(IDS_FONT_01);
    SETTING(IDC_FONT_SIZE) = TEXT("18");
    SETTING(IDC_IMAGE_DATA_SIZE) = doLoadString(IDS_512KB);
    SETTING(IDC_IMAGE_TITLE) = doLoadString(IDS_PICTITLE_01);
    SETTING(IDC_OUTPUT_NAME) = doLoadString(IDS_OUTPUT_04);
    SETTING(IDC_DRAW_BORDER) = doLoadString(IDS_YES);
    SETTING(IDC_PAGE_NUMBERS) = doLoadString(IDS_YES);
    SETTING(IDC_DONT_RESIZE_SMALL) = doLoadString(IDS_NO);

    m_list.clear();
}

// ダイアログを初期化する。
void GazoNarabe::InitDialog(HWND hwnd)
{
    // IDC_PAGE_SIZE: 用紙サイズ。
    SendDlgItemMessage(hwnd, IDC_PAGE_SIZE, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_A3));
    SendDlgItemMessage(hwnd, IDC_PAGE_SIZE, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_A4));
    SendDlgItemMessage(hwnd, IDC_PAGE_SIZE, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_A5));
    SendDlgItemMessage(hwnd, IDC_PAGE_SIZE, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_B4));
    SendDlgItemMessage(hwnd, IDC_PAGE_SIZE, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_B5));

    // IDC_PAGE_DIRECTION: ページの向き。
    SendDlgItemMessage(hwnd, IDC_PAGE_DIRECTION, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_PORTRAIT));
    SendDlgItemMessage(hwnd, IDC_PAGE_DIRECTION, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_LANDSCAPE));

    // IDC_MARGIN: ページ余白(mm)。
    SendDlgItemMessage(hwnd, IDC_MARGIN, CB_ADDSTRING, 0, (LPARAM)TEXT("0"));
    SendDlgItemMessage(hwnd, IDC_MARGIN, CB_ADDSTRING, 0, (LPARAM)TEXT("8"));
    SendDlgItemMessage(hwnd, IDC_MARGIN, CB_ADDSTRING, 0, (LPARAM)TEXT("16"));

    // IDC_ROWS: 1ページの行数。
    SendDlgItemMessage(hwnd, IDC_ROWS, CB_ADDSTRING, 0, (LPARAM)TEXT("1"));
    SendDlgItemMessage(hwnd, IDC_ROWS, CB_ADDSTRING, 0, (LPARAM)TEXT("2"));
    SendDlgItemMessage(hwnd, IDC_ROWS, CB_ADDSTRING, 0, (LPARAM)TEXT("3"));
    SendDlgItemMessage(hwnd, IDC_ROWS, CB_ADDSTRING, 0, (LPARAM)TEXT("4"));
    SendDlgItemMessage(hwnd, IDC_ROWS, CB_ADDSTRING, 0, (LPARAM)TEXT("5"));

    // IDC_COLUMNS: 1ページの列数。
    SendDlgItemMessage(hwnd, IDC_COLUMNS, CB_ADDSTRING, 0, (LPARAM)TEXT("1"));
    SendDlgItemMessage(hwnd, IDC_COLUMNS, CB_ADDSTRING, 0, (LPARAM)TEXT("2"));
    SendDlgItemMessage(hwnd, IDC_COLUMNS, CB_ADDSTRING, 0, (LPARAM)TEXT("3"));
    SendDlgItemMessage(hwnd, IDC_COLUMNS, CB_ADDSTRING, 0, (LPARAM)TEXT("4"));
    SendDlgItemMessage(hwnd, IDC_COLUMNS, CB_ADDSTRING, 0, (LPARAM)TEXT("5"));

    // IDC_FONT_NAME: フォント名。
    SendDlgItemMessage(hwnd, IDC_FONT_NAME, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_FONT_01));
    SendDlgItemMessage(hwnd, IDC_FONT_NAME, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_FONT_02));
    SendDlgItemMessage(hwnd, IDC_FONT_NAME, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_FONT_03));
    SendDlgItemMessage(hwnd, IDC_FONT_NAME, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_FONT_04));

    // IDC_FONT_SIZE: フォントサイズ(pt)。
    SendDlgItemMessage(hwnd, IDC_FONT_SIZE, CB_ADDSTRING, 0, (LPARAM)TEXT("10"));
    SendDlgItemMessage(hwnd, IDC_FONT_SIZE, CB_ADDSTRING, 0, (LPARAM)TEXT("11"));
    SendDlgItemMessage(hwnd, IDC_FONT_SIZE, CB_ADDSTRING, 0, (LPARAM)TEXT("12"));
    SendDlgItemMessage(hwnd, IDC_FONT_SIZE, CB_ADDSTRING, 0, (LPARAM)TEXT("14"));
    SendDlgItemMessage(hwnd, IDC_FONT_SIZE, CB_ADDSTRING, 0, (LPARAM)TEXT("18"));
    SendDlgItemMessage(hwnd, IDC_FONT_SIZE, CB_ADDSTRING, 0, (LPARAM)TEXT("24"));

    // IDC_IMAGE_DATA_SIZE: 画像データサイズ。
    SendDlgItemMessage(hwnd, IDC_IMAGE_DATA_SIZE, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_64KB));
    SendDlgItemMessage(hwnd, IDC_IMAGE_DATA_SIZE, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_128KB));
    SendDlgItemMessage(hwnd, IDC_IMAGE_DATA_SIZE, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_256KB));
    SendDlgItemMessage(hwnd, IDC_IMAGE_DATA_SIZE, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_512KB));
    SendDlgItemMessage(hwnd, IDC_IMAGE_DATA_SIZE, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_1MB));
    SendDlgItemMessage(hwnd, IDC_IMAGE_DATA_SIZE, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_2MB));
    SendDlgItemMessage(hwnd, IDC_IMAGE_DATA_SIZE, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_NOLIMIT));

    // IDC_IMAGE_TITLE: 画像ラベル。
    SendDlgItemMessage(hwnd, IDC_IMAGE_TITLE, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_NOSPEC));
    SendDlgItemMessage(hwnd, IDC_IMAGE_TITLE, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_PICTITLE_01));
    SendDlgItemMessage(hwnd, IDC_IMAGE_TITLE, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_PICTITLE_02));
    SendDlgItemMessage(hwnd, IDC_IMAGE_TITLE, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_PICTITLE_03));
    SendDlgItemMessage(hwnd, IDC_IMAGE_TITLE, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_PICTITLE_04));
    SendDlgItemMessage(hwnd, IDC_IMAGE_TITLE, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_PICTITLE_05));
    SendDlgItemMessage(hwnd, IDC_IMAGE_TITLE, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_PICTITLE_06));
    SendDlgItemMessage(hwnd, IDC_IMAGE_TITLE, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_PICTITLE_07));

    // IDC_OUTPUT_NAME: 出力ファイル名。
    SendDlgItemMessage(hwnd, IDC_OUTPUT_NAME, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_OUTPUT_01));
    SendDlgItemMessage(hwnd, IDC_OUTPUT_NAME, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_OUTPUT_02));
    SendDlgItemMessage(hwnd, IDC_OUTPUT_NAME, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_OUTPUT_03));
    SendDlgItemMessage(hwnd, IDC_OUTPUT_NAME, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_OUTPUT_04));
    SendDlgItemMessage(hwnd, IDC_OUTPUT_NAME, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_OUTPUT_05));
    SendDlgItemMessage(hwnd, IDC_OUTPUT_NAME, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_OUTPUT_06));
}

// ダイアログからデータへ。
BOOL GazoNarabe::DataFromDialog(HWND hwnd, BOOL bList)
{
    TCHAR szText[MAX_PATH];

    // コンボボックスからデータを取得する。
#define GET_COMBO_DATA(id) do { \
    getComboText(hwnd, (id), szText, _countof(szText)); \
    StrTrim(szText, TEXT(" \t\r\n")); \
    m_settings[TEXT(#id)] = szText; \
} while (0)
    GET_COMBO_DATA(IDC_PAGE_SIZE);
    GET_COMBO_DATA(IDC_PAGE_DIRECTION);
    GET_COMBO_DATA(IDC_MARGIN);
    GET_COMBO_DATA(IDC_ROWS);
    GET_COMBO_DATA(IDC_COLUMNS);
    GET_COMBO_DATA(IDC_FONT_NAME);
    GET_COMBO_DATA(IDC_FONT_SIZE);
    GET_COMBO_DATA(IDC_IMAGE_DATA_SIZE);
    GET_COMBO_DATA(IDC_IMAGE_TITLE);
    GET_COMBO_DATA(IDC_OUTPUT_NAME);
#undef GET_COMBO_DATA

    // チェックボックスからデータを取得する。
#define GET_CHECK_DATA(id) do { \
    if (IsDlgButtonChecked(hwnd, id) == BST_CHECKED) \
        m_settings[TEXT(#id)] = doLoadString(IDS_YES); \
    else \
        m_settings[TEXT(#id)] = doLoadString(IDS_NO); \
} while (0)
    GET_CHECK_DATA(IDC_DRAW_BORDER);
    GET_CHECK_DATA(IDC_PAGE_NUMBERS);
    GET_CHECK_DATA(IDC_DONT_RESIZE_SMALL);
#undef GET_CHECK_DATA

    // 余白。ゼロ以上の実数を指定します。
    {
        auto& margin = SETTING(IDC_MARGIN);
        LCMapString(GetUserDefaultLCID(), LCMAP_HALFWIDTH, margin.c_str(), -1, szText, _countof(szText));
        WCHAR *endptr;
        double value = wcstod(szText, &endptr);
        if (*endptr != 0 || value < 0)
        {
            OnInvalidString(hwnd, IDC_MARGIN, IDS_FIELD_MARGIN, IDS_REASON_POSITIVE_REAL);
            SETTING(IDC_MARGIN) = TEXT("8");
            return FALSE;
        }
        margin = szText;
    }
    // 1ページの行数。正の自然数ですよね。
    {
        auto& rows = SETTING(IDC_ROWS);
        LCMapString(GetUserDefaultLCID(), LCMAP_HALFWIDTH, rows.c_str(), -1, szText, _countof(szText));
        WCHAR *endptr;
        long value = wcstol(szText, &endptr, 10);
        if (*endptr != 0 || value <= 0)
        {
            OnInvalidString(hwnd, IDC_ROWS, IDS_FIELD_ROWS, IDS_REASON_POSITIVE_INTEGER);
            SETTING(IDC_ROWS) = TEXT("2");
            return FALSE;
        }
        rows = szText;
    }
    // 1ページの列数。正の自然数ですよね。
    {
        auto& columns = SETTING(IDC_COLUMNS);
        LCMapString(GetUserDefaultLCID(), LCMAP_HALFWIDTH, columns.c_str(), -1, szText, _countof(szText));
        WCHAR *endptr;
        long value = wcstol(szText, &endptr, 10);
        if (*endptr != 0 || value <= 0)
        {
            OnInvalidString(hwnd, IDC_COLUMNS, IDS_FIELD_COLUMNS, IDS_REASON_POSITIVE_INTEGER);
            SETTING(IDC_COLUMNS) = TEXT("2");
            return FALSE;
        }
        columns = szText;
    }
    // フォントサイズ(pt)。ゼロや負ではありません。
    {
        auto& font_size = SETTING(IDC_FONT_SIZE);
        LCMapString(GetUserDefaultLCID(), LCMAP_HALFWIDTH, font_size.c_str(), -1, szText, _countof(szText));
        WCHAR *endptr;
        double value = wcstod(szText, &endptr);
        if (*endptr != 0 || value <= 0)
        {
            OnInvalidString(hwnd, IDC_FONT_SIZE, IDS_FIELD_FONT_SIZE, IDS_REASON_POSITIVE_REAL);
            SETTING(IDC_FONT_SIZE) = TEXT("18");
            return FALSE;
        }
        font_size = szText;
    }
    // 出力ファイル名。空ではありません。
    {
        auto& output_name = SETTING(IDC_OUTPUT_NAME);
        if (output_name.empty())
        {
            OnInvalidString(hwnd, IDC_OUTPUT_NAME, IDS_FIELD_OUTPUT_NAME, IDS_REASON_NON_EMPTY_STRING);
            SETTING(IDC_OUTPUT_NAME) = doLoadString(IDS_OUTPUT_04);
            return FALSE;
        }
    }

    // リストボックスからデータを取得する。
    if (bList)
    {
        m_list.clear();
        HWND hLst1 = GetDlgItem(hwnd, lst1);
        INT iItem, cItems = ListBox_GetCount(hLst1);
        for (iItem = 0; iItem < cItems; ++iItem)
        {
            if (ListBox_GetTextLen(hLst1, iItem) < _countof(szText))
            {
                ListBox_GetText(hLst1, iItem, szText);
                m_list.push_back(szText);
            }
        }

        if (m_list.empty())
        {
            ::MessageBox(hwnd, doLoadString(IDS_LISTISEMPTY), g_szAppName, MB_ICONERROR);
            return FALSE;
        }
    }

    return TRUE;
}

// データからダイアログへ。
BOOL GazoNarabe::DialogFromData(HWND hwnd, BOOL bList)
{
    // コンボボックスへデータを設定する。
#define SET_COMBO_DATA(id) \
    setComboText(hwnd, (id), m_settings[TEXT(#id)].c_str());
    SET_COMBO_DATA(IDC_PAGE_SIZE);
    SET_COMBO_DATA(IDC_PAGE_DIRECTION);
    SET_COMBO_DATA(IDC_MARGIN);
    SET_COMBO_DATA(IDC_ROWS);
    SET_COMBO_DATA(IDC_COLUMNS);
    SET_COMBO_DATA(IDC_FONT_NAME);
    SET_COMBO_DATA(IDC_FONT_SIZE);
    SET_COMBO_DATA(IDC_IMAGE_DATA_SIZE);
    SET_COMBO_DATA(IDC_IMAGE_TITLE);
    SET_COMBO_DATA(IDC_OUTPUT_NAME);
#undef SET_COMBO_DATA

    // チェックボックスへデータを設定する。
#define SET_CHECK_DATA(id) do { \
    if (m_settings[TEXT(#id)] == doLoadString(IDS_YES)) \
        CheckDlgButton(hwnd, (id), BST_CHECKED); \
    else \
        CheckDlgButton(hwnd, (id), BST_UNCHECKED); \
} while (0)
    SET_CHECK_DATA(IDC_DRAW_BORDER);
    SET_CHECK_DATA(IDC_PAGE_NUMBERS);
    SET_CHECK_DATA(IDC_DONT_RESIZE_SMALL);
#undef SET_CHECK_DATA

    // リストボックスへデータを設定する。
    if (bList)
    {
        HWND hLst1 = GetDlgItem(hwnd, lst1);
        ListBox_ResetContent(hLst1);
        for (auto& item : m_list)
        {
            ListBox_AddString(hLst1, item.c_str());
        }
    }

    return TRUE;
}

// レジストリからデータへ。
BOOL GazoNarabe::DataFromReg(HWND hwnd)
{
    // ソフト固有のレジストリキーを開く。
    HKEY hKey;
    RegOpenKeyEx(HKEY_CURRENT_USER, TEXT("Software\\Katayama Hirofumi MZ\\GazoNarabePDF"), 0, KEY_READ, &hKey);
    if (!hKey)
        return FALSE; // 開けなかった。

    // レジストリからデータを取得する。
    TCHAR szText[MAX_PATH];
#define GET_REG_DATA(id) do { \
    szText[0] = 0; \
    DWORD cbText = sizeof(szText); \
    LONG error = RegQueryValueEx(hKey, TEXT(#id), NULL, NULL, (LPBYTE)szText, &cbText); \
    if (error == ERROR_SUCCESS) { \
        SETTING(id) = szText; \
    } \
} while(0)
    GET_REG_DATA(IDC_PAGE_SIZE);
    GET_REG_DATA(IDC_PAGE_DIRECTION);
    GET_REG_DATA(IDC_MARGIN);
    GET_REG_DATA(IDC_ROWS);
    GET_REG_DATA(IDC_COLUMNS);
    GET_REG_DATA(IDC_FONT_NAME);
    GET_REG_DATA(IDC_FONT_SIZE);
    GET_REG_DATA(IDC_IMAGE_DATA_SIZE);
    GET_REG_DATA(IDC_IMAGE_TITLE);
    GET_REG_DATA(IDC_OUTPUT_NAME);
    GET_REG_DATA(IDC_DRAW_BORDER);
    GET_REG_DATA(IDC_PAGE_NUMBERS);
    GET_REG_DATA(IDC_DONT_RESIZE_SMALL);
#undef GET_REG_DATA

    // 余白。ゼロ以上の実数を指定します。
    {
        auto& margin = SETTING(IDC_MARGIN);
        LCMapString(GetUserDefaultLCID(), LCMAP_HALFWIDTH, margin.c_str(), -1, szText, _countof(szText));
        WCHAR *endptr;
        double value = wcstod(szText, &endptr);
        if (*endptr != 0 || value < 0)
        {
            SETTING(IDC_MARGIN) = TEXT("8");
        }
        margin = szText;
    }
    // 1ページの行数。正の自然数ですよね。
    {
        auto& rows = SETTING(IDC_ROWS);
        LCMapString(GetUserDefaultLCID(), LCMAP_HALFWIDTH, rows.c_str(), -1, szText, _countof(szText));
        WCHAR *endptr;
        long value = wcstol(szText, &endptr, 10);
        if (*endptr != 0 || value <= 0)
        {
            SETTING(IDC_ROWS) = TEXT("2");
        }
        rows = szText;
    }
    // 1ページの列数。正の自然数ですよね。
    {
        auto& columns = SETTING(IDC_COLUMNS);
        LCMapString(GetUserDefaultLCID(), LCMAP_HALFWIDTH, columns.c_str(), -1, szText, _countof(szText));
        WCHAR *endptr;
        long value = wcstol(szText, &endptr, 10);
        if (*endptr != 0 || value <= 0)
        {
            SETTING(IDC_COLUMNS) = TEXT("2");
        }
        columns = szText;
    }
    // フォントサイズ(pt)。ゼロや負ではありません。
    {
        auto& font_size = SETTING(IDC_FONT_SIZE);
        LCMapString(GetUserDefaultLCID(), LCMAP_HALFWIDTH, font_size.c_str(), -1, szText, _countof(szText));
        WCHAR *endptr;
        double value = wcstod(szText, &endptr);
        if (*endptr != 0 || value <= 0)
        {
            SETTING(IDC_FONT_SIZE) = TEXT("18");
        }
        font_size = szText;
    }
    // 出力ファイル名。空ではありません。
    {
        auto& output_name = SETTING(IDC_OUTPUT_NAME);
        if (output_name.empty())
        {
            SETTING(IDC_OUTPUT_NAME) = doLoadString(IDS_OUTPUT_04);
        }
    }

    // レジストリキーを閉じる。
    RegCloseKey(hKey);
    return TRUE; // 成功。
}

// データからレジストリへ。
BOOL GazoNarabe::RegFromData(HWND hwnd)
{
    HKEY hCompanyKey = NULL, hAppKey = NULL;

    // 会社固有のレジストリキーを作成または開く。
    RegCreateKey(HKEY_CURRENT_USER, TEXT("Software\\Katayama Hirofumi MZ"), &hCompanyKey);
    if (hCompanyKey == NULL)
        return FALSE; // 失敗。

    // ソフト固有のレジストリキーを作成または開く。
    RegCreateKey(hCompanyKey, TEXT("GazoNarabePDF"), &hAppKey);
    if (hAppKey == NULL)
    {
        RegCloseKey(hCompanyKey);
        return FALSE; // 失敗。
    }

    // レジストリにデータを設定する。
#define SET_REG_DATA(id) do { \
    auto& str = m_settings[TEXT(#id)]; \
    DWORD cbText = (str.size() + 1) * sizeof(WCHAR); \
    RegSetValueEx(hAppKey, TEXT(#id), 0, REG_SZ, (LPBYTE)str.c_str(), cbText); \
} while(0)
    SET_REG_DATA(IDC_PAGE_SIZE);
    SET_REG_DATA(IDC_PAGE_DIRECTION);
    SET_REG_DATA(IDC_MARGIN);
    SET_REG_DATA(IDC_ROWS);
    SET_REG_DATA(IDC_COLUMNS);
    SET_REG_DATA(IDC_FONT_NAME);
    SET_REG_DATA(IDC_FONT_SIZE);
    SET_REG_DATA(IDC_IMAGE_DATA_SIZE);
    SET_REG_DATA(IDC_IMAGE_TITLE);
    SET_REG_DATA(IDC_OUTPUT_NAME);
    SET_REG_DATA(IDC_DRAW_BORDER);
    SET_REG_DATA(IDC_PAGE_NUMBERS);
    SET_REG_DATA(IDC_DONT_RESIZE_SMALL);
#undef SET_REG_DATA

    // レジストリキーを閉じる。
    RegCloseKey(hAppKey);
    RegCloseKey(hCompanyKey);

    return TRUE; // 成功。
}

// 文字列中に見つかった部分文字列をすべて置き換える。
template <typename T_STR>
inline bool
str_replace(T_STR& str, const T_STR& from, const T_STR& to)
{
    bool ret = false;
    size_t i = 0;
    for (;;) {
        i = str.find(from, i);
        if (i == T_STR::npos)
            break;
        ret = true;
        str.replace(i, from.size(), to);
        i += to.size();
    }
    return ret;
}
template <typename T_STR>
inline bool
str_replace(T_STR& str,
            const typename T_STR::value_type *from,
            const typename T_STR::value_type *to)
{
    return str_replace(str, T_STR(from), T_STR(to));
}

// ファイルサイズを取得する。
DWORD get_file_size(const string_t& filename)
{
    WIN32_FIND_DATA find;
    HANDLE hFind = FindFirstFile(filename.c_str(), &find);
    if (hFind == INVALID_HANDLE_VALUE)
        return 0; // エラー。
    FindClose(hFind);
    if (find.nFileSizeHigh)
        return 0; // 大きすぎるのでエラー。
    return find.nFileSizeLow; // ファイルサイズ。
}

// 特殊タグを「(ERR)」に置き換える。
void str_replace_tag_error(string_t& str)
{
    int in_tag = 0;
    string_t output;

    for (auto& ch : str)
    {
        if (ch == L'<')
        {
            ++in_tag;
        }
        else if (ch == L'>')
        {
            --in_tag;
            if (in_tag == 0)
            {
                output += L"(ERR)";
            }
        }
        else if (!in_tag)
        {
            output += ch;
        }
    }

    if (in_tag > 0)
    {
        output += L"(ERR)";
    }

    str = std::move(output);
}

// タグ内を小文字にする。
void str_lowercase_tags(string_t& str)
{
    int in_tag = 0;
    for (auto& ch : str)
    {
        if (ch == L'<')
        {
            ++in_tag;
        }
        else if (ch == L'>')
        {
            --in_tag;
        }
        else if (in_tag > 0)
        {
            ch = (WCHAR)(DWORD_PTR)CharLowerW((LPWSTR)(DWORD_PTR)ch);
        }
    }
}

// 特殊タグを規則に従って置き換える。
bool substitute_tags(string_t& str, const string_t& pathname,
                     INT iImage, INT cImages, INT iPage, INT cPages, bool is_output = false)
{
    // タグ内を小文字にする。
    str_lowercase_tags(str);

    // 画像ファイルに関するメタデータを取得する。
    int image_width, image_height;
    FILETIME ftCreated, ftModified;
    HBITMAP hbm = doLoadPic(pathname.c_str(), &image_width, &image_height, &ftCreated, &ftModified);
    if (hbm == NULL)
        return false;
    ::DeleteObject(hbm);

    // ファイルサイズを取得する。
    DWORD file_size = get_file_size(pathname.c_str());
    if (file_size == 0)
        return false;

    // 日時に関する情報を取得・変換する。
    SYSTEMTIME stNow, stCreated, stModified;
    ::GetLocalTime(&stNow);
    ::FileTimeToSystemTime(&ftCreated, &stCreated);
    ::FileTimeToSystemTime(&ftModified, &stModified);

    // 現在の日時を文字列にする。
    TCHAR now_date[64];
    TCHAR now_time[64];
    TCHAR now_datetime[128];
    StringCchPrintf(now_date, _countof(now_date), TEXT("%04u-%02u-%02u"), stNow.wYear, stNow.wMonth, stNow.wDay);
    StringCchPrintf(now_time, _countof(now_time), TEXT("%02u:%02u:%02u"), stNow.wHour, stNow.wMinute, stNow.wSecond);
    StringCchCopy(now_datetime, _countof(now_datetime), now_date);
    StringCchCat(now_datetime, _countof(now_datetime), TEXT(" "));
    StringCchCat(now_datetime, _countof(now_datetime), now_time);

    // 作成日時を文字列にする。
    TCHAR created_date[64];
    TCHAR created_time[64];
    TCHAR created_datetime[128];
    StringCchPrintf(created_date, _countof(created_date), TEXT("%04u-%02u-%02u"), stCreated.wYear, stCreated.wMonth, stCreated.wDay);
    StringCchPrintf(created_time, _countof(created_time), TEXT("%02u:%02u:%02u"), stCreated.wHour, stCreated.wMinute, stCreated.wSecond);
    StringCchCopy(created_datetime, _countof(created_datetime), created_date);
    StringCchCat(created_datetime, _countof(created_datetime), TEXT(" "));
    StringCchCat(created_datetime, _countof(created_datetime), created_time);

    // 更新日時を文字列にする。
    TCHAR modified_date[64];
    TCHAR modified_time[64];
    TCHAR modified_datetime[128];
    StringCchPrintf(modified_date, _countof(modified_date), TEXT("%04u-%02u-%02u"), stModified.wYear, stModified.wMonth, stModified.wDay);
    StringCchPrintf(modified_time, _countof(modified_time), TEXT("%02u:%02u:%02u"), stModified.wHour, stModified.wMinute, stModified.wSecond);
    StringCchCopy(modified_datetime, _countof(modified_datetime), modified_date);
    StringCchCat(modified_datetime, _countof(modified_datetime), TEXT(" "));
    StringCchCat(modified_datetime, _countof(modified_datetime), modified_time);

    // 短いパス名を取得する。
    TCHAR shortpathname[MAX_PATH];
    GetShortPathName(pathname.c_str(), shortpathname, _countof(shortpathname));

    // パスファイル名のファイルタイトルを取得する。
    LPCTSTR filename = PathFindFileName(pathname.c_str());
    LPCTSTR shortname = PathFindFileName(shortpathname);

    // ファイル名に関してタグを置き換える。
    if (is_output)
    {
        str_replace(str, L"<pathname>", L"(ERR)");
        str_replace(str, L"<filename>", L"(ERR)");
        str_replace(str, L"<short-name>", L"(ERR)");
    }
    else
    {
        str_replace(str, L"<pathname>", pathname.c_str());
        str_replace(str, L"<filename>", filename);
        str_replace(str, L"<short-name>", shortname);
    }

    // 画像番号・画像総数に関してタグを置き換える。
    str_replace(str, L"<image-no>", std::to_wstring(iImage + 1).c_str());
    str_replace(str, L"<image-total>", std::to_wstring(cImages).c_str());

    // ページ番号・ページ総数に関してタグを置き換える。
    str_replace(str, L"<page-no>", std::to_wstring(iPage + 1).c_str());
    str_replace(str, L"<page-total>", std::to_wstring(cPages).c_str());

    // 現在の日時に関してタグを置き換える。
    str_replace(str, L"<date>", now_date);
    str_replace(str, L"<time>", now_time);
    str_replace(str, L"<datetime>", now_datetime);
    str_replace(str, L"<year>", std::to_wstring(stNow.wYear).c_str());
    str_replace(str, L"<month>", std::to_wstring(stNow.wMonth).c_str());
    str_replace(str, L"<day>", std::to_wstring(stNow.wDay).c_str());
    str_replace(str, L"<today>", now_date);
    str_replace(str, L"<now>", now_datetime);

    // 撮影日時に関してタグを置き換える。
    str_replace(str, L"<date-shoot>", created_date);
    str_replace(str, L"<time-shoot>", created_time);
    str_replace(str, L"<datetime-shoot>", created_datetime);

    // 更新日時に関してタグを置き換える。
    str_replace(str, L"<date-modified>", created_date);
    str_replace(str, L"<time-modified>", created_time);
    str_replace(str, L"<datetime-modified>", created_datetime);

    // 画像の幅・高さ・ファイルサイズに関してタグを置き換える。
    str_replace(str, L"<width>", std::to_wstring(image_width).c_str());
    str_replace(str, L"<height>", std::to_wstring(image_height).c_str());
    str_replace(str, L"<file-size>", std::to_wstring(file_size).c_str());

    //
    // Japanese special
    //

    // ファイル名に関してタグを置き換える。
    if (is_output)
    {
        str_replace(str, doLoadString(IDS_TAG_PATHNAME), L"(ERR)");
        str_replace(str, doLoadString(IDS_TAG_FILENAME), L"(ERR)");
        str_replace(str, doLoadString(IDS_TAG_SHORTNAME), L"(ERR)");
    }
    else
    {
        str_replace(str, doLoadString(IDS_TAG_PATHNAME), pathname.c_str());
        str_replace(str, doLoadString(IDS_TAG_FILENAME), filename);
        str_replace(str, doLoadString(IDS_TAG_SHORTNAME), shortname);
    }

    // 画像番号・画像総数に関してタグを置き換える。
    str_replace(str, doLoadString(IDS_TAG_IMAGENO), std::to_wstring(iImage + 1).c_str());
    str_replace(str, doLoadString(IDS_TAG_IMAGETOTAL), std::to_wstring(cImages).c_str());

    // ページ番号・ページ総数に関してタグを置き換える。
    str_replace(str, doLoadString(IDS_TAG_PAGENO), std::to_wstring(iPage + 1).c_str());
    str_replace(str, doLoadString(IDS_TAG_PAGETOTAL), std::to_wstring(cPages).c_str());

    // 現在の日時に関してタグを置き換える。
    StringCchPrintf(now_date, _countof(now_date), doLoadString(IDS_DATEFORMAT), stNow.wYear, stNow.wMonth, stNow.wDay);
    StringCchPrintf(now_time, _countof(now_time), doLoadString(IDS_TIMEFORMAT), stNow.wHour, stNow.wMinute, stNow.wSecond);
    StringCchCopy(now_datetime, _countof(now_datetime), now_date);
    StringCchCat(now_datetime, _countof(now_datetime), now_time);
    str_replace(str, doLoadString(IDS_TAG_DATE), now_date);
    str_replace(str, doLoadString(IDS_TAG_TIME), now_time);
    str_replace(str, doLoadString(IDS_TAG_DATETIME), now_datetime);
    str_replace(str, doLoadString(IDS_TAG_YEAR), std::to_wstring(stNow.wYear).c_str());
    str_replace(str, doLoadString(IDS_TAG_MONTH), std::to_wstring(stNow.wMonth).c_str());
    str_replace(str, doLoadString(IDS_TAG_DAY), std::to_wstring(stNow.wDay).c_str());
    str_replace(str, doLoadString(IDS_TAG_TODAY), now_date);
    str_replace(str, doLoadString(IDS_TAG_NOW), now_datetime);

    // 撮影日時に関してタグを置き換える。
    StringCchPrintf(created_date, _countof(created_date), doLoadString(IDS_DATEFORMAT), stCreated.wYear, stCreated.wMonth, stCreated.wDay);
    StringCchPrintf(created_time, _countof(created_time), doLoadString(IDS_TIMEFORMAT), stCreated.wHour, stCreated.wMinute, stCreated.wSecond);
    StringCchCopy(created_datetime, _countof(created_datetime), created_date);
    StringCchCat(created_datetime, _countof(created_datetime), created_time);
    str_replace(str, doLoadString(IDS_TAG_DATE_SHOOT), created_date);
    str_replace(str, doLoadString(IDS_TAG_TIME_SHOOT), created_time);
    str_replace(str, doLoadString(IDS_TAG_DATETIME_SHOOT), created_datetime);

    // 更新日時に関してタグを置き換える。
    StringCchPrintf(modified_date, _countof(modified_date), doLoadString(IDS_DATEFORMAT), stModified.wYear, stModified.wMonth, stModified.wDay);
    StringCchPrintf(modified_time, _countof(modified_time), doLoadString(IDS_TIMEFORMAT), stModified.wHour, stModified.wMinute, stModified.wSecond);
    StringCchCopy(modified_datetime, _countof(modified_datetime), modified_date);
    StringCchCat(modified_datetime, _countof(modified_datetime), modified_time);
    str_replace(str, doLoadString(IDS_TAG_DATE_MODIFIED), modified_date);
    str_replace(str, doLoadString(IDS_TAG_TIME_MODIFIED), modified_time);
    str_replace(str, doLoadString(IDS_TAG_DATETIME_MODIFIED), modified_datetime);

    // 画像の幅・高さ・ファイルサイズに関してタグを置き換える。
    str_replace(str, doLoadString(IDS_TAG_WIDTH), std::to_wstring(image_width).c_str());
    str_replace(str, doLoadString(IDS_TAG_HEIGHT), std::to_wstring(image_height).c_str());
    str_replace(str, doLoadString(IDS_TAG_FILESIZE), std::to_wstring(file_size).c_str());

    // その他のタグを「(ERR)」に置き換える。
    str_replace_tag_error(str);

    return true;
}

// libHaruのエラーハンドラの実装。
void hpdf_error_handler(HPDF_STATUS error_no, HPDF_STATUS detail_no, void* user_data) {
    char message[1024];
    StringCchPrintfA(message, _countof(message), "error: error_no = %04X, detail_no = %d",
                     UINT(error_no), INT(detail_no));
    throw std::runtime_error(message);
}

// 長方形を描画する。
void hpdf_draw_box(HPDF_Page page, double x, double y, double width, double height)
{
    HPDF_Page_MoveTo(page, x, y);
    HPDF_Page_LineTo(page, x, y + height);
    HPDF_Page_LineTo(page, x + width, y + height);
    HPDF_Page_LineTo(page, x + width, y);
    HPDF_Page_ClosePath(page);
    HPDF_Page_Stroke(page);
}

// 画像を描く。
bool hpdf_draw_image(HPDF_Doc pdf, HPDF_Page page, double x, double y, double width, double height, const string_t& filename,
                     DWORD max_data_size, bool dont_resize_small)
{
    // ファイルサイズを取得する。
    DWORD file_size = get_file_size(filename);
    if (file_size == 0)
        return false;

    // 画像をHBITMAPとして読み込む。
    int image_width, image_height;
    HBITMAP hbm = doLoadPic(filename.c_str(), &image_width, &image_height);
    if (hbm == NULL)
        return false;

    // JPEG/TIFFではないビットマップ画像については特別扱いで画像の品質を向上させることができる。
    bool png_is_better = false;
    auto dotext = PathFindExtension(filename.c_str());
    if (lstrcmpi(dotext, TEXT(".png")) == 0 ||
        lstrcmpi(dotext, TEXT(".gif")) == 0 ||
        lstrcmpi(dotext, TEXT(".bmp")) == 0 ||
        lstrcmpi(dotext, TEXT(".dib")) == 0 ||
        lstrcmpi(dotext, TEXT(".ico")) == 0)
    {
        png_is_better = true;
    }

    TempFile tempfile;
    for (;;)
    {
        file_size = 0;

        // 一時ファイルを作成し、画像を保存する。
        if (png_is_better)
            tempfile.init(TEXT("GN2"), TEXT(".png"));
        else
            tempfile.init(TEXT("GN2"), TEXT(".jpg"));
        if (!gpimage_save(tempfile.make(), hbm))
            break;

        // ファイルサイズを取得する。
        file_size = get_file_size(tempfile.get());
        if (file_size == 0)
            break; // 失敗。

        // ファイルサイズは妥当か？
        if (max_data_size == 0 || file_size <= max_data_size)
            break; // 妥当。

        // 画像サイズを縮小した画像を作成する。ゼロにならないように補正する。
        image_width /= 2;
        image_height /= 2;
        if (image_width <= 0)
            image_width = 1;
        if (image_height <= 0)
            image_height = 1;
        HBITMAP hbmNew = gpimage_resize(hbm, image_width, image_height);
        if (hbmNew == NULL)
        {
            file_size = 0;
            break;
        }

        // hbmを縮小した画像に置き換える。
        ::DeleteObject(hbm);
        hbm = hbmNew;

        // 一時ファイルを削除。
        tempfile.erase();
    }

    // ファイルサイズがゼロなら、失敗。
    if (file_size == 0)
    {
        ::DeleteObject(hbm);
        return false; // 失敗。
    }

    // 画像を読み込む。
    HPDF_Image image;
    if (png_is_better)
        image = HPDF_LoadPngImageFromFile(pdf, ansi_from_wide(CP_ACP, tempfile.get()));
    else
        image = HPDF_LoadJpegImageFromFile(pdf, ansi_from_wide(CP_ACP, tempfile.get()));
    if (image == NULL)
    {
        ::DeleteObject(hbm);
        return false; // 失敗。
    }

    // 画像サイズとアスペクト比に従って処理を行う。
    double aspect1 = (double)image_width / (double)image_height;
    double aspect2 = width / height;
    double stretch_width, stretch_height;
    if (image_width <= width && image_height <= height && dont_resize_small)
    {
        // 小さい画像を拡大しない。
        stretch_width = image_width;
        stretch_height = image_height;
    }
    else
    {
        // 大きい画像はアスペクト比に従ってセルいっぱいに縮小する。
        if (aspect1 <= aspect2)
        {
            stretch_height = height;
            stretch_width = height * aspect1;
        }
        else
        {
            stretch_width = width;
            stretch_height = width / aspect1;
        }
    }

    // ゼロにならないように補正する。
    if (stretch_width <= 0)
        stretch_width = 1;
    if (stretch_height <= 0)
        stretch_height = 1;

    // 画像を配置する。
    double dx = (width - stretch_width) / 2;
    double dy = (height - stretch_height) / 2;
    HPDF_Page_DrawImage(page, image, x + dx, y + dy, stretch_width, stretch_height);

    // HBITMAPを破棄する。
    ::DeleteObject(hbm);

    return true;
}

// テキストを描画する。
void hpdf_draw_text(HPDF_Page page, HPDF_Font font, double font_size,
                    const char *text,
                    double x, double y, double width, double height,
                    int draw_box = 0)
{
    // フォントサイズを制限。
    if (font_size > HPDF_MAX_FONTSIZE)
        font_size = HPDF_MAX_FONTSIZE;

    // 長方形を描画する。
    if (draw_box == 1)
    {
        hpdf_draw_box(page, x, y, width, height);
    }

    // 長方形に収まるフォントサイズを計算する。
    double text_width, text_height;
    for (;;)
    {
        // フォントとフォントサイズを指定。
        HPDF_Page_SetFontAndSize(page, font, font_size);

        // テキストの幅と高さを取得する。
        text_width = HPDF_Page_TextWidth(page, text);
        text_height = HPDF_Page_GetCurrentFontSize(page);

        // テキストが長方形に収まるか？
        if (text_width <= width && text_height <= height)
        {
            // x,yを中央そろえ。
            x += (width - text_width) / 2;
            y += (height - text_height) / 2;
            break;
        }

        // フォントサイズを少し小さくして再計算。
        font_size *= 0.8;
    }

    // テキストを描画する。
    HPDF_Page_BeginText(page);
    {
        // ベースラインからdescentだけずらす。
        double descent = -HPDF_Font_GetDescent(font) * font_size / 1000.0;
        HPDF_Page_TextOut(page, x, y + descent, text);
    }
    HPDF_Page_EndText(page);

    // 長方形を描画する。
    if (draw_box == 2)
    {
        hpdf_draw_box(page, x, y, text_width, text_height);
    }
}

// メインディッシュ処理。
string_t GazoNarabe::JustDoIt(HWND hwnd)
{
    string_t ret;
    // PDFオブジェクトを作成する。
    HPDF_Doc pdf = HPDF_New(hpdf_error_handler, NULL);
    if (!pdf)
        return L"";

    try
    {
        // エンコーディング 90ms-RKSJ-H, 90ms-RKSJ-V, 90msp-RKSJ-H, EUC-H, EUC-V が利用可能となる
        HPDF_UseJPEncodings(pdf);

        // 日本語フォントの MS-(P)Mincyo, MS-(P)Gothic が利用可能となる
        HPDF_UseJPFonts(pdf);

        // 用紙の向き。
        HPDF_PageDirection direction;
        if (SETTING(IDC_PAGE_DIRECTION) == doLoadString(IDS_PORTRAIT))
            direction = HPDF_PAGE_PORTRAIT;
        else if (SETTING(IDC_PAGE_DIRECTION) == doLoadString(IDS_LANDSCAPE))
            direction = HPDF_PAGE_LANDSCAPE;
        else
            direction = HPDF_PAGE_LANDSCAPE;

        // ページサイズ。
        HPDF_PageSizes page_size;
        if (SETTING(IDC_PAGE_SIZE) == doLoadString(IDS_A3))
            page_size = HPDF_PAGE_SIZE_A3;
        else if (SETTING(IDC_PAGE_SIZE) == doLoadString(IDS_A4))
            page_size = HPDF_PAGE_SIZE_A4;
        else if (SETTING(IDC_PAGE_SIZE) == doLoadString(IDS_A5))
            page_size = HPDF_PAGE_SIZE_A5;
        else if (SETTING(IDC_PAGE_SIZE) == doLoadString(IDS_B4))
            page_size = HPDF_PAGE_SIZE_B4;
        else if (SETTING(IDC_PAGE_SIZE) == doLoadString(IDS_B5))
            page_size = HPDF_PAGE_SIZE_B5;
        else
            page_size = HPDF_PAGE_SIZE_A4;

        // ページ余白。
        double margin = pixels_from_mm(_wtof(SETTING(IDC_MARGIN).c_str()));

        // 1ページの行数。
        int rows = _ttoi(SETTING(IDC_ROWS).c_str());
        // 1ページの列数。
        int columns = _ttoi(SETTING(IDC_COLUMNS).c_str());

        // 枠線を描画するか？
        bool draw_border = SETTING(IDC_DRAW_BORDER) == doLoadString(IDS_YES);
        // ページ番号を付けるか？
        bool page_numbers = SETTING(IDC_PAGE_NUMBERS) == doLoadString(IDS_YES);
        // 小さい画像は拡大しないか？
        bool dont_resize_small = SETTING(IDC_DONT_RESIZE_SMALL) == doLoadString(IDS_YES);

        // フォント名。
        string_t font_name;
        if (SETTING(IDC_FONT_NAME) == doLoadString(IDS_FONT_01))
            font_name = TEXT("MS-PGothic");
        else if (SETTING(IDC_FONT_NAME) == doLoadString(IDS_FONT_02))
            font_name = TEXT("MS-PMincho");
        else if (SETTING(IDC_FONT_NAME) == doLoadString(IDS_FONT_03))
            font_name = TEXT("MS-Gothic");
        else if (SETTING(IDC_FONT_NAME) == doLoadString(IDS_FONT_04))
            font_name = TEXT("MS-Mincho");
        else
            font_name = TEXT("MS-PGothic");

        // フォントサイズ（pt）。
        double font_size = _wtof(SETTING(IDC_FONT_SIZE).c_str());

        // 画像データサイズ。
        int max_data_size = 0;
        if (SETTING(IDC_IMAGE_DATA_SIZE) == doLoadString(IDS_64KB))
            max_data_size = 1024 * 64;
        else if (SETTING(IDC_IMAGE_DATA_SIZE) == doLoadString(IDS_128KB))
            max_data_size = 1024 * 128;
        else if (SETTING(IDC_IMAGE_DATA_SIZE) == doLoadString(IDS_256KB))
            max_data_size = 1024 * 256;
        else if (SETTING(IDC_IMAGE_DATA_SIZE) == doLoadString(IDS_512KB))
            max_data_size = 1024 * 512;
        else if (SETTING(IDC_IMAGE_DATA_SIZE) == doLoadString(IDS_1MB))
            max_data_size = 1024 * 1024;
        else if (SETTING(IDC_IMAGE_DATA_SIZE) == doLoadString(IDS_2MB))
            max_data_size = 1024 * 1024 * 2;
        else
            max_data_size = 0;

        // 画像ラベル。
        string_t image_title = SETTING(IDC_IMAGE_TITLE);
        if (image_title == doLoadString(IDS_NOSPEC))
            image_title.clear();

        // 出力ファイル名。
        string_t output_name = SETTING(IDC_OUTPUT_NAME);

        // 線の幅。
        double border_width = 2;
        // フッターの高さ。
        double footer_height = pixels_from_mm(6);

        HPDF_Page page; // ページオブジェクト。
        HPDF_Font font; // フォントオブジェクト。
        INT iColumn = 0, iRow = 0, iPage = 0; // セル位置とページ番号。
        INT cItems = INT(m_list.size()); // 項目数。
        INT cPages = (cItems + (columns * rows) - 1) / (columns * rows); // ページ総数。
        double page_width, page_height; // ページサイズ。
        double content_x, content_y, content_width, content_height; // ページ内容の位置とサイズ。
        for (INT iItem = 0; iItem < cItems; ++iItem)
        {
            if (iColumn == 0 && iRow == 0) // 項目がページの最初ならば
            {
                // ページを追加する。
                page = HPDF_AddPage(pdf);

                // ページサイズと用紙の向きを指定。
                HPDF_Page_SetSize(page, page_size, direction);

                // ページサイズ（ピクセル単位）。
                page_width = HPDF_Page_GetWidth(page);
                page_height = HPDF_Page_GetHeight(page);

                // ページ内容の位置とサイズ。
                content_x = margin;
                content_y = margin;
                content_width = page_width - margin * 2;
                content_height = page_height - margin * 2;

                // 線の幅を指定。
                HPDF_Page_SetLineWidth(page, border_width);

                // 線の色を RGB で設定する。PDF では RGB 各値を [0,1] で指定することになっている。
                HPDF_Page_SetRGBStroke(page, 0, 0, 0);

                /* 塗りつぶしの色を RGB で設定する。PDF では RGB 各値を [0,1] で指定することになっている。*/
                HPDF_Page_SetRGBFill(page, 0, 0, 0);

                // フォントを指定する。
                auto font_name_a = ansi_from_wide(CP932, font_name.c_str());
                font = HPDF_GetFont(pdf, font_name_a, "90ms-RKSJ-H");

#ifndef NO_SHAREWARE
                // ロゴ文字列を描画する。
                if (!g_shareware.IsRegistered())
                {
                    auto logo_a = ansi_from_wide(CP932, g_szAppName);
                    double logo_x = content_x, logo_y = content_y;

                    // フォントとフォントサイズを指定。
                    HPDF_Page_SetFontAndSize(page, font, footer_height);

                    // テキストを描画する。
                    HPDF_Page_BeginText(page);
                    {
                        HPDF_Page_TextOut(page, logo_x, logo_y, logo_a);
                    }
                    HPDF_Page_EndText(page);
                }
#endif

                // フッター（ページ番号）を描画する。
                if (page_numbers)
                {
                    TCHAR footer_text[128];
                    StringCchPrintf(footer_text, _countof(footer_text),
                                    doLoadString(IDS_PAGENUMBER), (iPage + 1), cPages);
                    auto footer_text_a = ansi_from_wide(CP932, footer_text);
                    hpdf_draw_text(page, font, footer_height, footer_text_a,
                                   content_x, content_y,
                                   content_width, footer_height);
                    // フッターの分だけページ内容のサイズを縮小する。
                    content_y += footer_height;
                    content_height -= footer_height;
                }

                // 枠線を描く。
                if (draw_border)
                {
                    hpdf_draw_box(page, content_x, content_y, content_width, content_height);
                }
            }

            // セルの位置とサイズ。
            double cell_width = content_width / columns;
            double cell_height = content_height / rows;
            double cell_x = content_x + cell_width * iColumn;
            double cell_y = content_y + cell_height * (rows - iRow - 1);

            // セルの枠線を描く。
            if (draw_border)
            {
                hpdf_draw_box(page, cell_x, cell_y, cell_width, cell_height);

                // 枠線の分だけセルを縮小。
                cell_x += border_width;
                cell_y += border_width;
                cell_width -= border_width * 2;
                cell_height -= border_width * 2;
            }

            // テキストを描画する。
            if (image_title.size())
            {
                // タグを規則に従って置き換える。
                string_t text = image_title;
                substitute_tags(text, m_list[iItem], iItem, cItems, iPage, cPages);

                // ANSI文字列に変換してテキストを描画する。
                auto text_a = ansi_from_wide(CP932, text.c_str());
                hpdf_draw_text(page, font, font_size, text_a, cell_x, cell_y, cell_width, font_size);

                // セルのサイズを縮小する。
                cell_y += font_size;
                cell_height -= font_size;
            }

            // 画像を描く。
            hpdf_draw_image(pdf, page, cell_x, cell_y, cell_width, cell_height, m_list[iItem],
                            max_data_size, dont_resize_small);

            // 次のセルに進む。必要ならばページ番号を進める。
            ++iColumn;
            if (iColumn == columns)
            {
                iColumn = 0;
                ++iRow;
                if (iRow == rows)
                {
                    iRow = 0;
                    ++iPage;
                }
            }
        }

        {
            // 出力ファイル名のタグを置き換える。
            string_t text = output_name;
            substitute_tags(text, m_list[0], 0, cItems, 0, cPages, true);

            // ファイル名に使えない文字を置き換える。
            validate_filename(text);

            // PDFを一時ファイルに保存する。
            TempFile temp_file(TEXT("GN2"), TEXT(".pdf"));
            std::string temp_file_a = ansi_from_wide(CP_ACP, temp_file.make());
            HPDF_SaveToFile(pdf, temp_file_a.c_str());

            // デスクトップにファイルをコピー。
            TCHAR szPath[MAX_PATH];
            SHGetSpecialFolderPath(hwnd, szPath, CSIDL_DESKTOPDIRECTORY, FALSE);
            PathAppend(szPath, text.c_str());
            StringCchCat(szPath, _countof(szPath), TEXT(".pdf"));
            if (!CopyFile(temp_file.get(), szPath, FALSE))
            {
                auto err_msg = ansi_from_wide(CP_ACP, doLoadString(IDS_COPYFILEFAILED));
                throw std::runtime_error(err_msg);
            }

            // 成功メッセージを表示。
            StringCchCopy(szPath, _countof(szPath), text.c_str());
            StringCchCat(szPath, _countof(szPath), TEXT(".pdf"));
            TCHAR szText[MAX_PATH];
            StringCchPrintf(szText, _countof(szText), doLoadString(IDS_SUCCEEDED), szPath);
            ret = szText;
        }
    }
    catch (std::runtime_error& err)
    {
        // 失敗。
        auto wide = wide_from_ansi(CP_ACP, err.what());
        MessageBoxW(hwnd, wide, NULL, MB_ICONERROR);
        return TEXT("");
    }

    // PDFオブジェクトを解放する。
    HPDF_Free(pdf);

    return ret;
}

// リストの選択が変化した。
void OnListSelectionChange(HWND hwnd)
{
    // ユーザーデータ。
    GazoNarabe* pGN = (GazoNarabe*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    // リストボックス。
    HWND hLst1 = GetDlgItem(hwnd, lst1);

    // 横スクロール可能な幅を設定する。
    INT cItems = ListBox_GetCount(hLst1);
    TCHAR szText[1024];
    HFONT hFont = GetWindowFont(hLst1);
    HDC hDC = GetDC(hLst1);
    HGDIOBJ hFontOld = SelectObject(hDC, hFont);
    INT cxMax = 0;
    for (INT iItem = 0; iItem < cItems; ++iItem)
    {
        if (ListBox_GetTextLen(hLst1, iItem) < _countof(szText))
        {
            ListBox_GetText(hLst1, iItem, szText);
            SIZE siz;
            GetTextExtentPoint32(hDC, szText, lstrlen(szText), &siz);
            if (cxMax < siz.cx)
                cxMax = siz.cx;
        }
    }
    SelectObject(hDC, hFontOld);
    ReleaseDC(hLst1, hDC);
    ListBox_SetHorizontalExtent(hLst1, cxMax + 16);

    INT cSelected = ListBox_GetSelCount(hLst1);
    if (cSelected == LB_ERR || cSelected == 0) // 選択されてない。
    {
        // 選択に対するボタンを無効化する。
        EnableWindow(GetDlgItem(hwnd, IDC_UP), FALSE);
        EnableWindow(GetDlgItem(hwnd, IDC_DOWN), FALSE);
        EnableWindow(GetDlgItem(hwnd, IDC_DELETE), FALSE);
        // ビットマップをクリアする。
        SendDlgItemMessage(hwnd, ico1, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)NULL);
        return;
    }

    // 選択に対するボタンを有効化する。
    EnableWindow(GetDlgItem(hwnd, IDC_UP), TRUE);
    EnableWindow(GetDlgItem(hwnd, IDC_DOWN), TRUE);
    EnableWindow(GetDlgItem(hwnd, IDC_DELETE), TRUE);

    // 最初の選択のテキストを取得する。
    INT iItem;
    TCHAR szPath[MAX_PATH];
    ListBox_GetSelItems(hLst1, 1, &iItem);
    ListBox_GetText(hLst1, iItem, szPath);

    // プレビュービットマップを破棄する。
    if (pGN->m_hbmPreview)
    {
        ::DeleteObject(pGN->m_hbmPreview);
        pGN->m_hbmPreview = NULL;
    }

    // 画像を読み込み、画像のサイズを取得する。
    int width, height;
    HBITMAP hbm = doLoadPic(szPath, &width, &height);
    if (!hbm) // 失敗。
    {
        // ビットマップをクリアする。
        SendDlgItemMessage(hwnd, ico1, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)NULL);
        return;
    }

    // 必要ならば、アスペクト比を維持したままの縮小後のサイズを計算する。
    BOOL bShrink = FALSE;
#define SHRINK_SIZE 80
    if (width > height)
    {
        if (width > SHRINK_SIZE)
        {
            height = height * SHRINK_SIZE / width;
            width = SHRINK_SIZE;
            bShrink = TRUE;
        }
    }
    else
    {
        if (height > SHRINK_SIZE)
        {
            width = width * SHRINK_SIZE / height;
            height = SHRINK_SIZE;
            bShrink = TRUE;
        }
    }

    // 画像サイズがゼロにならないように補正する。
    if (height <= 0)
        height = 1;
    if (width <= 0)
        width = 1;

    // 必要ならば画像を縮小する。
    if (bShrink)
    {
        HBITMAP hbmNew = gpimage_resize(hbm, width, height);
        DeleteObject(hbm);
        if (hbmNew == NULL)
        {
            // ビットマップをクリアする。
            SendDlgItemMessage(hwnd, ico1, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)NULL);
            return;
        }
        hbm = hbmNew;
    }

    // プレビュービットマップを設定する。
    pGN->m_hbmPreview = hbm;
    SendDlgItemMessage(hwnd, ico1, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hbm);
}

// WM_INITDIALOG
// ダイアログの初期化。
BOOL OnInitDialog(HWND hwnd, HWND hwndFocus, LPARAM lParam)
{
    // ユーザデータ。
    GazoNarabe* pGN = (GazoNarabe*)lParam;

    // ユーザーデータをウィンドウハンドルに関連付ける。
    SetWindowLongPtr(hwnd, GWLP_USERDATA, lParam);

    // gpimageライブラリを初期化する。
    gpimage_init();

    // ファイルドロップを受け付ける。
    DragAcceptFiles(hwnd, TRUE);

    // アプリの名前。
    LoadString(NULL, IDS_APPNAME, g_szAppName, _countof(g_szAppName));

    // アイコンの設定。
    g_hIcon = LoadIcon(g_hInstance, MAKEINTRESOURCE(1));
    g_hIconSm = (HICON)LoadImage(g_hInstance, MAKEINTRESOURCE(1), IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
    SendMessage(hwnd, WM_SETICON, ICON_BIG, (WPARAM)g_hIcon);
    SendMessage(hwnd, WM_SETICON, ICON_SMALL, (WPARAM)g_hIconSm);

    // 初期化。
    pGN->InitDialog(hwnd);

    // レジストリからデータを読み込む。
    pGN->DataFromReg(hwnd);

    // ダイアログにデータを設定。
    pGN->DialogFromData(hwnd, FALSE);

    // リストの選択が変化した。
    OnListSelectionChange(hwnd);

#ifndef NO_SHAREWARE
    if (!g_shareware.Start(hwnd) || IsDebuggerPresent())
    {
        // 失敗。アプリケーションを終了する。
        EndDialog(hwnd, IDABORT);
    }
#endif

    // Susie プラグインを読み込む。
    CHAR szPathA[MAX_PATH];
    GetModuleFileNameA(NULL, szPathA, _countof(szPathA));
    PathRemoveFileSpecA(szPathA);
    if (!g_susie.load(szPathA))
    {
        PathAppendA(szPathA, "plugins");
        g_susie.load(szPathA);
    }

    return TRUE;
}

// 「OK」ボタンが押された。
BOOL OnOK(HWND hwnd)
{
    // ユーザデータ。
    GazoNarabe* pGN = (GazoNarabe*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    // 「処理中...」とボタンに表示する。
    HWND hButton = GetDlgItem(hwnd, IDC_GENERATE);
    SetWindowText(hButton, doLoadString(IDS_PROCESSINGNOW));

    // ダイアログからデータを取得。
    if (!pGN->DataFromDialog(hwnd, TRUE)) // 失敗。
    {
        // ボタンテキストを元に戻す。
        SetWindowText(hButton, doLoadString(IDS_GENERATE));

        return FALSE; // 失敗。
    }

    // 設定をレジストリに保存。
    pGN->RegFromData(hwnd);

    // メインディッシュ処理。
    string_t success = pGN->JustDoIt(hwnd);

    // ボタンテキストを元に戻す。
    SetWindowText(hButton, doLoadString(IDS_GENERATE));

    // 必要なら結果をメッセージボックスとして表示する。
    if (success.size())
    {
        MessageBox(hwnd, success.c_str(), g_szAppName, MB_ICONINFORMATION);
    }

    return TRUE; // 成功。
}

// 「リストに追加...」ボタン。
void OnAdd(HWND hwnd)
{
    // 「ファイルを開く」ダイアログの初期化を開始する。
    OPENFILENAME ofn = { sizeof(ofn), hwnd };

    // フィルター文字列をリソースから読み込む。
    string_t strFilter = doLoadString(IDS_OPENFILTER);

    // Susieプラグインのサポート。
    if (g_susie.is_loaded())
    {
        std::string additional = g_susie.get_filter();
        auto wide = wide_from_ansi(CP_ACP, additional.c_str());
        strFilter += doLoadString(IDS_SUSIE_IMAGES);
        strFilter += L" (";
        strFilter += wide;
        strFilter += L")|";
        strFilter += wide;
        strFilter += L"|";
    }

    // フィルター文字列をNULL区切りにする。
    for (auto& ch : strFilter)
    {
        if (ch == TEXT('|'))
            ch = 0;
    }

    // フィルター文字列を設定する。
    ofn.lpstrFilter = strFilter.c_str();

    // ファイルパス名の設定。
    TCHAR szFile[MAX_PATH] = TEXT("");
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = _countof(szFile);

    // 初期フォルダの設定。
    TCHAR szDir[MAX_PATH];
    SHGetSpecialFolderPath(hwnd, szDir, CSIDL_MYPICTURES, FALSE);
    ofn.lpstrInitialDir = szDir;

    // ダイアログタイトルの設定。
    string_t strTitle = doLoadString(IDS_ADDPICTURE);
    ofn.lpstrTitle = strTitle.c_str();

    // フラグ群。
    ofn.Flags = OFN_EXPLORER | OFN_ENABLESIZING | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;
    // 拡張子。
    ofn.lpstrDefExt = TEXT("jpg");

    // ユーザーにファイルを選んでもらう。
    if (GetOpenFileName(&ofn))
    {
        // リストに追加して選択する。
        HWND hLst1 = GetDlgItem(hwnd, lst1);
        INT iItem, cItems = ListBox_GetCount(hLst1);
        for (iItem = 0; iItem < cItems; ++iItem)
        {
            ListBox_SetSel(hLst1, FALSE, iItem);
        }
        iItem = ListBox_AddString(hLst1, szFile);
        ListBox_SetSel(hLst1, TRUE, iItem);

        // リストの選択が変化した。
        OnListSelectionChange(hwnd);
    }
}

// 「↑」ボタン。
void OnUp(HWND hwnd)
{
    // リストボックス。
    HWND hLst1 = GetDlgItem(hwnd, lst1);

    // 選択項目があるか？
    INT cSelected = ListBox_GetSelCount(hLst1);
    if (cSelected == LB_ERR || cSelected == 0)
        return;

    // 選択項目を取得する。
    std::vector<INT> items;
    items.resize(cSelected);
    ListBox_GetSelItems(hLst1, cSelected, &items[0]);

    if (items[0] <= 0)
        return; // これ以上、上に移動できない。

    // リストボックスの項目をひとつ上に移動する。
    TCHAR szText1[MAX_PATH], szText2[MAX_PATH];
    for (size_t i = 0; i < items.size(); ++i)
    {
        szText1[0] = szText2[0] = 0;
        INT iItem = items[i];
        ListBox_GetText(hLst1, iItem - 1, szText1);
        ListBox_GetText(hLst1, iItem, szText2);
        ListBox_DeleteString(hLst1, iItem - 1);
        ListBox_DeleteString(hLst1, iItem - 1);
        ListBox_InsertString(hLst1, iItem - 1, szText1);
        ListBox_InsertString(hLst1, iItem - 1, szText2);
        ListBox_SetSel(hLst1, TRUE, iItem - 1);
        ListBox_SetSel(hLst1, FALSE, iItem);
    }

    // 現在位置を表示する。
    ListBox_SetCaretIndex(hLst1, items[0]);

    // リストの選択が変化した。
    OnListSelectionChange(hwnd);
}

// 「↓」ボタン。
void OnDown(HWND hwnd)
{
    // リストボックス。
    HWND hLst1 = GetDlgItem(hwnd, lst1);

    // 選択項目があるか？
    INT cSelected = ListBox_GetSelCount(hLst1);
    if (cSelected == LB_ERR || cSelected == 0)
        return;

    // 選択項目を取得する。
    std::vector<INT> items;
    items.resize(cSelected);
    ListBox_GetSelItems(hLst1, cSelected, &items[0]);

    if (items[cSelected - 1] + 1 >= ListBox_GetCount(hLst1))
        return; // これ以上、下に移動できない。

    // リストボックスの項目をひとつ下に移動する。
    TCHAR szText1[MAX_PATH], szText2[MAX_PATH];
    for (size_t i = items.size() - 1; i < items.size(); --i)
    {
        szText1[0] = szText2[0] = 0;
        INT iItem = items[i];
        ListBox_GetText(hLst1, iItem, szText1);
        ListBox_GetText(hLst1, iItem + 1, szText2);
        ListBox_DeleteString(hLst1, iItem);
        ListBox_DeleteString(hLst1, iItem);
        ListBox_InsertString(hLst1, iItem, szText1);
        ListBox_InsertString(hLst1, iItem, szText2);
        ListBox_SetSel(hLst1, FALSE, iItem);
        ListBox_SetSel(hLst1, TRUE, iItem + 1);
    }

    // 現在位置を表示する。
    ListBox_SetCaretIndex(hLst1, items[cSelected - 1]);

    // リストの選択が変化した。
    OnListSelectionChange(hwnd);
}

// 「選択を削除」ボタン。
void OnDelete(HWND hwnd)
{
    // リストボックス。
    HWND hLst1 = GetDlgItem(hwnd, lst1);

    // 選択項目があるか？
    INT cSelected = ListBox_GetSelCount(hLst1);
    if (cSelected == LB_ERR || cSelected == 0)
        return;

    // 選択項目を取得する。
    std::vector<INT> items;
    items.resize(cSelected);
    ListBox_GetSelItems(hLst1, cSelected, &items[0]);

    // 項目を削除する。逆順にするとインデックスがずれない。
    for (size_t i = items.size() - 1; i < items.size(); --i)
    {
        INT iItem = items[i];
        ListBox_DeleteString(hLst1, iItem);
    }

    // 選択状態を更新する。
    cSelected = ListBox_GetSelCount(hLst1);
    if (cSelected == LB_ERR || cSelected == 0)
    {
        if (items[0] == ListBox_GetCount(hLst1))
            ListBox_SetSel(hLst1, TRUE, items[0] - 1);
        else
            ListBox_SetSel(hLst1, TRUE, items[0]);
    }

    // リストの選択が変化した。
    OnListSelectionChange(hwnd);
}

// 「設定の初期化」ボタン。
void OnEraseSettings(HWND hwnd)
{
    // ユーザーデータ。
    GazoNarabe* pGN = (GazoNarabe*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    // データをリセットする。
    pGN->Reset();

    // データからダイアログへ。
    pGN->DialogFromData(hwnd, FALSE);

    // データからレジストリへ。
    pGN->RegFromData(hwnd);

    // リストの選択が変化した。
    OnListSelectionChange(hwnd);
}

// リストがダブルクリックされた。
void OnListDoubleClick(HWND hwnd)
{
    // リストボックス。
    HWND hLst1 = GetDlgItem(hwnd, lst1);

    INT cSelected = ListBox_GetSelCount(hLst1);
    if (cSelected == LB_ERR || cSelected == 0)
        return; // 選択項目がない。

    // 選択項目を取得する。
    std::vector<INT> items;
    items.resize(cSelected);
    ListBox_GetSelItems(hLst1, cSelected, &items[0]);

    // リスト項目からテキストを取得し、それをコマンドラインとして開く。
    TCHAR szPath[MAX_PATH];
    for (size_t i = items.size() - 1; i < items.size(); --i)
    {
        INT iItem = items[i];
        ListBox_GetText(hLst1, iItem, szPath);
        ShellExecute(hwnd, NULL, szPath, NULL, NULL, SW_SHOWNORMAL);
    }
}

// 「特殊タグの説明」ボタン。
void OnTags(HWND hwnd)
{
    // 同じフォルダにある「TAGS.txt」ファイルを開く。
    auto pathname = findLocalFile(TEXT("TAGS.txt"));
    if (pathname)
        ShellExecute(hwnd, NULL, pathname, NULL, NULL, SW_SHOWNORMAL);
}

// WM_COMMAND
// コマンド。
void OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
    switch (id)
    {
    case IDC_GENERATE: // 「PDF生成」ボタン。
        OnOK(hwnd);
        break;
    case IDC_EXIT: // 「終了」ボタン。
        EndDialog(hwnd, id);
        break;
    case IDC_ADD: // 「リストに追加...」ボタン。
        OnAdd(hwnd);
        break;
    case IDC_UP: // 「↑」ボタン。
        OnUp(hwnd);
        break;
    case IDC_DOWN: // 「↓」ボタン。
        OnDown(hwnd);
        break;
    case IDC_DELETE: // 「選択を削除」ボタン。
        OnDelete(hwnd);
        break;
    case IDC_ERASESETTINGS: // 「設定の初期化」ボタン。
        OnEraseSettings(hwnd);
        break;
    case psh6: // 「特殊タグの説明」ボタン。
        OnTags(hwnd);
        break;
    case stc1:
    case stc2:
    case stc3:
    case stc4:
    case stc5:
    case stc6:
    case stc7:
    case stc8:
    case stc9:
    case stc10:
        // コンボボックスの前のラベルをクリックしたら、対応するコンボボックスにフォーカスを当てる。
        SetFocus(GetDlgItem(hwnd, cmb1 + (id - stc1)));
        break;
    case lst1:
        if (codeNotify == LBN_DBLCLK) // リストがダブルクリックされた。
        {
            OnListDoubleClick(hwnd);
            break;
        }
        if (codeNotify == LBN_SELCHANGE) // リストの選択が変化した。
        {
            OnListSelectionChange(hwnd);
            break;
        }
        break;
    }
}

// WM_DROPFILES
// ファイルがドロップされた。
void OnDropFiles(HWND hwnd, HDROP hdrop)
{
    // リストボックス。
    HWND hLst1 = GetDlgItem(hwnd, lst1);

    // ドロップされた項目数を取得する。
    UINT iFile, cFiles = DragQueryFile(hdrop, 0xFFFFFFFF, NULL, 0);

    // ドロップ項目をリストボックスに追加する。
    BOOL added = FALSE; // 追加されたか？
    for (iFile = 0; iFile < cFiles; ++iFile)
    {
        // ドロップ項目のパスファイル名を取得する。
        TCHAR szFile[MAX_PATH];
        DragQueryFile(hdrop, iFile, szFile, _countof(szFile));

        // 画像ファイルとして有効な拡張子のみ、追加。
        if (isValidImageFile(szFile))
        {
            // リストボックスに項目を追加する。
            ListBox_AddString(hLst1, szFile);

            added = TRUE; // 追加された。
        }
    }

    if (added) // 追加されたなら
    {
        // 選択項目の選択を解除する。
        INT cItems = ListBox_GetCount(hLst1);
        for (INT iItem = 0; iItem < cItems; ++iItem)
        {
            ListBox_SetSel(hLst1, FALSE, iItem);
        }

        // 最後の項目を選択する。
        ListBox_SetSel(hLst1, TRUE, cItems - 1);

        // 表示位置を更新する。
        ListBox_SetCaretIndex(hLst1, cItems - 1);

        // リストの選択が変化した。
        OnListSelectionChange(hwnd);
    }

    // ドロップ完了。
    DragFinish(hdrop);
}

// WM_VKEYTOITEM
// リストボックスでキーが押された。
int OnVkeyToItem(HWND hwnd, UINT vk, HWND hwndListbox, int iCaret)
{
    HWND hLst1 = GetDlgItem(hwnd, lst1);
    if (hLst1 != hwndListbox)
        return -1; // 既定のアクションを行う。

    if (vk == VK_DELETE) // Delキーだった。
    {
        // 削除する。
        OnDelete(hwnd);
        return -2; // さらなるアクションはしない。
    }

    return -1; // 既定のアクションを行う。
}

// WM_DESTROY
// ウィンドウが破棄された。
void OnDestroy(HWND hwnd)
{
    // アイコンを破棄。
    DestroyIcon(g_hIcon);
    DestroyIcon(g_hIconSm);
    g_hIcon = g_hIconSm = NULL;

    // gpimageライブラリを終了する。
    gpimage_exit();
}

// ダイアログプロシージャ。
INT_PTR CALLBACK
DialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        HANDLE_MSG(hwnd, WM_INITDIALOG, OnInitDialog);
        HANDLE_MSG(hwnd, WM_COMMAND, OnCommand);
        HANDLE_MSG(hwnd, WM_DROPFILES, OnDropFiles);
        HANDLE_MSG(hwnd, WM_VKEYTOITEM, OnVkeyToItem);
        HANDLE_MSG(hwnd, WM_DESTROY, OnDestroy);
    }
    return 0;
}

// ガゾーナラベのメイン関数。
INT GazoNarabe_Main(HINSTANCE hInstance, INT argc, LPTSTR *argv)
{
    g_hInstance = hInstance;
    InitCommonControls();

    GazoNarabe gn(hInstance, argc, argv);
    DialogBoxParam(hInstance, MAKEINTRESOURCE(1), NULL, DialogProc, (LPARAM)&gn);
    return 0;
}

// Windowsアプリのメイン関数。
INT WINAPI
WinMain(HINSTANCE   hInstance,
        HINSTANCE   hPrevInstance,
        LPSTR       lpCmdLine,
        INT         nCmdShow)
{
#ifdef UNICODE
    // wWinMainをサポートしていないコンパイラのために、コマンドラインの処理を行う。
    INT argc;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    INT ret = GazoNarabe_Main(hInstance, argc, argv);
    LocalFree(argv);
    return ret;
#else
    return GazoNarabe_Main(hInstance, __argc, __argv);
#endif
}
