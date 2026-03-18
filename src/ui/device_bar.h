#pragma once

#include <afxwin.h>
#include <afxcmn.h>

#include <string>
#include <vector>

#include "../hid/hid_scan.h"

class CDeviceBar : public CWnd {
public:
    bool Create(CWnd* parent);
    void ScanDevices();

    const std::vector<HidScanInfo>& Devices() const { return m_devices; }
    int SelectedIndex() const;
    bool SelectByPath(const std::wstring& path);
    bool SelectByPathHash(unsigned int path_hash);
    bool SelectByVidPid(unsigned short vid, unsigned short pid);

    void SetConnected(bool connected);
    void SetConnectionState(int state);
    void SetReportId(unsigned char report_id);
    bool IsAutoReconnectEnabled() const;
    bool IsShowAllHidEnabled() const;

    static UINT MessageId() { return WM_APP + 100; }
    enum Action {
        ActionConnectToggle = 1,
        ActionSelectionChanged = 2,
    };
    enum ConnectionState {
        StateDisconnected = 0,
        StateConnected = 1,
        StateRxActive = 2,
        StateRxTimeout = 3,
    };

protected:
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnScanClicked();
    afx_msg void OnConnectClicked();
    afx_msg void OnCopyClicked();
    afx_msg void OnComboChanged();
    afx_msg void OnShowAllClicked();
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
    afx_msg void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct);
    BOOL PreTranslateMessage(MSG* pMsg) override;

    DECLARE_MESSAGE_MAP()

private:
    void LayoutControls(int cx, int cy);
    void UpdateCombo();
    void UpdateCapabilityText();
    bool UpdateStatusText();
    void NotifyParent(Action action);
    std::wstring BuildDeviceSummary() const;

    CComboBox m_deviceCombo;
    CButton m_scanButton;
    CButton m_connectButton;
    CButton m_autoReconnectCheck;
    CButton m_showAllCheck;
    CStatic m_capTitle;
    CStatic m_capVidPid;
    CStatic m_capUsage;
    CStatic m_capReports;
    CStatic m_statusLabel;
    CButton m_copyButton;
    std::wstring m_statusText;
    std::vector<HidScanInfo> m_devices;
    bool m_connected;
    int m_connectionState;
    unsigned char m_reportId;
    CFont m_uiFont;
    CImageList m_iconScan;
    CImageList m_iconConnect;
    CImageList m_iconCopy;
    CToolTipCtrl m_tooltip;
    CBrush m_vidPidHighlightBrush;
};

