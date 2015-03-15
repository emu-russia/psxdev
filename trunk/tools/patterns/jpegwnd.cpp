#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h>
#include <windowsx.h>
#include "resource.h"

#include <stdio.h>
#include <math.h>

#include "jpegload.h"
#include "patternwnd.h"
#include "statuswnd.h"
#include "jpegwnd.h"

/*
Controls:

LMB �� ��������� Jpeg : �������� ����������� �����
Esc : ������ �����
Home : ScrollX = ScrollY = 0
LMB �� ��������� Patterns : �������������� ���������
Double click �� ��������� Patterns : Flip ��������
RMB : ��������� ���� ����������

*/

extern HWND FlipWnd;
extern float WorkspaceLamda, WorkspaceLamdaDelta;

LRESULT CALLBACK JpegProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

static HWND ParentWnd;
static HWND JpegWnd;

// Source Jpeg
static unsigned char *JpegBuffer = NULL;
static int JpegBufferSize;
static int JpegWidth, JpegHeight;
static HBITMAP JpegBitmap;

// Scrolling / Region Selection Control
static int ScrollX, ScrollY;
static bool ScrollingBegin;
static int SavedMouseX, SavedMouseY;
static int SavedScrollX, SavedScrollY;
static bool SelectionBegin, RegionSelected;
static int SelectionStartX, SelectionStartY;
static int SelectionEndX, SelectionEndY;
static BOOL DragOccureInPattern;

// Added Patterns Layer.

static PatternEntry * PatternLayer;
static int NumPatterns;

HBITMAP RemoveBitmap;
#define REMOVE_BITMAP_WIDTH 12

#define PATTERN_ENTRY_CLASS "PatternEntry"

static char * SavedImageName = NULL;

static int GetPatternEntryIndexByHwnd(HWND Hwnd)
{
    unsigned n;
    for (n = 0; n < NumPatterns; n++)
    {
        if (PatternLayer[n].Hwnd == Hwnd) return n;
    }
    return -1;
}

static void SaveEntryPositions(void)
{
    unsigned n;
    for (n = 0; n < NumPatterns; n++)
    {
        PatternLayer[n].SavedPosX = PatternLayer[n].PosX;
        PatternLayer[n].SavedPosY = PatternLayer[n].PosY;
    }
}

static void UpdateEntryPositions(int OffsetX, int OffsetY)
{
    unsigned n;
    for (n = 0; n < NumPatterns; n++)
    {
        PatternLayer[n].PosX = PatternLayer[n].SavedPosX + OffsetX;
        PatternLayer[n].PosY = PatternLayer[n].SavedPosY + OffsetY;
        MoveWindow(PatternLayer[n].Hwnd, PatternLayer[n].PosX, PatternLayer[n].PosY, PatternLayer[n].Width, PatternLayer[n].Height, FALSE);
    }
}

// Delete cell from patterns layer.
//  - Delete pattern window
//  - Rearrange patterns array without counting removed cell
static void RemovePatternEntry(int EntryIndex)
{
    PatternEntry * Entry = &PatternLayer[EntryIndex];
    PatternEntry * TempList;
    unsigned Count, Index;
    char Text[0x100];

    DestroyWindow(Entry->Hwnd);

    TempList = (PatternEntry *)malloc(sizeof(PatternEntry)* (NumPatterns - 1));

    for (Count = 0, Index = 0; Count < NumPatterns; Count++)
    {
        if (Count != EntryIndex) TempList[Index++] = PatternLayer[Count];
    }

    free(PatternLayer);
    PatternLayer = TempList;
    NumPatterns--;

    //
    // Update status line.
    //
    sprintf(Text, "Patterns Added : %i", NumPatterns);
    SetStatusText(STATUS_ADDED, Text);
}

