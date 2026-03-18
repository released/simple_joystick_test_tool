#include "main_frame.h"

#include "resource.h"

#include <afxres.h>
#include <string>
#include <vector>

namespace {

const UINT kIndicators[] = {
    ID_SEPARATOR,
    ID_INDICATOR_ERR,
    ID_INDICATOR_RX,
    ID_INDICATOR_TX,
};

const int kPaneConnection = 0;
const int kPaneError = 1;
const int kPaneRx = 2;
const int kPaneJoy = 3;

const UINT_PTR kRxTimerId = 1;
const UINT_PTR kReconnectTimerId = 2;
const UINT_PTR kStartupScanTimerId = 3;

}

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWnd)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_MESSAGE(CDeviceBar::MessageId(), &CMainFrame::OnDeviceBarAction)
    ON_MESSAGE(CRxPanel::MessageId(), &CMainFrame::OnRxEvent)
    ON_WM_CLOSE()
    ON_WM_TIMER()
END_MESSAGE_MAP()

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs) {
    if (!CFrameWnd::PreCreateWindow(cs)) {
        return FALSE;
    }
    cs.style |= WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
    HICON hIcon = AfxGetApp()->LoadIcon(IDI_APP_ICON);
    cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW, ::LoadCursor(nullptr, IDC_ARROW),
                                       reinterpret_cast<HBRUSH>(COLOR_3DFACE + 1), hIcon);
    return TRUE;
}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) {
    if (CFrameWnd::OnCreate(lpCreateStruct) == -1) {
        return -1;
    }

    if (!m_deviceBar.Create(this)) {
        return -1;
    }
    if (!m_rxPanel.Create(this)) {
        return -1;
    }
    if (!m_statusBar.Create(this)) {
        return -1;
    }

    InitStatusBar();

    HICON hIcon = static_cast<HICON>(::LoadImageW(AfxGetInstanceHandle(), MAKEINTRESOURCEW(IDI_APP_ICON),
                                                 IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR));
    HICON hIconSmall = static_cast<HICON>(::LoadImageW(AfxGetInstanceHandle(), MAKEINTRESOURCEW(IDI_APP_ICON),
                                                      IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));
    if (hIcon) {
        SetIcon(hIcon, TRUE);
    }
    if (hIconSmall) {
        SetIcon(hIconSmall, FALSE);
    }

    m_rxPanel.SetDevice(&m_device);

    m_config.Load();
    ApplyConfig();

    m_deviceBar.SetConnected(false);
    m_deviceBar.SetConnectionState(CDeviceBar::StateDisconnected);
    m_rxPanel.SetControlsEnabled(false);
    m_rxPanel.SetClearEnabled(true);
    m_rxCount = 0;
    m_lastRxTick = 0;

    if (!m_rxStatusTimer) {
        m_rxStatusTimer = SetTimer(kRxTimerId, 200, nullptr);
    }
    if (!m_startupScanTimer) {
        m_startupScanTimer = SetTimer(kStartupScanTimerId, 10, nullptr);
    }

    return 0;
}

void CMainFrame::OnSize(UINT nType, int cx, int cy) {
    CFrameWnd::OnSize(nType, cx, cy);
    LayoutControls(cx, cy);
}

void CMainFrame::LayoutControls(int cx, int cy) {
    if (cx <= 0 || cy <= 0) {
        return;
    }

    const int margin = 8;
    const int gap = 8;
    const int top_height = 170;
    const int status_height = 22;

    int width = cx - margin * 2;
    if (width < 0) {
        width = 0;
    }

    m_statusBar.MoveWindow(0, cy - status_height, cx, status_height);
    LayoutStatusBarPanes();

    int y = margin;
    m_deviceBar.MoveWindow(margin, y, width, top_height);
    y += top_height + gap;

    int rx_height = cy - status_height - margin - y;
    if (rx_height < 0) {
        rx_height = 0;
    }
    m_rxPanel.MoveWindow(margin, y, width, rx_height);
}

LRESULT CMainFrame::OnDeviceBarAction(WPARAM wParam, LPARAM) {
    int action = static_cast<int>(wParam);

    if (action == CDeviceBar::ActionConnectToggle) {
        if (m_device.IsOpen()) {
            HandleDisconnected(true);
            return 0;
        }

        ConnectSelectedDevice();
        return 0;
    }
    if (action == CDeviceBar::ActionSelectionChanged) {
        if (!m_device.IsOpen()) {
            UpdateSelectedDeviceDescriptorPreview();
        }
        return 0;
    }

    return 0;
}

