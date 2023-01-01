#pragma once

// gpimageライブラリの初期化処理。
BOOL gpimage_init(void);
// gpimageライブラリの終了処理。
void gpimage_exit(void);
// ファイル名（拡張子を含む）からMIMEの種類を取得する。
LPCWSTR gpimage_get_mime_from_filename(LPCWSTR filename);
// 拡張子が有効かをチェックする。
BOOL gpimage_is_valid_extension(LPCWSTR filename);
// ファイル名の拡張子からエンコーダーのCLSIDを取得する。
BOOL gpimage_get_encoder_from_filename(LPCWSTR filename, CLSID *pClsid);
// 画像をHBITMAPとして読み込む。
HBITMAP gpimage_load(LPCWSTR filename, int* width = NULL, int* height = NULL,
                     FILETIME* pftCreated = NULL, FILETIME* pftModified = NULL);
// HBITMAPを画像ファイルとして保存する。
BOOL gpimage_save(LPCWSTR filename, HBITMAP hBitmap);
// HBITMAPを拡大縮小する。
HBITMAP gpimage_resize(HBITMAP hbmSrc, int width, int height);
// ファイルの日付を取得する（撮影日時を除く）。
BOOL gpimage_load_datetime(LPCWSTR filename, FILETIME* pftCreated = NULL, FILETIME* pftModified = NULL);
