#include <stdio.h>
#include <string.h>
#include <libpad.h>

#include "types.h"
#include "font.h"
#include "poly.h"
#include "uiNetwork.h"
#include "uiVideo.h"
#include "mainloop_bgm.h"
#include "mainloop_smb.h"
#include "mainloop_ui.h"

/* The original iaddis Host/NetPlay screen was never a general remote ROM
   filesystem.  Keep its convenient tab and IP editor, but make the screen
   configure the read-only SMB browser that users actually need. */

/* OPL-style virtual keyboard: character pages shown as a 12x4 grid.
   The last two cells of every page are the SP (space) and DEL keys. */
#define SMBKB_COLS   12
#define SMBKB_ROWS   4
#define SMBKB_CELLS  (SMBKB_COLS * SMBKB_ROWS)
#define SMBKB_SP     (SMBKB_CELLS - 2)
#define SMBKB_DEL    (SMBKB_CELLS - 1)
#define SMBKB_PAGES  2

static const char kSmbKbPages[SMBKB_PAGES][SMBKB_CELLS - 1] = {
    "1234567890-_"  /* 12 */
    "qwertyuiop[]"  /* 12 */
    "asdfghjkl;'."  /* 12 */
    "zxcvbnm,\\/",  /* 10 */

    "!@#$%^&*()+"   /* 12 */
    "QWERTYUIOP{}"  /* 12 */
    "ASDFGHJKL:\"~" /* 12 */
    "ZXCVBNM<>?"    /* 10 */
};

static void SmbCenter(int x, int y, const char *text)
{
    FontPuts(x - FontGetStrWidth(text) / 2, y, text);
}

static void SmbHeader(int y, const char *text)
{
    PolyColor4f(0.0f, 0.2f, 0.2f, 0.5f);
    PolyRect(32, y, 192, 9);
    FontColor4f(0.0f, 0.8f, 0.8f, 1.0f);
    SmbCenter(128, y, text);
}

static void SmbRow(int y, int index, int selected,
                   const char *label, const char *value)
{
    if (index == selected)
    {
        PolyColor4f(0.0f, 0.5f, 0.0f, 0.5f);
        PolyRect(44, y - 1, 168, FontGetHeight() + 2);
    }
    FontColor4f(0.55f, 0.55f, 0.55f, 1.0f);
    FontPuts(50, y, label);
    FontColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    FontPuts(126, y, value);
}

static void SmbAction(int y, int index, int selected, const char *text)
{
    if (index == selected)
    {
        PolyColor4f(0.0f, 0.5f, 0.0f, 0.5f);
        PolyRect(64, y - 1, 128, FontGetHeight() + 2);
    }
    FontColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    SmbCenter(128, y, text);
}

CNetworkScreen::CNetworkScreen()
{
    m_iSelect = 0;
    m_iDigitIP = -1;
    m_iEditField = -1;
    m_iTextCursor = 0;
    m_iKbPage = 0;
    m_iKbCell = 0;
    m_bLoaded = FALSE;
    SmbConfigDefaults(&m_Config);
    SetEditIP(m_Config.serverIp);
}

void CNetworkScreen::Process()
{
}

void CNetworkScreen::LoadConfig()
{
    if (m_bLoaded)
        return;
    BgmIOBegin();
    if (SmbLoadCurrentConfig(&m_Config) < 0)
        SmbConfigDefaults(&m_Config);
    BgmIOEnd();
    SetEditIP(m_Config.serverIp);
    m_bLoaded = TRUE;
}

void CNetworkScreen::SetEditIP(const char *address)
{
    unsigned int octet[4] = {192, 168, 0, 2};
    int i;

    if (address)
        sscanf(address, "%u.%u.%u.%u", &octet[0], &octet[1],
               &octet[2], &octet[3]);
    for (i = 0; i < 4; ++i)
    {
        if (octet[i] > 255)
            octet[i] = 0;
        m_NetworkIP[i * 3 + 0] = (octet[i] / 100) % 10;
        m_NetworkIP[i * 3 + 1] = (octet[i] / 10) % 10;
        m_NetworkIP[i * 3 + 2] = octet[i] % 10;
    }
}

