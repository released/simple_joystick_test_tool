#include "device_bar.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <commctrl.h>
#include <functional>
#include <sstream>

#pragma comment(lib, "msimg32.lib")

namespace {

const int kComboId = 2001;
const int kScanId = 2002;
const int kConnectId = 2003;
const int kAutoReconnectId = 2004;
const int kCopyId = 2005;
const int kShowAllId = 2006;

unsigned short PayloadLength(unsigned short report_length) {
    if (report_length == 0) {
        return 0;
    }
    return report_length > 0 ? static_cast<unsigned short>(report_length - 1) : 0;
}

bool IsJoystickDevice(const HidScanInfo& info) {
    return (info.usage_page == 0x0001) && (info.usage == 0x0004 || info.usage == 0x0005);
}

std::wstring FormatDeviceLabel(const HidScanInfo& info) {
    std::wstringstream ss;
    ss << std::uppercase << std::hex;
    ss.width(4);
    ss.fill(L'0');
    ss << info.vendor_id;
    ss << L":";
    ss.width(4);
    ss.fill(L'0');
    ss << info.product_id;
    ss << std::dec;
    ss << L"  IN:" << PayloadLength(info.input_report_length);
    ss << L" OUT:" << PayloadLength(info.output_report_length);
    std::wstring name;
    if (!info.manufacturer.empty() && !info.product.empty()) {
        name = info.manufacturer + L" - " + info.product;
    } else if (!info.product.empty()) {
        name = info.product;
    } else if (!info.manufacturer.empty()) {
        name = info.manufacturer;
    }
    if (!name.empty()) {
        ss << L"  " << name;
    }
    return ss.str();
}

}

BEGIN_MESSAGE_MAP(CDeviceBar, CWnd)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_BN_CLICKED(kScanId, &CDeviceBar::OnScanClicked)
    ON_BN_CLICKED(kConnectId, &CDeviceBar::OnConnectClicked)
    ON_BN_CLICKED(kCopyId, &CDeviceBar::OnCopyClicked)
    ON_BN_CLICKED(kShowAllId, &CDeviceBar::OnShowAllClicked)
    ON_CBN_SELCHANGE(kComboId, &CDeviceBar::OnComboChanged)
    ON_WM_CTLCOLOR()
    ON_WM_DRAWITEM()
END_MESSAGE_MAP()

bool CDeviceBar::Create(CWnd* parent) {
    CString class_name = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW, ::LoadCursor(nullptr, IDC_ARROW),
                                            reinterpret_cast<HBRUSH>(COLOR_3DFACE + 1), nullptr);
    return CWnd::CreateEx(0, class_name, L"DeviceBar",
                          WS_CHILD | WS_VISIBLE | WS_BORDER | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                          CRect(0, 0, 0, 0), parent, 0);
}

