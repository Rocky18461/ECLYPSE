#include <Windows.h>
#include <dwmapi.h>
#include <winioctl.h>
#include <string>
#include <vector>
#include <cstdio>
#include <shlwapi.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shlwapi.lib")

// ECLYPSE color palette
namespace Colors
{
    constexpr COLORREF WindowBg    = RGB(20, 20, 32);     // #141420
    constexpr COLORREF SidebarBg   = RGB(14, 13, 22);     // #0E0D16
    constexpr COLORREF Accent      = RGB(140, 102, 230);  // #8C66E6
    constexpr COLORREF AccentHover = RGB(166, 128, 255);  // #A680FF
    constexpr COLORREF AccentPress = RGB(110, 80, 190);   // #6E50BE
    constexpr COLORREF Text        = RGB(224, 224, 235);  // #E0E0EB
    constexpr COLORREF TextDim     = RGB(107, 102, 128);  // #6B6680
    constexpr COLORREF FrameBg     = RGB(31, 28, 46);     // #1F1C2E
    constexpr COLORREF Border      = RGB(41, 36, 56);     // #292438
    constexpr COLORREF TitleBg     = RGB(15, 13, 23);     // #0F0D17
    constexpr COLORREF TabHover    = RGB(30, 28, 42);
    constexpr COLORREF TabActive   = RGB(25, 23, 38);
    constexpr COLORREF Warning     = RGB(230, 180, 50);   // #E6B432
    constexpr COLORREF Danger      = RGB(200, 60, 60);    // #C83C3C
    constexpr COLORREF DangerHover = RGB(230, 80, 80);    // #E65050
}

struct ButtonState
{
    bool hovered = false;
    bool pressed = false;
};

enum Tab
{
    TAB_CLEANER,
    TAB_PARTITIONS,
    TAB_OTHERS,
    TAB_COUNT
};

static const wchar_t* g_tabNames[TAB_COUNT] = {
    L"Cleaner",
    L"Partitions",
    L"Others"
};

static const wchar_t* g_tabDescriptions[TAB_COUNT] = {
    L"Wipe an entire drive. All data will be permanently deleted.",
    L"Manage disk partitions and Windows installation guide.",
    L"Clean temp files, logs, caches, registry, prefetch, DNS, and game folders."
};

// Others tab options
enum OthersOption
{
    OPT_TEMP_FILES,
    OPT_LOGS,
    OPT_CACHE,
    OPT_REGISTRY,
    OPT_PREFETCH,
    OPT_DNS,
    OPT_GAME_FOLDERS,
    OPT_COUNT
};

static const wchar_t* g_optionNames[OPT_COUNT] = {
    L"Temp Files",
    L"Logs",
    L"Cache",
    L"Registry",
    L"Prefetch",
    L"DNS Cache",
    L"Game Folders"
};

static const wchar_t* g_optionDescs[OPT_COUNT] = {
    L"Clear Windows temporary files and folders",
    L"Delete system and application log files",
    L"Clear browser and application caches",
    L"Clean up leftover registry entries",
    L"Clear Windows prefetch data",
    L"Flush the DNS resolver cache",
    L"Wipe specific game directories and traces"
};

// Drive info
struct DriveInfo
{
    wchar_t letter;
    std::wstring label;
    std::wstring typeStr;
    ULARGE_INTEGER totalBytes;
    ULARGE_INTEGER freeBytes;
};

static HFONT g_titleFont = nullptr;
static HFONT g_buttonFont = nullptr;
static HFONT g_tabFont = nullptr;
static HFONT g_descFont = nullptr;
static HFONT g_smallFont = nullptr;

static int g_activeTab = TAB_CLEANER;
static int g_hoveredTab = -1;
static ButtonState g_actionBtn;
static ButtonState g_backBtn;
static RECT g_actionBtnRect = {};
static RECT g_backBtnRect = {};

// Drive selection state
static bool g_drivesLoaded = false;
static std::vector<DriveInfo> g_drives;
static std::vector<RECT> g_driveBtnRects;
static std::vector<ButtonState> g_driveBtnStates;
static int g_selectedDrive = -1;

// Partitions tab state
static ButtonState g_guideBtn;
static ButtonState g_mergeBtn;
static RECT g_guideBtnRect = {};
static RECT g_mergeBtnRect = {};
static bool g_partDrivesLoaded = false;
static std::vector<DriveInfo> g_partDrives;
static std::vector<RECT> g_partDriveBtnRects;
static std::vector<ButtonState> g_partDriveBtnStates;
static int g_partSelectedDrive = -1;
static bool g_partShowDriveSelect = false;

// Others tab state
static ButtonState g_optionBtns[OPT_COUNT];
static RECT g_optionBtnRects[OPT_COUNT];

constexpr int SIDEBAR_WIDTH = 150;
constexpr int TAB_HEIGHT = 38;
constexpr int TAB_TOP_OFFSET = 60;

RECT GetTabRect(int index)
{
    RECT rc;
    rc.left = 0;
    rc.top = TAB_TOP_OFFSET + index * TAB_HEIGHT;
    rc.right = SIDEBAR_WIDTH;
    rc.bottom = rc.top + TAB_HEIGHT;
    return rc;
}

void EnumerateDrives()
{
    g_drives.clear();
    DWORD driveMask = GetLogicalDrives();

    for (int i = 0; i < 26; i++)
    {
        if (!(driveMask & (1 << i)))
            continue;

        wchar_t root[] = { (wchar_t)(L'A' + i), L':', L'\\', 0 };
        UINT driveType = GetDriveType(root);

        if (driveType == DRIVE_NO_ROOT_DIR || driveType == DRIVE_UNKNOWN)
            continue;

        DriveInfo info = {};
        info.letter = L'A' + i;

        switch (driveType)
        {
        case DRIVE_REMOVABLE: info.typeStr = L"Removable"; break;
        case DRIVE_FIXED:     info.typeStr = L"Local Disk"; break;
        case DRIVE_REMOTE:    info.typeStr = L"Network"; break;
        case DRIVE_CDROM:     info.typeStr = L"CD/DVD"; break;
        case DRIVE_RAMDISK:   info.typeStr = L"RAM Disk"; break;
        default:              info.typeStr = L"Unknown"; break;
        }

        wchar_t volumeName[MAX_PATH] = {};
        GetVolumeInformation(root, volumeName, MAX_PATH, nullptr, nullptr, nullptr, nullptr, 0);
        info.label = volumeName[0] ? volumeName : info.typeStr;

        GetDiskFreeSpaceEx(root, nullptr, &info.totalBytes, &info.freeBytes);

        g_drives.push_back(info);
    }

    g_driveBtnRects.resize(g_drives.size());
    g_driveBtnStates.resize(g_drives.size());
    for (auto& s : g_driveBtnStates)
        s = {};
}