void CNetworkScreen::CommitEditIP()
{
    unsigned int a = ((unsigned int)GetOctet(0)) & 255;
    unsigned int b = ((unsigned int)GetOctet(1)) & 255;
    unsigned int c = ((unsigned int)GetOctet(2)) & 255;
    unsigned int d = ((unsigned int)GetOctet(3)) & 255;
    snprintf(m_Config.serverIp, sizeof(m_Config.serverIp), "%u.%u.%u.%u",
             a, b, c, d);
}

int CNetworkScreen::GetOctet(int index) const
{
    int base = index * 3;
    return m_NetworkIP[base] * 100 + m_NetworkIP[base + 1] * 10 +
           m_NetworkIP[base + 2];
}

char *CNetworkScreen::GetEditText(int field, int *maxLength)
{
    if (field == 2)
    {
        *maxLength = 40;
        return m_Config.share;
    }
    if (field == 3)
    {
        *maxLength = 32;
        return m_Config.user;
    }
    *maxLength = 32;
    return m_Config.password;
}

void CNetworkScreen::BeginTextEdit(int field)
{
    char *text;
    int maxLength;

    m_iEditField = field;
    text = GetEditText(field, &maxLength);
    (void)maxLength;
    m_iTextCursor = strlen(text);
    m_iKbCell = 0;
}

void CNetworkScreen::InputIP(Uint32 trigger)
{
    int base;
    int octet;

    if (trigger & PAD_LEFT)
    {
        --m_iDigitIP;
        if (m_iDigitIP < 0) m_iDigitIP = 3;
    }
    if (trigger & PAD_RIGHT)
    {
        ++m_iDigitIP;
        if (m_iDigitIP > 3) m_iDigitIP = 0;
    }
    if (trigger & (PAD_UP | PAD_DOWN))
    {
        base = m_iDigitIP * 3;
        octet = GetOctet(m_iDigitIP);
        if (trigger & PAD_UP) octet = (octet + 1) & 255;
        else                  octet = (octet + 255) & 255;
        m_NetworkIP[base] = (octet / 100) % 10;
        m_NetworkIP[base + 1] = (octet / 10) % 10;
        m_NetworkIP[base + 2] = octet % 10;
    }
    if (trigger & (PAD_CROSS | PAD_TRIANGLE | PAD_START))
    {
        CommitEditIP();
        m_iDigitIP = -1;
    }
}

void CNetworkScreen::InputText(Uint32 trigger)
{
    char *text;
    int maxLength;
    int length;
    int row;
    int col;

    text = GetEditText(m_iEditField, &maxLength);

    /* Triangle switches between the character pages. */
    if (trigger & PAD_TRIANGLE)
        m_iKbPage = (m_iKbPage + 1) % SMBKB_PAGES;

    row = m_iKbCell / SMBKB_COLS;
    col = m_iKbCell % SMBKB_COLS;
    if (trigger & PAD_LEFT)
    {
        --col;
        if (col < 0) col = SMBKB_COLS - 1;
    }
    if (trigger & PAD_RIGHT)
    {
        ++col;
        if (col >= SMBKB_COLS) col = 0;
    }
    if (trigger & PAD_UP)
    {
        --row;
        if (row < 0) row = SMBKB_ROWS - 1;
    }
    if (trigger & PAD_DOWN)
    {
        ++row;
        if (row >= SMBKB_ROWS) row = 0;
    }
    m_iKbCell = row * SMBKB_COLS + col;

    if (trigger & (PAD_CROSS | PAD_SQUARE))
    {
        int wantDelete = (trigger & PAD_SQUARE) || m_iKbCell == SMBKB_DEL;

        length = strlen(text);
        if (wantDelete)
        {
            if (length > 0)
                text[length - 1] = '\0';
        }
        else if (length < maxLength)
        {
            text[length] = (m_iKbCell == SMBKB_SP)
                ? ' '
                : kSmbKbPages[m_iKbPage][m_iKbCell];
            text[length + 1] = '\0';
        }
        m_iTextCursor = strlen(text);
    }

    if (trigger & PAD_START)
    {
        if (m_iEditField == 2 && !m_Config.share[0])
            strcpy(m_Config.share, "ROMS");
        if (m_iEditField == 3 && !m_Config.user[0])
            strcpy(m_Config.user, "GUEST");
        m_iEditField = -1;
    }
}