int CDeviceBar::OnCreate(LPCREATESTRUCT lpCreateStruct) {
    if (CWnd::OnCreate(lpCreateStruct) == -1) {
        return -1;
    }

    m_connected = false;
    m_connectionState = StateDisconnected;
    m_reportId = 0x00;
    if (!m_vidPidHighlightBrush.GetSafeHandle()) {
        m_vidPidHighlightBrush.CreateSolidBrush(RGB(55, 65, 81));
    }

    CClientDC dc(this);
    int font_height = -MulDiv(9, dc.GetDeviceCaps(LOGPIXELSY), 72);
    m_uiFont.CreateFontW(font_height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, 0, DEFAULT_CHARSET,
                         OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                         DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    m_deviceCombo.Create(WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                         CRect(0, 0, 0, 0), this, kComboId);

    m_scanButton.Create(L"Scan Device", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                        CRect(0, 0, 0, 0), this, kScanId);

    m_connectButton.Create(L"Connect", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW,
                           CRect(0, 0, 0, 0), this, kConnectId);

    m_autoReconnectCheck.Create(L"Auto reconnect", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                CRect(0, 0, 0, 0), this, kAutoReconnectId);
    m_showAllCheck.Create(L"Show All HID", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                          CRect(0, 0, 0, 0), this, kShowAllId);

    m_capTitle.Create(L"Device Capability:", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this);
    m_capVidPid.Create(L"VID:PID        : 0x---- : 0x----", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this);
    m_capUsage.Create(L"UsagePage/Usage: 0x---- / 0x----", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this);
    m_capReports.Create(L"Reports        : IN=--  OUT=--  FEAT=--  ReportID=0x00", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this);

    m_statusLabel.Create(L"Status: Disconnected", WS_CHILD | WS_VISIBLE | SS_RIGHT, CRect(0, 0, 0, 0), this);
    m_copyButton.Create(L"Copy Device Info", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                        CRect(0, 0, 0, 0), this, kCopyId);

    CWnd* controls[] = {
        &m_deviceCombo, &m_scanButton, &m_connectButton, &m_autoReconnectCheck, &m_showAllCheck,
        &m_capTitle, &m_capVidPid, &m_capUsage, &m_capReports,
        &m_statusLabel, &m_copyButton
    };
    for (CWnd* control : controls) {
        control->SetFont(&m_uiFont);
    }
    m_statusLabel.ShowWindow(SW_HIDE);

    auto set_icon = [](CButton& button, CImageList& list, HICON icon) {
        list.Create(16, 16, ILC_COLOR32 | ILC_MASK, 1, 1);
        list.Add(icon);
        BUTTON_IMAGELIST bi = {};
        bi.himl = list.GetSafeHandle();
        bi.uAlign = BUTTON_IMAGELIST_ALIGN_LEFT;
        bi.margin.left = 2;
        bi.margin.right = 2;
        ::SendMessageW(button.GetSafeHwnd(), BCM_SETIMAGELIST, 0, reinterpret_cast<LPARAM>(&bi));
    };

    set_icon(m_scanButton, m_iconScan, LoadIconW(nullptr, IDI_INFORMATION));
    set_icon(m_connectButton, m_iconConnect, LoadIconW(nullptr, IDI_SHIELD));
    set_icon(m_copyButton, m_iconCopy, LoadIconW(nullptr, IDI_INFORMATION));

    m_tooltip.Create(this, TTS_ALWAYSTIP);
    m_tooltip.SetMaxTipWidth(300);
    m_tooltip.AddTool(&m_deviceCombo, L"Select joystick/gamepad HID device");
    m_tooltip.AddTool(&m_scanButton, L"Scan HID joystick/gamepad devices");
    m_tooltip.AddTool(&m_connectButton, L"Connect / Disconnect selected device");
    m_tooltip.AddTool(&m_autoReconnectCheck, L"Auto reconnect when device returns");
    m_tooltip.AddTool(&m_showAllCheck, L"Show all HID devices (disable joystick/gamepad filter)");
    m_tooltip.AddTool(&m_copyButton, L"Copy device capability info to clipboard");

    UpdateStatusText();

    return 0;
}

void CDeviceBar::OnSize(UINT nType, int cx, int cy) {
    CWnd::OnSize(nType, cx, cy);
    LayoutControls(cx, cy);
}

void CDeviceBar::LayoutControls(int cx, int cy) {
    (void)cy;
    const int margin = 8;
    const int gap = 6;
    const int row_height = 26;
    const int button_width = 110;
    const int auto_width = 130;
    const int show_all_width = 120;
    const int status_width = 160;
    const int text_height = 22;

    int width = cx - margin * 2;
    if (width < 0) {
        width = 0;
    }

    int y = margin + 2;

    int top_width = width - status_width - gap;
    if (top_width < 0) {
        top_width = 0;
    }

    int fixed = button_width * 2 + auto_width + show_all_width + gap * 4;
    int combo_width = top_width - fixed;
    int desired = top_width / 2;
    if (combo_width > desired) {
        combo_width = desired;
    }
    if (combo_width < 200) {
        combo_width = 200;
    }
    if (combo_width > top_width - fixed) {
        combo_width = top_width - fixed;
    }
    if (combo_width < 120) {
        combo_width = 120;
    }
    if (combo_width > top_width - fixed && top_width > fixed) {
        combo_width = top_width - fixed;
    }

    int x = margin;
    m_deviceCombo.MoveWindow(x, y, combo_width, row_height);
    x += combo_width + gap;
    m_scanButton.MoveWindow(x, y, button_width, row_height);
    x += button_width + gap;
    m_connectButton.MoveWindow(x, y, button_width, row_height);
    x += button_width + gap;
    m_autoReconnectCheck.MoveWindow(x, y + 2, auto_width, row_height);
    x += auto_width + gap;
    m_showAllCheck.MoveWindow(x, y + 2, show_all_width, row_height);

    int status_x = margin + width - status_width;
    if (status_x < margin) {
        status_x = margin;
    }
    m_statusLabel.MoveWindow(status_x + 16, y + 4, status_width - 16, text_height);

    int y2 = y + row_height + gap;
    const int copy_width = 160;
    const int report_margin_px = 10;
    int required_cap_width = 0;
    {
        CClientDC dc(this);
        CFont* old_font = dc.SelectObject(&m_uiFont);
        required_cap_width = dc.GetTextExtent(L"Reports        : IN=64  OUT=64  FEAT=0  ReportID=0x00").cx + report_margin_px;
        dc.SelectObject(old_font);
    }

    int max_left_width = width - copy_width - gap;
    if (max_left_width < 180) {
        max_left_width = width;
    }

    int left_width = required_cap_width;
    if (left_width > max_left_width) {
        left_width = max_left_width;
    }

    int copy_x = margin + left_width + gap;
    if (copy_x + copy_width > margin + width) {
        copy_x = margin + width - copy_width;
    }
    if (copy_x < margin) {
        copy_x = margin;
    }

    const int line_gap = text_height + 6;
    m_capTitle.MoveWindow(margin, y2, left_width, text_height);
    m_capVidPid.MoveWindow(margin, y2 + line_gap, left_width, text_height);
    m_capUsage.MoveWindow(margin, y2 + line_gap * 2, left_width, text_height);
    m_capReports.MoveWindow(margin, y2 + line_gap * 3, left_width, text_height);

    m_copyButton.MoveWindow(copy_x, y2, copy_width, row_height);
}

void CDeviceBar::OnPaint() {
    CPaintDC paint_dc(this);
    CRect rect;
    GetClientRect(&rect);
    if (rect.Width() <= 0 || rect.Height() <= 0) {
        return;
    }

    CDC dc;
    dc.CreateCompatibleDC(&paint_dc);
    CBitmap back_buffer;
    back_buffer.CreateCompatibleBitmap(&paint_dc, rect.Width(), rect.Height());
    CBitmap* old_bitmap = dc.SelectObject(&back_buffer);

    TRIVERTEX verts[2] = {};
    verts[0].x = rect.left;
    verts[0].y = rect.top;
    verts[0].Red = static_cast<COLOR16>(248 << 8);
    verts[0].Green = static_cast<COLOR16>(250 << 8);
    verts[0].Blue = static_cast<COLOR16>(252 << 8);
    verts[1].x = rect.right;
    verts[1].y = rect.bottom;
    verts[1].Red = static_cast<COLOR16>(226 << 8);
    verts[1].Green = static_cast<COLOR16>(232 << 8);
    verts[1].Blue = static_cast<COLOR16>(240 << 8);
    GRADIENT_RECT grad = { 0, 1 };
    ::GradientFill(dc.GetSafeHdc(), verts, 2, &grad, 1, GRADIENT_FILL_RECT_V);

    dc.DrawEdge(&rect, EDGE_SUNKEN, BF_RECT);

    dc.SetBkMode(TRANSPARENT);
    dc.SelectObject(&m_uiFont);
    dc.SetTextColor(RGB(40, 40, 40));

    CRect statusRect;
    m_statusLabel.GetWindowRect(&statusRect);
    ScreenToClient(&statusRect);
    dc.SelectObject(&m_uiFont);
    CSize textSize = dc.GetTextExtent(m_statusText.c_str());
    const int dot_size = 8;
    const int dot_gap = 4;
    int dot_x = statusRect.left - 5;
    int dot_y = statusRect.top + (statusRect.Height() - dot_size) / 2;
    COLORREF dot_color = RGB(148, 163, 184);
    switch (m_connectionState) {
    case StateConnected:
        dot_color = RGB(34, 197, 94);
        break;
    case StateRxActive:
        dot_color = RGB(56, 189, 248);
        break;
    case StateRxTimeout:
        dot_color = RGB(249, 115, 22);
        break;
    default:
        dot_color = RGB(148, 163, 184);
        break;
    }
    CBrush brush(dot_color);
    CBrush* old_brush = dc.SelectObject(&brush);
    dc.Ellipse(dot_x, dot_y, dot_x + dot_size, dot_y + dot_size);
    dc.SelectObject(old_brush);

    dc.SetTextColor(RGB(30, 30, 35));
    CRect text_rect = statusRect;
    text_rect.left = dot_x + dot_size + dot_gap;
    dc.DrawText(m_statusText.c_str(), text_rect, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    paint_dc.BitBlt(0, 0, rect.Width(), rect.Height(), &dc, 0, 0, SRCCOPY);
    dc.SelectObject(old_bitmap);
}

BOOL CDeviceBar::OnEraseBkgnd(CDC* pDC) {
    (void)pDC;
    return TRUE;
}

BOOL CDeviceBar::PreTranslateMessage(MSG* pMsg) {
    if (m_tooltip.GetSafeHwnd()) {
        m_tooltip.RelayEvent(pMsg);
    }
    return CWnd::PreTranslateMessage(pMsg);
}

HBRUSH CDeviceBar::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor) {
    HBRUSH brush = CWnd::OnCtlColor(pDC, pWnd, nCtlColor);
    if (nCtlColor == CTLCOLOR_STATIC) {
        if (pWnd->GetSafeHwnd() == m_capVidPid.GetSafeHwnd()) {
            pDC->SetTextColor(RGB(241, 245, 249));
            pDC->SetBkColor(RGB(55, 65, 81));
            return m_vidPidHighlightBrush;
        }
        if (pWnd->GetSafeHwnd() == m_statusLabel.GetSafeHwnd()) {
            pDC->SetTextColor(RGB(30, 30, 35));
            pDC->SetBkMode(TRANSPARENT);
            return reinterpret_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
        }
    }
    return brush;
}

void CDeviceBar::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct) {
    if (nIDCtl != kConnectId || !lpDrawItemStruct) {
        CWnd::OnDrawItem(nIDCtl, lpDrawItemStruct);
        return;
    }

    CDC dc;
    dc.Attach(lpDrawItemStruct->hDC);
    CRect rect(lpDrawItemStruct->rcItem);

    COLORREF fill = m_connected ? RGB(210, 80, 80) : RGB(70, 150, 90);
    if (lpDrawItemStruct->itemState & ODS_SELECTED) {
        fill = m_connected ? RGB(190, 70, 70) : RGB(60, 130, 80);
    }
    dc.FillSolidRect(&rect, fill);
    dc.DrawEdge(&rect, (lpDrawItemStruct->itemState & ODS_SELECTED) ? EDGE_SUNKEN : EDGE_RAISED, BF_RECT);

    CString text;
    m_connectButton.GetWindowTextW(text);
    dc.SetBkMode(TRANSPARENT);
    dc.SetTextColor(RGB(255, 255, 255));
    dc.SelectObject(&m_uiFont);
    dc.DrawText(text, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    dc.Detach();
}

void CDeviceBar::ScanDevices() {
    std::wstring error;
    if (!EnumerateHidDevices(&m_devices, &error)) {
        m_devices.clear();
    }

    if (!IsShowAllHidEnabled()) {
        m_devices.erase(
            std::remove_if(m_devices.begin(), m_devices.end(),
                           [](const HidScanInfo& info) { return !IsJoystickDevice(info); }),
            m_devices.end());
    }

    UpdateCombo();
}

void CDeviceBar::UpdateCombo() {
    m_deviceCombo.ResetContent();

    std::sort(m_devices.begin(), m_devices.end(), [](const HidScanInfo& a, const HidScanInfo& b) {
        if (a.vendor_id != b.vendor_id) {
            return a.vendor_id < b.vendor_id;
        }
        if (a.product_id != b.product_id) {
            return a.product_id < b.product_id;
        }
        if (a.usage_page != b.usage_page) {
            return a.usage_page < b.usage_page;
        }
        if (a.usage != b.usage) {
            return a.usage < b.usage;
        }
        if (a.manufacturer != b.manufacturer) {
            return a.manufacturer < b.manufacturer;
        }
        if (a.product != b.product) {
            return a.product < b.product;
        }
        return a.device_path < b.device_path;
    });

    for (const auto& device : m_devices) {
        std::wstring label = FormatDeviceLabel(device);
        m_deviceCombo.AddString(label.c_str());
    }

    if (!m_devices.empty()) {
        m_deviceCombo.SetCurSel(0);
    }
    UpdateCapabilityText();
    NotifyParent(ActionSelectionChanged);

    int dropped_width = 520;
    if (m_deviceCombo.GetSafeHwnd()) {
        m_deviceCombo.SetDroppedWidth(dropped_width);
        int count = m_deviceCombo.GetCount();
        int item_height = m_deviceCombo.GetItemHeight(0);
        int sel_height = m_deviceCombo.GetItemHeight(-1);
        if (item_height <= 0) {
            item_height = 16;
        }
        if (sel_height <= 0) {
            sel_height = 26;
        }
        int visible = count;
        if (visible > 12) {
            visible = 12;
        } else if (visible < 1) {
            visible = 1;
        }
        int drop_height = sel_height + item_height * visible + 4;
        CRect combo_rect;
        m_deviceCombo.GetWindowRect(&combo_rect);
        ScreenToClient(&combo_rect);
        m_deviceCombo.MoveWindow(combo_rect.left, combo_rect.top, combo_rect.Width(), drop_height);
        COMBOBOXINFO info = {};
        info.cbSize = sizeof(COMBOBOXINFO);
        if (GetComboBoxInfo(m_deviceCombo.GetSafeHwnd(), &info)) {
            LONG_PTR style = GetWindowLongPtr(info.hwndList, GWL_STYLE);
            SetWindowLongPtr(info.hwndList, GWL_STYLE, style | WS_HSCROLL);
            ::SendMessageW(info.hwndList, LB_SETHORIZONTALEXTENT, dropped_width, 0);
        }
    }
}

int CDeviceBar::SelectedIndex() const {
    int sel = m_deviceCombo.GetCurSel();
    return sel == CB_ERR ? -1 : sel;
}

bool CDeviceBar::SelectByPath(const std::wstring& path) {
    if (path.empty()) {
        return false;
    }
    for (size_t i = 0; i < m_devices.size(); ++i) {
        if (m_devices[i].device_path == path) {
            m_deviceCombo.SetCurSel(static_cast<int>(i));
            UpdateCapabilityText();
            NotifyParent(ActionSelectionChanged);
            return true;
        }
    }
    return false;
}

bool CDeviceBar::SelectByPathHash(unsigned int path_hash) {
    if (path_hash == 0) {
        return false;
    }
    for (size_t i = 0; i < m_devices.size(); ++i) {
        if (m_devices[i].path_hash == path_hash) {
            m_deviceCombo.SetCurSel(static_cast<int>(i));
            UpdateCapabilityText();
            NotifyParent(ActionSelectionChanged);
            return true;
        }
    }
    return false;
}

bool CDeviceBar::SelectByVidPid(unsigned short vid, unsigned short pid) {
    if (vid == 0 && pid == 0) {
        return false;
    }
    for (size_t i = 0; i < m_devices.size(); ++i) {
        if (m_devices[i].vendor_id == vid && m_devices[i].product_id == pid) {
            m_deviceCombo.SetCurSel(static_cast<int>(i));
            UpdateCapabilityText();
            NotifyParent(ActionSelectionChanged);
            return true;
        }
    }
    return false;
}

void CDeviceBar::OnScanClicked() {
    ScanDevices();
}

void CDeviceBar::OnConnectClicked() {
    NotifyParent(ActionConnectToggle);
}

void CDeviceBar::OnCopyClicked() {
    std::wstring text = BuildDeviceSummary();
    if (text.empty()) {
        return;
    }

    if (!OpenClipboard()) {
        return;
    }
    EmptyClipboard();
    size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL data = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (data) {
        void* ptr = GlobalLock(data);
        if (ptr) {
            memcpy(ptr, text.c_str(), bytes);
            GlobalUnlock(data);
            SetClipboardData(CF_UNICODETEXT, data);
        } else {
            GlobalFree(data);
        }
    }
    CloseClipboard();
}

void CDeviceBar::OnComboChanged() {
    UpdateCapabilityText();
    NotifyParent(ActionSelectionChanged);
}

void CDeviceBar::OnShowAllClicked() {
    unsigned int selected_hash = 0;
    int sel = SelectedIndex();
    if (sel >= 0 && sel < static_cast<int>(m_devices.size())) {
        selected_hash = m_devices[static_cast<size_t>(sel)].path_hash;
    }
    ScanDevices();
    if (selected_hash != 0) {
        SelectByPathHash(selected_hash);
    }
}

void CDeviceBar::NotifyParent(Action action) {
    CWnd* parent = GetParent();
    if (!parent) {
        return;
    }
    int selected = SelectedIndex();
    parent->PostMessage(MessageId(), static_cast<WPARAM>(action), static_cast<LPARAM>(selected));
}

void CDeviceBar::SetConnected(bool connected) {
    m_connected = connected;
    m_connectButton.SetWindowTextW(connected ? L"Disconnect" : L"Connect");
    m_deviceCombo.EnableWindow(!connected);
    m_scanButton.EnableWindow(!connected);
    m_showAllCheck.EnableWindow(!connected);
    m_connectButton.Invalidate();
}

void CDeviceBar::SetConnectionState(int state) {
    if (state == m_connectionState) {
        return;
    }
    m_connectionState = state;
    bool text_changed = UpdateStatusText();

    CRect status_rect;
    if (m_statusLabel.GetSafeHwnd()) {
        m_statusLabel.GetWindowRect(&status_rect);
        ScreenToClient(&status_rect);
        status_rect.left -= 20;
        status_rect.right += 6;
        status_rect.top -= 2;
        status_rect.bottom += 2;
        InvalidateRect(&status_rect, FALSE);
    } else if (text_changed) {
        Invalidate(FALSE);
    }
}

void CDeviceBar::SetReportId(unsigned char report_id) {
    m_reportId = report_id;
    UpdateCapabilityText();
    InvalidateRect(nullptr, FALSE);
}

bool CDeviceBar::IsAutoReconnectEnabled() const {
    return m_autoReconnectCheck.GetCheck() == BST_CHECKED;
}

bool CDeviceBar::IsShowAllHidEnabled() const {
    return m_showAllCheck.GetCheck() == BST_CHECKED;
}

void CDeviceBar::UpdateCapabilityText() {
    int sel = SelectedIndex();
    if (sel < 0 || sel >= static_cast<int>(m_devices.size())) {
        wchar_t buffer[160] = {};
        m_capVidPid.SetWindowTextW(L"VID:PID        : 0x---- : 0x----");
        m_capUsage.SetWindowTextW(L"UsagePage/Usage: 0x---- / 0x----");
        swprintf_s(buffer, L"Reports        : IN=--  OUT=--  FEAT=--  ReportID=0x%02X", m_reportId);
        m_capReports.SetWindowTextW(buffer);
        return;
    }
    const HidScanInfo& info = m_devices[static_cast<size_t>(sel)];
    wchar_t buffer[160] = {};
    swprintf_s(buffer, L"VID:PID        : 0x%04X : 0x%04X", info.vendor_id, info.product_id);
    m_capVidPid.SetWindowTextW(buffer);
    swprintf_s(buffer, L"UsagePage/Usage: 0x%04X / 0x%04X", info.usage_page, info.usage);
    m_capUsage.SetWindowTextW(buffer);

    unsigned short in_len = PayloadLength(info.input_report_length);
    unsigned short out_len = PayloadLength(info.output_report_length);
    unsigned short feat_len = PayloadLength(info.feature_report_length);
    swprintf_s(buffer, L"Reports        : IN=%u  OUT=%u  FEAT=%u  ReportID=0x%02X", in_len, out_len, feat_len, m_reportId);
    m_capReports.SetWindowTextW(buffer);
}


bool CDeviceBar::UpdateStatusText() {
    const wchar_t* text = L"Status: Disconnected";
    switch (m_connectionState) {
    case StateConnected:
        text = L"Status: Connected";
        break;
    case StateRxActive:
        text = L"Status: RX Active";
        break;
    case StateRxTimeout:
        text = L"Status: RX Timeout";
        break;
    default:
        text = L"Status: Disconnected";
        break;
    }
    if (m_statusText == text) {
        return false;
    }
    m_statusText = text;
    m_statusLabel.SetWindowTextW(text);
    return true;
}

std::wstring CDeviceBar::BuildDeviceSummary() const {
    int sel = SelectedIndex();
    if (sel < 0 || sel >= static_cast<int>(m_devices.size())) {
        return L"";
    }
    const HidScanInfo& info = m_devices[static_cast<size_t>(sel)];
    std::wstringstream ss;
    ss << std::uppercase << std::hex;
    ss << L"VID:PID = 0x";
    ss.width(4);
    ss.fill(L'0');
    ss << info.vendor_id;
    ss << L" : 0x";
    ss.width(4);
    ss.fill(L'0');
    ss << info.product_id;
    ss << L"\r\nUsagePage/Usage = 0x";
    ss.width(4);
    ss.fill(L'0');
    ss << info.usage_page;
    ss << L" / 0x";
    ss.width(4);
    ss.fill(L'0');
    ss << info.usage;
    ss << std::dec;
    ss << L"\r\nInputReport  = " << PayloadLength(info.input_report_length);
    ss << L"\r\nOutputReport = " << PayloadLength(info.output_report_length);
    ss << L"\r\nFeatureReport= " << PayloadLength(info.feature_report_length);
    ss << std::uppercase << std::hex;
    ss << L"\r\nPathHash     = ";
    ss.width(8);
    ss.fill(L'0');
    ss << info.path_hash;
    return ss.str();
}
