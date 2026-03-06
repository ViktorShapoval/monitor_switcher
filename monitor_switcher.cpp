#include <windows.h>
#include <stdio.h>
#include <string.h>

// monitor_switcher.exe --list
// monitor_switcher.exe --toggle
// monitor_switcher.exe --set-primary 2
// monitor_switcher.exe --set-primary \\.\DISPLAY2

struct DisplayInfo {
    char name[64];
    DEVMODEA dm;
    bool isPrimary;
};

static int g_displayCount = 0;
static DisplayInfo g_displays[16];

// ------------------------------------------------------------------
// Enumerate all active displays
// ------------------------------------------------------------------
void EnumerateDisplays()
{
    g_displayCount = 0;
    DISPLAY_DEVICEA dd = {};
    dd.cb = sizeof(dd);
    for (DWORD i = 0; EnumDisplayDevicesA(NULL, i, &dd, 0) && g_displayCount < 16; i++) {
        if (!(dd.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)) {
            dd = {};
            dd.cb = sizeof(dd);
            continue;
        }

        DisplayInfo& di = g_displays[g_displayCount];
        strncpy(di.name, dd.DeviceName, sizeof(di.name) - 1);
        di.name[sizeof(di.name) - 1] = '\0';
        di.isPrimary = (dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) != 0;

        memset(&di.dm, 0, sizeof(di.dm));
        di.dm.dmSize = sizeof(di.dm);
        EnumDisplaySettingsA(di.name, ENUM_CURRENT_SETTINGS, &di.dm);

        g_displayCount++;
        dd = {};
        dd.cb = sizeof(dd);
    }
}

// ------------------------------------------------------------------
// Print display list
// ------------------------------------------------------------------
void PrintDisplays()
{
    printf("Displays (%d found):\n\n", g_displayCount);
    for (int i = 0; i < g_displayCount; i++) {
        DisplayInfo& di = g_displays[i];
        printf("  [%d]  %s  %lux%lu @ %luHz  pos(%ld,%ld)%s\n",
               i + 1,
               di.name,
               di.dm.dmPelsWidth, di.dm.dmPelsHeight,
               di.dm.dmDisplayFrequency,
               di.dm.dmPosition.x, di.dm.dmPosition.y,
               di.isPrimary ? "  [PRIMARY]" : "");
    }
    printf("\n");
}

// ------------------------------------------------------------------
// Switch primary to the given display index (0-based internal)
// ------------------------------------------------------------------
int SwitchPrimary(int targetIdx)
{
    if (targetIdx < 0 || targetIdx >= g_displayCount) {
        printf("Error: invalid display index.\n");
        return 1;
    }

    if (g_displays[targetIdx].isPrimary) {
        printf("'%s' is already the primary display.\n", g_displays[targetIdx].name);
        return 0;
    }

    LONG offsetX = -g_displays[targetIdx].dm.dmPosition.x;
    LONG offsetY = -g_displays[targetIdx].dm.dmPosition.y;

    printf("Switching primary to [%d] %s\n", targetIdx + 1, g_displays[targetIdx].name);
    printf("  Current pos (%ld,%ld), applying offset (%ld,%ld)\n\n",
           g_displays[targetIdx].dm.dmPosition.x,
           g_displays[targetIdx].dm.dmPosition.y,
           offsetX, offsetY);

    // Stage new primary FIRST
    auto stageDisplay = [&](int idx) -> bool {
        DisplayInfo& di = g_displays[idx];
        DEVMODEA dm = di.dm;

        dm.dmPosition.x += offsetX;
        dm.dmPosition.y += offsetY;
        dm.dmFields |= DM_POSITION;

        DWORD flags = CDS_UPDATEREGISTRY | CDS_NORESET;
        if (idx == targetIdx)
            flags |= CDS_SET_PRIMARY;

        printf("  %s %s: %lux%lu @ %luHz  pos(%ld,%ld) -> pos(%ld,%ld)\n",
               (idx == targetIdx) ? ">>" : "  ",
               di.name,
               dm.dmPelsWidth, dm.dmPelsHeight,
               dm.dmDisplayFrequency,
               di.dm.dmPosition.x, di.dm.dmPosition.y,
               dm.dmPosition.x, dm.dmPosition.y);

        LONG ret = ChangeDisplaySettingsExA(di.name, &dm, NULL, flags, NULL);
        if (ret != DISP_CHANGE_SUCCESSFUL) {
            printf("     ** FAILED (code %ld)\n", ret);
            return false;
        }
        return true;
    };

    bool ok = stageDisplay(targetIdx);
    for (int i = 0; i < g_displayCount; i++) {
        if (i == targetIdx) continue;
        if (!stageDisplay(i)) ok = false;
    }

    if (!ok)
        printf("\nWarning: some displays failed staging. Attempting commit...\n");

    LONG ret = ChangeDisplaySettingsExA(NULL, NULL, NULL, 0, NULL);
    if (ret == DISP_CHANGE_SUCCESSFUL) {
        printf("\nDone. Primary is now [%d] %s\n", targetIdx + 1, g_displays[targetIdx].name);
        return 0;
    } else {
        printf("\nError: commit failed (code %ld).\n", ret);
        return 1;
    }
}

