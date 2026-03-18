#include "app.h"

#include "main_frame.h"
#include "resource.h"

#include <string>

namespace {

class CStartupSplash : public CWnd {
public:
    bool CreateSplash() {
        CString class_name = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW, ::LoadCursor(nullptr, IDC_WAIT),
                                                 reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1), nullptr);
        const int width = 360;
        const int height = 96;
        const int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
        const int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
        if (!CreateEx(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, class_name, L"",
                      WS_POPUP | WS_VISIBLE, x, y, width, height, nullptr, 0)) {
            return false;
        }
        m_text = L"Initializing Joystick Test Tool...";
        CClientDC dc(this);
        int font_height = -MulDiv(10, dc.GetDeviceCaps(LOGPIXELSY), 72);
        m_font.CreateFontW(font_height, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, 0, DEFAULT_CHARSET,
                           OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                           DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        return true;
    }

protected:
    afx_msg void OnPaint() {
        CPaintDC dc(this);
        CRect rect;
        GetClientRect(&rect);
        dc.FillSolidRect(&rect, RGB(15, 23, 42));
        dc.SetTextColor(RGB(241, 245, 249));
        dc.SetBkMode(TRANSPARENT);
        CFont* old = dc.SelectObject(&m_font);
        dc.DrawTextW(m_text.c_str(), &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if (old) {
            dc.SelectObject(old);
        }
        dc.Draw3dRect(&rect, RGB(56, 189, 248), RGB(56, 189, 248));
    }

    DECLARE_MESSAGE_MAP()

private:
    std::wstring m_text;
    CFont m_font;
};

BEGIN_MESSAGE_MAP(CStartupSplash, CWnd)
    ON_WM_PAINT()
END_MESSAGE_MAP()

}

BEGIN_MESSAGE_MAP(CJoyStickTestToolApp, CWinApp)
END_MESSAGE_MAP()

BOOL CJoyStickTestToolApp::InitInstance() {
    CWinApp::InitInstance();

    CStartupSplash splash;
    if (splash.CreateSplash()) {
        splash.UpdateWindow();
    }

    auto* frame = new CMainFrame();
    if (!frame->Create(nullptr, L"Joystick Test Tool", WS_OVERLAPPEDWINDOW, CRect(0, 0, 1024, 768))) {
        if (splash.GetSafeHwnd()) {
            splash.DestroyWindow();
        }
        delete frame;
        return FALSE;
    }

    m_pMainWnd = frame;
    frame->ShowWindow(SW_SHOWMAXIMIZED);
    frame->UpdateWindow();
    if (splash.GetSafeHwnd()) {
        splash.DestroyWindow();
    }
    return TRUE;
}