static LRESULT CALLBACK PatternEntryProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    PAINTSTRUCT ps;
    HDC hdc;
    HDC hdcMem;
    HGDIOBJ oldBitmap;
    BITMAP bitmap;
    RECT Rect;
    int EntryIndex;
    PatternEntry * Entry;
    PatternItem * Item;
    LRESULT hit;
    int CursorX, CursorY;

    switch (msg)
    {
    case WM_CREATE:
        break;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        break;
    case WM_DESTROY:
        break;

    case WM_ERASEBKGND:
        break;

    //
    // Drag window
    //

    case WM_MOVE:
        EntryIndex = GetPatternEntryIndexByHwnd(hwnd);
        if (EntryIndex != -1)
        {
            Entry = &PatternLayer[EntryIndex];
            Entry->PosX = (int)(short)LOWORD(lParam);
            Entry->PosY = (int)(short)HIWORD(lParam);
        }
        break;

    case WM_MOUSEMOVE:
        EntryIndex = GetPatternEntryIndexByHwnd(hwnd);
        if (wParam == MK_LBUTTON && EntryIndex != -1)
        {
            Entry = &PatternLayer[EntryIndex];
            CursorX = (int)(short)LOWORD(lParam);
            CursorY = (int)(short)LOWORD(lParam);
            if (!(CursorY < REMOVE_BITMAP_WIDTH && CursorX >= (Entry->Width - REMOVE_BITMAP_WIDTH)))
            {
                SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, NULL);
            }
        }
        return FALSE;

    //
    // Pattern remove button press handler.
    //

    case WM_LBUTTONUP:
        EntryIndex = GetPatternEntryIndexByHwnd(hwnd);
        if (EntryIndex != -1)
        {
            Entry = &PatternLayer[EntryIndex];
            CursorX = LOWORD(lParam);
            CursorY = HIWORD(lParam);

            if (CursorY < REMOVE_BITMAP_WIDTH && CursorX >= (Entry->Width - REMOVE_BITMAP_WIDTH))
            {
                if (MessageBox(NULL, "Are you sure?", "User confirm", MB_ICONQUESTION | MB_YESNO) == IDYES)
                {
                    RemovePatternEntry(EntryIndex);
                }
            }
        }
        break;

    //
    // ����������� ��������� ��������� �������� ��������, ���� ������� �� RMB ��������� ������ ��������� ���������.
    //

    case WM_RBUTTONDOWN:
        DragOccureInPattern = true;
        break;

    case WM_RBUTTONUP:
        DragOccureInPattern = false;
        break;

    //
    // Flip entry
    //
    case WM_LBUTTONDBLCLK:
        EntryIndex = GetPatternEntryIndexByHwnd(hwnd);
        if (EntryIndex != -1)
        {
            Entry = &PatternLayer[EntryIndex];
            Entry->Flipped ^= 1;
            Entry->Flipped &= 1;
            InvalidateRect(Entry->Hwnd, NULL, TRUE);
            UpdateWindow(Entry->Hwnd);
        }
        break;

    case WM_PAINT:
        hdc = BeginPaint(hwnd, &ps);

        EntryIndex = GetPatternEntryIndexByHwnd(hwnd);
        if (EntryIndex != -1)
        {
            Entry = &PatternLayer[EntryIndex];
            Item = PatternGetItem(Entry->PatternName);
            Rect.left = 0;
            Rect.top = 0;
            Rect.right = Entry->Width;
            Rect.bottom = Entry->Height;
            DrawPattern(Item, hdc, &Rect, Entry->Flipped, TRUE, TRUE);

            //
            // Pattern remove button.
            //

            if (RemoveBitmap)
            {
                hdcMem = CreateCompatibleDC(hdc);
                oldBitmap = SelectObject(hdcMem, RemoveBitmap);

                GetObject(RemoveBitmap, sizeof(bitmap), &bitmap);
                BitBlt(hdc, Entry->Width - REMOVE_BITMAP_WIDTH, 0, REMOVE_BITMAP_WIDTH, REMOVE_BITMAP_WIDTH, hdcMem, 0, 0, SRCCOPY);

                SelectObject(hdcMem, oldBitmap);
                DeleteDC(hdcMem);
            }
        }

        EndPaint(hwnd, &ps);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

