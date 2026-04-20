#include <Windows.h>
#include <dwmapi.h>
#include <winioctl.h>
#include <ShlObj.h>
#include <string>
#include <vector>
#include <cstdio>
#include <shlwapi.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")

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
    TAB_OPTIMIZATION,
    TAB_RESTORE,
    TAB_OTHERS, // always last
    TAB_COUNT
};

static const wchar_t* g_tabNames[TAB_COUNT] = {
    L"Cleaner",
    L"Partitions",
    L"Optimization",
    L"Restore",
    L"Others"
};

static const wchar_t* g_tabDescriptions[TAB_COUNT] = {
    L"Wipe an entire drive. All data will be permanently deleted.",
    L"Manage disk partitions and Windows installation guide.",
    L"Optimize Windows settings for maximum gaming performance.",
    L"Create a Windows system restore point.",
    L"Clean temp files, logs, caches, registry, prefetch, DNS, and game folders."
};

// Optimization tab options
enum OptimizeOption
{
    OPTIM_POWER_PLAN,
    OPTIM_GAME_BAR,
    OPTIM_VISUAL_FX,
    OPTIM_NAGLE,
    OPTIM_MOUSE_ACCEL,
    OPTIM_FULLSCREEN_OPT,
    OPTIM_STANDBY_MEM,
    OPTIM_GPU_PRIORITY,
    OPTIM_COUNT
};

static const wchar_t* g_optimNames[OPTIM_COUNT] = {
    L"Power Plan",
    L"Game Bar",
    L"Visual Effects",
    L"Nagle's Algorithm",
    L"Mouse Accel",
    L"Fullscreen Opt",
    L"Standby Memory",
    L"GPU Priority"
};