LRESULT CMainFrame::OnRxEvent(WPARAM wParam, LPARAM) {
    int event = static_cast<int>(wParam);
    if (event == CRxPanel::RxCount) {
        ++m_rxCount;
        UpdateRxCount(m_rxCount);
        m_lastRxTick = GetTickCount();
        UpdateRxActivity();

        std::wstring joy_text;
        if (m_rxPanel.GetLatestJoystickSummary(&joy_text)) {
            UpdateJoyStatus(joy_text);
        }
    } else if (event == CRxPanel::RxDeviceRemoved) {
        UpdateErrorStatus(L"Device removed.");
        HandleDisconnected(true);
    }
    return 0;
}

void CMainFrame::OnClose() {
    HandleDisconnected(false);

    if (m_rxStatusTimer) {
        KillTimer(m_rxStatusTimer);
        m_rxStatusTimer = 0;
    }
    if (m_startupScanTimer) {
        KillTimer(m_startupScanTimer);
        m_startupScanTimer = 0;
    }

    SaveConfig();
    CFrameWnd::OnClose();
}

void CMainFrame::InitStatusBar() {
    m_statusBar.SetIndicators(kIndicators, static_cast<int>(sizeof(kIndicators) / sizeof(kIndicators[0])));
    m_statusBar.SetPaneInfo(kPaneConnection, ID_SEPARATOR, SBPS_NORMAL, 0);
    m_statusBar.SetPaneInfo(kPaneError, ID_INDICATOR_ERR, SBPS_NORMAL, 0);
    m_statusBar.SetPaneInfo(kPaneRx, ID_INDICATOR_RX, SBPS_NORMAL, 0);
    m_statusBar.SetPaneInfo(kPaneJoy, ID_INDICATOR_TX, SBPS_NORMAL, 0);
    LayoutStatusBarPanes();

    UpdateConnectionStatus(L"Disconnected");
    UpdateErrorStatus(L"");
    UpdateRxCount(0);
    UpdateJoyStatus(L"N/A");
}

void CMainFrame::LayoutStatusBarPanes() {
    if (!m_statusBar.GetSafeHwnd()) {
        return;
    }

    CRect rect;
    m_statusBar.GetClientRect(&rect);
    int width = rect.Width();
    if (width <= 0) {
        return;
    }

    int conn = 220;
    int rx = 100;
    int err = 180;
    int joy = 0;

    int joy_target = 360;
    {
        CClientDC dc(&m_statusBar);
        CFont* old_font = dc.SelectObject(m_statusBar.GetFont());
        std::wstring joy_text = L"JOY: " + m_lastJoyStatus;
        CSize text_size = dc.GetTextExtent(joy_text.c_str());
        dc.SelectObject(old_font);
        joy_target = text_size.cx + 24;
        if (joy_target < 360) {
            joy_target = 360;
        }
    }

    joy = width - (conn + rx + err);
    auto shrink_left_for_joy = [&](int need) {
        const int err_min = 80;
        const int conn_min = 150;
        const int rx_min = 80;
        if (need <= 0) {
            return;
        }
        int dec = min(need, err - err_min);
        err -= dec;
        need -= dec;
        if (need <= 0) {
            return;
        }
        dec = min(need, conn - conn_min);
        conn -= dec;
        need -= dec;
        if (need <= 0) {
            return;
        }
        dec = min(need, rx - rx_min);
        rx -= dec;
    };

    if (joy < joy_target) {
        shrink_left_for_joy(joy_target - joy);
    }
    joy = width - (conn + rx + err);
    if (joy < 200) {
        shrink_left_for_joy(200 - joy);
        joy = width - (conn + rx + err);
    }
    if (joy < 120) {
        joy = 120;
    }
    err = width - (conn + rx + joy);
    if (err < 40) {
        err = 40;
        joy = width - (conn + rx + err);
        if (joy < 120) {
            joy = 120;
        }
    }

    m_statusBar.SetPaneInfo(kPaneConnection, ID_SEPARATOR, SBPS_NORMAL, conn);
    m_statusBar.SetPaneInfo(kPaneError, ID_INDICATOR_ERR, SBPS_NORMAL, err);
    m_statusBar.SetPaneInfo(kPaneRx, ID_INDICATOR_RX, SBPS_NORMAL, rx);
    m_statusBar.SetPaneInfo(kPaneJoy, ID_INDICATOR_TX, SBPS_NORMAL, joy);
}

void CMainFrame::UpdateConnectionStatus(const std::wstring& text) {
    m_statusBar.SetPaneText(kPaneConnection, (L"Conn: " + text).c_str());
}