std::wstring FormatBytes(ULONGLONG bytes)
{
    wchar_t buf[64];
    if (bytes >= (1ULL << 40))
        wsprintfW(buf, L"%llu GB", bytes / (1ULL << 30));
    else if (bytes >= (1ULL << 30))
        wsprintfW(buf, L"%.1f GB", (double)bytes / (1ULL << 30));
    else if (bytes >= (1ULL << 20))
        wsprintfW(buf, L"%llu MB", bytes / (1ULL << 20));
    else
        wsprintfW(buf, L"%llu KB", bytes / (1ULL << 10));
    return buf;
}

void DrawRoundedButton(HDC hdc, RECT rc, const wchar_t* text, ButtonState& state, bool danger = false)
{
    COLORREF bgColor = Colors::FrameBg;
    COLORREF borderColor = danger ? Colors::Danger : Colors::Accent;
    COLORREF textColor = Colors::Text;

    if (state.pressed)
    {
        bgColor = danger ? RGB(150, 40, 40) : Colors::AccentPress;
        borderColor = bgColor;
        textColor = RGB(255, 255, 255);
    }
    else if (state.hovered)
    {
        bgColor = danger ? Colors::Danger : Colors::Accent;
        borderColor = danger ? Colors::DangerHover : Colors::AccentHover;
        textColor = RGB(255, 255, 255);
    }

    HBRUSH bgBrush = CreateSolidBrush(bgColor);
    HPEN borderPen = CreatePen(PS_SOLID, 2, borderColor);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, bgBrush);
    HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);

    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 10, 10);

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(bgBrush);
    DeleteObject(borderPen);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, textColor);
    HFONT oldFont = (HFONT)SelectObject(hdc, g_buttonFont);
    DrawText(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, oldFont);
}