void CNetworkScreen::Input(Uint32 buttons, Uint32 trigger)
{
    (void)buttons;
    LoadConfig();

    if (m_iDigitIP >= 0)
    {
        InputIP(trigger);
        return;
    }
    if (m_iEditField >= 0)
    {
        InputText(trigger);
        return;
    }

    if (trigger & PAD_UP)
    {
        --m_iSelect;
        if (m_iSelect < 0) m_iSelect = 6;
    }
    if (trigger & PAD_DOWN)
    {
        ++m_iSelect;
        if (m_iSelect > 6) m_iSelect = 0;
    }

    if ((trigger & (PAD_LEFT | PAD_RIGHT)) && m_iSelect == 1)
        m_Config.serverPort = (m_Config.serverPort == 445) ? 139 : 445;

    if (trigger & PAD_SQUARE)
    {
        if (m_iSelect == 0)
        {
            strcpy(m_Config.serverIp, "192.168.0.2");
            SetEditIP(m_Config.serverIp);
        }
        else if (m_iSelect == 1) m_Config.serverPort = 445;
        else if (m_iSelect == 2) strcpy(m_Config.share, "ROMS");
        else if (m_iSelect == 3) strcpy(m_Config.user, "GUEST");
        else if (m_iSelect == 4) m_Config.password[0] = '\0';
    }

    if (trigger & (PAD_CROSS | PAD_START))
    {
        if (m_iSelect == 0)
            m_iDigitIP = 0;
        else if (m_iSelect >= 2 && m_iSelect <= 4)
            BeginTextEdit(m_iSelect);
        else if (m_iSelect == 1)
            m_Config.serverPort = (m_Config.serverPort == 445) ? 139 : 445;
        else if (m_iSelect == 5)
        {
            CommitEditIP();
            MainLoopModalPrintf(1, "SMB:\xe4\xbf\x9d\xe5\xad\x98\xe9\x85\x8d\xe7\xbd\xae\xe4\xb8\xad...");
            if (SmbSaveAndConnect(&m_Config) == 0)
                MainLoopModalPrintf(60 * 2, "SMB:\xe5\xb7\xb2\xe8\xbf\x9e\xe6\x8e\xa5\n%s",
                                    SmbGetConfigPath());
            else
                MainLoopModalPrintf(60 * 3, "SMB: %s (error %d)",
                                    SmbGetStatusText(), SmbGetLastError());
            VideoSettingsSave();
        }
        else if (m_iSelect == 6)
        {
            BgmIOBegin();
            SmbDisconnect();
            BgmIOEnd();
            MainLoopModalPrintf(60, "SMB:\xe5\xb7\xb2\xe6\x96\xad\xe5\xbc\x80");
        }
    }

    /* Circle reloads the saved values without attempting a connection. */
    if (trigger & PAD_CIRCLE)
    {
        m_bLoaded = FALSE;
        LoadConfig();
    }
}

void CNetworkScreen::DrawIP(int x, int y)
{
    char part[4][8];
    int i;
    int cursor = x;

    for (i = 0; i < 4; ++i)
        snprintf(part[i], sizeof(part[i]), "%d", GetOctet(i));

    for (i = 0; i < 4; ++i)
    {
        if (m_iDigitIP == i)
        {
            PolyColor4f(0.0f, 0.7f, 0.0f, 0.7f);
            PolyRect(cursor - 1, y - 1, FontGetStrWidth(part[i]) + 2,
                     FontGetHeight() + 2);
        }
        FontPuts(cursor, y, part[i]);
        cursor += FontGetStrWidth(part[i]);
        if (i != 3)
        {
            FontPuts(cursor, y, ".");
            cursor += FontGetStrWidth(".");
        }
    }
}