void CMainFrame::UpdateErrorStatus(const std::wstring& text) {
    if (text.empty()) {
        m_statusBar.SetPaneText(kPaneError, L"");
        return;
    }
    m_statusBar.SetPaneText(kPaneError, (L"Err: " + text).c_str());
}

void CMainFrame::UpdateRxCount(unsigned long long count) {
    std::wstring text = L"RX: " + std::to_wstring(count);
    m_statusBar.SetPaneText(kPaneRx, text.c_str());
}

void CMainFrame::UpdateJoyStatus(const std::wstring& text) {
    const std::wstring value = text.empty() ? L"N/A" : text;
    bool need_relayout = (value.size() != m_lastJoyStatus.size());
    m_lastJoyStatus = value;
    if (need_relayout) {
        LayoutStatusBarPanes();
    }
    m_statusBar.SetPaneText(kPaneJoy, (L"JOY: " + m_lastJoyStatus).c_str());
}

void CMainFrame::UpdateRxActivity() {
    if (!m_device.IsOpen()) {
        m_deviceBar.SetConnectionState(CDeviceBar::StateDisconnected);
        m_rxPanel.SetRxState(CRxPanel::RxIdle);
        UpdateConnectionStatus(L"Disconnected");
        return;
    }

    DWORD now = GetTickCount();
    if (m_lastRxTick == 0) {
        m_deviceBar.SetConnectionState(CDeviceBar::StateConnected);
        m_rxPanel.SetRxState(CRxPanel::RxIdle);
        UpdateConnectionStatus(L"Connected");
        return;
    }

    DWORD elapsed = now - m_lastRxTick;
    if (elapsed <= 100) {
        m_deviceBar.SetConnectionState(CDeviceBar::StateRxActive);
        m_rxPanel.SetRxState(CRxPanel::RxReceiving);
        UpdateConnectionStatus(L"RX Active");
    } else if (elapsed > 1000) {
        m_deviceBar.SetConnectionState(CDeviceBar::StateRxTimeout);
        m_rxPanel.SetRxState(CRxPanel::RxTimeout);
        UpdateConnectionStatus(L"RX Timeout");
    } else {
        m_deviceBar.SetConnectionState(CDeviceBar::StateConnected);
        m_rxPanel.SetRxState(CRxPanel::RxIdle);
        UpdateConnectionStatus(L"Connected");
    }
}

void CMainFrame::RestoreDeviceSelection() {
    if (m_config.device_path_hash != 0) {
        m_deviceBar.SelectByPathHash(m_config.device_path_hash);
    } else if (!m_config.device_path.empty()) {
        m_deviceBar.SelectByPath(m_config.device_path);
    } else if (m_config.device_vid != 0 || m_config.device_pid != 0) {
        m_deviceBar.SelectByVidPid(m_config.device_vid, m_config.device_pid);
    }
}

void CMainFrame::StartReconnectTimer() {
    if (m_reconnectTimer) {
        return;
    }
    m_reconnectTimer = SetTimer(kReconnectTimerId, 1000, nullptr);
}

void CMainFrame::StopReconnectTimer() {
    if (m_reconnectTimer) {
        KillTimer(m_reconnectTimer);
        m_reconnectTimer = 0;
    }
}

void CMainFrame::UpdateSelectedDeviceDescriptorPreview() {
    HidJoystickVisualState preview;
    std::vector<std::wstring> mapping_lines;

    const auto& devices = m_deviceBar.Devices();
    int index = m_deviceBar.SelectedIndex();
    if (index >= 0 && index < static_cast<int>(devices.size())) {
        std::wstring probe_error;
        if (!HidDevice::ProbeJoystickDescriptor(devices[static_cast<size_t>(index)],
                                                &preview,
                                                &mapping_lines,
                                                &probe_error)) {
            preview = HidJoystickVisualState{};
            mapping_lines.clear();
            if (!probe_error.empty()) {
                mapping_lines.push_back(L"(descriptor probe failed)");
            } else {
                mapping_lines.push_back(L"(descriptor mapping unavailable)");
            }
        }
    } else {
        mapping_lines.push_back(L"(waiting for descriptor mapping)");
    }

    m_rxPanel.SetDescriptorPreview(preview, mapping_lines);
}

