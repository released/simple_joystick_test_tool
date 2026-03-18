#pragma once

#include <afxwin.h>
#include <afxext.h>

#include "ui/device_bar.h"
#include "ui/rx_panel.h"
#include "hid/hid_device.h"
#include "config/config_manager.h"

class CMainFrame : public CFrameWnd {
public:
    CMainFrame() = default;

protected:
    BOOL PreCreateWindow(CREATESTRUCT& cs) override;
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg LRESULT OnDeviceBarAction(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnRxEvent(WPARAM wParam, LPARAM lParam);
    afx_msg void OnClose();
    afx_msg void OnTimer(UINT_PTR nIDEvent);

    DECLARE_MESSAGE_MAP()

private:
    void LayoutControls(int cx, int cy);
    void LayoutStatusBarPanes();
    void InitStatusBar();

    void UpdateConnectionStatus(const std::wstring& text);
    void UpdateErrorStatus(const std::wstring& text);
    void UpdateRxCount(unsigned long long count);
    void UpdateJoyStatus(const std::wstring& text);
    void UpdateRxActivity();

    void RestoreDeviceSelection();
    void StartReconnectTimer();
    void StopReconnectTimer();
    void UpdateSelectedDeviceDescriptorPreview();
    bool ConnectSelectedDevice();
    void HandleDisconnected(bool allow_reconnect);

    void ApplyConfig();
    void SaveConfig();

    CDeviceBar m_deviceBar;
    CRxPanel m_rxPanel;
    CStatusBar m_statusBar;
    HidDevice m_device;
    ConfigManager m_config;

    unsigned long long m_rxCount = 0;

    HidScanInfo m_lastConnected;
    bool m_hasLastConnected = false;
    DWORD m_lastRxTick = 0;
    UINT_PTR m_rxStatusTimer = 0;
    UINT_PTR m_reconnectTimer = 0;
    UINT_PTR m_startupScanTimer = 0;
    std::wstring m_lastJoyStatus = L"N/A";
};