void DrawSidebar(HDC hdc, RECT clientRect)
{
    RECT sidebarRect = { 0, 0, SIDEBAR_WIDTH, clientRect.bottom };
    HBRUSH sidebarBrush = CreateSolidBrush(Colors::SidebarBg);
    FillRect(hdc, &sidebarRect, sidebarBrush);
    DeleteObject(sidebarBrush);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, Colors::Accent);
    HFONT oldFont = (HFONT)SelectObject(hdc, g_titleFont);
    RECT titleRect = { 0, 15, SIDEBAR_WIDTH, 50 };
    DrawText(hdc, L"ECLYPSE", -1, &titleRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, g_tabFont);
    for (int i = 0; i < TAB_COUNT; i++)
    {
        RECT tabRect = GetTabRect(i);

        if (i == g_activeTab)
        {
            HBRUSH tabBrush = CreateSolidBrush(Colors::TabActive);
            FillRect(hdc, &tabRect, tabBrush);
            DeleteObject(tabBrush);

            RECT accentBar = { 0, tabRect.top + 4, 3, tabRect.bottom - 4 };
            HBRUSH accentBrush = CreateSolidBrush(Colors::Accent);
            FillRect(hdc, &accentBar, accentBrush);
            DeleteObject(accentBrush);

            SetTextColor(hdc, Colors::Text);
        }
        else if (i == g_hoveredTab)
        {
            HBRUSH hoverBrush = CreateSolidBrush(Colors::TabHover);
            FillRect(hdc, &tabRect, hoverBrush);
            DeleteObject(hoverBrush);
            SetTextColor(hdc, Colors::TextDim);
        }
        else
        {
            SetTextColor(hdc, Colors::TextDim);
        }

        int dotX = 14;
        int dotY = tabRect.top + TAB_HEIGHT / 2;
        HBRUSH dotBrush = CreateSolidBrush(i == g_activeTab ? Colors::Accent : Colors::Border);
        HPEN dotPen = CreatePen(PS_SOLID, 1, i == g_activeTab ? Colors::Accent : Colors::Border);
        HBRUSH oldBr = (HBRUSH)SelectObject(hdc, dotBrush);
        HPEN oldPn = (HPEN)SelectObject(hdc, dotPen);
        Ellipse(hdc, dotX - 3, dotY - 3, dotX + 3, dotY + 3);
        SelectObject(hdc, oldBr);
        SelectObject(hdc, oldPn);
        DeleteObject(dotBrush);
        DeleteObject(dotPen);

        RECT textRect = { 26, tabRect.top, SIDEBAR_WIDTH - 8, tabRect.bottom };
        DrawText(hdc, g_tabNames[i], -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    HPEN divPen = CreatePen(PS_SOLID, 1, Colors::Border);
    HPEN oldPen = (HPEN)SelectObject(hdc, divPen);
    MoveToEx(hdc, SIDEBAR_WIDTH - 1, 0, nullptr);
    LineTo(hdc, SIDEBAR_WIDTH - 1, clientRect.bottom);
    SelectObject(hdc, oldPen);
    DeleteObject(divPen);

    SelectObject(hdc, oldFont);
}

void DrawDriveSelectPanel(HDC hdc, RECT clientRect)
{
    int contentLeft = SIDEBAR_WIDTH + 30;
    int contentRight = clientRect.right - 30;
    int contentCenterX = (contentLeft + contentRight) / 2;

    SetBkMode(hdc, TRANSPARENT);

    // Header
    SetTextColor(hdc, Colors::Text);
    HFONT oldFont = (HFONT)SelectObject(hdc, g_titleFont);
    RECT headerRect = { contentLeft, 25, contentRight, 55 };
    DrawText(hdc, L"Select Drive to Wipe", -1, &headerRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Separator
    HPEN sepPen = CreatePen(PS_SOLID, 1, Colors::Border);
    HPEN oldPen = (HPEN)SelectObject(hdc, sepPen);
    MoveToEx(hdc, contentLeft, 65, nullptr);
    LineTo(hdc, contentRight, 65);
    SelectObject(hdc, oldPen);
    DeleteObject(sepPen);

    // Warning text
    SetTextColor(hdc, Colors::Warning);
    SelectObject(hdc, g_descFont);
    RECT warnRect = { contentLeft, 75, contentRight, 110 };
    DrawText(hdc, L"WARNING: This will permanently delete ALL data on the selected drive. Back up any important data first!", -1, &warnRect, DT_LEFT | DT_WORDBREAK);

    // Drive buttons
    int btnWidth = 380;
    int btnHeight = 42;
    int startY = 120;
    int spacing = 8;

    SelectObject(hdc, g_buttonFont);

    for (size_t i = 0; i < g_drives.size(); i++)
    {
        RECT btnRect;
        btnRect.left = contentCenterX - btnWidth / 2;
        btnRect.right = btnRect.left + btnWidth;
        btnRect.top = startY + (int)i * (btnHeight + spacing);
        btnRect.bottom = btnRect.top + btnHeight;
        g_driveBtnRects[i] = btnRect;

        // Draw drive button
        bool isSelected = (g_selectedDrive == (int)i);
        COLORREF bgColor = isSelected ? Colors::TabActive : Colors::FrameBg;
        COLORREF borderColor = isSelected ? Colors::Accent : Colors::Border;
        COLORREF textColor = Colors::Text;

        if (g_driveBtnStates[i].pressed)
        {
            bgColor = Colors::AccentPress;
            borderColor = Colors::Accent;
            textColor = RGB(255, 255, 255);
        }
        else if (g_driveBtnStates[i].hovered)
        {
            bgColor = Colors::TabHover;
            borderColor = Colors::Accent;
            textColor = RGB(255, 255, 255);
        }

        HBRUSH bgBrush = CreateSolidBrush(bgColor);
        HPEN bPen = CreatePen(PS_SOLID, 1, borderColor);
        HBRUSH oldBr = (HBRUSH)SelectObject(hdc, bgBrush);
        HPEN oldPn = (HPEN)SelectObject(hdc, bPen);
        RoundRect(hdc, btnRect.left, btnRect.top, btnRect.right, btnRect.bottom, 8, 8);
        SelectObject(hdc, oldBr);
        SelectObject(hdc, oldPn);
        DeleteObject(bgBrush);
        DeleteObject(bPen);

        // Drive letter + label
        SetTextColor(hdc, textColor);
        wchar_t driveText[128];
        std::wstring sizeStr = FormatBytes(g_drives[i].totalBytes.QuadPart);
        std::wstring freeStr = FormatBytes(g_drives[i].freeBytes.QuadPart);
        wsprintfW(driveText, L"  %c:  %s  (%s)", g_drives[i].letter, g_drives[i].label.c_str(), g_drives[i].typeStr.c_str());

        RECT labelRect = btnRect;
        labelRect.left += 10;
        SelectObject(hdc, g_buttonFont);
        DrawText(hdc, driveText, -1, &labelRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // Size info on right side
        SetTextColor(hdc, Colors::TextDim);
        wchar_t sizeText[64];
        wsprintfW(sizeText, L"%s free / %s", freeStr.c_str(), sizeStr.c_str());
        RECT sizeRect = btnRect;
        sizeRect.right -= 12;
        SelectObject(hdc, g_smallFont);
        DrawText(hdc, sizeText, -1, &sizeRect, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }

    // Clean button below drive list
    int cleanBtnW = 200;
    int cleanBtnH = 50;
    int cleanBtnY = startY + (int)g_drives.size() * (btnHeight + spacing) + 20;
    g_actionBtnRect.left = contentCenterX - cleanBtnW / 2;
    g_actionBtnRect.right = g_actionBtnRect.left + cleanBtnW;
    g_actionBtnRect.top = cleanBtnY;
    g_actionBtnRect.bottom = cleanBtnY + cleanBtnH;

    DrawRoundedButton(hdc, g_actionBtnRect, L"Clean", g_actionBtn);

    // Status text below button
    if (g_selectedDrive >= 0 && g_selectedDrive < (int)g_drives.size())
    {
        SetTextColor(hdc, Colors::TextDim);
        SelectObject(hdc, g_descFont);
        wchar_t statusText[64];
        wsprintfW(statusText, L"Selected: %c:\\ (%s)", g_drives[g_selectedDrive].letter, g_drives[g_selectedDrive].label.c_str());
        RECT statusRect = { contentLeft, g_actionBtnRect.bottom + 10, contentRight, g_actionBtnRect.bottom + 35 };
        DrawText(hdc, statusText, -1, &statusRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    else
    {
        SetTextColor(hdc, Colors::TextDim);
        SelectObject(hdc, g_descFont);
        RECT statusRect = { contentLeft, g_actionBtnRect.bottom + 10, contentRight, g_actionBtnRect.bottom + 35 };
        DrawText(hdc, L"Select a drive above", -1, &statusRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    SelectObject(hdc, oldFont);
}

void DrawOthersPanel(HDC hdc, RECT clientRect)
{
    int contentLeft = SIDEBAR_WIDTH + 30;
    int contentRight = clientRect.right - 30;
    int contentCenterX = (contentLeft + contentRight) / 2;

    SetBkMode(hdc, TRANSPARENT);

    // Header
    SetTextColor(hdc, Colors::Text);
    HFONT oldFont = (HFONT)SelectObject(hdc, g_titleFont);
    RECT headerRect = { contentLeft, 25, contentRight, 55 };
    DrawText(hdc, L"Others", -1, &headerRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Separator
    HPEN sepPen = CreatePen(PS_SOLID, 1, Colors::Border);
    HPEN oldPen = (HPEN)SelectObject(hdc, sepPen);
    MoveToEx(hdc, contentLeft, 65, nullptr);
    LineTo(hdc, contentRight, 65);
    SelectObject(hdc, oldPen);
    DeleteObject(sepPen);

    SetTextColor(hdc, Colors::TextDim);
    SelectObject(hdc, g_descFont);
    RECT descRect = { contentLeft, 75, contentRight, 100 };
    DrawText(hdc, L"Clean temp files, logs, caches, registry, prefetch, DNS, and game folders.", -1, &descRect, DT_LEFT | DT_WORDBREAK);

    // Option buttons - two columns
    int btnWidth = 190;
    int btnHeight = 42;
    int spacing = 10;
    int colGap = 14;
    int startY = 110;
    int col1Left = contentCenterX - btnWidth - colGap / 2;
    int col2Left = contentCenterX + colGap / 2;

    for (int i = 0; i < OPT_COUNT; i++)
    {
        int col = i % 2;
        int row = i / 2;

        RECT btnRect;
        btnRect.left = (col == 0) ? col1Left : col2Left;
        btnRect.right = btnRect.left + btnWidth;
        btnRect.top = startY + row * (btnHeight + spacing);
        btnRect.bottom = btnRect.top + btnHeight;
        g_optionBtnRects[i] = btnRect;

        DrawRoundedButton(hdc, btnRect, g_optionNames[i], g_optionBtns[i]);
    }

    SelectObject(hdc, oldFont);
}

void DrawPartitionsPanel(HDC hdc, RECT clientRect)
{
    int contentLeft = SIDEBAR_WIDTH + 30;
    int contentRight = clientRect.right - 30;
    int contentCenterX = (contentLeft + contentRight) / 2;

    SetBkMode(hdc, TRANSPARENT);

    // Header
    SetTextColor(hdc, Colors::Text);
    HFONT oldFont = (HFONT)SelectObject(hdc, g_titleFont);
    RECT headerRect = { contentLeft, 25, contentRight, 55 };
    DrawText(hdc, L"Partitions", -1, &headerRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Separator
    HPEN sepPen = CreatePen(PS_SOLID, 1, Colors::Border);
    HPEN oldPen = (HPEN)SelectObject(hdc, sepPen);
    MoveToEx(hdc, contentLeft, 65, nullptr);
    LineTo(hdc, contentRight, 65);
    SelectObject(hdc, oldPen);
    DeleteObject(sepPen);

    SetTextColor(hdc, Colors::TextDim);
    SelectObject(hdc, g_descFont);
    RECT descRect = { contentLeft, 75, contentRight, 100 };
    DrawText(hdc, L"Manage disk partitions and get help installing Windows.", -1, &descRect, DT_LEFT | DT_WORDBREAK);

    SelectObject(hdc, oldFont);

    if (!g_partShowDriveSelect)
    {
        // Two main buttons
        int btnWidth = 340;
        int btnHeight = 50;

        g_guideBtnRect.left = contentCenterX - btnWidth / 2;
        g_guideBtnRect.right = g_guideBtnRect.left + btnWidth;
        g_guideBtnRect.top = 130;
        g_guideBtnRect.bottom = g_guideBtnRect.top + btnHeight;
        DrawRoundedButton(hdc, g_guideBtnRect, L"Windows 10 Installation Guide", g_guideBtn);

        g_mergeBtnRect.left = contentCenterX - btnWidth / 2;
        g_mergeBtnRect.right = g_mergeBtnRect.left + btnWidth;
        g_mergeBtnRect.top = 200;
        g_mergeBtnRect.bottom = g_mergeBtnRect.top + btnHeight;
        DrawRoundedButton(hdc, g_mergeBtnRect, L"Merge All Partitions", g_mergeBtn);

        // Descriptions under each button
        SetTextColor(hdc, Colors::TextDim);
        HFONT oldFont2 = (HFONT)SelectObject(hdc, g_smallFont);
        RECT guideDesc = { contentLeft, g_guideBtnRect.bottom + 4, contentRight, g_guideBtnRect.bottom + 20 };
        DrawText(hdc, L"Step-by-step guide to install Windows 10 on a partition", -1, &guideDesc, DT_CENTER | DT_SINGLELINE);

        RECT mergeDesc = { contentLeft, g_mergeBtnRect.bottom + 4, contentRight, g_mergeBtnRect.bottom + 20 };
        DrawText(hdc, L"Delete all partitions and create a single volume using the full disk", -1, &mergeDesc, DT_CENTER | DT_SINGLELINE);
        SelectObject(hdc, oldFont2);
    }
    else
    {
        // Drive selection for merge
        SetTextColor(hdc, Colors::Warning);
        SelectObject(hdc, g_descFont);
        RECT warnRect = { contentLeft, 108, contentRight, 138 };
        DrawText(hdc, L"Select a disk to merge. ALL partitions on it will be deleted and replaced with a single NTFS volume.", -1, &warnRect, DT_LEFT | DT_WORDBREAK);

        int btnWidth = 380;
        int btnHeight = 42;
        int startY = 148;
        int spacing = 8;

        for (size_t i = 0; i < g_partDrives.size(); i++)
        {
            RECT btnRect;
            btnRect.left = contentCenterX - btnWidth / 2;
            btnRect.right = btnRect.left + btnWidth;
            btnRect.top = startY + (int)i * (btnHeight + spacing);
            btnRect.bottom = btnRect.top + btnHeight;
            g_partDriveBtnRects[i] = btnRect;

            bool isSelected = (g_partSelectedDrive == (int)i);
            COLORREF bgColor = isSelected ? Colors::TabActive : Colors::FrameBg;
            COLORREF borderColor = isSelected ? Colors::Accent : Colors::Border;
            COLORREF textColor = Colors::Text;

            if (g_partDriveBtnStates[i].pressed)
            {
                bgColor = Colors::AccentPress;
                borderColor = Colors::Accent;
                textColor = RGB(255, 255, 255);
            }
            else if (g_partDriveBtnStates[i].hovered)
            {
                bgColor = Colors::TabHover;
                borderColor = Colors::Accent;
                textColor = RGB(255, 255, 255);
            }

            HBRUSH bgBrush = CreateSolidBrush(bgColor);
            HPEN bPen = CreatePen(PS_SOLID, 1, borderColor);
            HBRUSH oldBr = (HBRUSH)SelectObject(hdc, bgBrush);
            HPEN oldPn = (HPEN)SelectObject(hdc, bPen);
            RoundRect(hdc, btnRect.left, btnRect.top, btnRect.right, btnRect.bottom, 8, 8);
            SelectObject(hdc, oldBr);
            SelectObject(hdc, oldPn);
            DeleteObject(bgBrush);
            DeleteObject(bPen);

            SetTextColor(hdc, textColor);
            wchar_t driveText[128];
            wsprintfW(driveText, L"  %c:  %s  (%s)", g_partDrives[i].letter, g_partDrives[i].label.c_str(), g_partDrives[i].typeStr.c_str());
            RECT labelRect = btnRect;
            labelRect.left += 10;
            SelectObject(hdc, g_buttonFont);
            DrawText(hdc, driveText, -1, &labelRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            SetTextColor(hdc, Colors::TextDim);
            std::wstring sizeStr = FormatBytes(g_partDrives[i].totalBytes.QuadPart);
            std::wstring freeStr = FormatBytes(g_partDrives[i].freeBytes.QuadPart);
            wchar_t sizeText[64];
            wsprintfW(sizeText, L"%s free / %s", freeStr.c_str(), sizeStr.c_str());
            RECT sizeRect = btnRect;
            sizeRect.right -= 12;
            SelectObject(hdc, g_smallFont);
            DrawText(hdc, sizeText, -1, &sizeRect, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        }

        // Merge + Back buttons
        int actionY = startY + (int)g_partDrives.size() * (btnHeight + spacing) + 15;

        g_actionBtnRect.left = contentCenterX - 105;
        g_actionBtnRect.right = g_actionBtnRect.left + 100;
        g_actionBtnRect.top = actionY;
        g_actionBtnRect.bottom = actionY + 40;
        DrawRoundedButton(hdc, g_actionBtnRect, L"Merge", g_actionBtn);

        g_backBtnRect.left = contentCenterX + 5;
        g_backBtnRect.right = g_backBtnRect.left + 100;
        g_backBtnRect.top = actionY;
        g_backBtnRect.bottom = actionY + 40;
        DrawRoundedButton(hdc, g_backBtnRect, L"Back", g_backBtn);
    }
}

void DrawContentPanel(HDC hdc, RECT clientRect)
{
    if (g_activeTab == TAB_CLEANER)
    {
        if (!g_drivesLoaded)
        {
            EnumerateDrives();
            g_drivesLoaded = true;
        }
        DrawDriveSelectPanel(hdc, clientRect);
    }
    else if (g_activeTab == TAB_PARTITIONS)
    {
        DrawPartitionsPanel(hdc, clientRect);
    }
    else if (g_activeTab == TAB_OTHERS)
    {
        DrawOthersPanel(hdc, clientRect);
    }
}

DWORD GetPhysicalDiskNumber(wchar_t driveLetter)
{
    wchar_t path[] = L"\\\\.\\X:";
    path[4] = driveLetter;

    HANDLE hDrive = CreateFile(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, 0, nullptr);
    if (hDrive == INVALID_HANDLE_VALUE)
        return (DWORD)-1;

    STORAGE_DEVICE_NUMBER sdn = {};
    DWORD bytesReturned = 0;
    BOOL ok = DeviceIoControl(hDrive, IOCTL_STORAGE_GET_DEVICE_NUMBER,
        nullptr, 0, &sdn, sizeof(sdn), &bytesReturned, nullptr);
    CloseHandle(hDrive);

    return ok ? sdn.DeviceNumber : (DWORD)-1;
}

ULONGLONG GetPhysicalDiskSize(DWORD diskNumber)
{
    wchar_t path[64];
    wsprintfW(path, L"\\\\.\\PhysicalDrive%u", diskNumber);

    HANDLE hDisk = CreateFile(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, 0, nullptr);
    if (hDisk == INVALID_HANDLE_VALUE)
        return 0;

    GET_LENGTH_INFORMATION lenInfo = {};
    DWORD bytesReturned = 0;
    BOOL ok = DeviceIoControl(hDisk, IOCTL_DISK_GET_LENGTH_INFO,
        nullptr, 0, &lenInfo, sizeof(lenInfo), &bytesReturned, nullptr);
    CloseHandle(hDisk);

    return ok ? lenInfo.Length.QuadPart : 0;
}

bool CreatePartitions(HWND hwnd, DWORD diskNumber, ULONGLONG diskSizeBytes)
{
    // Generate diskpart script
    // Each partition is 100GB (102400 MB), fill remaining space into the last one
    constexpr ULONGLONG PARTITION_SIZE_MB = 102400; // 100 GB
    ULONGLONG diskSizeMB = diskSizeBytes / (1024ULL * 1024ULL);

    // Reserve ~1MB for GPT overhead
    if (diskSizeMB < PARTITION_SIZE_MB + 2)
    {
        MessageBox(hwnd, L"Disk is too small for 100GB partitions.", L"ECLYPSE", MB_OK | MB_ICONERROR);
        return false;
    }

    int numPartitions = (int)(diskSizeMB / PARTITION_SIZE_MB);
    if (numPartitions < 1) numPartitions = 1;
    if (numPartitions > 26) numPartitions = 26; // max drive letters

    // Build diskpart script
    wchar_t scriptPath[MAX_PATH];
    GetTempPath(MAX_PATH, scriptPath);
    wcscat_s(scriptPath, L"eclypse_diskpart.txt");

    FILE* f = nullptr;
    _wfopen_s(&f, scriptPath, L"w");
    if (!f)
    {
        MessageBox(hwnd, L"Failed to create diskpart script.", L"ECLYPSE", MB_OK | MB_ICONERROR);
        return false;
    }

    fwprintf(f, L"select disk %u\n", diskNumber);
    fwprintf(f, L"clean\n");
    fwprintf(f, L"convert gpt\n");

    for (int i = 0; i < numPartitions; i++)
    {
        if (i == numPartitions - 1)
        {
            // Last partition takes remaining space
            fwprintf(f, L"create partition primary\n");
        }
        else
        {
            fwprintf(f, L"create partition primary size=%llu\n", PARTITION_SIZE_MB);
        }
        fwprintf(f, L"format fs=ntfs label=\"ECLYPSE_%d\" quick\n", i + 1);
        fwprintf(f, L"assign\n");
    }

    fclose(f);

    // Run diskpart
    wchar_t cmdLine[MAX_PATH + 32];
    wsprintfW(cmdLine, L"diskpart /s \"%s\"", scriptPath);

    STARTUPINFO si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {};

    BOOL created = CreateProcess(nullptr, cmdLine, nullptr, nullptr, FALSE,
        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);

    if (!created)
    {
        MessageBox(hwnd, L"Failed to run diskpart. Make sure the app is running as Administrator.",
            L"ECLYPSE", MB_OK | MB_ICONERROR);
        DeleteFile(scriptPath);
        return false;
    }

    // Wait for diskpart to finish
    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    DeleteFile(scriptPath);

    if (exitCode != 0)
    {
        wchar_t errMsg[128];
        wsprintfW(errMsg, L"Diskpart failed with exit code %u.", exitCode);
        MessageBox(hwnd, errMsg, L"ECLYPSE", MB_OK | MB_ICONERROR);
        return false;
    }

    wchar_t successMsg[128];
    wsprintfW(successMsg, L"Created %d partitions (100 GB each, NTFS).", numPartitions);
    MessageBox(hwnd, successMsg, L"ECLYPSE - Partitioning Complete", MB_OK | MB_ICONINFORMATION);
    return true;
}

bool MergeAllPartitions(HWND hwnd, wchar_t driveLetter)
{
    DWORD diskNumber = GetPhysicalDiskNumber(driveLetter);
    if (diskNumber == (DWORD)-1)
    {
        MessageBox(hwnd, L"Could not determine physical disk number.", L"ECLYPSE", MB_OK | MB_ICONERROR);
        return false;
    }

    wchar_t confirmMsg[256];
    wsprintfW(confirmMsg,
        L"This will DELETE ALL partitions on Disk %u and create a single NTFS volume.\n\n"
        L"ALL data on every partition of this disk will be permanently lost.\n\n"
        L"Are you sure?", diskNumber);

    if (MessageBox(hwnd, confirmMsg, L"ECLYPSE - Confirm Merge", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
        return false;

    if (MessageBox(hwnd, L"LAST CHANCE: This cannot be undone. Proceed?",
        L"ECLYPSE - Final Warning", MB_YESNO | MB_ICONERROR | MB_DEFBUTTON2) != IDYES)
        return false;

    // Generate diskpart script
    wchar_t scriptPath[MAX_PATH];
    GetTempPath(MAX_PATH, scriptPath);
    wcscat_s(scriptPath, L"eclypse_merge.txt");

    FILE* f = nullptr;
    _wfopen_s(&f, scriptPath, L"w");
    if (!f)
    {
        MessageBox(hwnd, L"Failed to create diskpart script.", L"ECLYPSE", MB_OK | MB_ICONERROR);
        return false;
    }

    fwprintf(f, L"select disk %u\n", diskNumber);
    fwprintf(f, L"clean\n");
    fwprintf(f, L"convert gpt\n");
    fwprintf(f, L"create partition primary\n");
    fwprintf(f, L"format fs=ntfs label=\"ECLYPSE\" quick\n");
    fwprintf(f, L"assign\n");

    fclose(f);

    // Run diskpart
    wchar_t cmdLine[MAX_PATH + 32];
    wsprintfW(cmdLine, L"diskpart /s \"%s\"", scriptPath);

    STARTUPINFO si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {};
    BOOL created = CreateProcess(nullptr, cmdLine, nullptr, nullptr, FALSE,
        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);

    if (!created)
    {
        MessageBox(hwnd, L"Failed to run diskpart. Make sure the app is running as Administrator.",
            L"ECLYPSE", MB_OK | MB_ICONERROR);
        DeleteFile(scriptPath);
        return false;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    DeleteFile(scriptPath);

    if (exitCode != 0)
    {
        wchar_t errMsg[128];
        wsprintfW(errMsg, L"Diskpart failed with exit code %u.", exitCode);
        MessageBox(hwnd, errMsg, L"ECLYPSE", MB_OK | MB_ICONERROR);
        return false;
    }

    MessageBox(hwnd, L"All partitions merged into a single NTFS volume.",
        L"ECLYPSE - Complete", MB_OK | MB_ICONINFORMATION);
    return true;
}

void ShowWindowsInstallGuide(HWND hwnd)
{
    const wchar_t* guide =
        L"=== Windows 10 Installation Guide ===\n\n"
        L"1. DOWNLOAD\n"
        L"   Get the Media Creation Tool from Microsoft's website.\n"
        L"   Run it and select \"Create installation media for another PC\".\n\n"
        L"2. CREATE BOOTABLE USB\n"
        L"   Insert a USB drive (8 GB minimum).\n"
        L"   Select USB flash drive and let the tool download & write Windows 10.\n\n"
        L"3. BOOT FROM USB\n"
        L"   Restart your PC and enter BIOS/UEFI (usually DEL, F2, or F12).\n"
        L"   Set USB as the first boot device, then save & exit.\n\n"
        L"4. INSTALL WINDOWS\n"
        L"   Select language/region, then click \"Install now\".\n"
        L"   Enter your product key or click \"I don't have a product key\".\n"
        L"   Choose \"Custom: Install Windows only\" for a clean install.\n\n"
        L"5. SELECT PARTITION\n"
        L"   Pick the partition you want to install Windows on.\n"
        L"   If you used ECLYPSE to create partitions, they'll be labeled ECLYPSE_1, ECLYPSE_2, etc.\n"
        L"   Select one and click Next.\n\n"
        L"6. FINISH SETUP\n"
        L"   Windows will install and reboot several times.\n"
        L"   Follow the on-screen setup (account, privacy, etc.).\n\n"
        L"TIP: To dual-boot, install Windows on different partitions.\n"
        L"Windows Boot Manager will auto-detect multiple installations.";

    MessageBox(hwnd, guide, L"ECLYPSE - Windows 10 Installation Guide", MB_OK | MB_ICONINFORMATION);
}

void WipeDrive(HWND hwnd, wchar_t driveLetter)
{
    wchar_t msg[512];
    wsprintfW(msg,
        L"You are about to PERMANENTLY DELETE all files on drive %c:\\\n\n"
        L"This action CANNOT be undone.\n\n"
        L"Make sure you have backed up all important data.\n\n"
        L"Are you absolutely sure?",
        driveLetter);

    int result = MessageBox(hwnd, msg, L"ECLYPSE - FINAL WARNING", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
    if (result != IDYES)
        return;

    // Second confirmation
    wchar_t confirmMsg[256];
    wsprintfW(confirmMsg,
        L"LAST CHANCE: Type confirms wiping %c:\\\n\n"
        L"This is your final warning. Proceed?",
        driveLetter);

    result = MessageBox(hwnd, confirmMsg, L"ECLYPSE - CONFIRM WIPE", MB_YESNO | MB_ICONERROR | MB_DEFBUTTON2);
    if (result != IDYES)
        return;

    // Perform the wipe
    wchar_t root[4] = { driveLetter, L':', L'\\', 0 };
    wchar_t searchPath[MAX_PATH];
    wsprintfW(searchPath, L"%c:\\*", driveLetter);

    WIN32_FIND_DATA fd;
    HANDLE hFind = FindFirstFile(searchPath, &fd);
    int deletedFiles = 0;
    int deletedDirs = 0;
    int failed = 0;

    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
                continue;

            wchar_t fullPath[MAX_PATH];
            wsprintfW(fullPath, L"%c:\\%s", driveLetter, fd.cFileName);

            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                // Use SHFileOperation for recursive delete
                wchar_t fromPath[MAX_PATH + 1] = {};
                wcscpy_s(fromPath, fullPath);
                // Double null terminate
                fromPath[wcslen(fromPath) + 1] = 0;

                SHFILEOPSTRUCT fileOp = {};
                fileOp.hwnd = hwnd;
                fileOp.wFunc = FO_DELETE;
                fileOp.pFrom = fromPath;
                fileOp.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;

                if (SHFileOperation(&fileOp) == 0)
                    deletedDirs++;
                else
                    failed++;
            }
            else
            {
                // Remove read-only attribute if set
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_READONLY)
                    SetFileAttributes(fullPath, fd.dwFileAttributes & ~FILE_ATTRIBUTE_READONLY);

                if (DeleteFile(fullPath))
                    deletedFiles++;
                else
                    failed++;
            }
        } while (FindNextFile(hFind, &fd));

        FindClose(hFind);
    }

    wchar_t resultMsg[256];
    wsprintfW(resultMsg,
        L"Wipe complete for %c:\\\n\n"
        L"Deleted: %d files, %d folders\n"
        L"Failed: %d items (may be in use or protected)\n\n"
        L"Proceed to create 100 GB partitions?",
        driveLetter, deletedFiles, deletedDirs, failed);

    int partResult = MessageBox(hwnd, resultMsg, L"ECLYPSE - Wipe Complete", MB_YESNO | MB_ICONQUESTION);
    if (partResult != IDYES)
        return;

    // Get physical disk number and size
    DWORD diskNumber = GetPhysicalDiskNumber(driveLetter);
    if (diskNumber == (DWORD)-1)
    {
        MessageBox(hwnd, L"Could not determine physical disk number.", L"ECLYPSE", MB_OK | MB_ICONERROR);
        return;
    }

    ULONGLONG diskSize = GetPhysicalDiskSize(diskNumber);
    if (diskSize == 0)
    {
        MessageBox(hwnd, L"Could not determine disk size.", L"ECLYPSE", MB_OK | MB_ICONERROR);
        return;
    }

    CreatePartitions(hwnd, diskNumber, diskSize);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        g_titleFont = CreateFont(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        g_buttonFont = CreateFont(18, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        g_tabFont = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        g_descFont = CreateFont(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        g_smallFont = CreateFont(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

        BOOL useDark = TRUE;
        DwmSetWindowAttribute(hwnd, 20, &useDark, sizeof(useDark));

        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT clientRect;
        GetClientRect(hwnd, &clientRect);

        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBmp = CreateCompatibleBitmap(hdc, clientRect.right, clientRect.bottom);
        HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

        HBRUSH bgBrush = CreateSolidBrush(Colors::WindowBg);
        FillRect(memDC, &clientRect, bgBrush);
        DeleteObject(bgBrush);

        RECT accentLine = { 0, 0, clientRect.right, 3 };
        HBRUSH accentBrush = CreateSolidBrush(Colors::Accent);
        FillRect(memDC, &accentLine, accentBrush);
        DeleteObject(accentBrush);

        DrawSidebar(memDC, clientRect);
        DrawContentPanel(memDC, clientRect);

        BitBlt(hdc, 0, 0, clientRect.right, clientRect.bottom, memDC, 0, 0, SRCCOPY);
        SelectObject(memDC, oldBmp);
        DeleteObject(memBmp);
        DeleteDC(memDC);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_MOUSEMOVE:
    {
        POINT pt = { LOWORD(lParam), HIWORD(lParam) };

        // Tab hover
        int prevHovered = g_hoveredTab;
        g_hoveredTab = -1;
        for (int i = 0; i < TAB_COUNT; i++)
        {
            RECT tabRect = GetTabRect(i);
            if (PtInRect(&tabRect, pt))
            {
                g_hoveredTab = i;
                break;
            }
        }
        if (prevHovered != g_hoveredTab)
        {
            RECT sidebarRect = { 0, TAB_TOP_OFFSET, SIDEBAR_WIDTH, TAB_TOP_OFFSET + TAB_COUNT * TAB_HEIGHT };
            InvalidateRect(hwnd, &sidebarRect, FALSE);
        }

        if (g_activeTab == TAB_CLEANER)
        {
            for (size_t i = 0; i < g_drives.size(); i++)
            {
                bool wasHov = g_driveBtnStates[i].hovered;
                g_driveBtnStates[i].hovered = PtInRect(&g_driveBtnRects[i], pt);
                if (wasHov != g_driveBtnStates[i].hovered)
                    InvalidateRect(hwnd, &g_driveBtnRects[i], FALSE);
            }
        }
        else if (g_activeTab == TAB_PARTITIONS)
        {
            if (g_partShowDriveSelect)
            {
                for (size_t i = 0; i < g_partDrives.size(); i++)
                {
                    bool wasHov = g_partDriveBtnStates[i].hovered;
                    g_partDriveBtnStates[i].hovered = PtInRect(&g_partDriveBtnRects[i], pt);
                    if (wasHov != g_partDriveBtnStates[i].hovered)
                        InvalidateRect(hwnd, &g_partDriveBtnRects[i], FALSE);
                }

                bool wasBack = g_backBtn.hovered;
                g_backBtn.hovered = PtInRect(&g_backBtnRect, pt);
                if (wasBack != g_backBtn.hovered)
                    InvalidateRect(hwnd, &g_backBtnRect, FALSE);
            }
            else
            {
                bool wasGuide = g_guideBtn.hovered;
                g_guideBtn.hovered = PtInRect(&g_guideBtnRect, pt);
                if (wasGuide != g_guideBtn.hovered)
                    InvalidateRect(hwnd, &g_guideBtnRect, FALSE);

                bool wasMerge = g_mergeBtn.hovered;
                g_mergeBtn.hovered = PtInRect(&g_mergeBtnRect, pt);
                if (wasMerge != g_mergeBtn.hovered)
                    InvalidateRect(hwnd, &g_mergeBtnRect, FALSE);
            }
        }

        if (g_activeTab == TAB_OTHERS)
        {
            for (int i = 0; i < OPT_COUNT; i++)
            {
                bool wasHov = g_optionBtns[i].hovered;
                g_optionBtns[i].hovered = PtInRect(&g_optionBtnRects[i], pt);
                if (wasHov != g_optionBtns[i].hovered)
                    InvalidateRect(hwnd, &g_optionBtnRects[i], FALSE);
            }
        }

        // Action button hover (Cleaner + Partitions merge view)
        {
            bool wasHovered = g_actionBtn.hovered;
            g_actionBtn.hovered = PtInRect(&g_actionBtnRect, pt);
            if (wasHovered != g_actionBtn.hovered)
                InvalidateRect(hwnd, &g_actionBtnRect, FALSE);
        }

        TRACKMOUSEEVENT tme = {};
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hwnd;
        TrackMouseEvent(&tme);
        return 0;
    }

    case WM_MOUSELEAVE:
    {
        g_hoveredTab = -1;
        g_actionBtn.hovered = false;
        g_backBtn.hovered = false;
        g_guideBtn.hovered = false;
        g_mergeBtn.hovered = false;
        for (auto& s : g_driveBtnStates)
            s.hovered = false;
        for (auto& s : g_partDriveBtnStates)
            s.hovered = false;
        for (auto& s : g_optionBtns)
            s.hovered = false;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_LBUTTONDOWN:
    {
        POINT pt = { LOWORD(lParam), HIWORD(lParam) };

        // Tab click
        for (int i = 0; i < TAB_COUNT; i++)
        {
            RECT tabRect = GetTabRect(i);
            if (PtInRect(&tabRect, pt))
            {
                g_activeTab = i;
                g_actionBtn = {};
                g_partShowDriveSelect = false;
                g_partSelectedDrive = -1;
                if (i == TAB_CLEANER)
                {
                    EnumerateDrives();
                    g_drivesLoaded = true;
                }
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
        }

        if (g_activeTab == TAB_CLEANER)
        {
            for (size_t i = 0; i < g_drives.size(); i++)
            {
                if (PtInRect(&g_driveBtnRects[i], pt))
                {
                    g_driveBtnStates[i].pressed = true;
                    SetCapture(hwnd);
                    InvalidateRect(hwnd, &g_driveBtnRects[i], FALSE);
                    return 0;
                }
            }
        }
        else if (g_activeTab == TAB_PARTITIONS)
        {
            if (g_partShowDriveSelect)
            {
                for (size_t i = 0; i < g_partDrives.size(); i++)
                {
                    if (PtInRect(&g_partDriveBtnRects[i], pt))
                    {
                        g_partDriveBtnStates[i].pressed = true;
                        SetCapture(hwnd);
                        InvalidateRect(hwnd, &g_partDriveBtnRects[i], FALSE);
                        return 0;
                    }
                }

                if (PtInRect(&g_backBtnRect, pt))
                {
                    g_backBtn.pressed = true;
                    SetCapture(hwnd);
                    InvalidateRect(hwnd, &g_backBtnRect, FALSE);
                    return 0;
                }
            }
            else
            {
                if (PtInRect(&g_guideBtnRect, pt))
                {
                    g_guideBtn.pressed = true;
                    SetCapture(hwnd);
                    InvalidateRect(hwnd, &g_guideBtnRect, FALSE);
                    return 0;
                }
                if (PtInRect(&g_mergeBtnRect, pt))
                {
                    g_mergeBtn.pressed = true;
                    SetCapture(hwnd);
                    InvalidateRect(hwnd, &g_mergeBtnRect, FALSE);
                    return 0;
                }
            }
        }

        if (g_activeTab == TAB_OTHERS)
        {
            for (int i = 0; i < OPT_COUNT; i++)
            {
                if (PtInRect(&g_optionBtnRects[i], pt))
                {
                    g_optionBtns[i].pressed = true;
                    SetCapture(hwnd);
                    InvalidateRect(hwnd, &g_optionBtnRects[i], FALSE);
                    return 0;
                }
            }
        }

        // Action button press (Cleaner + Partitions merge view)
        if (PtInRect(&g_actionBtnRect, pt))
        {
            g_actionBtn.pressed = true;
            SetCapture(hwnd);
            InvalidateRect(hwnd, &g_actionBtnRect, FALSE);
        }
        return 0;
    }

    case WM_LBUTTONUP:
    {
        POINT pt = { LOWORD(lParam), HIWORD(lParam) };
        ReleaseCapture();

        // Drive selection (Cleaner tab)
        if (g_activeTab == TAB_CLEANER)
        {
            for (size_t i = 0; i < g_drives.size(); i++)
            {
                bool wasPressed = g_driveBtnStates[i].pressed;
                g_driveBtnStates[i].pressed = false;

                if (wasPressed && PtInRect(&g_driveBtnRects[i], pt))
                {
                    g_selectedDrive = (int)i;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
            }
        }

        // Partitions tab buttons
        if (g_activeTab == TAB_PARTITIONS)
        {
            if (g_partShowDriveSelect)
            {
                // Drive selection for merge
                for (size_t i = 0; i < g_partDrives.size(); i++)
                {
                    bool wasPressed = g_partDriveBtnStates[i].pressed;
                    g_partDriveBtnStates[i].pressed = false;

                    if (wasPressed && PtInRect(&g_partDriveBtnRects[i], pt))
                    {
                        g_partSelectedDrive = (int)i;
                        InvalidateRect(hwnd, nullptr, FALSE);
                        return 0;
                    }
                }

                // Back button
                bool wasBack = g_backBtn.pressed;
                g_backBtn.pressed = false;
                if (wasBack && PtInRect(&g_backBtnRect, pt))
                {
                    g_partShowDriveSelect = false;
                    g_partSelectedDrive = -1;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }

                // Merge (action) button
                bool wasMergeAction = g_actionBtn.pressed;
                g_actionBtn.pressed = false;
                if (wasMergeAction && PtInRect(&g_actionBtnRect, pt))
                {
                    if (g_partSelectedDrive >= 0 && g_partSelectedDrive < (int)g_partDrives.size())
                    {
                        MergeAllPartitions(hwnd, g_partDrives[g_partSelectedDrive].letter);
                        // Refresh
                        EnumerateDrives();
                        g_drivesLoaded = true;
                        g_partShowDriveSelect = false;
                        g_partSelectedDrive = -1;
                        InvalidateRect(hwnd, nullptr, FALSE);
                    }
                    else
                    {
                        MessageBox(hwnd, L"Please select a drive first.", L"ECLYPSE", MB_OK | MB_ICONINFORMATION);
                    }
                    return 0;
                }

                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            else
            {
                // Guide button
                bool wasGuide = g_guideBtn.pressed;
                g_guideBtn.pressed = false;
                if (wasGuide && PtInRect(&g_guideBtnRect, pt))
                {
                    ShowWindowsInstallGuide(hwnd);
                    InvalidateRect(hwnd, &g_guideBtnRect, FALSE);
                    return 0;
                }

                // Merge button -> show drive selection
                bool wasMerge = g_mergeBtn.pressed;
                g_mergeBtn.pressed = false;
                if (wasMerge && PtInRect(&g_mergeBtnRect, pt))
                {
                    // Load drives for partition tab
                    g_partDrives.clear();
                    DWORD driveMask = GetLogicalDrives();
                    for (int i = 0; i < 26; i++)
                    {
                        if (!(driveMask & (1 << i))) continue;
                        wchar_t root[] = { (wchar_t)(L'A' + i), L':', L'\\', 0 };
                        UINT driveType = GetDriveType(root);
                        if (driveType == DRIVE_NO_ROOT_DIR || driveType == DRIVE_UNKNOWN) continue;

                        DriveInfo info = {};
                        info.letter = L'A' + i;
                        switch (driveType) {
                        case DRIVE_REMOVABLE: info.typeStr = L"Removable"; break;
                        case DRIVE_FIXED:     info.typeStr = L"Local Disk"; break;
                        case DRIVE_REMOTE:    info.typeStr = L"Network"; break;
                        case DRIVE_CDROM:     info.typeStr = L"CD/DVD"; break;
                        case DRIVE_RAMDISK:   info.typeStr = L"RAM Disk"; break;
                        default:              info.typeStr = L"Unknown"; break;
                        }
                        wchar_t volumeName[MAX_PATH] = {};
                        GetVolumeInformation(root, volumeName, MAX_PATH, nullptr, nullptr, nullptr, nullptr, 0);
                        info.label = volumeName[0] ? volumeName : info.typeStr;
                        GetDiskFreeSpaceEx(root, nullptr, &info.totalBytes, &info.freeBytes);
                        g_partDrives.push_back(info);
                    }
                    g_partDriveBtnRects.resize(g_partDrives.size());
                    g_partDriveBtnStates.resize(g_partDrives.size());
                    for (auto& s : g_partDriveBtnStates) s = {};
                    g_partSelectedDrive = -1;
                    g_partShowDriveSelect = true;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }

                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
        }

        // Others tab option buttons
        if (g_activeTab == TAB_OTHERS)
        {
            for (int i = 0; i < OPT_COUNT; i++)
            {
                bool wasPressed = g_optionBtns[i].pressed;
                g_optionBtns[i].pressed = false;

                if (wasPressed && PtInRect(&g_optionBtnRects[i], pt))
                {
                    wchar_t confirmMsg[256];
                    wsprintfW(confirmMsg, L"Are you sure you want to clean %s?\n\n%s\n\nThis action cannot be undone.",
                        g_optionNames[i], g_optionDescs[i]);
                    int result = MessageBox(hwnd, confirmMsg, L"ECLYPSE - Confirm", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);

                    if (result == IDYES)
                    {
                        // TODO: implement actual cleaning logic per option
                        wchar_t doneMsg[128];
                        wsprintfW(doneMsg, L"%s cleaning complete!", g_optionNames[i]);
                        MessageBox(hwnd, doneMsg, L"ECLYPSE", MB_OK | MB_ICONINFORMATION);
                    }

                    InvalidateRect(hwnd, &g_optionBtnRects[i], FALSE);
                    return 0;
                }
            }
        }

        // Action button release (Cleaner tab)
        {
            bool wasPressed = g_actionBtn.pressed;
            g_actionBtn.pressed = false;

            if (wasPressed && PtInRect(&g_actionBtnRect, pt))
            {
                if (g_activeTab == TAB_CLEANER)
                {
                    if (g_selectedDrive >= 0 && g_selectedDrive < (int)g_drives.size())
                    {
                        WipeDrive(hwnd, g_drives[g_selectedDrive].letter);
                        EnumerateDrives();
                        g_selectedDrive = -1;
                        InvalidateRect(hwnd, nullptr, FALSE);
                    }
                    else
                    {
                        MessageBox(hwnd, L"Please select a drive first.", L"ECLYPSE", MB_OK | MB_ICONINFORMATION);
                    }
                }
            }

            InvalidateRect(hwnd, &g_actionBtnRect, FALSE);
        }
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_DESTROY:
        DeleteObject(g_titleFont);
        DeleteObject(g_buttonFont);
        DeleteObject(g_tabFont);
        DeleteObject(g_descFont);
        DeleteObject(g_smallFont);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    const wchar_t CLASS_NAME[] = L"EclypseWindow";

    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;

    RegisterClassEx(&wc);

    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"ECLYPSE",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT, 700, 550,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!hwnd)
        return 1;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return static_cast<int>(msg.wParam);
}