void CNetworkScreen::DrawKeyboard(int y)
{
    static const char *kLabels[5] = {
        "", "",
        "\xe5\x85\xb1\xe4\xba\xab\xe5\x90\x8d",  /* 共享名 */
        "\xe7\x94\xa8\xe6\x88\xb7\xe5\x90\x8d",  /* 用户名 */
        "\xe5\xaf\x86\xe7\xa0\x81"               /* 密码 */
    };
    char display[80];
    char pageLabel[24];
    char *text;
    int maxLength;
    int row;
    int col;

    text = GetEditText(m_iEditField, &maxLength);
    m_iTextCursor = strlen(text);
    BuildDisplayText(display, sizeof(display), text, m_iEditField == 4, 1);
    snprintf(pageLabel, sizeof(pageLabel), "%d/%d %s", m_iKbPage + 1,
             SMBKB_PAGES, m_iKbPage ? "ABC" : "abc");

    FontColor4f(0.55f, 0.55f, 0.55f, 1.0f);
    FontPuts(12, y, kLabels[m_iEditField]);
    FontColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    FontPuts(76, y, display);
    FontColor4f(0.3f, 0.8f, 0.8f, 1.0f);
    FontPuts(244 - FontGetStrWidth(pageLabel), y, pageLabel);

    y += 13;
    for (row = 0; row < SMBKB_ROWS; ++row)
    {
        int cy = y + row * 12;
        for (col = 0; col < SMBKB_COLS; ++col)
        {
            int cell = row * SMBKB_COLS + col;
            int cx = 8 + col * 20;
            char label[2] = { 0, 0 };
            const char *draw;

            if (cell == SMBKB_SP)       draw = "SP";
            else if (cell == SMBKB_DEL) draw = "DEL";
            else
            {
                label[0] = kSmbKbPages[m_iKbPage][cell];
                draw = label;
            }
            if (cell == m_iKbCell)
            {
                PolyColor4f(0.0f, 0.5f, 0.0f, 0.5f);
                PolyRect(cx, cy, 19, 11);
            }
            FontColor4f(1.0f, 1.0f, 1.0f, 1.0f);
            FontPuts(cx + (19 - FontGetStrWidth(draw)) / 2, cy + 2, draw);
        }
    }
}

void CNetworkScreen::BuildDisplayText(char *output, int outputSize,
                                      const char *text, int password,
                                      int editing)
{
    int i;
    int out = 0;
    int length = strlen(text);
    int start = 0;

    if (editing && m_iTextCursor > 12)
        start = m_iTextCursor - 12;

    for (i = start; i < length && out < outputSize - 4 && i < start + 14; ++i)
    {
        if (editing && i == m_iTextCursor) output[out++] = '[';
        output[out++] = password ? '*' : text[i];
        if (editing && i == m_iTextCursor) output[out++] = ']';
    }
    if (editing && m_iTextCursor == length && out < outputSize - 4)
    {
        output[out++] = '[';
        output[out++] = '_';
        output[out++] = ']';
    }
    output[out] = '\0';
    if (!output[0]) strcpy(output, password ? "\xe8\xae\xbf\xe5\xae\xa2" : "(\xe7\xa9\xba)");
}