// ------------------------------------------------------------------
// Print usage
// ------------------------------------------------------------------
void PrintUsage(const char* exe)
{
    printf("%s - Switch the primary display on Windows\n\n", exe);
    printf("Usage:\n");
    printf("  %s --list                  List all active displays\n", exe);
    printf("  %s --toggle                Toggle primary between two displays\n", exe);
    printf("  %s --set-primary <index>   Set primary by display number (1, 2, ...)\n", exe);
    printf("  %s --set-primary <name>    Set primary by device name (\\\\.\\DISPLAY2)\n", exe);
    printf("\nExamples:\n");
    printf("  %s --list\n", exe);
    printf("  %s --toggle\n", exe);
    printf("  %s --set-primary 2\n", exe);
    printf("  %s --set-primary \\\\.\\DISPLAY2\n", exe);
}

// ------------------------------------------------------------------
// Main
// ------------------------------------------------------------------
int main(int argc, char* argv[])
{
    // Trim path to just filename (e.g. "C:\Users\x\foo.exe" -> "foo.exe")
    const char* exeName = argv[0];
    const char* slash = strrchr(exeName, '\\');
    if (slash) exeName = slash + 1;
    slash = strrchr(exeName, '/');
    if (slash) exeName = slash + 1;

    if (argc < 2) {
        PrintUsage(exeName);
        return 1;
    }

    EnumerateDisplays();

    if (g_displayCount == 0) {
        printf("Error: no active displays found.\n");
        return 1;
    }

    // --list
    if (_stricmp(argv[1], "--list") == 0) {
        PrintDisplays();
        return 0;
    }

    // --toggle
    if (_stricmp(argv[1], "--toggle") == 0) {
        if (g_displayCount != 2) {
            printf("Error: --toggle requires exactly 2 active displays (found %d).\n", g_displayCount);
            PrintDisplays();
            return 1;
        }

        int targetIdx = g_displays[0].isPrimary ? 1 : 0;
        PrintDisplays();
        return SwitchPrimary(targetIdx);
    }

    // --set-primary <index|name>
    if (_stricmp(argv[1], "--set-primary") == 0) {
        if (argc < 3) {
            printf("Error: --set-primary requires a display number or device name.\n\n");
            PrintDisplays();
            return 1;
        }

        const char* arg = argv[2];
        int targetIdx = -1;

        // Try as a 1-based numeric index first
        int num = atoi(arg);
        if (num >= 1 && num <= g_displayCount) {
            targetIdx = num - 1;
        } else {
            // Try as a device name (e.g. \\.\DISPLAY2)
            for (int i = 0; i < g_displayCount; i++) {
                if (_stricmp(g_displays[i].name, arg) == 0) {
                    targetIdx = i;
                    break;
                }
            }
        }

        if (targetIdx < 0) {
            printf("Error: '%s' is not a valid display number or device name.\n\n", arg);
            PrintDisplays();
            return 1;
        }

        PrintDisplays();
        return SwitchPrimary(targetIdx);
    }

    printf("Unknown option: %s\n\n", argv[1]);
    PrintUsage(exeName);
    return 1;
}