#include <windows.h>
#include <gdiplus.h>
#include <shlwapi.h>
#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

// GDI+を使用するために必要なデータ。
GdiplusStartupInput g_gdiplusStartupInput;
ULONG_PTR g_gdiplusToken = 0;

// gpimageライブラリの初期化処理。
BOOL gpimage_init(void)
{
    // GDI+の初期化。
    return GdiplusStartup(&g_gdiplusToken, &g_gdiplusStartupInput, NULL) == Ok;
}

// gpimageライブラリの終了処理。
void gpimage_exit(void)
{
    // GDI+ の終了処理。
    GdiplusShutdown(g_gdiplusToken);
    g_gdiplusToken = 0;
}

// ファイル名（拡張子を含む）からMIMEの種類を取得する。
LPCWSTR gpimage_get_mime_from_filename(LPCWSTR filename)
{
    // 拡張子を取得する。
    LPWSTR dotext = PathFindExtensionW(filename);
    if (!dotext || !*dotext)
        return NULL;

    // MIMEを取得する。
    if (lstrcmpiW(dotext, L".jpg") == 0 ||
        lstrcmpiW(dotext, L".jpeg") == 0 ||
        lstrcmpiW(dotext, L".jpe") == 0 ||
        lstrcmpiW(dotext, L".jfif") == 0)
    {
        return L"image/jpeg";
    }

    if (lstrcmpiW(dotext, L".png") == 0)
    {
        return L"image/png";
    }

    if (lstrcmpiW(dotext, L".gif") == 0)
    {
        return L"image/gif";
    }

    if (lstrcmpiW(dotext, L".bmp") == 0 || lstrcmpiW(dotext, L".dib") == 0)
    {
        return L"image/bmp";
    }

    if (lstrcmpiW(dotext, L".tiff") == 0 || lstrcmpiW(dotext, L".tif") == 0)
    {
        return L"image/tiff";
    }

    return NULL;
}

// 拡張子が有効かをチェックする。
BOOL gpimage_is_valid_extension(LPCWSTR filename)
{
    return gpimage_get_mime_from_filename(filename) != NULL;
}

// ファイル名の拡張子からエンコーダーのCLSIDを取得する。
BOOL gpimage_get_encoder_from_filename(LPCWSTR filename, CLSID *pClsid)
{
    *pClsid = GUID_NULL;

    LPCWSTR mime = gpimage_get_mime_from_filename(filename);
    if (mime == NULL)
        return FALSE;

    // フォーマットを指定して、エンコーダーの GUID を取得する
    UINT num = 0, size = 0;
    GetImageEncodersSize(&num, &size);
    if (size == 0)
        return FALSE;

    ImageCodecInfo* pImageCodecInfo = (ImageCodecInfo*)malloc(size);
    if (pImageCodecInfo == NULL)
    {
        return FALSE;
    }

    GetImageEncoders(num, size, pImageCodecInfo);
    for (UINT j = 0; j < num; ++j)
    {
        if (lstrcmpiW(pImageCodecInfo[j].MimeType, mime) == 0)
        {
            free(pImageCodecInfo);
            *pClsid = pImageCodecInfo[j].Clsid;
            return TRUE;
        }
    }

    free(pImageCodecInfo);
    return FALSE;
}

