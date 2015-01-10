#define _CRT_SECURE_NO_WARNINGS

// ���� ��� ����������� ���������.

#include <stdio.h>
#include <setjmp.h>
#include <math.h>
#include <Windows.h>
#include <windowsx.h>

#include "jpegwnd.h"
#include "jpegload.h"
#include "statuswnd.h"

#define FLIP_BUTTON_HEIGHT  30

typedef struct PatternItem
{
    char    Name[128];
    float   Lamda;
    unsigned char * PatternRawImage;
    int     PatternBufferSize;
    int     PatternWidth;
    int     PatternHeight;
    HBITMAP PatternBitmap;
    bool    Hidden;
} PatternItem;

static HWND ParentWnd;
static HWND PatternWnd;

extern HWND FlipWnd;
extern float WorkspaceLamda, WorkspaceLamdaDelta;

static PatternItem * Patterns = NULL;
static int NumPatterns;

static HWND *PatternTiles;

int GetPatternIndexByHwnd(HWND hwnd)
{
    unsigned n;
    for (n = 0; n < NumPatterns; n++)
    {
        if (PatternTiles[n] == hwnd) return n;
    }
    return -1;
}

void DrawPattern(PatternItem *Item, HDC hdc, LPRECT Rect)
{
    HGDIOBJ oldBitmap;
    HDC hdcMem;
    BITMAP bitmap;

    hdcMem = CreateCompatibleDC(hdc);
    oldBitmap = SelectObject(hdcMem, Item->PatternBitmap);

    GetObject(Item->PatternBitmap, sizeof(bitmap), &bitmap);

    SetStretchBltMode(hdc, HALFTONE);
    if (Button_GetCheck(FlipWnd) == BST_CHECKED)
    {
        StretchBlt(hdc, Rect->right, Rect->bottom, -Rect->right, -Rect->bottom, hdcMem, 0, 0, Item->PatternWidth, Item->PatternHeight, SRCCOPY);
    }
    else
    {
        StretchBlt(hdc, Rect->left, Rect->top, Rect->right, Rect->bottom, hdcMem, 0, 0, Item->PatternWidth, Item->PatternHeight, SRCCOPY);
    }

    SelectObject(hdcMem, oldBitmap);
    DeleteDC(hdcMem);
}