bool CMainFrame::ConnectSelectedDevice() {
    const auto& devices = m_deviceBar.Devices();
    int index = m_deviceBar.SelectedIndex();
    if (index < 0 || index >= static_cast<int>(devices.size())) {
        UpdateErrorStatus(L"No HID device selected.");
        return false;
    }

    m_rxPanel.StopCapture();
    m_device.Close();

    std::wstring error;
    if (!m_device.Open(devices[static_cast<size_t>(index)], &error)) {
        std::wstring message = L"Error: ";
        message += error.empty() ? L"Connect failed." : error;
        UpdateErrorStatus(message);
        UpdateConnectionStatus(L"Disconnected");
        m_deviceBar.SetConnectionState(CDeviceBar::StateDisconnected);
        return false;
    }

    m_lastConnected = devices[static_cast<size_t>(index)];
    m_hasLastConnected = true;
    m_lastRxTick = 0;
    m_rxCount = 0;

    UpdateConnectionStatus(L"Connected");
    UpdateErrorStatus(L"");
    UpdateRxCount(m_rxCount);
    UpdateJoyStatus(L"waiting...");

    m_deviceBar.SetConnected(true);
    m_deviceBar.SetConnectionState(CDeviceBar::StateConnected);
    m_rxPanel.SetControlsEnabled(true);
    m_rxPanel.SetClearEnabled(false);
    m_rxPanel.StartCapture();

    StopReconnectTimer();
    return true;
}

void CMainFrame::HandleDisconnected(bool allow_reconnect) {
    m_rxPanel.StopCapture();
    m_device.CancelPendingIo();
    m_device.Close();

    m_deviceBar.SetConnected(false);
    m_deviceBar.SetConnectionState(CDeviceBar::StateDisconnected);
    m_lastRxTick = 0;

    m_rxPanel.SetControlsEnabled(false);
    m_rxPanel.SetClearEnabled(true);

    UpdateConnectionStatus(L"Disconnected");
    UpdateJoyStatus(L"N/A");
    UpdateSelectedDeviceDescriptorPreview();

    if (allow_reconnect && m_deviceBar.IsAutoReconnectEnabled()) {
        StartReconnectTimer();
    } else {
        StopReconnectTimer();
    }
}

void CMainFrame::OnTimer(UINT_PTR nIDEvent) {
    if (nIDEvent == kRxTimerId) {
        UpdateRxActivity();
    } else if (nIDEvent == kStartupScanTimerId) {
        if (m_startupScanTimer) {
            KillTimer(m_startupScanTimer);
            m_startupScanTimer = 0;
        }
        m_deviceBar.ScanDevices();
        RestoreDeviceSelection();
        UpdateSelectedDeviceDescriptorPreview();
    } else if (nIDEvent == kReconnectTimerId) {
        if (m_device.IsOpen()) {
            StopReconnectTimer();
            return;
        }
        if (!m_deviceBar.IsAutoReconnectEnabled()) {
            StopReconnectTimer();
            return;
        }

        m_deviceBar.ScanDevices();
        if (m_hasLastConnected) {
            if (!m_deviceBar.SelectByPathHash(m_lastConnected.path_hash)) {
                m_deviceBar.SelectByVidPid(m_lastConnected.vendor_id, m_lastConnected.product_id);
            }
        } else {
            RestoreDeviceSelection();
        }

        if (m_deviceBar.SelectedIndex() >= 0) {
            ConnectSelectedDevice();
        }
    }

    CFrameWnd::OnTimer(nIDEvent);
}

void CMainFrame::ApplyConfig() {
    m_rxPanel.SetLogEnabled(m_config.log_enabled);
    m_rxPanel.SetTimestampEnabled(m_config.timestamp_enabled);
}

void CMainFrame::SaveConfig() {
    if (m_hasLastConnected) {
        const auto& info = m_lastConnected;
        m_config.device_path = info.device_path;
        m_config.device_vid = info.vendor_id;
        m_config.device_pid = info.product_id;
        m_config.device_path_hash = info.path_hash;
    } else {
        int sel = m_deviceBar.SelectedIndex();
        if (sel >= 0 && sel < static_cast<int>(m_deviceBar.Devices().size())) {
            const auto& info = m_deviceBar.Devices()[static_cast<size_t>(sel)];
            m_config.device_path = info.device_path;
            m_config.device_vid = info.vendor_id;
            m_config.device_pid = info.product_id;
            m_config.device_path_hash = info.path_hash;
        } else {
            m_config.device_path.clear();
            m_config.device_vid = 0;
            m_config.device_pid = 0;
            m_config.device_path_hash = 0;
        }
    }

    m_config.log_enabled = m_rxPanel.IsLogEnabled();
    m_config.timestamp_enabled = m_rxPanel.IsTimestampEnabled();

    m_config.Save();
}