void AddPatternEntry(char * PatternName)
{
    PatternItem * Item = PatternGetItem(PatternName);
    PatternEntry Entry;
    RECT Region;
    bool Selected = JpegGetSelectRegion(&Region);
    float LamdaWidth, LamdaHeight;
    char Text[0x100];

    if (Selected && Item)
    {
        Entry.BlendLevel = 1.0f;
        Entry.Flipped = (Button_GetCheck(FlipWnd) == BST_CHECKED);
        strcpy(Entry.PatternName, PatternName);

        //
        // Width / Height
        //

        LamdaWidth = (float)Item->PatternWidth / Item->Lamda;
        LamdaHeight = (float)Item->PatternHeight / Item->Lamda;
        Entry.Width = LamdaWidth * WorkspaceLamda;
        Entry.Height = LamdaHeight * WorkspaceLamda;

        //
        // Position
        //

        Entry.PlaneX = SelectionStartX - ScrollX;
        Entry.PlaneY = SelectionStartY - ScrollY;

        Entry.PosX = SelectionStartX;
        Entry.PosY = SelectionStartY;

        //
        // Create Window
        //

        Entry.Hwnd = CreateWindowEx(
            0,
            PATTERN_ENTRY_CLASS,
            "PatternEntryPopup",
            WS_OVERLAPPED | WS_CHILDWINDOW | WS_EX_LAYERED | WS_EX_NOPARENTNOTIFY,
            Entry.PosX,
            Entry.PosY,
            Entry.Width,
            Entry.Height,
            JpegWnd,
            NULL,
            GetModuleHandle(NULL),
            NULL);

        ShowWindow(Entry.Hwnd, SW_NORMAL);
        UpdateWindow(Entry.Hwnd);

        //
        // Add Entry in List.
        //

        PatternLayer = (PatternEntry *)realloc(PatternLayer, sizeof(PatternEntry)* (NumPatterns + 1));
        PatternLayer[NumPatterns++] = Entry;

        sprintf(Text, "Patterns Added : %i", NumPatterns);
        SetStatusText(STATUS_ADDED, Text);
    }
}

// �������� ����������� ������� ���������� ���������� (��������� �� ������ / ���������, Flip ���.)
void UpdatePatternEntry(int EntryIndex, PatternEntry * Entry)
{
    PatternEntry * Orig = GetPatternEntry(EntryIndex);

    Orig->BlendLevel = Entry->BlendLevel;
    Orig->Flipped = Entry->Flipped;

    Orig->PosX = Entry->PosX;
    Orig->PosY = Entry->PosY;
    Orig->PlaneX = Entry->PlaneX;
    Orig->PlaneY = Entry->PlaneY;

    MoveWindow(Orig->Hwnd, Orig->PosX, Orig->PosY, Orig->Width, Orig->Height, TRUE);

    //InvalidateRect(Orig->Hwnd, NULL, TRUE);
    //UpdateWindow(Orig->Hwnd);
}

static void UpdateSelectionStatus(void)
{
    char Text[0x100];
    int Width, Height;
    float LamdaWidth, LamdaHeight;

    Width = abs(SelectionEndX - SelectionStartX);
    Height = abs(SelectionEndY - SelectionStartY);

    if (RegionSelected && Width > 10 && Height > 10)
    {
        LamdaWidth = (float)Width / WorkspaceLamda;
        LamdaHeight = (float)Height / WorkspaceLamda;
        sprintf(Text, "Selected: %i / %ipx, %.1f / %.1fl", Width, Height, LamdaWidth, LamdaHeight);
        SetStatusText(STATUS_SELECTED, Text);
    }
    else SetStatusText(STATUS_SELECTED, "Selected: ---");
}