LRESULT CALLBACK PatternTileProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    PAINTSTRUCT ps;
    HDC hdc;
    RECT Rect;
    int PatternIndex;
    PatternItem *Item;

    switch (msg)
    {
    case WM_CREATE:
        break;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        break;
    case WM_DESTROY:
        break;

    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
        PatternIndex = GetPatternIndexByHwnd(hwnd);
        if (PatternIndex != -1)
        {
            Item = &Patterns[PatternIndex];
            MessageBox(0, Item->Name, "Clicked", 0);
        }
        break;

    case WM_PAINT:
        hdc = BeginPaint(hwnd, &ps);

        GetClientRect(hwnd, &Rect);

        PatternIndex = GetPatternIndexByHwnd(hwnd);
        if (PatternIndex != -1)
        {
            Item = &Patterns[PatternIndex];
            if (!Item->Hidden) DrawPattern(Item, hdc, &Rect);
        }

        EndPaint(hwnd, &ps);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

bool CheckHidden(int PatternIndex)
{
    PatternItem * Item = &Patterns[PatternIndex];
    RECT Region;
    bool RegionSelected = JpegGetSelectRegion(&Region);
    bool Fit;

    float RegionWidth, RegionHeight;
    float LamdaWidth, LamdaHeight;
    float PLamdaWidth, PLamdaHeight;

    if (RegionSelected)
    {
        //
        // �������� ������ � ������ ����� ��������� � lamda
        //

        RegionWidth = (float)abs(Region.right - Region.left);
        RegionHeight = (float)abs(Region.bottom - Region.top);
        LamdaWidth = RegionWidth / WorkspaceLamda;
        LamdaHeight = RegionHeight / WorkspaceLamda;

        //
        // �������� ������ � ������ �������� � lamda
        //

        PLamdaWidth = (float)Item->PatternWidth / Item->Lamda;
        PLamdaHeight = (float)Item->PatternHeight / Item->Lamda;

        //
        // ��������� ������� � ������ lamda delta
        //

        Fit = false;
        if (PLamdaWidth >= (LamdaWidth - WorkspaceLamda) && PLamdaWidth < (LamdaWidth + WorkspaceLamdaDelta)) Fit = true;
        if (PLamdaHeight >= (LamdaHeight - WorkspaceLamda) && PLamdaHeight < (LamdaHeight + WorkspaceLamdaDelta)) Fit = true;
        return !Fit;
    }
    else return false;
}

// ��������� �� �������� ���� ��������� ������� thumbnail-�������, ���� ������� �� ���������.
// ���������� ���:
//   - ��������� �������� ������������� ����
//   - ��� ������� �� flip
//   - ��� ���������� ������ �������� �� ����������
void RearrangePatternTiles(void)
{
    unsigned n;
    RECT Rect;

    GetClientRect(PatternWnd, &Rect);

    char Text[0x100];
    int Border = 2;
    int Width = Rect.right;
    int TileWidth = Width / 3 - (Border << 1);
    int TileHeight;
    float Aspect;
    int HorPos, VerPos = Border;
    int ColCount = 0, RowCount = 0;
    int MaxHeight = 0;
    int ShownPatterns = 0;

    for (n = 0; n < NumPatterns; n++)
    {
        if (ColCount == 0) HorPos = Border;

        Aspect = (float)Patterns[n].PatternWidth / (float)TileWidth;
        TileHeight = floor ( (float)Patterns[n].PatternHeight / Aspect );

        if (CheckHidden(n))
        {
            Patterns[n].Hidden = true;
            MoveWindow(PatternTiles[n], 0, 0, 0, 0, TRUE);
            continue;
        }
        else
        {
            Patterns[n].Hidden = false;
            MoveWindow(PatternTiles[n], HorPos, VerPos, TileWidth, TileHeight, TRUE);
            ShownPatterns++;
        }

        if (TileHeight > MaxHeight) MaxHeight = TileHeight;

        HorPos += TileWidth + (Border << 1);

        ColCount++;
        if (ColCount > 2)
        {
            ColCount = 0;
            RowCount++;
            VerPos += MaxHeight + Border;
            MaxHeight = 0;
        }
    }

    sprintf(Text, "Patterns : %i / %i", ShownPatterns, NumPatterns);
    SetStatusText(STATUS_PATTERNS, Text);
}

static void PatternAddScanline(unsigned char *buffer, int stride, void *Param)
{
    PatternItem *Item = (PatternItem *)Param;
    memcpy((void *)(Item->PatternRawImage + Item->PatternBufferSize), buffer, stride);
    Item->PatternBufferSize += stride;
}

// ��������� ����� �������.
// ������������� JPEG �������� �� ������� example.c �� libjpeg.
void AddNewPattern(char *name, char *jpeg_path, float lamda)
{
    WNDCLASSEX wc;
    char ClassName[256];
    PatternItem Item;
    char buffer[1024];
    int DecodeResult;

    memset(&Item, 0, sizeof(PatternItem));

    strncpy(Item.Name, name, sizeof(Item.Name) - 1);
    Item.Lamda = lamda;
    Item.Hidden = false;

    DecodeResult = JpegLoad(
        jpeg_path,
        PatternAddScanline,
        &Item.PatternRawImage,
        &Item.PatternBufferSize,
        &Item.PatternWidth,
        &Item.PatternHeight,
        &Item);

    if (DecodeResult == 0)
    {
        MessageBox(0, "Error, decoding pattern JPEG", "Error", 0);
        return;
    }

    Item.PatternBitmap = CreateBitmapFromPixels(GetWindowDC(PatternWnd), Item.PatternWidth, Item.PatternHeight, 24, Item.PatternRawImage);

    Patterns = (PatternItem *)realloc(Patterns, sizeof(PatternItem) * (NumPatterns + 1));
    Patterns[NumPatterns] = Item;

    //
    // �������� ����� ���� ��������
    //

    PatternTiles = (HWND *)realloc(PatternTiles, sizeof(HWND)* (NumPatterns + 1));

    sprintf(ClassName, "PatternTile%i", NumPatterns);

    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = PatternTileProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 2);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = ClassName;
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassEx(&wc))
    {
        MessageBox(0, "Cannot register Pattern TileWnd Class", "Error", 0);
    }

    PatternTiles[NumPatterns] = CreateWindowEx(
        0,
        ClassName,
        "PatternTilePopup",
        WS_OVERLAPPED | WS_CHILDWINDOW,
        2,
        2,
        100,
        100,
        PatternWnd,
        NULL,
        GetModuleHandle(NULL),
        NULL);

    ShowWindow(PatternTiles[NumPatterns], SW_NORMAL);
    UpdateWindow(PatternTiles[NumPatterns]);

    NumPatterns++;

    RearrangePatternTiles();

    // DEBUG
    //sprintf(buffer, "Add Pattern: name:%s, lamda:%f, path:[%s]\n", name, lamda, jpeg_path);
    //MessageBox(0, buffer, "New Pattern", 0);
}

// ������� ��������� � �������� ������� � ������� � ������.
char *TrimString(char *str)
{
    int length, n;
    while (*str == ' ' || *str == '\"') str++;
    length = strlen(str);
    for (n = length - 1; n > 0; n--)
    {
        if (str[n] == ' ' || str[n] == '\"') str[n] = 0;
        else break;
    }
    return str;
}