// 画像をHBITMAPとして読み込む。
HBITMAP gpimage_load(LPCWSTR filename, int* width, int* height, FILETIME* pftCreated, FILETIME* pftModified)
{
    HBITMAP hBitmap = NULL;
 
    // 画像を読み込む
    if (Image *image = Image::FromFile(filename))
    {
        // 画像の回転角度を取得する
        int orientation = 0;
        {
            PropertyItem *propertyItem;
            UINT size = image->GetPropertyItemSize(PropertyTagOrientation);
            if (size)
            {
                if (PropertyItem* propertyItem = (PropertyItem*)malloc(size))
                {
                    image->GetPropertyItem(PropertyTagOrientation, size, propertyItem);
                    orientation = *(int*)propertyItem->value;
                    free(propertyItem);
                }
            }
        }

        // 画像の撮影日時または作成日時を取得する。
        if (pftCreated)
        {
            ZeroMemory(pftCreated, sizeof(*pftCreated));
            BOOL found = FALSE;

            // EXIFから撮影日時の取得を試みる。
            UINT size = image->GetPropertyItemSize(PropertyTagExifDTOrig);
            if (PropertyItem* propertyItem = (PropertyItem*)malloc(size))
            {
                image->GetPropertyItem(PropertyTagExifDTOrig, size, propertyItem);
                if (propertyItem->type == PropertyTagTypeASCII)
                {
                    // 「YYYY:MM:DD HH:MM:SS」
                    auto datetime = (const char*)propertyItem->value;
                    SYSTEMTIME st;
                    st.wYear = (WORD)atoi(&datetime[0]);
                    st.wMonth = (WORD)atoi(&datetime[5]);
                    st.wDay = (WORD)atoi(&datetime[8]);
                    st.wHour = (WORD)atoi(&datetime[11]);
                    st.wMinute = (WORD)atoi(&datetime[14]);
                    st.wSecond = (WORD)atoi(&datetime[17]);
                    st.wMilliseconds = 0;
                    if (::SystemTimeToFileTime(&st, pftCreated))
                        found = TRUE;
                }
                free(propertyItem);
            }

            // 取得できなければ、ファイル作成日時を取得する。
            if (!found)
            {
                WIN32_FIND_DATAW find;
                HANDLE hFind = FindFirstFileW(filename, &find);
                if (hFind != INVALID_HANDLE_VALUE)
                {
                    FindClose(hFind);
                    ::FileTimeToLocalFileTime(&find.ftCreationTime, pftCreated);
                }
            }
        }

        // ファイル更新日時を取得する。
        if (pftModified)
        {
            ZeroMemory(pftModified, sizeof(*pftModified));
            WIN32_FIND_DATAW find;
            HANDLE hFind = ::FindFirstFileW(filename, &find);
            if (hFind != INVALID_HANDLE_VALUE)
            {
                ::FindClose(hFind);
                ::FileTimeToLocalFileTime(&find.ftLastWriteTime, pftModified);
            }
        }

        // 画像を回転する
        switch (orientation)
        {
        case 1: // 回転無し
            break;
        case 2: // 左右反転
            image->RotateFlip(RotateNoneFlipX);
            break;
        case 3: // 180度回転
            image->RotateFlip(Rotate180FlipNone);
            break;
        case 4: // 上下反転
            image->RotateFlip(RotateNoneFlipY);
            break;
        case 5: // 左右反転 + 90度回転
            image->RotateFlip(Rotate90FlipX);
            break;
        case 6: // 90度回転
            image->RotateFlip(Rotate90FlipNone);
            break;
        case 7: // 左右反転 + 270度回転
            image->RotateFlip(Rotate270FlipX);
            break;
        case 8: // 270度回転
            image->RotateFlip(Rotate270FlipNone);
            break;
        }
        //// 回転角度プロパティを削除する。
        //image->RemovePropertyItem(PropertyTagOrientation);

        // 画像のサイズを取得する
        int cx = image->GetWidth();
        int cy = image->GetHeight();

        if (width)
            *width = cx;
        if (height)
            *height = cy;

        // HBITMAPを作成する
        BITMAPINFO bmi;
        ZeroMemory(&bmi, sizeof(BITMAPINFO));
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = cx;
        bmi.bmiHeader.biHeight = -cy;  // 縦方向を反転する
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 24;
        hBitmap = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, NULL, NULL, 0);
        if (hBitmap)
        {
            // 画像をHBITMAPに描画する
            if (HDC hdcMem = CreateCompatibleDC(NULL))
            {
                HGDIOBJ hbmOld = SelectObject(hdcMem, hBitmap);
                {
                    RECT rc = { 0, 0, cx, cy };
                    FillRect(hdcMem, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));
                    Graphics graphics(hdcMem);
                    graphics.DrawImage(image, 0, 0, cx, cy);
                }
                SelectObject(hdcMem, hbmOld);
                DeleteDC(hdcMem);
            }
            else
            {
                DeleteObject(hBitmap);
                hBitmap = NULL;
            }
        }

        delete image;
    }

    return hBitmap;
}

// HBITMAPを画像ファイルとして保存する。
BOOL gpimage_save(LPCWSTR filename, HBITMAP hBitmap)
{
    BOOL ret = FALSE;

    CLSID clsid;
    if (!gpimage_get_encoder_from_filename(filename, &clsid))
        return FALSE;

    // HBITMAP を GDI+ の Bitmap に変換する
    if (Bitmap* pBitmap = Bitmap::FromHBITMAP(hBitmap, NULL))
    {
        // 保存する
        if (pBitmap->Save(filename, &clsid, NULL) == Ok)
        {
            ret = TRUE;
        }

        // 破棄する。
        delete pBitmap;
    }

    return ret;
}

// HBITMAPを拡大縮小する。
HBITMAP gpimage_resize(HBITMAP hbmSrc, int width, int height)
{
    BITMAP bm;
    if (!GetObject(hbmSrc, sizeof(bm), &bm))
        return NULL;

    // DCを作成する。
    HDC hdcSrc = CreateCompatibleDC(NULL);
    if (!hdcSrc)
        return NULL;
    HDC hdcDest = CreateCompatibleDC(NULL);
    if (!hdcDest)
    {
        DeleteDC(hdcSrc);
        return NULL;
    }

    // HBITMAPを作成する
    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(BITMAPINFO));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height; // 縦方向を反転する
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    HBITMAP hbmDest = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, NULL, NULL, 0);
    if (hbmDest)
    {
        // ビットマップを選択。
        HGDIOBJ hbmSrcOld = SelectObject(hdcSrc, hbmSrc);
        HGDIOBJ hbmDestOld = SelectObject(hdcDest, hbmDest);
        // なめらかに拡大縮小する。
        SetStretchBltMode(hdcDest, STRETCH_HALFTONE);
        StretchBlt(hdcDest, 0, 0, width, height, hdcSrc, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
        // ビットマップの選択を解除。
        SelectObject(hdcSrc, hbmSrcOld);
        SelectObject(hdcDest, hbmDestOld);
    }

    // DCを破棄する。
    DeleteDC(hdcDest);
    DeleteDC(hdcSrc);

    return hbmDest;
}