LRESULT CALLBACK JpegProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    PAINTSTRUCT ps;
    HDC hdc;
    HGDIOBJ oldBitmap;
    HDC hdcMem;
    BITMAP bitmap;
    HGDIOBJ oldColor;
    RECT Rect;
    char Text[0x100];

    switch (msg)
    {
    case WM_CREATE:
        break;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        break;
    case WM_DESTROY:
        break;

    case WM_PAINT:
        hdc = BeginPaint(hwnd, &ps);

        if (JpegBitmap)
        {
            hdcMem = CreateCompatibleDC(hdc);
            oldBitmap = SelectObject(hdcMem, JpegBitmap);

            GetObject(JpegBitmap, sizeof(bitmap), &bitmap);
            BitBlt(hdc, ScrollX, ScrollY, bitmap.bmWidth, bitmap.bmHeight, hdcMem, 0, 0, SRCCOPY);

            SelectObject(hdcMem, oldBitmap);
            DeleteDC(hdcMem);
        }

        // Select box.
        if (RegionSelected)
        {
            oldColor = SelectObject(hdc, GetStockObject(DC_PEN));
            SetDCPenColor(hdc, RGB(0xaa, 0xaa, 0xaa));
            MoveToEx(hdc, SelectionStartX, SelectionStartY, NULL);
            LineTo(hdc, SelectionEndX, SelectionStartY);
            LineTo(hdc, SelectionEndX, SelectionEndY);
            LineTo(hdc, SelectionStartX, SelectionEndY);
            LineTo(hdc, SelectionStartX, SelectionStartY);
            SelectObject(hdc, oldColor);
        }

        EndPaint(hwnd, &ps);
        break;

    case WM_LBUTTONDOWN:
        RegionSelected = false;
        SelectionBegin = true;
        SelectionStartX = LOWORD(lParam);
        SelectionStartY = HIWORD(lParam);
        InvalidateRect(JpegWnd, NULL, TRUE);
        UpdateWindow(JpegWnd);
        break;

    case WM_RBUTTONDOWN:
        if (DragOccureInPattern == false)
        {
            ScrollingBegin = true;
            RegionSelected = false;
            SavedMouseX = LOWORD(lParam);
            SavedMouseY = HIWORD(lParam);
            SavedScrollX = ScrollX;
            SavedScrollY = ScrollY;
            SaveEntryPositions();
        }
        break;

    case WM_MOUSEMOVE:
        if (ScrollingBegin)
        {
            ScrollX = SavedScrollX + LOWORD(lParam) - SavedMouseX;
            ScrollY = SavedScrollY + HIWORD(lParam) - SavedMouseY;
            InvalidateRect(JpegWnd, NULL, FALSE);
            UpdateWindow(JpegWnd);

            UpdateEntryPositions(LOWORD(lParam) - SavedMouseX, HIWORD(lParam) - SavedMouseY);

            sprintf(Text, "Scroll : %i / %ipx", ScrollX, ScrollY);
            SetStatusText(STATUS_SCROLL, Text);
        }
        if (SelectionBegin)
        {
            SelectionEndX = LOWORD(lParam);
            SelectionEndY = HIWORD(lParam);
            RegionSelected = true;
            Rect.left = SelectionStartX;
            Rect.top = SelectionStartY;
            Rect.right = SelectionEndX + 1;
            Rect.bottom = SelectionEndY + 1;
            InvalidateRect(JpegWnd, &Rect, TRUE);
            UpdateWindow(JpegWnd);
            RearrangePatternTiles();
            PatternRedraw();
            UpdateSelectionStatus();
        }
        break;

    case WM_RBUTTONUP:
        if (DragOccureInPattern == false)
        {
            ScrollingBegin = false;
            //InvalidateRect(JpegWnd, NULL, TRUE);
            //UpdateWindow(JpegWnd);

            UpdateEntryPositions(LOWORD(lParam) - SavedMouseX, HIWORD(lParam) - SavedMouseY);

            sprintf(Text, "Scroll : %i / %ipx", ScrollX, ScrollY);
            SetStatusText(STATUS_SCROLL, Text);
        }
        DragOccureInPattern = false;
        break;

    case WM_LBUTTONUP:
        SelectionBegin = false;
        InvalidateRect(JpegWnd, NULL, TRUE);
        UpdateWindow(JpegWnd);
        RearrangePatternTiles();
        PatternRedraw();
        UpdateSelectionStatus();
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

void JpegInit(HWND Parent)
{
    WNDCLASSEX wc;

    ScrollX = ScrollY = 0;
    ScrollingBegin = false;
    SelectionBegin = RegionSelected = false;

    ParentWnd = Parent;

    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = JpegProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 2);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = "JpegWnd";
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassEx(&wc))
    {
        MessageBox(0, "Cannot register JpgWnd Class", "Error", 0);
        return;
    }

    JpegWnd = CreateWindowEx(
        0,
        "JpegWnd",
        "JpegWndPopup",
        WS_OVERLAPPED | WS_CHILDWINDOW,
        2,
        2,
        100,
        100,
        ParentWnd,
        NULL,
        GetModuleHandle(NULL),
        NULL);

    ShowWindow(JpegWnd, SW_NORMAL);
    UpdateWindow(JpegWnd);

    //
    // Register pattern window class.
    //

    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = PatternEntryProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_HAND);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 2);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = PATTERN_ENTRY_CLASS;
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassEx(&wc))
    {
        MessageBox(0, "Cannot register Pattern EntryWnd Class", "Error", 0);
    }

    //
    // Load pattern remove button picture
    //

    RemoveBitmap = LoadBitmap(GetModuleHandle(NULL), MAKEINTRESOURCE(IDB_REMOVE));
}

static void JpegAddScanline(unsigned char *buffer, int stride, void *Param)
{
    memcpy((void *)(JpegBuffer + JpegBufferSize), buffer, stride);
    JpegBufferSize += stride;
}