void CNetworkScreen::Draw()
{
    char port[16];
    char share[80];
    char user[80];
    char password[80];
    char pathDisplay[48];
    const char *path;
    int y = 15;

    LoadConfig();
    snprintf(port, sizeof(port), "%d", m_Config.serverPort);
    BuildDisplayText(share, sizeof(share), m_Config.share, 0,
                     m_iEditField == 2);
    BuildDisplayText(user, sizeof(user), m_Config.user, 0,
                     m_iEditField == 3);
    BuildDisplayText(password, sizeof(password), m_Config.password, 1,
                     m_iEditField == 4);

    FontSelect(0);
    SmbHeader(y, "SMB\xe7\xbd\x91\xe7\xbb\x9c");
    y += 15;

    FontColor4f(0.55f, 0.55f, 0.55f, 1.0f);
    FontPuts(50, y, "\xe7\x8a\xb6\xe6\x80\x81");
    FontColor4f(SmbIsMounted() ? 0.3f : 1.0f,
                SmbIsMounted() ? 1.0f : 0.85f, 0.3f, 1.0f);
    FontPuts(126, y, SmbGetStatusText());
    y += 15;

    SmbHeader(y, "\xe6\x9c\x8d\xe5\x8a\xa1\xe5\x99\xa8/\xe5\x85\xb1\xe4\xba\xab");
    y += 13;
    SmbRow(y, 0, m_iSelect, "\xe6\x9c\x8d\xe5\x8a\xa1\xe5\x99\xa8IP", "");
    FontColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    DrawIP(126, y); y += 12;
    SmbRow(y, 1, m_iSelect, "\xe7\xab\xaf\xe5\x8f\xa3", port); y += 12;
    SmbRow(y, 2, m_iSelect, "\xe5\x85\xb1\xe4\xba\xab\xe5\x90\x8d", share); y += 12;
    SmbRow(y, 3, m_iSelect, "\xe7\x94\xa8\xe6\x88\xb7\xe5\x90\x8d", user); y += 12;
    SmbRow(y, 4, m_iSelect, "\xe5\xaf\x86\xe7\xa0\x81", password); y += 15;

    if (m_iEditField >= 0)
    {
        DrawKeyboard(y);

        y = 190;
        FontColor4f(0.6f, 0.6f, 0.6f, 1.0f);
        SmbCenter(128, y,
            "X:\xe8\xbe\x93\xe5\x85\xa5  \xe2\x96\xa1:\xe5\x88\xa0\xe9\x99\xa4  \xe2\x96\xb3:\xe6\x8d\xa2\xe9\xa1\xb5  START:\xe5\xae\x8c\xe6\x88\x90");
    }
    else
    {
        SmbHeader(y, "\xe6\x93\x8d\xe4\xbd\x9c"); y += 13;
        SmbAction(y, 5, m_iSelect, "\xe4\xbf\x9d\xe5\xad\x98\xe5\xb9\xb6\xe8\xbf\x9e\xe6\x8e\xa5"); y += 12;
        SmbAction(y, 6, m_iSelect, "\xe6\x96\xad\xe5\xbc\x80\xe8\xbf\x9e\xe6\x8e\xa5");

        y = 183;
        FontColor4f(0.6f, 0.6f, 0.6f, 1.0f);
        if (m_iDigitIP >= 0)
        {
            SmbCenter(128, y, "\xe5\xb7\xa6/\xe5\x8f\xb3:\xe4\xbd\x8d  \xe4\xb8\x8a/\xe4\xb8\x8b:\xe6\x94\xb9\xe5\x80\xbc"); y += 11;
            SmbCenter(128, y, "X/Triangle: done");
        }
        else
        {
            SmbCenter(128, y, "X:\xe7\xbc\x96\xe8\xbe\x91/\xe9\x80\x89\xe6\x8b\xa9  \xe2\x96\xa1:\xe9\x87\x8d\xe7\xbd\xae"); y += 11;
            SmbCenter(128, y, "\xe2\x97\x8b:\xe9\x87\x8d\xe6\x96\xb0\xe5\x8a\xa0\xe8\xbd\xbd\xe9\x85\x8d\xe7\xbd\xae");
        }
    }

    path = SmbGetConfigPath();
    if (path && path[0])
    {
        size_t pathLength = strlen(path);
        if (pathLength < sizeof(pathDisplay))
            strcpy(pathDisplay, path);
        else
        {
            strcpy(pathDisplay, "...");
            strncpy(pathDisplay + 3,
                    path + pathLength - (sizeof(pathDisplay) - 4),
                    sizeof(pathDisplay) - 4);
            pathDisplay[sizeof(pathDisplay) - 1] = '\0';
        }
        FontSelect(2);
        FontColor4f(0.35f, 0.65f, 0.65f, 1.0f);
        FontPuts(8, 207, pathDisplay);
    }
}