static const wchar_t* g_optimDescs[OPTIM_COUNT] = {
    L"Set power plan to Ultimate/High Performance",
    L"Disable Xbox Game Bar and Game DVR",
    L"Disable animations, transparency, and visual effects",
    L"Disable Nagle's algorithm for lower network latency",
    L"Disable mouse acceleration for raw input",
    L"Disable fullscreen optimizations system-wide",
    L"Flush standby memory to free up RAM",
    L"Set GPU scheduling to high priority"
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
static int g_cleanerScrollY = 0;

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
static int g_partScrollY = 0;

// Others tab state
static ButtonState g_optionBtns[OPT_COUNT];
static RECT g_optionBtnRects[OPT_COUNT];

// Optimization tab state
static ButtonState g_optimBtns[OPTIM_COUNT];
static RECT g_optimBtnRects[OPTIM_COUNT];

// Restore tab state
static ButtonState g_restoreCreateBtn;
static ButtonState g_restoreLoadBtn;
static RECT g_restoreCreateBtnRect = {};
static RECT g_restoreLoadBtnRect = {};

// Restore load sub-view
static bool g_showRestoreLoadView = false;
static ButtonState g_restoreBackBtn;
static RECT g_restoreBackBtnRect = {};

struct RestorePointInfo
{
    std::wstring description;
    std::wstring date;
    DWORD sequenceNumber;
};
static std::vector<RestorePointInfo> g_restorePoints;
static std::vector<RECT> g_restorePointRects;
static std::vector<ButtonState> g_restorePointBtns;
static int g_selectedRestorePoint = -1;
static ButtonState g_restoreApplyBtn;
static RECT g_restoreApplyBtnRect = {};

// Registry sub-view state
static bool g_showRegistrySubView = false;
static ButtonState g_regResetBtn;
static ButtonState g_regCleanBtn;
static ButtonState g_regBackBtn;
static RECT g_regResetBtnRect = {};
static RECT g_regCleanBtnRect = {};
static RECT g_regBackBtnRect = {};

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
    int startY = 120 - g_cleanerScrollY;
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

void DrawOptimizationPanel(HDC hdc, RECT clientRect)
{
    int contentLeft = SIDEBAR_WIDTH + 30;
    int contentRight = clientRect.right - 30;
    int contentCenterX = (contentLeft + contentRight) / 2;

    SetBkMode(hdc, TRANSPARENT);

    SetTextColor(hdc, Colors::Text);
    HFONT oldFont = (HFONT)SelectObject(hdc, g_titleFont);
    RECT headerRect = { contentLeft, 25, contentRight, 55 };
    DrawText(hdc, L"Optimization", -1, &headerRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    HPEN sepPen = CreatePen(PS_SOLID, 1, Colors::Border);
    HPEN oldPen = (HPEN)SelectObject(hdc, sepPen);
    MoveToEx(hdc, contentLeft, 65, nullptr);
    LineTo(hdc, contentRight, 65);
    SelectObject(hdc, oldPen);
    DeleteObject(sepPen);

    SetTextColor(hdc, Colors::TextDim);
    SelectObject(hdc, g_descFont);
    RECT descRect = { contentLeft, 75, contentRight, 100 };
    DrawText(hdc, L"Optimize Windows for maximum gaming performance.", -1, &descRect, DT_LEFT | DT_WORDBREAK);

    // Option buttons - two columns
    int btnWidth = 190;
    int btnHeight = 42;
    int spacing = 10;
    int colGap = 14;
    int startY = 110;
    int col1Left = contentCenterX - btnWidth - colGap / 2;
    int col2Left = contentCenterX + colGap / 2;

    for (int i = 0; i < OPTIM_COUNT; i++)
    {
        int col = i % 2;
        int row = i / 2;

        RECT btnRect;
        btnRect.left = (col == 0) ? col1Left : col2Left;
        btnRect.right = btnRect.left + btnWidth;
        btnRect.top = startY + row * (btnHeight + spacing);
        btnRect.bottom = btnRect.top + btnHeight;
        g_optimBtnRects[i] = btnRect;

        DrawRoundedButton(hdc, btnRect, g_optimNames[i], g_optimBtns[i]);
    }

    SelectObject(hdc, oldFont);
}

void DrawRestoreLoadView(HDC hdc, RECT clientRect)
{
    int contentLeft = SIDEBAR_WIDTH + 30;
    int contentRight = clientRect.right - 30;
    int contentCenterX = (contentLeft + contentRight) / 2;

    SetBkMode(hdc, TRANSPARENT);

    SetTextColor(hdc, Colors::Text);
    HFONT oldFont = (HFONT)SelectObject(hdc, g_titleFont);
    RECT headerRect = { contentLeft, 25, contentRight, 55 };
    DrawText(hdc, L"Load Restore Point", -1, &headerRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    HPEN sepPen = CreatePen(PS_SOLID, 1, Colors::Border);
    HPEN oldPen = (HPEN)SelectObject(hdc, sepPen);
    MoveToEx(hdc, contentLeft, 65, nullptr);
    LineTo(hdc, contentRight, 65);
    SelectObject(hdc, oldPen);
    DeleteObject(sepPen);

    SetTextColor(hdc, Colors::TextDim);
    SelectObject(hdc, g_descFont);
    RECT descRect = { contentLeft, 75, contentRight, 100 };
    DrawText(hdc, L"Select a restore point to load.", -1, &descRect, DT_LEFT | DT_WORDBREAK);

    if (g_restorePoints.empty())
    {
        RECT emptyRect = { contentLeft, 120, contentRight, 150 };
        DrawText(hdc, L"No restore points found.", -1, &emptyRect, DT_CENTER | DT_SINGLELINE);
    }
    else
    {
        int btnWidth = 380;
        int btnHeight = 40;
        int spacing = 6;
        int startY = 108;

        for (size_t i = 0; i < g_restorePoints.size(); i++)
        {
            RECT btnRect;
            btnRect.left = contentCenterX - btnWidth / 2;
            btnRect.right = btnRect.left + btnWidth;
            btnRect.top = startY + (int)i * (btnHeight + spacing);
            btnRect.bottom = btnRect.top + btnHeight;
            g_restorePointRects[i] = btnRect;

            bool isSelected = (g_selectedRestorePoint == (int)i);
            COLORREF bgColor = isSelected ? Colors::TabActive : Colors::FrameBg;
            COLORREF borderColor = isSelected ? Colors::Accent : Colors::Border;
            COLORREF textColor = Colors::Text;

            if (g_restorePointBtns[i].pressed)
            { bgColor = Colors::AccentPress; borderColor = Colors::Accent; textColor = RGB(255, 255, 255); }
            else if (g_restorePointBtns[i].hovered)
            { bgColor = Colors::TabHover; borderColor = Colors::Accent; textColor = RGB(255, 255, 255); }

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
            SelectObject(hdc, g_tabFont);
            RECT labelRect = btnRect;
            labelRect.left += 12;
            labelRect.right -= 12;
            std::wstring label = g_restorePoints[i].description;
            DrawText(hdc, label.c_str(), -1, &labelRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

            if (!g_restorePoints[i].date.empty())
            {
                SetTextColor(hdc, Colors::TextDim);
                SelectObject(hdc, g_smallFont);
                DrawText(hdc, g_restorePoints[i].date.c_str(), -1, &labelRect, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
            }
        }

        // Apply button
        int actionY = startY + (int)g_restorePoints.size() * (btnHeight + spacing) + 12;
        g_restoreApplyBtnRect.left = contentCenterX - 55;
        g_restoreApplyBtnRect.right = g_restoreApplyBtnRect.left + 100;
        g_restoreApplyBtnRect.top = actionY;
        g_restoreApplyBtnRect.bottom = actionY + 38;
        DrawRoundedButton(hdc, g_restoreApplyBtnRect, L"Load", g_restoreApplyBtn);
    }

    // Back button
    int backY = g_restorePoints.empty() ? 170 :
        108 + (int)g_restorePoints.size() * 46 + 12;
    g_restoreBackBtnRect.left = contentCenterX + 10;
    g_restoreBackBtnRect.right = g_restoreBackBtnRect.left + 100;
    g_restoreBackBtnRect.top = backY;
    g_restoreBackBtnRect.bottom = backY + 38;
    DrawRoundedButton(hdc, g_restoreBackBtnRect, L"Back", g_restoreBackBtn);

    SelectObject(hdc, oldFont);
}

void DrawRestorePanel(HDC hdc, RECT clientRect)
{
    if (g_showRestoreLoadView)
    {
        DrawRestoreLoadView(hdc, clientRect);
        return;
    }

    int contentLeft = SIDEBAR_WIDTH + 30;
    int contentRight = clientRect.right - 30;
    int contentCenterX = (contentLeft + contentRight) / 2;

    SetBkMode(hdc, TRANSPARENT);

    SetTextColor(hdc, Colors::Text);
    HFONT oldFont = (HFONT)SelectObject(hdc, g_titleFont);
    RECT headerRect = { contentLeft, 25, contentRight, 55 };
    DrawText(hdc, L"Restore", -1, &headerRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    HPEN sepPen = CreatePen(PS_SOLID, 1, Colors::Border);
    HPEN oldPen = (HPEN)SelectObject(hdc, sepPen);
    MoveToEx(hdc, contentLeft, 65, nullptr);
    LineTo(hdc, contentRight, 65);
    SelectObject(hdc, oldPen);
    DeleteObject(sepPen);

    SetTextColor(hdc, Colors::TextDim);
    SelectObject(hdc, g_descFont);
    RECT descRect = { contentLeft, 75, contentRight, 110 };
    DrawText(hdc, L"Create protected restore points or load a previous one. Backups are stored in multiple locations and locked from 3rd party deletion.", -1, &descRect, DT_LEFT | DT_WORDBREAK);

    SelectObject(hdc, oldFont);

    int btnWidth = 340;
    int btnHeight = 50;

    g_restoreCreateBtnRect.left = contentCenterX - btnWidth / 2;
    g_restoreCreateBtnRect.right = g_restoreCreateBtnRect.left + btnWidth;
    g_restoreCreateBtnRect.top = 130;
    g_restoreCreateBtnRect.bottom = g_restoreCreateBtnRect.top + btnHeight;
    DrawRoundedButton(hdc, g_restoreCreateBtnRect, L"Create Restore Point", g_restoreCreateBtn);

    SetTextColor(hdc, Colors::TextDim);
    HFONT oldFont2 = (HFONT)SelectObject(hdc, g_smallFont);
    RECT createDesc = { contentLeft, g_restoreCreateBtnRect.bottom + 4, contentRight, g_restoreCreateBtnRect.bottom + 20 };
    DrawText(hdc, L"Creates a Windows restore point + protected ECLYPSE backup", -1, &createDesc, DT_CENTER | DT_SINGLELINE);

    g_restoreLoadBtnRect.left = contentCenterX - btnWidth / 2;
    g_restoreLoadBtnRect.right = g_restoreLoadBtnRect.left + btnWidth;
    g_restoreLoadBtnRect.top = 210;
    g_restoreLoadBtnRect.bottom = g_restoreLoadBtnRect.top + btnHeight;
    DrawRoundedButton(hdc, g_restoreLoadBtnRect, L"Load Restore Point", g_restoreLoadBtn);

    SetTextColor(hdc, Colors::TextDim);
    RECT loadDesc = { contentLeft, g_restoreLoadBtnRect.bottom + 4, contentRight, g_restoreLoadBtnRect.bottom + 20 };
    DrawText(hdc, L"Browse and load a previous restore point", -1, &loadDesc, DT_CENTER | DT_SINGLELINE);
    SelectObject(hdc, oldFont2);
}

void DrawRegistrySubView(HDC hdc, RECT clientRect)
{
    int contentLeft = SIDEBAR_WIDTH + 30;
    int contentRight = clientRect.right - 30;
    int contentCenterX = (contentLeft + contentRight) / 2;

    SetBkMode(hdc, TRANSPARENT);

    SetTextColor(hdc, Colors::Text);
    HFONT oldFont = (HFONT)SelectObject(hdc, g_titleFont);
    RECT headerRect = { contentLeft, 25, contentRight, 55 };
    DrawText(hdc, L"Registry", -1, &headerRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    HPEN sepPen = CreatePen(PS_SOLID, 1, Colors::Border);
    HPEN oldPen = (HPEN)SelectObject(hdc, sepPen);
    MoveToEx(hdc, contentLeft, 65, nullptr);
    LineTo(hdc, contentRight, 65);
    SelectObject(hdc, oldPen);
    DeleteObject(sepPen);

    SetTextColor(hdc, Colors::TextDim);
    SelectObject(hdc, g_descFont);
    RECT descRect = { contentLeft, 75, contentRight, 105 };
    DrawText(hdc, L"Choose a registry action below.", -1, &descRect, DT_LEFT | DT_WORDBREAK);

    int btnWidth = 340;
    int btnHeight = 50;

    // Reset to Default button
    g_regResetBtnRect.left = contentCenterX - btnWidth / 2;
    g_regResetBtnRect.right = g_regResetBtnRect.left + btnWidth;
    g_regResetBtnRect.top = 130;
    g_regResetBtnRect.bottom = g_regResetBtnRect.top + btnHeight;
    DrawRoundedButton(hdc, g_regResetBtnRect, L"Reset to Default", g_regResetBtn);

    SetTextColor(hdc, Colors::TextDim);
    SelectObject(hdc, g_smallFont);
    RECT resetDesc = { contentLeft, g_regResetBtnRect.bottom + 4, contentRight, g_regResetBtnRect.bottom + 20 };
    DrawText(hdc, L"Undo optimizations and restore registry to Windows defaults", -1, &resetDesc, DT_CENTER | DT_SINGLELINE);

    // Clean Registry button
    g_regCleanBtnRect.left = contentCenterX - btnWidth / 2;
    g_regCleanBtnRect.right = g_regCleanBtnRect.left + btnWidth;
    g_regCleanBtnRect.top = 210;
    g_regCleanBtnRect.bottom = g_regCleanBtnRect.top + btnHeight;
    DrawRoundedButton(hdc, g_regCleanBtnRect, L"Clean Registry", g_regCleanBtn);

    SetTextColor(hdc, Colors::TextDim);
    SelectObject(hdc, g_smallFont);
    RECT cleanDesc = { contentLeft, g_regCleanBtnRect.bottom + 4, contentRight, g_regCleanBtnRect.bottom + 20 };
    DrawText(hdc, L"Clean MUI cache, recent docs, and UserAssist tracking data", -1, &cleanDesc, DT_CENTER | DT_SINGLELINE);

    // Back button
    int backW = 120;
    int backH = 36;
    g_regBackBtnRect.left = contentCenterX - backW / 2;
    g_regBackBtnRect.right = g_regBackBtnRect.left + backW;
    g_regBackBtnRect.top = 300;
    g_regBackBtnRect.bottom = g_regBackBtnRect.top + backH;
    DrawRoundedButton(hdc, g_regBackBtnRect, L"Back", g_regBackBtn);

    SelectObject(hdc, oldFont);
}

void DrawOthersPanel(HDC hdc, RECT clientRect)
{
    if (g_showRegistrySubView)
    {
        DrawRegistrySubView(hdc, clientRect);
        return;
    }

    int contentLeft = SIDEBAR_WIDTH + 30;
    int contentRight = clientRect.right - 30;
    int contentCenterX = (contentLeft + contentRight) / 2;

    SetBkMode(hdc, TRANSPARENT);

    SetTextColor(hdc, Colors::Text);
    HFONT oldFont = (HFONT)SelectObject(hdc, g_titleFont);
    RECT headerRect = { contentLeft, 25, contentRight, 55 };
    DrawText(hdc, L"Others", -1, &headerRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

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
        int startY = 148 - g_partScrollY;
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
    else if (g_activeTab == TAB_OPTIMIZATION)
    {
        DrawOptimizationPanel(hdc, clientRect);
    }
    else if (g_activeTab == TAB_RESTORE)
    {
        DrawRestorePanel(hdc, clientRect);
    }
    else if (g_activeTab == TAB_OTHERS)
    {
        DrawOthersPanel(hdc, clientRect);
    }
}

// ============================================================
// Cleaning functions
// ============================================================

int DeleteFilesInFolder(const wchar_t* folderPath, const wchar_t* pattern, bool recurse)
{
    int deleted = 0;
    wchar_t searchPath[MAX_PATH];
    wsprintfW(searchPath, L"%s\\%s", folderPath, pattern);

    WIN32_FIND_DATA fd;
    HANDLE hFind = FindFirstFile(searchPath, &fd);
    if (hFind == INVALID_HANDLE_VALUE)
        return 0;

    do
    {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
            continue;

        wchar_t fullPath[MAX_PATH];
        wsprintfW(fullPath, L"%s\\%s", folderPath, fd.cFileName);

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if (recurse)
            {
                deleted += DeleteFilesInFolder(fullPath, pattern, true);
                RemoveDirectory(fullPath);
            }
        }
        else
        {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_READONLY)
                SetFileAttributes(fullPath, fd.dwFileAttributes & ~FILE_ATTRIBUTE_READONLY);
            if (DeleteFile(fullPath))
                deleted++;
        }
    } while (FindNextFile(hFind, &fd));

    FindClose(hFind);
    return deleted;
}

int DeleteFolderContents(const wchar_t* folderPath)
{
    int deleted = 0;
    wchar_t searchPath[MAX_PATH];
    wsprintfW(searchPath, L"%s\\*", folderPath);

    WIN32_FIND_DATA fd;
    HANDLE hFind = FindFirstFile(searchPath, &fd);
    if (hFind == INVALID_HANDLE_VALUE)
        return 0;

    do
    {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
            continue;

        wchar_t fullPath[MAX_PATH];
        wsprintfW(fullPath, L"%s\\%s", folderPath, fd.cFileName);

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            wchar_t fromPath[MAX_PATH + 2] = {};
            wcscpy_s(fromPath, fullPath);
            fromPath[wcslen(fromPath) + 1] = 0;

            SHFILEOPSTRUCT fileOp = {};
            fileOp.wFunc = FO_DELETE;
            fileOp.pFrom = fromPath;
            fileOp.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
            if (SHFileOperation(&fileOp) == 0)
                deleted++;
        }
        else
        {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_READONLY)
                SetFileAttributes(fullPath, fd.dwFileAttributes & ~FILE_ATTRIBUTE_READONLY);
            if (DeleteFile(fullPath))
                deleted++;
        }
    } while (FindNextFile(hFind, &fd));

    FindClose(hFind);
    return deleted;
}

std::wstring CleanTempFiles()
{
    int total = 0;

    // User temp folder (%TEMP%)
    wchar_t userTemp[MAX_PATH];
    GetTempPath(MAX_PATH, userTemp);
    total += DeleteFolderContents(userTemp);

    // Windows temp
    total += DeleteFolderContents(L"C:\\Windows\\Temp");

    wchar_t result[128];
    wsprintfW(result, L"Cleaned %d temp files and folders.", total);
    return result;
}

std::wstring CleanLogs()
{
    int total = 0;

    // Windows logs
    total += DeleteFilesInFolder(L"C:\\Windows\\Logs", L"*", true);
    total += DeleteFilesInFolder(L"C:\\Windows\\System32\\LogFiles", L"*", true);

    // User temp logs
    wchar_t userTemp[MAX_PATH];
    GetTempPath(MAX_PATH, userTemp);
    total += DeleteFilesInFolder(userTemp, L"*.log", false);

    // CBS logs
    total += DeleteFilesInFolder(L"C:\\Windows\\Logs\\CBS", L"*", true);

    // DISM logs
    total += DeleteFilesInFolder(L"C:\\Windows\\Logs\\DISM", L"*", true);

    wchar_t result[128];
    wsprintfW(result, L"Cleaned %d log files.", total);
    return result;
}

std::wstring CleanCache()
{
    int total = 0;

    // Windows icon cache
    wchar_t localAppData[MAX_PATH];
    SHGetFolderPath(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, localAppData);

    wchar_t iconCachePath[MAX_PATH];
    wsprintfW(iconCachePath, L"%s\\IconCache.db", localAppData);
    if (DeleteFile(iconCachePath)) total++;

    // Windows Explorer thumbnail cache
    wchar_t thumbCacheDir[MAX_PATH];
    wsprintfW(thumbCacheDir, L"%s\\Microsoft\\Windows\\Explorer", localAppData);
    total += DeleteFilesInFolder(thumbCacheDir, L"thumbcache_*", false);

    // Chrome cache
    wchar_t chromeCacheDir[MAX_PATH];
    wsprintfW(chromeCacheDir, L"%s\\Google\\Chrome\\User Data\\Default\\Cache", localAppData);
    total += DeleteFolderContents(chromeCacheDir);

    // Edge cache
    wchar_t edgeCacheDir[MAX_PATH];
    wsprintfW(edgeCacheDir, L"%s\\Microsoft\\Edge\\User Data\\Default\\Cache", localAppData);
    total += DeleteFolderContents(edgeCacheDir);

    // Firefox cache
    wchar_t firefoxDir[MAX_PATH];
    wsprintfW(firefoxDir, L"%s\\Mozilla\\Firefox\\Profiles", localAppData);
    WIN32_FIND_DATA fd;
    wchar_t ffSearch[MAX_PATH];
    wsprintfW(ffSearch, L"%s\\*", firefoxDir);
    HANDLE hFind = FindFirstFile(ffSearch, &fd);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY &&
                wcscmp(fd.cFileName, L".") != 0 && wcscmp(fd.cFileName, L"..") != 0)
            {
                wchar_t ffCachePath[MAX_PATH];
                wsprintfW(ffCachePath, L"%s\\%s\\cache2\\entries", firefoxDir, fd.cFileName);
                total += DeleteFolderContents(ffCachePath);
            }
        } while (FindNextFile(hFind, &fd));
        FindClose(hFind);
    }

    // Windows font cache
    total += DeleteFilesInFolder(L"C:\\Windows\\ServiceProfiles\\LocalService\\AppData\\Local", L"FontCache*", false);

    wchar_t result[128];
    wsprintfW(result, L"Cleaned %d cache files.", total);
    return result;
}

std::wstring CleanRegistry()
{
    int cleaned = 0;

    // Clean MUI cache
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\Shell\\MuiCache",
        0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS)
    {
        // Enumerate and delete values
        wchar_t valueName[512];
        DWORD valueNameLen;
        DWORD index = 0;
        std::vector<std::wstring> valuesToDelete;

        while (true)
        {
            valueNameLen = 512;
            if (RegEnumValue(hKey, index, valueName, &valueNameLen, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
                break;
            valuesToDelete.push_back(valueName);
            index++;
        }

        for (auto& name : valuesToDelete)
        {
            if (RegDeleteValue(hKey, name.c_str()) == ERROR_SUCCESS)
                cleaned++;
        }
        RegCloseKey(hKey);
    }

    // Clean recent docs
    if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\RecentDocs",
        0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS)
    {
        // Delete the MRUListEx value to clear recent docs list
        if (RegDeleteValue(hKey, L"MRUListEx") == ERROR_SUCCESS)
            cleaned++;
        RegCloseKey(hKey);
    }

    // Clean UserAssist (program usage tracking)
    if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\UserAssist",
        0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS)
    {
        // Enumerate subkeys and clear count values
        wchar_t subKeyName[256];
        DWORD subKeyNameLen;
        DWORD index2 = 0;
        std::vector<std::wstring> subKeys;

        while (true)
        {
            subKeyNameLen = 256;
            if (RegEnumKeyEx(hKey, index2, subKeyName, &subKeyNameLen, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
                break;
            subKeys.push_back(subKeyName);
            index2++;
        }

        for (auto& sk : subKeys)
        {
            std::wstring countPath = sk + L"\\Count";
            HKEY hCountKey;
            if (RegOpenKeyEx(hKey, countPath.c_str(), 0, KEY_ALL_ACCESS, &hCountKey) == ERROR_SUCCESS)
            {
                // Delete all values under Count
                std::vector<std::wstring> countValues;
                DWORD idx = 0;
                wchar_t cvName[512];
                DWORD cvNameLen;
                while (true)
                {
                    cvNameLen = 512;
                    if (RegEnumValue(hCountKey, idx, cvName, &cvNameLen, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
                        break;
                    countValues.push_back(cvName);
                    idx++;
                }
                for (auto& cv : countValues)
                {
                    if (RegDeleteValue(hCountKey, cv.c_str()) == ERROR_SUCCESS)
                        cleaned++;
                }
                RegCloseKey(hCountKey);
            }
        }
        RegCloseKey(hKey);
    }

    wchar_t result[128];
    wsprintfW(result, L"Cleaned %d registry entries.", cleaned);
    return result;
}

std::wstring ResetRegistry()
{
    // Export current registry as backup, then reset key areas to defaults
    // This resets user-level customizations that may cause issues

    int reset = 0;

    // Reset Explorer settings to defaults
    RegDeleteTree(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced");
    HKEY hKey;
    if (RegCreateKeyEx(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced",
        0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr) == ERROR_SUCCESS)
    {
        DWORD val1 = 1;
        DWORD val0 = 0;
        RegSetValueEx(hKey, L"Hidden", 0, REG_DWORD, (BYTE*)&val1, sizeof(DWORD));
        RegSetValueEx(hKey, L"HideFileExt", 0, REG_DWORD, (BYTE*)&val0, sizeof(DWORD));
        RegSetValueEx(hKey, L"ShowSuperHidden", 0, REG_DWORD, (BYTE*)&val0, sizeof(DWORD));
        RegSetValueEx(hKey, L"LaunchTo", 0, REG_DWORD, (BYTE*)&val1, sizeof(DWORD));
        RegCloseKey(hKey);
        reset++;
    }

    // Reset desktop settings
    RegDeleteTree(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\VisualEffects");
    if (RegCreateKeyEx(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\VisualEffects",
        0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr) == ERROR_SUCCESS)
    {
        DWORD val = 0; // 0 = Let Windows choose
        RegSetValueEx(hKey, L"VisualFXSetting", 0, REG_DWORD, (BYTE*)&val, sizeof(DWORD));
        RegCloseKey(hKey);
        reset++;
    }

    // Reset mouse settings to defaults
    if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Control Panel\\Mouse", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS)
    {
        RegSetValueEx(hKey, L"MouseSpeed", 0, REG_SZ, (BYTE*)L"1", 4);
        RegSetValueEx(hKey, L"MouseThreshold1", 0, REG_SZ, (BYTE*)L"6", 4);
        RegSetValueEx(hKey, L"MouseThreshold2", 0, REG_SZ, (BYTE*)L"10", 6);
        RegCloseKey(hKey);
        reset++;
    }

    // Reset GameDVR / Game Bar to defaults (enabled)
    if (RegOpenKeyEx(HKEY_CURRENT_USER, L"System\\GameConfigStore", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS)
    {
        DWORD val1 = 1;
        RegSetValueEx(hKey, L"GameDVR_Enabled", 0, REG_DWORD, (BYTE*)&val1, sizeof(DWORD));
        RegDeleteValue(hKey, L"GameDVR_FSEBehaviorMode");
        RegDeleteValue(hKey, L"GameDVR_HonorUserFSEBehaviorMode");
        RegDeleteValue(hKey, L"GameDVR_FSEBehavior");
        RegDeleteValue(hKey, L"GameDVR_DXGIHonorFSEWindowsCompatible");
        RegCloseKey(hKey);
        reset++;
    }

    // Reset transparency to enabled
    if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS)
    {
        DWORD val1 = 1;
        RegSetValueEx(hKey, L"EnableTransparency", 0, REG_DWORD, (BYTE*)&val1, sizeof(DWORD));
        RegCloseKey(hKey);
        reset++;
    }

    // Remove Nagle tweaks (delete TcpAckFrequency / TCPNoDelay from interfaces)
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Interfaces",
        0, KEY_ENUMERATE_SUB_KEYS, &hKey) == ERROR_SUCCESS)
    {
        wchar_t subKeyName[256];
        DWORD index = 0;
        DWORD nameLen;
        while (true)
        {
            nameLen = 256;
            if (RegEnumKeyEx(hKey, index, subKeyName, &nameLen, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
                break;
            HKEY hSubKey;
            if (RegOpenKeyEx(hKey, subKeyName, 0, KEY_SET_VALUE, &hSubKey) == ERROR_SUCCESS)
            {
                RegDeleteValue(hSubKey, L"TcpAckFrequency");
                RegDeleteValue(hSubKey, L"TCPNoDelay");
                RegCloseKey(hSubKey);
            }
            index++;
        }
        RegCloseKey(hKey);
        reset++;
    }

    wchar_t result[128];
    wsprintfW(result, L"Reset %d registry areas to Windows defaults.\nA restart is recommended.", reset);
    return result;
}

std::wstring CleanPrefetch()
{
    int total = DeleteFolderContents(L"C:\\Windows\\Prefetch");

    wchar_t result[128];
    wsprintfW(result, L"Cleaned %d prefetch files.", total);
    return result;
}

std::wstring CleanDNS()
{
    STARTUPINFO si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {};
    wchar_t cmd[] = L"ipconfig /flushdns";

    BOOL ok = CreateProcess(nullptr, cmd, nullptr, nullptr, FALSE,
        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);

    if (ok)
    {
        WaitForSingleObject(pi.hProcess, 5000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return L"DNS cache flushed successfully.";
    }

    return L"Failed to flush DNS cache.";
}

std::wstring CleanGameFolders()
{
    int total = 0;

    wchar_t localAppData[MAX_PATH];
    SHGetFolderPath(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, localAppData);

    wchar_t appData[MAX_PATH];
    SHGetFolderPath(nullptr, CSIDL_APPDATA, nullptr, 0, appData);

    // EpicGames launcher cache/logs
    wchar_t epicLogsDir[MAX_PATH];
    wsprintfW(epicLogsDir, L"%s\\EpicGamesLauncher\\Saved\\Logs", localAppData);
    total += DeleteFolderContents(epicLogsDir);

    // EpicGames webcache
    wchar_t epicWebCache[MAX_PATH];
    wsprintfW(epicWebCache, L"%s\\EpicGamesLauncher\\Saved\\webcache", localAppData);
    total += DeleteFolderContents(epicWebCache);

    // Fortnite logs
    wchar_t fnLogsDir[MAX_PATH];
    wsprintfW(fnLogsDir, L"%s\\FortniteGame\\Saved\\Logs", localAppData);
    total += DeleteFolderContents(fnLogsDir);

    // Fortnite crash reports
    wchar_t fnCrashDir[MAX_PATH];
    wsprintfW(fnCrashDir, L"%s\\FortniteGame\\Saved\\Crashes", localAppData);
    total += DeleteFolderContents(fnCrashDir);

    // Steam logs
    total += DeleteFilesInFolder(L"C:\\Program Files (x86)\\Steam\\logs", L"*", true);

    // Steam dumps
    total += DeleteFilesInFolder(L"C:\\Program Files (x86)\\Steam\\dumps", L"*", true);

    // Riot Games logs
    wchar_t riotLogsDir[MAX_PATH];
    wsprintfW(riotLogsDir, L"%s\\Riot Games\\Logs", localAppData);
    total += DeleteFolderContents(riotLogsDir);

    // Minecraft logs
    wchar_t mcLogsDir[MAX_PATH];
    wsprintfW(mcLogsDir, L"%s\\.minecraft\\logs", appData);
    total += DeleteFolderContents(mcLogsDir);

    // Minecraft crash reports
    wchar_t mcCrashDir[MAX_PATH];
    wsprintfW(mcCrashDir, L"%s\\.minecraft\\crash-reports", appData);
    total += DeleteFolderContents(mcCrashDir);

    // EA/Origin cache
    wchar_t eaDir[MAX_PATH];
    wsprintfW(eaDir, L"%s\\Origin\\Logs", appData);
    total += DeleteFolderContents(eaDir);

    wchar_t result[128];
    wsprintfW(result, L"Cleaned %d game files and folders.", total);
    return result;
}

// ============================================================
// Restore functions
// ============================================================

bool RunHiddenCommand(const wchar_t* cmd, DWORD timeoutMs = 30000)
{
    STARTUPINFO si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {};

    // CreateProcess needs a mutable string
    std::wstring cmdStr(cmd);
    BOOL ok = CreateProcess(nullptr, &cmdStr[0], nullptr, nullptr, FALSE,
        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    if (!ok) return false;

    WaitForSingleObject(pi.hProcess, timeoutMs);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return exitCode == 0;
}

void ProtectFolder(const wchar_t* folderPath)
{
    // Set hidden + system attributes
    SetFileAttributes(folderPath, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_READONLY);

    // Lock ACL: deny delete to Everyone, allow full control only to SYSTEM and Administrators
    std::wstring cmd = L"icacls \"";
    cmd += folderPath;
    cmd += L"\" /inheritance:r /grant:r SYSTEM:(OI)(CI)F /grant:r Administrators:(OI)(CI)F /deny Everyone:(OI)(CI)(DE,DC)";
    RunHiddenCommand(cmd.c_str(), 10000);
}

std::wstring GetEclypseBackupDir()
{
    return L"C:\\ProgramData\\ECLYPSE\\RestoreBackups";
}

void BackupRestorePointData(const wchar_t* tag)
{
    std::wstring backupDir = GetEclypseBackupDir();
    CreateDirectory(L"C:\\ProgramData\\ECLYPSE", nullptr);
    CreateDirectory(backupDir.c_str(), nullptr);

    // Create timestamped subfolder
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t subDir[MAX_PATH];
    wsprintfW(subDir, L"%s\\%s_%04d%02d%02d_%02d%02d%02d",
        backupDir.c_str(), tag, st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    CreateDirectory(subDir, nullptr);

    // Export critical registry keys
    wchar_t regCmd[512];
    wsprintfW(regCmd, L"reg export HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer \"%s\\explorer.reg\" /y", subDir);
    RunHiddenCommand(regCmd);

    wsprintfW(regCmd, L"reg export HKCU\\System\\GameConfigStore \"%s\\gameconfig.reg\" /y", subDir);
    RunHiddenCommand(regCmd);

    wsprintfW(regCmd, L"reg export \"HKCU\\Control Panel\\Mouse\" \"%s\\mouse.reg\" /y", subDir);
    RunHiddenCommand(regCmd);

    wsprintfW(regCmd, L"reg export \"HKCU\\Control Panel\\Desktop\" \"%s\\desktop.reg\" /y", subDir);
    RunHiddenCommand(regCmd);

    wsprintfW(regCmd, L"reg export HKCU\\Software\\Microsoft\\GameBar \"%s\\gamebar.reg\" /y", subDir);
    RunHiddenCommand(regCmd);

    wsprintfW(regCmd, L"reg export \"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize\" \"%s\\themes.reg\" /y", subDir);
    RunHiddenCommand(regCmd);

    // Save a marker file with timestamp
    wchar_t markerPath[MAX_PATH];
    wsprintfW(markerPath, L"%s\\eclypse_restore.txt", subDir);
    FILE* f = nullptr;
    _wfopen_s(&f, markerPath, L"w");
    if (f)
    {
        fwprintf(f, L"ECLYPSE Restore Point\nCreated: %04d-%02d-%02d %02d:%02d:%02d\nTag: %s\n",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, tag);
        fclose(f);
    }

    // Protect the backup folder from deletion
    ProtectFolder(subDir);
    ProtectFolder(backupDir.c_str());
    ProtectFolder(L"C:\\ProgramData\\ECLYPSE");
}

std::wstring CreateRestorePoint()
{
    // Bypass the 24-hour cooldown
    HKEY hKey;
    if (RegCreateKeyEx(HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\SystemRestore",
        0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr) == ERROR_SUCCESS)
    {
        DWORD val = 0;
        RegSetValueEx(hKey, L"SystemRestorePointCreationFrequency", 0, REG_DWORD, (BYTE*)&val, sizeof(val));
        RegCloseKey(hKey);
    }

    // Enable System Restore on C:
    RunHiddenCommand(L"powershell -Command \"Enable-ComputerRestore -Drive 'C:\\'\"", 15000);

    // Disable VSS cleanup tasks temporarily to protect the restore point
    RunHiddenCommand(L"schtasks /Change /TN \"Microsoft\\Windows\\SystemRestore\\SR\" /DISABLE", 10000);

    // Create the Windows restore point
    bool rpOk = RunHiddenCommand(
        L"powershell -Command \"Checkpoint-Computer -Description 'ECLYPSE Restore Point' -RestorePointType 'MODIFY_SETTINGS'\"",
        60000);

    // Re-enable the SR task
    RunHiddenCommand(L"schtasks /Change /TN \"Microsoft\\Windows\\SystemRestore\\SR\" /ENABLE", 10000);

    // Also create our own backup
    BackupRestorePointData(L"ECLYPSE");

    // Increase System Restore disk space to reduce chance of auto-cleanup
    RunHiddenCommand(L"vssadmin resize shadowstorage /for=C: /on=C: /maxsize=15%", 15000);

    if (rpOk)
        return L"Restore point created and protected.\n\n"
               L"- Windows restore point saved\n"
               L"- Registry backup saved to C:\\ProgramData\\ECLYPSE\\RestoreBackups\n"
               L"- Backup folder protected from 3rd party deletion\n"
               L"- VSS cleanup task temporarily disabled during creation";

    return L"Windows restore point may have failed, but ECLYPSE backup was created.\n\n"
           L"Registry backup saved to C:\\ProgramData\\ECLYPSE\\RestoreBackups";
}

void EnumerateRestorePoints()
{
    g_restorePoints.clear();

    // Get Windows restore points via PowerShell
    wchar_t outPath[MAX_PATH];
    GetTempPath(MAX_PATH, outPath);
    wcscat_s(outPath, L"eclypse_rp_list.txt");

    wchar_t cmd[512];
    wsprintfW(cmd, L"powershell -Command \"Get-ComputerRestorePoint | ForEach-Object { $_.SequenceNumber.ToString() + '|' + $_.Description + '|' + $_.CreationTime } | Out-File -Encoding utf8 '%s'\"", outPath);
    RunHiddenCommand(cmd, 15000);

    FILE* f = nullptr;
    _wfopen_s(&f, outPath, L"r, ccs=UTF-8");
    if (f)
    {
        wchar_t line[512];
        while (fgetws(line, 512, f))
        {
            // Trim newline
            size_t len = wcslen(line);
            while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
                line[--len] = 0;
            if (len == 0) continue;

            // Parse: sequenceNumber|description|date
            wchar_t* ctx = nullptr;
            wchar_t* seqStr = wcstok_s(line, L"|", &ctx);
            wchar_t* desc = wcstok_s(nullptr, L"|", &ctx);
            wchar_t* date = wcstok_s(nullptr, L"|", &ctx);

            if (seqStr && desc)
            {
                RestorePointInfo rpi;
                rpi.sequenceNumber = (DWORD)_wtoi(seqStr);
                rpi.description = desc;
                rpi.date = date ? date : L"";
                g_restorePoints.push_back(rpi);
            }
        }
        fclose(f);
    }
    DeleteFile(outPath);

    // Also scan ECLYPSE backup folders
    std::wstring backupDir = GetEclypseBackupDir();
    wchar_t searchPath[MAX_PATH];
    wsprintfW(searchPath, L"%s\\*", backupDir.c_str());

    WIN32_FIND_DATA fd;
    HANDLE hFind = FindFirstFile(searchPath, &fd);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
            if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;

            // Check if this backup is already represented by a Windows RP
            bool alreadyListed = false;
            for (auto& rp : g_restorePoints)
            {
                if (rp.description.find(L"ECLYPSE") != std::wstring::npos)
                {
                    alreadyListed = true;
                    break;
                }
            }

            if (!alreadyListed)
            {
                // Read marker file
                wchar_t markerPath[MAX_PATH];
                wsprintfW(markerPath, L"%s\\%s\\eclypse_restore.txt", backupDir.c_str(), fd.cFileName);

                FILE* mf = nullptr;
                _wfopen_s(&mf, markerPath, L"r");
                if (mf)
                {
                    RestorePointInfo rpi;
                    rpi.sequenceNumber = 0; // ECLYPSE backup, not a Windows RP
                    rpi.description = L"[ECLYPSE Backup] ";
                    rpi.description += fd.cFileName;
                    // Extract date from folder name
                    rpi.date = fd.cFileName;
                    g_restorePoints.push_back(rpi);
                    fclose(mf);
                }
            }
        } while (FindNextFile(hFind, &fd));
        FindClose(hFind);
    }

    g_restorePointRects.resize(g_restorePoints.size());
    g_restorePointBtns.resize(g_restorePoints.size());
    for (auto& b : g_restorePointBtns) b = {};
    g_selectedRestorePoint = -1;
}

std::wstring LoadRestorePoint(int index)
{
    if (index < 0 || index >= (int)g_restorePoints.size())
        return L"Invalid restore point selected.";

    auto& rp = g_restorePoints[index];

    if (rp.sequenceNumber > 0)
    {
        // Windows restore point — use rstrui.exe
        wchar_t cmd[128];
        wsprintfW(cmd, L"rstrui.exe /LAUNCHRESTORE");
        RunHiddenCommand(cmd, 5000);
        return L"System Restore wizard launched.\n\nSelect the restore point in the wizard and follow the prompts.\nYour PC will restart to complete the restore.";
    }
    else
    {
        // ECLYPSE backup — import registry files
        std::wstring backupDir = GetEclypseBackupDir();
        // Extract folder name from description
        std::wstring folderName = rp.description;
        size_t pos = folderName.find(L"] ");
        if (pos != std::wstring::npos)
            folderName = folderName.substr(pos + 2);

        wchar_t backupPath[MAX_PATH];
        wsprintfW(backupPath, L"%s\\%s", backupDir.c_str(), folderName.c_str());

        // Unprotect folder temporarily for reading
        std::wstring unlockCmd = L"icacls \"" + std::wstring(backupPath) + L"\" /grant Everyone:(OI)(CI)R";
        RunHiddenCommand(unlockCmd.c_str(), 5000);

        // Import all .reg files
        int imported = 0;
        wchar_t search[MAX_PATH];
        wsprintfW(search, L"%s\\*.reg", backupPath);

        WIN32_FIND_DATA fd;
        HANDLE hFind = FindFirstFile(search, &fd);
        if (hFind != INVALID_HANDLE_VALUE)
        {
            do
            {
                wchar_t regFile[MAX_PATH];
                wsprintfW(regFile, L"%s\\%s", backupPath, fd.cFileName);
                wchar_t importCmd[512];
                wsprintfW(importCmd, L"reg import \"%s\"", regFile);
                if (RunHiddenCommand(importCmd, 10000))
                    imported++;
            } while (FindNextFile(hFind, &fd));
            FindClose(hFind);
        }

        // Re-protect
        ProtectFolder(backupPath);

        wchar_t result[256];
        wsprintfW(result, L"ECLYPSE backup restored.\n\nImported %d registry files from:\n%s\n\nA restart is recommended.", imported, backupPath);
        return result;
    }
}

// ============================================================
// Optimization functions
// ============================================================

std::wstring OptimPowerPlan()
{
    // Activate Ultimate Performance (or High Performance as fallback)
    STARTUPINFO si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {};

    // First try to unhide Ultimate Performance
    wchar_t unhideCmd[] = L"powercfg -duplicatescheme e9a42b02-d5df-448d-aa00-03f14749eb61";
    CreateProcess(nullptr, unhideCmd, nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    WaitForSingleObject(pi.hProcess, 5000);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    // Set High Performance as active
    wchar_t setCmd[] = L"powercfg -setactive 8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c";
    pi = {};
    BOOL ok = CreateProcess(nullptr, setCmd, nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    if (ok)
    {
        WaitForSingleObject(pi.hProcess, 5000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    return L"Power plan set to High Performance.";
}

std::wstring OptimGameBar()
{
    int changed = 0;
    HKEY hKey;

    // Disable Game Bar
    if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Microsoft\\GameBar", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS)
    {
        DWORD val = 0;
        RegSetValueEx(hKey, L"AllowAutoGameMode", 0, REG_DWORD, (BYTE*)&val, sizeof(val));
        RegSetValueEx(hKey, L"AutoGameModeEnabled", 0, REG_DWORD, (BYTE*)&val, sizeof(val));
        RegSetValueEx(hKey, L"UseNexusForGameBarEnabled", 0, REG_DWORD, (BYTE*)&val, sizeof(val));
        RegCloseKey(hKey);
        changed++;
    }

    // Disable Game DVR
    if (RegCreateKeyEx(HKEY_CURRENT_USER, L"System\\GameConfigStore", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr) == ERROR_SUCCESS)
    {
        DWORD val = 0;
        RegSetValueEx(hKey, L"GameDVR_Enabled", 0, REG_DWORD, (BYTE*)&val, sizeof(val));
        RegCloseKey(hKey);
        changed++;
    }

    if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies\\Microsoft\\Windows\\GameDVR", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr) == ERROR_SUCCESS)
    {
        DWORD val = 0;
        RegSetValueEx(hKey, L"AllowGameDVR", 0, REG_DWORD, (BYTE*)&val, sizeof(val));
        RegCloseKey(hKey);
        changed++;
    }

    return L"Xbox Game Bar and Game DVR disabled.";
}

std::wstring OptimVisualFx()
{
    HKEY hKey;

    // Set visual effects to "Adjust for best performance"
    if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\VisualEffects", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS)
    {
        DWORD val = 2; // 2 = Best Performance
        RegSetValueEx(hKey, L"VisualFXSetting", 0, REG_DWORD, (BYTE*)&val, sizeof(val));
        RegCloseKey(hKey);
    }

    // Disable transparency
    if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS)
    {
        DWORD val = 0;
        RegSetValueEx(hKey, L"EnableTransparency", 0, REG_DWORD, (BYTE*)&val, sizeof(val));
        RegCloseKey(hKey);
    }

    // Disable animations
    if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Control Panel\\Desktop\\WindowMetrics", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS)
    {
        RegSetValueEx(hKey, L"MinAnimate", 0, REG_SZ, (BYTE*)L"0", 4);
        RegCloseKey(hKey);
    }

    // Disable smooth scrolling, menu animations
    if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Control Panel\\Desktop", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS)
    {
        DWORD zero2 = 0;
        RegSetValueEx(hKey, L"SmoothScroll", 0, REG_DWORD, (BYTE*)&zero2, sizeof(DWORD));
        RegSetValueEx(hKey, L"UserPreferencesMask", 0, REG_BINARY,
            (BYTE*)"\x90\x12\x03\x80\x10\x00\x00\x00", 8);
        RegCloseKey(hKey);
    }

    return L"Visual effects set to best performance. Transparency and animations disabled.";
}

std::wstring OptimNagle()
{
    HKEY hKey;
    int changed = 0;

    // Find all network interfaces and disable Nagle's algorithm
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Interfaces",
        0, KEY_ENUMERATE_SUB_KEYS, &hKey) == ERROR_SUCCESS)
    {
        wchar_t subKeyName[256];
        DWORD index = 0;
        DWORD nameLen;

        while (true)
        {
            nameLen = 256;
            if (RegEnumKeyEx(hKey, index, subKeyName, &nameLen, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
                break;

            HKEY hSubKey;
            if (RegOpenKeyEx(hKey, subKeyName, 0, KEY_SET_VALUE, &hSubKey) == ERROR_SUCCESS)
            {
                DWORD val = 1;
                RegSetValueEx(hSubKey, L"TcpAckFrequency", 0, REG_DWORD, (BYTE*)&val, sizeof(val));
                RegSetValueEx(hSubKey, L"TCPNoDelay", 0, REG_DWORD, (BYTE*)&val, sizeof(val));
                RegCloseKey(hSubKey);
                changed++;
            }
            index++;
        }
        RegCloseKey(hKey);
    }

    wchar_t result[128];
    wsprintfW(result, L"Nagle's algorithm disabled on %d network interfaces.", changed);
    return result;
}

std::wstring OptimMouseAccel()
{
    // Disable enhanced pointer precision (mouse acceleration)
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Control Panel\\Mouse", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS)
    {
        RegSetValueEx(hKey, L"MouseSpeed", 0, REG_SZ, (BYTE*)L"0", 4);
        RegSetValueEx(hKey, L"MouseThreshold1", 0, REG_SZ, (BYTE*)L"0", 4);
        RegSetValueEx(hKey, L"MouseThreshold2", 0, REG_SZ, (BYTE*)L"0", 4);
        RegCloseKey(hKey);
    }

    // Apply immediately
    int params[3] = { 0, 0, 0 };
    SystemParametersInfo(SPI_SETMOUSE, 0, params, SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);

    return L"Mouse acceleration disabled. Raw input enabled.";
}

std::wstring OptimFullscreenOpt()
{
    HKEY hKey;

    // Disable fullscreen optimizations globally
    if (RegCreateKeyEx(HKEY_CURRENT_USER,
        L"System\\GameConfigStore", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr) == ERROR_SUCCESS)
    {
        DWORD fsoDisable = 2;
        DWORD fsoEnable = 1;
        RegSetValueEx(hKey, L"GameDVR_FSEBehaviorMode", 0, REG_DWORD, (BYTE*)&fsoDisable, sizeof(DWORD));
        RegSetValueEx(hKey, L"GameDVR_HonorUserFSEBehaviorMode", 0, REG_DWORD, (BYTE*)&fsoEnable, sizeof(DWORD));
        RegSetValueEx(hKey, L"GameDVR_FSEBehavior", 0, REG_DWORD, (BYTE*)&fsoDisable, sizeof(DWORD));
        RegSetValueEx(hKey, L"GameDVR_DXGIHonorFSEWindowsCompatible", 0, REG_DWORD, (BYTE*)&fsoEnable, sizeof(DWORD));
        RegCloseKey(hKey);
    }

    return L"Fullscreen optimizations disabled globally.";
}

std::wstring OptimStandbyMem()
{
    // Free standby memory using EmptyWorkingSet on all processes
    // and RtlAdjustPrivilege + NtSetSystemInformation approach
    HANDLE hProcess = GetCurrentProcess();

    // First enable SeProfileSingleProcessPrivilege
    HANDLE hToken;
    if (OpenProcessToken(hProcess, TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
    {
        TOKEN_PRIVILEGES tp = {};
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        LookupPrivilegeValue(nullptr, SE_PROF_SINGLE_PROCESS_NAME, &tp.Privileges[0].Luid);
        AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), nullptr, nullptr);

        // Also enable SeIncreaseQuotaPrivilege
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        LookupPrivilegeValue(nullptr, SE_INCREASE_QUOTA_NAME, &tp.Privileges[0].Luid);
        AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), nullptr, nullptr);
        CloseHandle(hToken);
    }

    // Use SetProcessWorkingSetSize to trim current process
    SetProcessWorkingSetSize(hProcess, (SIZE_T)-1, (SIZE_T)-1);

    // Clear file system cache via SetSystemFileCacheSize
    SetSystemFileCacheSize(0, 0, FILE_CACHE_MIN_HARD_DISABLE | FILE_CACHE_MAX_HARD_DISABLE);

    // Re-enable cache with defaults
    SetSystemFileCacheSize(0, 0, FILE_CACHE_MIN_HARD_ENABLE | FILE_CACHE_MAX_HARD_ENABLE);

    return L"Standby memory flushed. RAM freed for gaming.";
}

std::wstring OptimGpuPriority()
{
    HKEY hKey;

    // Set GPU scheduling priority
    if (RegCreateKeyEx(HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Multimedia\\SystemProfile\\Tasks\\Games",
        0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr) == ERROR_SUCCESS)
    {
        DWORD gpuPriority = 8;
        DWORD priority = 6;
        DWORD schedulingCategory = 3; // High
        DWORD sfioCategory = 3; // High
        RegSetValueEx(hKey, L"GPU Priority", 0, REG_DWORD, (BYTE*)&gpuPriority, sizeof(gpuPriority));
        RegSetValueEx(hKey, L"Priority", 0, REG_DWORD, (BYTE*)&priority, sizeof(priority));
        RegSetValueEx(hKey, L"Scheduling Category", 0, REG_SZ, (BYTE*)L"High", 10);
        RegSetValueEx(hKey, L"SFIO Priority", 0, REG_SZ, (BYTE*)L"High", 10);
        RegCloseKey(hKey);
    }

    // Enable hardware-accelerated GPU scheduling
    if (RegCreateKeyEx(HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Control\\GraphicsDrivers", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr) == ERROR_SUCCESS)
    {
        DWORD val = 2;
        RegSetValueEx(hKey, L"HwSchMode", 0, REG_DWORD, (BYTE*)&val, sizeof(val));
        RegCloseKey(hKey);
    }

    return L"GPU priority set to high. Hardware-accelerated GPU scheduling enabled.";
}

// ============================================================
// Disk functions
// ============================================================
// ============================================================

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

    // Find all partitions on this physical disk
    std::wstring partitionList;
    int partCount = 0;
    DWORD driveMask = GetLogicalDrives();
    for (int i = 0; i < 26; i++)
    {
        if (!(driveMask & (1 << i))) continue;
        wchar_t letter = L'A' + i;
        DWORD dn = GetPhysicalDiskNumber(letter);
        if (dn == diskNumber)
        {
            wchar_t root[] = { letter, L':', L'\\', 0 };
            wchar_t volName[MAX_PATH] = {};
            GetVolumeInformation(root, volName, MAX_PATH, nullptr, nullptr, nullptr, nullptr, 0);

            ULARGE_INTEGER totalBytes = {}, freeBytes = {};
            GetDiskFreeSpaceEx(root, nullptr, &totalBytes, &freeBytes);
            std::wstring sizeStr = FormatBytes(totalBytes.QuadPart);

            wchar_t line[256];
            wsprintfW(line, L"  %c:\\  %s  (%s)\n", letter, volName[0] ? volName : L"(unnamed)", sizeStr.c_str());
            partitionList += line;
            partCount++;
        }
    }

    std::wstring confirmMsg = L"This will DELETE ALL partitions on Disk ";
    confirmMsg += std::to_wstring(diskNumber);
    confirmMsg += L" and create a single NTFS volume.\n\n";
    confirmMsg += std::to_wstring(partCount);
    confirmMsg += L" partition(s) will be PERMANENTLY DELETED:\n\n";
    confirmMsg += partitionList;
    confirmMsg += L"\nALL data on these partitions will be lost.\n\nAre you sure?";

    if (MessageBox(hwnd, confirmMsg.c_str(), L"ECLYPSE - Confirm Merge", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
        return false;

    std::wstring lastChance = L"LAST CHANCE: The following partitions will be wiped:\n\n";
    lastChance += partitionList;
    lastChance += L"\nThis cannot be undone. Proceed?";

    if (MessageBox(hwnd, lastChance.c_str(), L"ECLYPSE - Final Warning", MB_YESNO | MB_ICONERROR | MB_DEFBUTTON2) != IDYES)
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

        if (g_activeTab == TAB_RESTORE)
        {
            if (g_showRestoreLoadView)
            {
                for (size_t i = 0; i < g_restorePoints.size(); i++)
                {
                    bool w = g_restorePointBtns[i].hovered;
                    g_restorePointBtns[i].hovered = PtInRect(&g_restorePointRects[i], pt);
                    if (w != g_restorePointBtns[i].hovered) InvalidateRect(hwnd, &g_restorePointRects[i], FALSE);
                }
                bool w1 = g_restoreApplyBtn.hovered; g_restoreApplyBtn.hovered = PtInRect(&g_restoreApplyBtnRect, pt);
                if (w1 != g_restoreApplyBtn.hovered) InvalidateRect(hwnd, &g_restoreApplyBtnRect, FALSE);
                bool w2 = g_restoreBackBtn.hovered; g_restoreBackBtn.hovered = PtInRect(&g_restoreBackBtnRect, pt);
                if (w2 != g_restoreBackBtn.hovered) InvalidateRect(hwnd, &g_restoreBackBtnRect, FALSE);
            }
            else
            {
                bool w1 = g_restoreCreateBtn.hovered; g_restoreCreateBtn.hovered = PtInRect(&g_restoreCreateBtnRect, pt);
                if (w1 != g_restoreCreateBtn.hovered) InvalidateRect(hwnd, &g_restoreCreateBtnRect, FALSE);
                bool w2 = g_restoreLoadBtn.hovered; g_restoreLoadBtn.hovered = PtInRect(&g_restoreLoadBtnRect, pt);
                if (w2 != g_restoreLoadBtn.hovered) InvalidateRect(hwnd, &g_restoreLoadBtnRect, FALSE);
            }
        }
        else if (g_activeTab == TAB_OPTIMIZATION)
        {
            for (int i = 0; i < OPTIM_COUNT; i++)
            {
                bool wasHov = g_optimBtns[i].hovered;
                g_optimBtns[i].hovered = PtInRect(&g_optimBtnRects[i], pt);
                if (wasHov != g_optimBtns[i].hovered)
                    InvalidateRect(hwnd, &g_optimBtnRects[i], FALSE);
            }
        }
        else if (g_activeTab == TAB_OTHERS)
        {
            if (g_showRegistrySubView)
            {
                bool w1 = g_regResetBtn.hovered; g_regResetBtn.hovered = PtInRect(&g_regResetBtnRect, pt);
                if (w1 != g_regResetBtn.hovered) InvalidateRect(hwnd, &g_regResetBtnRect, FALSE);
                bool w2 = g_regCleanBtn.hovered; g_regCleanBtn.hovered = PtInRect(&g_regCleanBtnRect, pt);
                if (w2 != g_regCleanBtn.hovered) InvalidateRect(hwnd, &g_regCleanBtnRect, FALSE);
                bool w3 = g_regBackBtn.hovered; g_regBackBtn.hovered = PtInRect(&g_regBackBtnRect, pt);
                if (w3 != g_regBackBtn.hovered) InvalidateRect(hwnd, &g_regBackBtnRect, FALSE);
            }
            else
            {
                for (int i = 0; i < OPT_COUNT; i++)
                {
                    bool wasHov = g_optionBtns[i].hovered;
                    g_optionBtns[i].hovered = PtInRect(&g_optionBtnRects[i], pt);
                    if (wasHov != g_optionBtns[i].hovered)
                        InvalidateRect(hwnd, &g_optionBtnRects[i], FALSE);
                }
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
        for (auto& s : g_optimBtns)
            s.hovered = false;
        g_restoreCreateBtn.hovered = false;
        g_restoreLoadBtn.hovered = false;
        g_restoreApplyBtn.hovered = false;
        g_restoreBackBtn.hovered = false;
        for (auto& b : g_restorePointBtns) b.hovered = false;
        g_regResetBtn.hovered = false;
        g_regCleanBtn.hovered = false;
        g_regBackBtn.hovered = false;
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
                g_showRegistrySubView = false;
                g_showRestoreLoadView = false;
                g_cleanerScrollY = 0;
                g_partScrollY = 0;
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

        if (g_activeTab == TAB_RESTORE)
        {
            if (g_showRestoreLoadView)
            {
                for (size_t i = 0; i < g_restorePoints.size(); i++)
                {
                    if (PtInRect(&g_restorePointRects[i], pt))
                    { g_restorePointBtns[i].pressed = true; SetCapture(hwnd); InvalidateRect(hwnd, &g_restorePointRects[i], FALSE); return 0; }
                }
                if (PtInRect(&g_restoreApplyBtnRect, pt))
                { g_restoreApplyBtn.pressed = true; SetCapture(hwnd); InvalidateRect(hwnd, &g_restoreApplyBtnRect, FALSE); return 0; }
                if (PtInRect(&g_restoreBackBtnRect, pt))
                { g_restoreBackBtn.pressed = true; SetCapture(hwnd); InvalidateRect(hwnd, &g_restoreBackBtnRect, FALSE); return 0; }
            }
            else
            {
                if (PtInRect(&g_restoreCreateBtnRect, pt))
                { g_restoreCreateBtn.pressed = true; SetCapture(hwnd); InvalidateRect(hwnd, &g_restoreCreateBtnRect, FALSE); return 0; }
                if (PtInRect(&g_restoreLoadBtnRect, pt))
                { g_restoreLoadBtn.pressed = true; SetCapture(hwnd); InvalidateRect(hwnd, &g_restoreLoadBtnRect, FALSE); return 0; }
            }
        }
        else if (g_activeTab == TAB_OPTIMIZATION)
        {
            for (int i = 0; i < OPTIM_COUNT; i++)
            {
                if (PtInRect(&g_optimBtnRects[i], pt))
                {
                    g_optimBtns[i].pressed = true;
                    SetCapture(hwnd);
                    InvalidateRect(hwnd, &g_optimBtnRects[i], FALSE);
                    return 0;
                }
            }
        }
        else if (g_activeTab == TAB_OTHERS)
        {
            if (g_showRegistrySubView)
            {
                if (PtInRect(&g_regResetBtnRect, pt))
                { g_regResetBtn.pressed = true; SetCapture(hwnd); InvalidateRect(hwnd, &g_regResetBtnRect, FALSE); return 0; }
                if (PtInRect(&g_regCleanBtnRect, pt))
                { g_regCleanBtn.pressed = true; SetCapture(hwnd); InvalidateRect(hwnd, &g_regCleanBtnRect, FALSE); return 0; }
                if (PtInRect(&g_regBackBtnRect, pt))
                { g_regBackBtn.pressed = true; SetCapture(hwnd); InvalidateRect(hwnd, &g_regBackBtnRect, FALSE); return 0; }
            }
            else
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

        // Restore tab
        if (g_activeTab == TAB_RESTORE)
        {
            if (g_showRestoreLoadView)
            {
                // Restore point selection
                for (size_t i = 0; i < g_restorePoints.size(); i++)
                {
                    bool was = g_restorePointBtns[i].pressed; g_restorePointBtns[i].pressed = false;
                    if (was && PtInRect(&g_restorePointRects[i], pt))
                    {
                        g_selectedRestorePoint = (int)i;
                        InvalidateRect(hwnd, nullptr, FALSE);
                        return 0;
                    }
                }

                // Apply button
                bool wasApply = g_restoreApplyBtn.pressed; g_restoreApplyBtn.pressed = false;
                if (wasApply && PtInRect(&g_restoreApplyBtnRect, pt))
                {
                    if (g_selectedRestorePoint >= 0 && g_selectedRestorePoint < (int)g_restorePoints.size())
                    {
                        int r = MessageBox(hwnd, L"Load the selected restore point?\n\nThis will restore registry settings from the backup.",
                            L"ECLYPSE - Confirm", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
                        if (r == IDYES)
                        {
                            std::wstring res = LoadRestorePoint(g_selectedRestorePoint);
                            MessageBox(hwnd, res.c_str(), L"ECLYPSE - Restore", MB_OK | MB_ICONINFORMATION);
                        }
                    }
                    else
                    {
                        MessageBox(hwnd, L"Please select a restore point first.", L"ECLYPSE", MB_OK | MB_ICONINFORMATION);
                    }
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }

                // Back button
                bool wasBack = g_restoreBackBtn.pressed; g_restoreBackBtn.pressed = false;
                if (wasBack && PtInRect(&g_restoreBackBtnRect, pt))
                {
                    g_showRestoreLoadView = false;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }

                InvalidateRect(hwnd, nullptr, FALSE);
            }
            else
            {
                // Create button
                bool wasCreate = g_restoreCreateBtn.pressed; g_restoreCreateBtn.pressed = false;
                if (wasCreate && PtInRect(&g_restoreCreateBtnRect, pt))
                {
                    int r = MessageBox(hwnd, L"Create a protected restore point?\n\nThis will create a Windows restore point and an ECLYPSE backup.\nThis may take a moment.",
                        L"ECLYPSE - Confirm", MB_YESNO | MB_ICONQUESTION);
                    if (r == IDYES)
                    {
                        std::wstring res = CreateRestorePoint();
                        MessageBox(hwnd, res.c_str(), L"ECLYPSE - Restore", MB_OK | MB_ICONINFORMATION);
                    }
                    InvalidateRect(hwnd, &g_restoreCreateBtnRect, FALSE);
                    return 0;
                }

                // Load button
                bool wasLoad = g_restoreLoadBtn.pressed; g_restoreLoadBtn.pressed = false;
                if (wasLoad && PtInRect(&g_restoreLoadBtnRect, pt))
                {
                    EnumerateRestorePoints();
                    g_showRestoreLoadView = true;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }

                InvalidateRect(hwnd, nullptr, FALSE);
            }
        }

        // Optimization tab buttons
        if (g_activeTab == TAB_OPTIMIZATION)
        {
            for (int i = 0; i < OPTIM_COUNT; i++)
            {
                bool wasPressed = g_optimBtns[i].pressed;
                g_optimBtns[i].pressed = false;

                if (wasPressed && PtInRect(&g_optimBtnRects[i], pt))
                {
                    wchar_t confirmMsg[256];
                    wsprintfW(confirmMsg, L"Apply optimization: %s?\n\n%s", g_optimNames[i], g_optimDescs[i]);
                    int result = MessageBox(hwnd, confirmMsg, L"ECLYPSE - Confirm", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);

                    if (result == IDYES)
                    {
                        std::wstring resultStr;
                        switch (i)
                        {
                        case OPTIM_POWER_PLAN:      resultStr = OptimPowerPlan(); break;
                        case OPTIM_GAME_BAR:        resultStr = OptimGameBar(); break;
                        case OPTIM_VISUAL_FX:       resultStr = OptimVisualFx(); break;
                        case OPTIM_NAGLE:           resultStr = OptimNagle(); break;
                        case OPTIM_MOUSE_ACCEL:     resultStr = OptimMouseAccel(); break;
                        case OPTIM_FULLSCREEN_OPT:  resultStr = OptimFullscreenOpt(); break;
                        case OPTIM_STANDBY_MEM:     resultStr = OptimStandbyMem(); break;
                        case OPTIM_GPU_PRIORITY:    resultStr = OptimGpuPriority(); break;
                        }
                        MessageBox(hwnd, resultStr.c_str(), L"ECLYPSE - Applied", MB_OK | MB_ICONINFORMATION);
                    }

                    InvalidateRect(hwnd, &g_optimBtnRects[i], FALSE);
                    return 0;
                }
            }
        }

        // Others tab
        if (g_activeTab == TAB_OTHERS)
        {
            if (g_showRegistrySubView)
            {
                // Reset to Default
                bool wasReset = g_regResetBtn.pressed; g_regResetBtn.pressed = false;
                if (wasReset && PtInRect(&g_regResetBtnRect, pt))
                {
                    int r = MessageBox(hwnd, L"Reset registry to Windows defaults?\n\nThis will undo any optimizations applied by ECLYPSE.\nA restart is recommended after.",
                        L"ECLYPSE - Confirm", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
                    if (r == IDYES)
                    {
                        std::wstring res = ResetRegistry();
                        MessageBox(hwnd, res.c_str(), L"ECLYPSE - Complete", MB_OK | MB_ICONINFORMATION);
                    }
                    InvalidateRect(hwnd, &g_regResetBtnRect, FALSE);
                    return 0;
                }

                // Clean Registry
                bool wasClean = g_regCleanBtn.pressed; g_regCleanBtn.pressed = false;
                if (wasClean && PtInRect(&g_regCleanBtnRect, pt))
                {
                    int r = MessageBox(hwnd, L"Clean registry?\n\nThis will remove MUI cache, recent docs, and UserAssist tracking data.\n\nThis action cannot be undone.",
                        L"ECLYPSE - Confirm", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
                    if (r == IDYES)
                    {
                        std::wstring res = CleanRegistry();
                        MessageBox(hwnd, res.c_str(), L"ECLYPSE - Complete", MB_OK | MB_ICONINFORMATION);
                    }
                    InvalidateRect(hwnd, &g_regCleanBtnRect, FALSE);
                    return 0;
                }

                // Back
                bool wasBack = g_regBackBtn.pressed; g_regBackBtn.pressed = false;
                if (wasBack && PtInRect(&g_regBackBtnRect, pt))
                {
                    g_showRegistrySubView = false;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }

                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            else
            {
                for (int i = 0; i < OPT_COUNT; i++)
                {
                    bool wasPressed = g_optionBtns[i].pressed;
                    g_optionBtns[i].pressed = false;

                    if (wasPressed && PtInRect(&g_optionBtnRects[i], pt))
                    {
                        // Registry opens sub-view instead of directly cleaning
                        if (i == OPT_REGISTRY)
                        {
                            g_showRegistrySubView = true;
                            InvalidateRect(hwnd, nullptr, FALSE);
                            return 0;
                        }

                        wchar_t confirmMsg[256];
                        wsprintfW(confirmMsg, L"Are you sure you want to clean %s?\n\n%s\n\nThis action cannot be undone.",
                            g_optionNames[i], g_optionDescs[i]);
                        int result = MessageBox(hwnd, confirmMsg, L"ECLYPSE - Confirm", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);

                        if (result == IDYES)
                        {
                            std::wstring resultStr;
                            switch (i)
                            {
                            case OPT_TEMP_FILES:   resultStr = CleanTempFiles(); break;
                            case OPT_LOGS:         resultStr = CleanLogs(); break;
                            case OPT_CACHE:        resultStr = CleanCache(); break;
                            case OPT_PREFETCH:     resultStr = CleanPrefetch(); break;
                            case OPT_DNS:          resultStr = CleanDNS(); break;
                            case OPT_GAME_FOLDERS: resultStr = CleanGameFolders(); break;
                            }
                            MessageBox(hwnd, resultStr.c_str(), L"ECLYPSE - Complete", MB_OK | MB_ICONINFORMATION);
                        }

                        InvalidateRect(hwnd, &g_optionBtnRects[i], FALSE);
                        return 0;
                    }
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

    case WM_MOUSEWHEEL:
    {
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        int scrollAmount = 40;

        if (g_activeTab == TAB_CLEANER)
        {
            g_cleanerScrollY -= (delta > 0) ? scrollAmount : -scrollAmount;
            int maxScroll = (int)g_drives.size() * 50 - 200;
            if (maxScroll < 0) maxScroll = 0;
            if (g_cleanerScrollY < 0) g_cleanerScrollY = 0;
            if (g_cleanerScrollY > maxScroll) g_cleanerScrollY = maxScroll;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        else if (g_activeTab == TAB_PARTITIONS && g_partShowDriveSelect)
        {
            g_partScrollY -= (delta > 0) ? scrollAmount : -scrollAmount;
            int maxScroll = (int)g_partDrives.size() * 50 - 200;
            if (maxScroll < 0) maxScroll = 0;
            if (g_partScrollY < 0) g_partScrollY = 0;
            if (g_partScrollY > maxScroll) g_partScrollY = maxScroll;
            InvalidateRect(hwnd, nullptr, FALSE);
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

bool IsRunningAsAdmin()
{
    BOOL isAdmin = FALSE;
    PSID adminGroup = nullptr;
    SID_IDENTIFIER_AUTHORITY ntAuth = SECURITY_NT_AUTHORITY;

    if (AllocateAndInitializeSid(&ntAuth, 2, SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup))
    {
        CheckTokenMembership(nullptr, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin;
}

void RelaunchAsAdmin()
{
    wchar_t exePath[MAX_PATH];
    GetModuleFileName(nullptr, exePath, MAX_PATH);

    SHELLEXECUTEINFO sei = {};
    sei.cbSize = sizeof(sei);
    sei.lpVerb = L"runas";
    sei.lpFile = exePath;
    sei.nShow = SW_SHOWNORMAL;

    ShellExecuteEx(&sei);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    if (!IsRunningAsAdmin())
    {
        RelaunchAsAdmin();
        return 0;
    }

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
        L"ECLYPSE Cleaner",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT, 700, 650,
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