// Load source Jpeg image
void JpegLoadImage(char *filename, bool Silent)
{
    char Text[1024];

    SetCursor(LoadCursor(NULL, IDC_WAIT));

    JpegLoad(
        filename,
        JpegAddScanline,
        &JpegBuffer,
        &JpegBufferSize,
        &JpegWidth,
        &JpegHeight,
        NULL );

    if (JpegBitmap)
    {
        DeleteObject(JpegBitmap);
        JpegBitmap = NULL;
    }
    if (JpegBuffer)
        JpegBitmap = CreateBitmapFromPixels(GetWindowDC(JpegWnd), JpegWidth, JpegHeight, 24, JpegBuffer);

    if (!Silent) MessageBox(0, "Loaded", "Loaded", MB_OK);

    sprintf(Text, "Source Image : %s", filename);
    SetStatusText(STATUS_SOURCE_IMAGE, Text);

    ScrollX = ScrollY = 0;

    if (SavedImageName)
    {
        free(SavedImageName);
        SavedImageName = NULL;
    }
    SavedImageName = (char *)malloc(strlen(filename) + 1);
    strcpy(SavedImageName, filename);
    SavedImageName[strlen(filename)] = 0;

    SetCursor(LoadCursor(NULL, IDC_ARROW));

    JpegRedraw();
}

// ���������� true � ��������� ������, ���� ����� ����
// ���������� false, ���� ����� ���
bool JpegGetSelectRegion(LPRECT Region)
{
    int Width, Height;

    if (RegionSelected)
    {
        Width = abs(SelectionEndX - SelectionStartX);
        Height = abs(SelectionEndY - SelectionStartY);

        if (Width * Height < 100) return false;

        Region->left = SelectionStartX;
        Region->top = SelectionStartY;
        Region->right = SelectionEndX;
        Region->bottom = SelectionEndY;

        return true;
    }
    else return false;
}

void JpegSetSelectRegion(LPRECT Region)
{
    SelectionStartX = Region->left;
    SelectionStartY = Region->top;
    SelectionEndX = Region->right;
    SelectionEndY = Region->bottom;

    RegionSelected = true;
}

// �������� ������ ���� Jpeg � ������������ � ��������� ������������� ����.
void JpegResize(int Width, int Height)
{
    MoveWindow(JpegWnd, 2, 2, max(100, Width - 300), max(100, Height - 5), TRUE);

    // �������� ������ ��������� ��� ��������� �������� ����.
    JpegRemoveSelection();
}

// ������� ������ ����.
int JpegWindowWidth(void)
{
    RECT Rect;
    GetClientRect(JpegWnd, &Rect);
    return Rect.right;
}

void JpegRemoveSelection(void)
{
    RegionSelected = false;
    SetStatusText(STATUS_SELECTED, "Selected: ---");
    JpegRedraw();
}

void JpegRedraw(void)
{
    InvalidateRect(JpegWnd, NULL, TRUE);
    UpdateWindow(JpegWnd);
}

PatternEntry * GetPatternEntry(int EntryIndex)
{
    return &PatternLayer[EntryIndex];
}

int GetPatternEntryNum(void)
{
    return NumPatterns;
}

// ���������� ��� ������� ����� ����, ����� ����� ������� �� ������.
void JpegDestroy(void)
{
    unsigned Count;
    PatternEntry *Entry;

    JpegRemoveSelection();

    ScrollX = ScrollY = 0;

    //
    // ��������� �������� ��������.
    //

    if (JpegBitmap)
    {
        DeleteObject(JpegBitmap);
        JpegBitmap = NULL;
    }

    //
    // ��������� ���������.
    //

    for (Count = 0; Count < NumPatterns; Count++)
    {
        Entry = GetPatternEntry(Count);
        DestroyWindow(Entry->Hwnd);
    }

    if (PatternLayer)
    {
        free(PatternLayer);
        PatternLayer = NULL;
    }
    NumPatterns = 0;

    JpegRedraw();
}

char * JpegGetImageName(void)
{
    return SavedImageName;
}

void JpegGetScroll(LPPOINT Offset)
{
    Offset->x = ScrollX;
    Offset->y = ScrollY;
}

void JpegSetScroll(LPPOINT Offset)
{
    char Text[0x100];

    ScrollX = Offset->x;
    ScrollY = Offset->y;

    JpegRedraw();

    sprintf(Text, "Scroll : %i / %ipx", ScrollX, ScrollY);
    SetStatusText(STATUS_SCROLL, Text);
}