char *trimwhitespace(char *str)
{
    static char *WHITESPACE = " \t\n\r\"";
    int spacesAtStart = strspn(str, WHITESPACE);
    char *result = str + spacesAtStart;
    int lengthOfNonSpace = strcspn(result, WHITESPACE);
    result[lengthOfNonSpace] = 0;
    return result;
}

void ParseLine(char *line)
{
    char c;
    char command[32];
    char name[128];
    char lamda[16];
    char path[256];
    char buffer[1024];

    // ���������� �������.
    while (*line)
    {
        c = *line;
        if (c > 0 && c <= ' ') line++;
        else break;
    }

    // ���� ������ ������ - �������.
    if (strlen(line) == 0) return;

    // ��������� �����������
    if (*line == '#') return;

    sscanf(line, "%s %[^','],%[^','],%s", command, name, lamda, path);

    if (!_stricmp(command, "pattern"))
    {
        AddNewPattern(TrimString(name), TrimString(path), (float)atof(lamda));
    }
    
    // DEBUG.
    //sprintf(buffer, "Case: command:%s, name:%s, lamda:%s, path:%s\n", command, name, lamda, path);
    //MessageBox(0, buffer, "Line", 0);
}

void ParseDatabase(char *text)
{
    char Text[0x100];
    char linebuffer[0x10000], *ptr = text;
    char *lineptr = linebuffer, c = *ptr;

    // ��������� �� �������.
    while (c)
    {
        c = *ptr++;
        if (c == '\n' || c == 0)
        {
            *lineptr++ = 0;
            ParseLine(linebuffer);
            lineptr = linebuffer;
        }
        else *lineptr++ = c;
    }

    sprintf(Text, "Patterns : %i / %i", NumPatterns, NumPatterns);
    SetStatusText(STATUS_PATTERNS, Text);
}

LRESULT CALLBACK PatternProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    TRACKMOUSEEVENT Tme;

    switch (msg)
    {
    case WM_CREATE:
        break;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        break;
    case WM_DESTROY:
        break;

    //case WM_MOUSEHOVER:
    //    SetCursor(LoadCursor(NULL, IDC_HAND));
    //    break;

    //case WM_MOUSELEAVE:
    //    SetCursor(LoadCursor(NULL, IDC_HAND));
    //    break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

void PatternInit(HWND Parent, char * dbfile)
{
    FILE *f;
    WNDCLASSEX wc;
    char *buffer;
    int filesize;

    ParentWnd = Parent;

    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = PatternProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 2);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = "PatternWnd";
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassEx(&wc))
    {
        MessageBox(0, "Cannot register PatternWnd Class", "Error", 0);
        return;
    }

    PatternWnd = CreateWindowEx(
        0,
        "PatternWnd",
        "PatternWndPopup",
        WS_OVERLAPPED | WS_CHILDWINDOW | WS_VSCROLL,
        2,
        2,
        100,
        100,
        ParentWnd,
        NULL,
        GetModuleHandle(NULL),
        NULL);

    ShowWindow(PatternWnd, SW_NORMAL);
    UpdateWindow(PatternWnd);

    //
    // �������� ������� "flip"
    //

    FlipWnd = CreateWindow(
        "BUTTON", 
        "flip", 
        BS_CHECKBOX | WS_CHILDWINDOW, 
        0, 0, 100, FLIP_BUTTON_HEIGHT - 3, 
        ParentWnd, NULL, GetModuleHandle(NULL), NULL);

    ShowWindow(FlipWnd, SW_NORMAL);
    UpdateWindow(FlipWnd);

    // ��������� � ���������� ���� ������ ���������.
    f = fopen(dbfile, "rb");
    if (f)
    {
        fseek(f, 0, SEEK_END);
        filesize = ftell(f);
        fseek(f, 0, SEEK_SET);
        buffer = (char *)malloc(filesize + 1);
        fread(buffer, 1, filesize, f);
        buffer[filesize] = 0;
        fclose(f);
        ParseDatabase(buffer);
    }
    else MessageBox(0, "Cannot load patterns database info.", "Error", MB_OK);
}

void PatternResize(int Width, int Height)
{
    int NewWidth = max(100, Width - JpegWindowWidth() - 15);

    MoveWindow(PatternWnd, JpegWindowWidth() + 10, FLIP_BUTTON_HEIGHT + 2, NewWidth, max(100, Height - FLIP_BUTTON_HEIGHT - 5), TRUE);

    MoveWindow(FlipWnd, JpegWindowWidth() + 10, 2, NewWidth, FLIP_BUTTON_HEIGHT - 3, TRUE);

    RearrangePatternTiles();
}

void PatternRedraw(void)
{
    InvalidateRect(PatternWnd, NULL, TRUE);
    UpdateWindow(PatternWnd);
}