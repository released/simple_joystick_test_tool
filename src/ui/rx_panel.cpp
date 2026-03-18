#include "rx_panel.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cwchar>
#include <sstream>

#pragma comment(lib, "msimg32.lib")

namespace {

const int kSaveCheckId = 3001;
const int kTimestampCheckId = 3002;
const int kClearId = 3003;
const int kSwapDpadL3Id = 3004;
const int kRxByteBase = 6000;
const int kRxCopyBase = 7000;
const UINT_PTR kUiTimerId = 1;
const double kPi = 3.14159265358979323846;

std::wstring BytesToHex(const unsigned char* data, size_t length) {
    std::wstringstream ss;
    ss << std::uppercase << std::hex;
    for (size_t i = 0; i < length; ++i) {
        if (i > 0) {
            ss << L' ';
        }
        ss.width(2);
        ss.fill(L'0');
        ss << static_cast<int>(data[i]);
    }
    return ss.str();
}

bool CopyTextToClipboard(const std::wstring& text) {
    if (text.empty()) {
        return false;
    }
    if (!OpenClipboard(nullptr)) {
        return false;
    }
    EmptyClipboard();
    size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL data = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!data) {
        CloseClipboard();
        return false;
    }
    void* ptr = GlobalLock(data);
    if (!ptr) {
        GlobalFree(data);
        CloseClipboard();
        return false;
    }
    memcpy(ptr, text.c_str(), bytes);
    GlobalUnlock(data);
    SetClipboardData(CF_UNICODETEXT, data);
    CloseClipboard();
    return true;
}

int ClampNorm(int value) {
    if (value < 0) {
        return 0;
    }
    if (value > 1000) {
        return 1000;
    }
    return value;
}

int AxisToPixel(int norm, int start, int span) {
    if (span <= 1) {
        return start;
    }
    return start + (ClampNorm(norm) * (span - 1) / 1000);
}

std::wstring PovText(const HidJoystickVisualState& state) {
    if (!state.pov_present || state.pov_centered || state.pov_degrees < 0) {
        return L"CENTER";
    }
    const int deg = state.pov_degrees % 360;
    if (deg >= 338 || deg < 23) {
        return L"N";
    }
    if (deg < 68) {
        return L"NE";
    }
    if (deg < 113) {
        return L"E";
    }
    if (deg < 158) {
        return L"SE";
    }
    if (deg < 203) {
        return L"S";
    }
    if (deg < 248) {
        return L"SW";
    }
    if (deg < 293) {
        return L"W";
    }
    return L"NW";
}

const wchar_t* ButtonRoleLabel(size_t one_based) {
    switch (one_based) {
    case 1: return L"X";
    case 2: return L"A";
    case 3: return L"B";
    case 4: return L"Y";
    case 5: return L"L1";
    case 6: return L"R1";
    case 7: return L"L2";
    case 8: return L"R2";
    case 9: return L"SEL";
    case 10: return L"STA";
    case 11: return L"L3";
    case 12: return L"R3";
    default: return L"";
    }
}

}

BEGIN_MESSAGE_MAP(CRxPanel, CWnd)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_DESTROY()
    ON_WM_TIMER()
    ON_BN_CLICKED(kSaveCheckId, &CRxPanel::OnSaveLogClicked)
    ON_BN_CLICKED(kTimestampCheckId, &CRxPanel::OnTimestampClicked)
    ON_BN_CLICKED(kClearId, &CRxPanel::OnClearClicked)
    ON_BN_CLICKED(kSwapDpadL3Id, &CRxPanel::OnSwapDpadL3Clicked)
    ON_CONTROL_RANGE(BN_CLICKED, kRxCopyBase, kRxCopyBase + 9, &CRxPanel::OnCopyRowClicked)
END_MESSAGE_MAP()

bool CRxPanel::Create(CWnd* parent) {
    CString class_name = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW, ::LoadCursor(nullptr, IDC_ARROW),
                                            reinterpret_cast<HBRUSH>(COLOR_3DFACE + 1), nullptr);
    return CWnd::CreateEx(0, class_name, L"RxPanel",
                          WS_CHILD | WS_VISIBLE | WS_BORDER | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                          CRect(0, 0, 0, 0), parent, 0);
}

int CRxPanel::OnCreate(LPCREATESTRUCT lpCreateStruct) {
    if (CWnd::OnCreate(lpCreateStruct) == -1) {
        return -1;
    }

    CClientDC dc(this);
    int font_height = -MulDiv(9, dc.GetDeviceCaps(LOGPIXELSY), 72);
    m_uiFont.CreateFontW(font_height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, 0, DEFAULT_CHARSET,
                         OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                         DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    m_monoFont.CreateFontW(font_height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, 0, DEFAULT_CHARSET,
                           OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                           FIXED_PITCH | FF_MODERN, L"Consolas");

    m_saveCheck.Create(L"Save to log", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                       CRect(0, 0, 0, 0), this, kSaveCheckId);
    m_saveCheck.SetFont(&m_uiFont);
    m_timestampCheck.Create(L"Add timestamp", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                            CRect(0, 0, 0, 0), this, kTimestampCheckId);
    m_timestampCheck.SetFont(&m_uiFont);
    m_clearButton.Create(L"Clear Data", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                         CRect(0, 0, 0, 0), this, kClearId);
    m_clearButton.SetFont(&m_uiFont);
    m_swapDpadL3Check.Create(L"Swap DPad <-> L3", WS_CHILD | BS_AUTOCHECKBOX,
                             CRect(0, 0, 0, 0), this, kSwapDpadL3Id);
    m_swapDpadL3Check.SetFont(&m_uiFont);
    m_swapDpadL3Check.SetCheck(BST_CHECKED);
    m_swapDpadL3Enabled = true;

    m_tooltip.Create(this, TTS_ALWAYSTIP);
    m_tooltip.SetMaxTipWidth(260);
    m_tooltip.AddTool(&m_saveCheck, L"Write TX/RX data to log file");
    m_tooltip.AddTool(&m_timestampCheck, L"Prefix RX lines with timestamp");
    m_tooltip.AddTool(&m_clearButton, L"Clear all RX rows");
    m_tooltip.AddTool(&m_swapDpadL3Check, L"Checked: X/Y -> UDLR, POV -> L3");

    for (int row = 0; row < kRowCount; ++row) {
        for (int col = 0; col < kByteCount; ++col) {
            int id = kRxByteBase + row * kByteCount + col;
            m_cells[row][col].Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_CENTER | ES_READONLY | WS_DISABLED,
                                     CRect(0, 0, 0, 0), this, id);
            m_cells[row][col].SetFont(&m_monoFont);
            m_cells[row][col].SetWindowTextW(L"");
        }
        int copy_id = kRxCopyBase + row;
        m_copyButtons[row].Create(L"Copy", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                  CRect(0, 0, 0, 0), this, copy_id);
        m_copyButtons[row].SetFont(&m_uiFont);
        m_copyButtons[row].EnableWindow(FALSE);
        m_tooltip.AddTool(&m_copyButtons[row], L"Copy this RX row as hex bytes");
    }

    m_device = nullptr;
    m_thread = nullptr;
    m_stopRequested = false;
    m_logEnabled = false;
    m_timestampEnabled = false;
    m_lastJoystickSummary = L"X=0 Y=0 Z=0 Rz=0 POV=CENTER BTN=none";
    m_lastVisualState = HidJoystickVisualState{};
    m_lastRawMappingLines.push_back(L"(waiting for descriptor mapping)");
    m_rxEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    InitializeCriticalSection(&m_payloadLock);
    InitializeCriticalSection(&m_pendingLock);
    if (!m_uiTimer) {
        m_uiTimer = SetTimer(kUiTimerId, 50, nullptr);
    }
    return 0;
}

void CRxPanel::OnSize(UINT nType, int cx, int cy) {
    CWnd::OnSize(nType, cx, cy);
    LayoutControls(cx, cy);
}

void CRxPanel::LayoutControls(int cx, int cy) {
    const int margin = 10;
    const int header_height = 16;
    const int gap = 8;
    const int control_height = 24;
    const int copy_gap = 6;

    int width = cx - margin * 2;
    if (width < 0) {
        width = 0;
    }

    const int content_top = margin + header_height;

    CClientDC dc(this);
    dc.SelectObject(&m_monoFont);
    TEXTMETRIC tm = {};
    dc.GetTextMetrics(&tm);
    int min_row_height = tm.tmHeight + 6;
    m_headerHeight = tm.tmHeight + 4;
    m_timestampWidth = dc.GetTextExtent(L"00:00:00.000 ").cx + 4;
    m_rowLabelWidth = dc.GetTextExtent(L"No.10").cx + 8;
    if (m_rowLabelWidth < 24) {
        m_rowLabelWidth = 24;
    }

    const int tx_check_width = 70;
    const int tx_gap = 6;
    const int tx_button_width = 90;

    CString handshakeText(L"Enable Handshake (Send 1)");
    dc.SelectObject(&m_uiFont);
    int handshake_text_width = dc.GetTextExtent(handshakeText).cx + 26;
    int handshake_base_width = tx_button_width * 2 + tx_gap;
    int handshake_extra = handshake_text_width > handshake_base_width ? (handshake_text_width - handshake_base_width) : 0;

    int edit_width = width - tx_check_width - tx_button_width * 2 - tx_gap * 3 - handshake_extra;
    if (edit_width < 120) {
        edit_width = 120;
    }
    if (m_hasExternalGrid) {
        m_dataStartX = m_externalStartX;
        m_cellWidthBase = m_externalCellBase;
        m_cellWidthExtra = m_externalCellExtra;
        if (m_externalRowHeight > 0) {
            m_rowHeight = m_externalRowHeight;
        } else {
            m_rowHeight = min_row_height;
        }
        if (m_externalRowGap > 0) {
            m_rowGap = m_externalRowGap;
        } else {
            m_rowGap = 6;
        }
    } else {
        m_dataStartX = margin + tx_check_width + tx_gap;
        m_cellWidthBase = edit_width / 64;
        m_cellWidthExtra = edit_width % 64;
        if (m_cellWidthBase < 8) {
            m_cellWidthBase = 8;
            m_cellWidthExtra = 0;
        }
        m_rowHeight = min_row_height;
        m_rowGap = 6;
    }

    const int bottom_limit = cy - margin;
    int available_for_block = bottom_limit - content_top;
    if (available_for_block < 0) {
        available_for_block = 0;
    }

    int mapping_height = 72;
    const int min_mapping_height = 40;
    if (mapping_height > available_for_block) {
        mapping_height = available_for_block;
    }

    int grid_height = m_rowHeight * 10 + m_rowGap * 9;
    int reserved_without_grid = mapping_height + gap + control_height + gap + m_headerHeight + 4;
    int rows_available = available_for_block - reserved_without_grid;
    if (rows_available < 0) {
        rows_available = 0;
    }

    if (!m_hasExternalGrid && rows_available > 0 && grid_height > rows_available) {
        int gap_override = m_rowGap;
        int available = rows_available - gap_override * 9;
        if (available < 10) {
            gap_override = 2;
            available = rows_available - gap_override * 9;
        }
        if (available > 0) {
            int shrink_height = available / 10;
            if (shrink_height < m_rowHeight) {
                m_rowHeight = shrink_height;
                if (m_rowHeight < 8) {
                    m_rowHeight = 8;
                }
            }
        }
        m_rowGap = gap_override;
        grid_height = m_rowHeight * 10 + m_rowGap * 9;
    }

    int mapping_max = available_for_block - (control_height + gap + m_headerHeight + 4 + grid_height);
    if (mapping_max < min_mapping_height) {
        mapping_max = min_mapping_height;
    }
    if (mapping_height > mapping_max) {
        mapping_height = mapping_max;
    }
    if (mapping_height < min_mapping_height) {
        mapping_height = min_mapping_height;
    }

    int block_height = mapping_height + gap + control_height + gap + m_headerHeight + 4 + grid_height;
    if (block_height > available_for_block) {
        int overflow = block_height - available_for_block;
        mapping_height -= overflow;
        if (mapping_height < min_mapping_height) {
            mapping_height = min_mapping_height;
        }
        block_height = mapping_height + gap + control_height + gap + m_headerHeight + 4 + grid_height;
    }

    int block_top = bottom_limit - block_height;
    if (block_top < content_top) {
        block_top = content_top;
    }

    int mapping_top = block_top;
    int control_top = mapping_top + mapping_height + gap;
    int header_top = control_top + control_height + gap;

    m_mappingRect = CRect(margin, mapping_top, margin + width, mapping_top + mapping_height);

    m_headerRect = CRect(margin, header_top, margin + width, header_top + m_headerHeight);
    int grid_top = m_headerRect.bottom + 4;
    m_gridRect = CRect(margin, grid_top, margin + width, grid_top + grid_height);

    int check_width = 140;
    m_saveCheck.MoveWindow(margin, control_top, check_width, control_height);
    m_timestampCheck.MoveWindow(margin + check_width + gap, control_top, check_width + 20, control_height);
    m_clearButton.MoveWindow(margin + check_width * 2 + gap * 2 + 20, control_top, 110, control_height);

    int visual_bottom = mapping_top - gap;
    if (visual_bottom < content_top) {
        visual_bottom = content_top;
    }
    m_visualRect = CRect(margin, content_top, margin + width, visual_bottom);
    CRect gamepad_panel;
    CRect gamepad_draw_area;
    if (CalcGamepadPanelRects(m_visualRect, &gamepad_panel, &gamepad_draw_area)) {
        const int check_x = gamepad_panel.left + 8;
        const int check_y = gamepad_panel.top + 22;
        int check_w = gamepad_panel.Width() - 16;
        if (check_w > 170) {
            check_w = 170;
        }
        if (check_w < 120) {
            check_w = 120;
        }
        m_swapDpadL3Check.MoveWindow(check_x, check_y, check_w, 18);
        m_swapDpadL3Check.ShowWindow(SW_SHOW);
    } else {
        m_swapDpadL3Check.ShowWindow(SW_HIDE);
    }

    int data_end = m_dataStartX + m_cellWidthBase * 64 + m_cellWidthExtra;
    int max_data_end = m_gridRect.right - m_copyButtonWidth - copy_gap - 2;
    if (max_data_end < m_dataStartX) {
        max_data_end = m_dataStartX;
    }
    if (data_end > max_data_end) {
        data_end = max_data_end;
    }
    if (data_end < m_dataStartX) {
        data_end = m_dataStartX;
    }
    m_dataEndX = data_end;
    m_copyButtonX = m_dataEndX + copy_gap;
    if (m_copyButtonX + m_copyButtonWidth > m_gridRect.right - 1) {
        m_copyButtonX = m_gridRect.right - m_copyButtonWidth - 1;
    }
    if (m_copyButtonX < m_gridRect.left + m_rowLabelWidth + 2) {
        m_copyButtonX = m_gridRect.left + m_rowLabelWidth + 2;
    }

    for (int row = 0; row < kRowCount; ++row) {
        int row_top = m_gridRect.top + row * (m_rowHeight + m_rowGap);
        int x = m_dataStartX;
        for (int col = 0; col < kByteCount; ++col) {
            int cell_width = m_cellWidthBase + (col < m_cellWidthExtra ? 1 : 0);
            if (x >= data_end) {
                break;
            }
            m_cells[row][col].MoveWindow(x, row_top, cell_width, m_rowHeight);
            x += cell_width;
        }
        m_copyButtons[row].MoveWindow(m_copyButtonX, row_top, m_copyButtonWidth, m_rowHeight);
    }
}

void CRxPanel::OnPaint() {
    CPaintDC paint_dc(this);
    CRect rect;
    GetClientRect(&rect);
    if (rect.Width() <= 0 || rect.Height() <= 0) {
        return;
    }

    CDC mem_dc;
    mem_dc.CreateCompatibleDC(&paint_dc);
    CBitmap back_buffer;
    back_buffer.CreateCompatibleBitmap(&paint_dc, rect.Width(), rect.Height());
    CBitmap* old_bitmap = mem_dc.SelectObject(&back_buffer);

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
    ::GradientFill(mem_dc.GetSafeHdc(), verts, 2, &grad, 1, GRADIENT_FILL_RECT_V);

    mem_dc.DrawEdge(&rect, EDGE_SUNKEN, BF_RECT);

    const int title_height = 16;
    CRect titleRect(rect.left + 10, rect.top + 4, rect.right - 10, rect.top + 4 + title_height);
    mem_dc.SetBkMode(TRANSPARENT);
    mem_dc.SelectObject(&m_uiFont);
    mem_dc.SetTextColor(RGB(15, 23, 42));
    const wchar_t* state_title = L"RX: Idle";
    if (m_rxState == RxReceiving) {
        state_title = L"RX: Receiving";
    } else if (m_rxState == RxTimeout) {
        state_title = L"RX: Timeout";
    } else if (m_cleared) {
        state_title = L"RX: Idle (cleared)";
    }
    HidJoystickVisualState visual_state;
    std::vector<std::wstring> mapping_lines;
    EnterCriticalSection(&m_payloadLock);
    visual_state = m_lastVisualState;
    mapping_lines = m_lastRawMappingLines;
    LeaveCriticalSection(&m_payloadLock);
    mem_dc.DrawText(state_title, titleRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    if (m_visualRect.Width() > 0 && m_visualRect.Height() > 0) {
        DrawJoystickVisualizer(&mem_dc, m_visualRect, visual_state);
    }
    if (m_mappingRect.Width() > 0 && m_mappingRect.Height() > 0) {
        DrawRawMappingTable(&mem_dc, m_mappingRect, mapping_lines);
    }

    if (m_headerRect.Width() > 0 && m_gridRect.Width() > 0) {
        mem_dc.SelectObject(&m_monoFont);
        mem_dc.SetTextColor(RGB(15, 23, 42));

        int data_start = m_dataStartX;
        int data_end = m_dataEndX;
        if (data_end <= data_start) {
            int data_width = m_cellWidthBase * 64 + m_cellWidthExtra;
            data_end = data_start + data_width;
            if (data_end > m_gridRect.right - 2) {
                data_end = m_gridRect.right - 2;
            }
        }

        CRect header_band(data_start - 2, m_headerRect.top, data_end + 2, m_headerRect.bottom);
        mem_dc.FillSolidRect(&header_band, RGB(226, 232, 240));
        CPen hpen(PS_SOLID, 1, RGB(203, 213, 225));
        CPen* old_hpen = mem_dc.SelectObject(&hpen);
        mem_dc.MoveTo(header_band.left, header_band.bottom - 1);
        mem_dc.LineTo(header_band.right, header_band.bottom - 1);
        mem_dc.SelectObject(old_hpen);

        int x = data_start;
        for (int i = 0; i < 64; ++i) {
            int cell_width = m_cellWidthBase + (i < m_cellWidthExtra ? 1 : 0);
            CRect cell(x, m_headerRect.top, x + cell_width, m_headerRect.bottom);
            wchar_t buf[4] = {};
            swprintf_s(buf, L"%u", i);
            mem_dc.DrawText(buf, &cell, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            x += cell_width;
            if (x >= data_end) {
                break;
            }
        }

        mem_dc.SetTextColor(RGB(15, 23, 42));
        for (int row = 0; row < kRowCount; ++row) {
            int row_top = m_gridRect.top + row * (m_rowHeight + m_rowGap);
            if (row_top >= m_gridRect.bottom) {
                break;
            }
            wchar_t idx_buf[8] = {};
            swprintf_s(idx_buf, L"No.%d", row + 1);
            CRect idx_rect(m_gridRect.left + 2, row_top, m_gridRect.left + 2 + m_rowLabelWidth, row_top + m_rowHeight);
            mem_dc.DrawText(idx_buf, &idx_rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }
    }

    paint_dc.BitBlt(0, 0, rect.Width(), rect.Height(), &mem_dc, 0, 0, SRCCOPY);
    mem_dc.SelectObject(old_bitmap);
}

BOOL CRxPanel::OnEraseBkgnd(CDC* pDC) {
    (void)pDC;
    return TRUE;
}

BOOL CRxPanel::PreTranslateMessage(MSG* pMsg) {
    if (m_tooltip.GetSafeHwnd()) {
        m_tooltip.RelayEvent(pMsg);
    }
    return CWnd::PreTranslateMessage(pMsg);
}

void CRxPanel::OnDestroy() {
    StopCapture();
    if (m_uiTimer) {
        KillTimer(m_uiTimer);
        m_uiTimer = 0;
    }
    if (m_rxEvent) {
        CloseHandle(m_rxEvent);
        m_rxEvent = nullptr;
    }
    DeleteCriticalSection(&m_payloadLock);
    DeleteCriticalSection(&m_pendingLock);
    CWnd::OnDestroy();
}

void CRxPanel::SetDevice(HidDevice* device) {
    m_device = device;
    EnterCriticalSection(&m_payloadLock);
    m_lastRawMappingLines.clear();
    m_lastRawMappingLines.push_back(L"(waiting for descriptor mapping)");
    LeaveCriticalSection(&m_payloadLock);
}

void CRxPanel::StartCapture() {
    if (m_thread) {
        return;
    }
    if (m_device && m_device->IsOpen()) {
        std::vector<std::wstring> lines;
        if (!m_device->GetJoystickMappingLines(&lines) || lines.empty()) {
            lines.push_back(L"(descriptor mapping unavailable)");
        }
        EnterCriticalSection(&m_payloadLock);
        m_lastRawMappingLines = lines;
        LeaveCriticalSection(&m_payloadLock);
        InvalidateRect(m_mappingRect, FALSE);
    }
    m_rxState = RxIdle;
    m_stopRequested = false;
    m_thread = AfxBeginThread(RxThreadProc, this, THREAD_PRIORITY_NORMAL, 0, CREATE_SUSPENDED);
    if (!m_thread) {
        return;
    }
    m_thread->m_bAutoDelete = FALSE;
    m_thread->ResumeThread();
}

void CRxPanel::StopCapture() {
    m_stopRequested = true;
    if (m_device) {
        m_device->CancelPendingIo();
    }
    if (m_thread) {
        WaitForSingleObject(m_thread->m_hThread, 1500);
        delete m_thread;
        m_thread = nullptr;
    }
    m_rxState = RxIdle;
}

UINT CRxPanel::RxThreadProc(LPVOID param) {
    CRxPanel* panel = reinterpret_cast<CRxPanel*>(param);
    return panel ? panel->ThreadLoop() : 0;
}

UINT CRxPanel::ThreadLoop() {
    while (!m_stopRequested) {
        if (!m_device || !m_device->IsOpen()) {
            Sleep(100);
            continue;
        }

        std::wstring error;
        std::vector<unsigned char> report;
        bool ok = m_device->ReadInputReport(&report, 200, &error);
        if (!ok) {
            if (error == L"Device removed.") {
                CWnd* parent = GetParent();
                if (parent) {
                    parent->PostMessage(MessageId(), static_cast<WPARAM>(RxDeviceRemoved), 0);
                }
                m_stopRequested = true;
            }
            continue;
        }

        std::array<unsigned char, 64> payload = ExtractPayload(report);
        if (m_logEnabled.load()) {
            std::wstring hex = BytesToHex(payload.data(), payload.size());
            WriteDirectionalLogLine(L"RX", hex, m_timestampEnabled.load());
        }
        UpdateLastPayload(report);
        EnterCriticalSection(&m_pendingLock);
        m_pendingPayloads.push_back(payload);
        LeaveCriticalSection(&m_pendingLock);

        CWnd* parent = GetParent();
        if (parent) {
            parent->PostMessage(MessageId(), static_cast<WPARAM>(RxCount), 0);
        }
    }
    return 0;
}

std::array<unsigned char, 64> CRxPanel::ExtractPayload(const std::vector<unsigned char>& report) const {
    std::array<unsigned char, 64> payload{};
    if (report.empty()) {
        return payload;
    }

    size_t offset = 0;
    if (m_device && m_device->IsOpen()) {
        const unsigned short in_len = m_device->info().input_report_length;
        if (in_len > 0 && report.size() == static_cast<size_t>(in_len) && report.size() > 1) {
            offset = 1;
        }
    } else if (report.size() == 65) {
        offset = 1;
    }

    size_t available = (report.size() > offset) ? (report.size() - offset) : 0;
    size_t copy_len = std::min<size_t>(64, available);
    if (copy_len > 0) {
        std::copy_n(report.data() + offset, copy_len, payload.begin());
    }
    return payload;
}

void CRxPanel::UpdateLastPayload(const std::vector<unsigned char>& report) {
    std::array<unsigned char, 64> payload = ExtractPayload(report);
    std::vector<unsigned char> payload_vec(payload.begin(), payload.end());
    HidJoystickVisualState visual_state;
    std::vector<std::wstring> mapping_lines;
    bool decoded = false;
    bool is_joystick_usage = false;
    if (m_device && m_device->IsOpen()) {
        const auto& info = m_device->info();
        is_joystick_usage = (info.usage_page == 0x0001) && (info.usage == 0x0004 || info.usage == 0x0005);
        decoded = m_device->DecodeJoystickState(report, &visual_state);
        m_device->GetJoystickMappingLines(&mapping_lines);
    }
    if (!decoded && is_joystick_usage) {
        visual_state = BuildLegacyVisualState(payload);
    }
    if (mapping_lines.empty() && is_joystick_usage) {
        mapping_lines = BuildLegacyMappingLines(payload);
    } else if (mapping_lines.empty()) {
        mapping_lines.push_back(L"(descriptor preview is limited to joystick/gamepad)");
    }
    std::wstring joy_summary = BuildJoystickSummary(visual_state);

    EnterCriticalSection(&m_payloadLock);
    m_lastPayload.swap(payload_vec);
    m_lastJoystickSummary = joy_summary;
    m_lastVisualState = visual_state;
    m_lastRawMappingLines = mapping_lines;
    LeaveCriticalSection(&m_payloadLock);

    if (m_rxEvent) {
        SetEvent(m_rxEvent);
    }
}

HidJoystickVisualState CRxPanel::BuildLegacyVisualState(const std::array<unsigned char, 64>& payload) const {
    HidJoystickVisualState state;
    state.valid = true;

    size_t base = 0;
    if (payload[0] == 0x00 && (payload[1] != 0x00 || payload[2] != 0x00 || payload[3] != 0x00)) {
        base = 1;
    }

    const int x = static_cast<int>(static_cast<int8_t>(payload[base + 0]));
    const int y = static_cast<int>(static_cast<int8_t>(payload[base + 1]));
    const uint8_t buttons = payload[base + 2];

    state.x_present = true;
    state.x_min = -127;
    state.x_max = 127;
    state.x_raw = x;
    state.x_norm = ClampNorm((x + 127) * 1000 / 254);

    state.y_present = true;
    state.y_min = -127;
    state.y_max = 127;
    state.y_raw = y;
    state.y_norm = ClampNorm((y + 127) * 1000 / 254);

    state.buttons.assign(8, 0);
    for (int i = 0; i < 8 && i < static_cast<int>(state.buttons.size()); ++i) {
        state.buttons[static_cast<size_t>(i)] = ((buttons & (1U << i)) != 0U) ? 1 : 0;
    }
    return state;
}

std::vector<std::wstring> CRxPanel::BuildLegacyMappingLines(const std::array<unsigned char, 64>& payload) const {
    std::vector<std::wstring> lines;
    size_t base = 0;
    if (payload[0] == 0x00 && (payload[1] != 0x00 || payload[2] != 0x00 || payload[3] != 0x00)) {
        base = 1;
    }

    lines.push_back(std::wstring(L"X        : payload[") + std::to_wstring(base + 0) + L"] bit0..7 (legacy int8)");
    lines.push_back(std::wstring(L"Y        : payload[") + std::to_wstring(base + 1) + L"] bit0..7 (legacy int8)");
    lines.push_back(std::wstring(L"Btn1..8  : payload[") + std::to_wstring(base + 2) + L"] bit0..7");
    lines.push_back(L"Z/Rz/POV : (not defined in legacy fallback)");
    return lines;
}

std::wstring CRxPanel::BuildJoystickSummary(const HidJoystickVisualState& state) const {
    if (!state.valid) {
        return L"waiting...";
    }

    auto axis_text = [](bool present, LONG raw) {
        if (!present) {
            return std::wstring(L"-");
        }
        return std::to_wstring(raw);
    };

    std::wstring pressed;
    for (size_t i = 0; i < state.buttons.size(); ++i) {
        if (state.buttons[i] != 0) {
            if (!pressed.empty()) {
                pressed += L",";
            }
            pressed += std::to_wstring(i + 1);
        }
    }
    if (pressed.empty()) {
        pressed = L"none";
    }

    std::wstring pov = PovText(state);

    wchar_t buffer[200] = {};
    std::wstring btn_span = state.buttons.empty() ? L"(none)" : (L"(1-" + std::to_wstring(state.buttons.size()) + L")");
    swprintf_s(buffer, L"X=%s Y=%s Z=%s Rz=%s POV=%s BTN%s=%s",
               axis_text(state.x_present, state.x_raw).c_str(),
               axis_text(state.y_present, state.y_raw).c_str(),
               axis_text(state.z_present, state.z_raw).c_str(),
               axis_text(state.rz_present, state.rz_raw).c_str(),
               pov.c_str(),
               btn_span.c_str(),
               pressed.c_str());
    return buffer;
}

bool CRxPanel::CalcGamepadPanelRects(const CRect& area, CRect* out_panel, CRect* out_draw_area) const {
    if (!out_panel || !out_draw_area) {
        return false;
    }

    CRect frame = area;
    frame.DeflateRect(2, 2);
    if (frame.Width() < 120 || frame.Height() < 60) {
        return false;
    }

    CRect content = frame;
    content.DeflateRect(10, 8);
    if (content.Width() < 100 || content.Height() < 40) {
        return false;
    }

    const int panel_gap = 12;
    int center_x = content.left + content.Width() / 2;
    CRect left_panel(content.left, content.top, center_x - panel_gap / 2, content.bottom);
    CRect image_panel(center_x + (panel_gap - panel_gap / 2), content.top, content.right, content.bottom);
    bool show_gamepad_panel = (left_panel.Width() >= 120 && image_panel.Width() >= 120 && content.Width() >= 260);
    if (!show_gamepad_panel) {
        return false;
    }

    CRect panel = image_panel;
    panel.DeflateRect(2, 2);
    if (panel.Width() <= 80 || panel.Height() <= 60) {
        return false;
    }

    CRect panel_title(panel.left + 8, panel.top + 2, panel.right - 8, panel.top + 22);
    CRect draw_area = panel;
    draw_area.DeflateRect(8, 8);
    draw_area.top = panel_title.bottom + 20;
    if (draw_area.Width() <= 120 || draw_area.Height() <= 80) {
        return false;
    }

    *out_panel = panel;
    *out_draw_area = draw_area;
    return true;
}

void CRxPanel::DrawJoystickVisualizer(CDC* dc, const CRect& area, const HidJoystickVisualState& state) {
    if (!dc || area.Width() <= 0 || area.Height() <= 0) {
        return;
    }

    CRect frame = area;
    frame.DeflateRect(2, 2);
    if (frame.Width() < 120 || frame.Height() < 60) {
        return;
    }

    dc->FillSolidRect(frame, RGB(241, 245, 249));
    dc->DrawEdge(frame, EDGE_ETCHED, BF_RECT);

    CRect content = frame;
    content.DeflateRect(10, 8);
    if (content.Width() < 100 || content.Height() < 40) {
        return;
    }

    const int panel_gap = 12;
    int center_x = content.left + content.Width() / 2;
    CRect left_panel(content.left, content.top, center_x - panel_gap / 2, content.bottom);
    CRect image_panel(center_x + (panel_gap - panel_gap / 2), content.top, content.right, content.bottom);
    bool show_gamepad_panel = (left_panel.Width() >= 120 && image_panel.Width() >= 120 && content.Width() >= 260);
    if (!show_gamepad_panel) {
        left_panel = content;
        image_panel.SetRectEmpty();
    }

    dc->SetBkMode(TRANSPARENT);
    dc->SelectObject(&m_uiFont);
    dc->SetTextColor(RGB(30, 41, 59));

    left_panel.DeflateRect(2, 2);
    if (left_panel.Width() < 120 || left_panel.Height() < 60) {
        return;
    }
    dc->FillSolidRect(left_panel, RGB(248, 250, 252));
    dc->DrawEdge(left_panel, EDGE_ETCHED, BF_RECT);

    CRect left_panel_title(left_panel.left + 8, left_panel.top + 2, left_panel.right - 8, left_panel.top + 22);
    dc->DrawText(L"Joystick Visualizer", &left_panel_title, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    CRect left_content = left_panel;
    left_content.DeflateRect(8, 8);
    left_content.top = left_panel_title.bottom + 2;
    if (left_content.Width() < 100 || left_content.Height() < 40) {
        return;
    }
    content = left_content;

    const int section_count = 4;
    const int section_gap = 10;
    const int title_band = 22;
    const int plot_padding = 4;

    CRect sections[section_count];
    int usable_width = content.Width() - section_gap * (section_count - 1);
    if (usable_width < section_count * 40) {
        return;
    }
    int base = usable_width / section_count;
    int extra = usable_width % section_count;
    int x = content.left;
    for (int i = 0; i < section_count; ++i) {
        int w = base + (i < extra ? 1 : 0);
        sections[i] = CRect(x, content.top, x + w, content.bottom);
        x += w + section_gap;
    }

    auto draw_title = [&](const CRect& section, const std::wstring& text) {
        CRect title_rect(section.left, section.top, section.right, section.top + title_band);
        dc->DrawText(text.c_str(), &title_rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    };

    auto plot_rect = [&](const CRect& section) {
        CRect r(section.left + plot_padding, section.top + title_band, section.right - plot_padding, section.bottom - plot_padding);
        if (r.Height() < 10) {
            r.bottom = r.top + 10;
        }
        return r;
    };

    {
        CRect section = sections[0];
        std::wstring title = L"X / Y (" + std::to_wstring(state.x_raw) + L", " + std::to_wstring(state.y_raw) + L")";
        draw_title(section, title);

        CRect plot = plot_rect(section);
        int side = (plot.Width() < plot.Height()) ? plot.Width() : plot.Height();
        if (side >= 20) {
            CRect xy(plot.left + (plot.Width() - side) / 2, plot.top + (plot.Height() - side) / 2,
                     plot.left + (plot.Width() - side) / 2 + side, plot.top + (plot.Height() - side) / 2 + side);
            dc->Rectangle(xy);
            dc->MoveTo(xy.left + xy.Width() / 2, xy.top);
            dc->LineTo(xy.left + xy.Width() / 2, xy.bottom);
            dc->MoveTo(xy.left, xy.top + xy.Height() / 2);
            dc->LineTo(xy.right, xy.top + xy.Height() / 2);

            if (state.x_present && state.y_present) {
                int px = AxisToPixel(state.x_norm, xy.left, xy.Width());
                int py = AxisToPixel(state.y_norm, xy.top, xy.Height());
                CBrush knob(RGB(56, 189, 248));
                CBrush* old_brush = dc->SelectObject(&knob);
                dc->Ellipse(px - 8, py - 8, px + 9, py + 9);
                dc->SelectObject(old_brush);
            }
        }
    }

    {
        CRect section = sections[1];
        draw_title(section, L"Z / Rz");

        CRect plot = plot_rect(section);
        int bar_gap = 18;  // previous 8 + requested extra spacing (about +10 px)
        int bar_w = 20;
        int required = bar_w * 2 + bar_gap;
        if (required > plot.Width()) {
            bar_w = (plot.Width() - bar_gap) / 2;
            if (bar_w < 10) {
                bar_w = 10;
                bar_gap = 4;
            }
            required = bar_w * 2 + bar_gap;
        }
        if (bar_w >= 10 && required <= plot.Width()) {
            const int label_h = 14;
            const int label_gap = 2;
            int bar_top = plot.top + label_h + label_gap;
            if (bar_top > plot.bottom - 12) {
                bar_top = plot.top;
            }
            int bar_bottom = plot.bottom - 8;
            if (bar_bottom <= bar_top + 12) {
                bar_bottom = plot.bottom;
            }

            int left = plot.left + (plot.Width() - required) / 2;
            CRect zbar(left, bar_top, left + bar_w, bar_bottom);
            CRect rzbar(zbar.right + bar_gap, bar_top, zbar.right + bar_gap + bar_w, bar_bottom);

            const int label_w = 56;
            CRect zlabel((zbar.left + zbar.right) / 2 - label_w / 2, plot.top, (zbar.left + zbar.right) / 2 + label_w / 2, bar_top);
            CRect rzlabel((rzbar.left + rzbar.right) / 2 - label_w / 2, plot.top, (rzbar.left + rzbar.right) / 2 + label_w / 2, bar_top);

            std::wstring z_text = state.z_present ? std::to_wstring(state.z_raw) : L"-";
            std::wstring rz_text = state.rz_present ? std::to_wstring(state.rz_raw) : L"-";
            dc->SetTextColor(RGB(15, 23, 42));
            dc->DrawText(z_text.c_str(), &zlabel, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            dc->DrawText(rz_text.c_str(), &rzlabel, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

            auto draw_bar = [&](const CRect& bar, bool present, int norm) {
                dc->Rectangle(bar);
                if (present) {
                    int fill_h = ClampNorm(norm) * (bar.Height() - 2) / 1000;
                    CRect fill(bar.left + 1, bar.bottom - 1 - fill_h, bar.right - 1, bar.bottom - 1);
                    dc->FillSolidRect(fill, RGB(34, 197, 94));
                }
            };

            draw_bar(zbar, state.z_present, state.z_norm);
            draw_bar(rzbar, state.rz_present, state.rz_norm);
        }
    }

    {
        CRect section = sections[2];
        std::wstring pov = L"POV(LRUD):" + PovText(state);
        if (state.pov_present && !state.pov_centered && state.pov_degrees >= 0) {
            pov += L" (" + std::to_wstring(state.pov_degrees) + L" deg)";
        }
        draw_title(section, pov);

        CRect plot = plot_rect(section);
        int side = (plot.Width() < plot.Height()) ? plot.Width() : plot.Height();
        if (side >= 20) {
            CRect circle(plot.left + (plot.Width() - side) / 2, plot.top + (plot.Height() - side) / 2,
                         plot.left + (plot.Width() - side) / 2 + side, plot.top + (plot.Height() - side) / 2 + side);
            dc->Ellipse(circle);
            CPoint center((circle.left + circle.right) / 2, (circle.top + circle.bottom) / 2);
            dc->MoveTo(center.x, circle.top + 4);
            dc->LineTo(center.x, circle.bottom - 4);
            dc->MoveTo(circle.left + 4, center.y);
            dc->LineTo(circle.right - 4, center.y);

            if (state.pov_present && !state.pov_centered && state.pov_degrees >= 0) {
                const double rad = (static_cast<double>(state.pov_degrees) - 90.0) * (kPi / 180.0);
                int radius = circle.Width() / 2 - 6;
                if (radius < 4) {
                    radius = 4;
                }
                const int x2 = center.x + static_cast<int>(std::cos(rad) * radius);
                const int y2 = center.y + static_cast<int>(std::sin(rad) * radius);
                CPen pen(PS_SOLID, 3, RGB(249, 115, 22));
                CPen* old_pen = dc->SelectObject(&pen);
                dc->MoveTo(center.x, center.y);
                dc->LineTo(x2, y2);
                dc->SelectObject(old_pen);
                CBrush tip_brush(RGB(249, 115, 22));
                CBrush* old_tip_brush = dc->SelectObject(&tip_brush);
                dc->Ellipse(x2 - 5, y2 - 5, x2 + 6, y2 + 6);
                dc->SelectObject(old_tip_brush);
            } else {
                CBrush center_dot(RGB(148, 163, 184));
                CBrush* old_brush = dc->SelectObject(&center_dot);
                dc->Ellipse(center.x - 3, center.y - 3, center.x + 4, center.y + 4);
                dc->SelectObject(old_brush);
            }
        }
    }

    {
        CRect section = sections[3];
        size_t button_count = state.buttons.size();
        if (button_count == 0) {
            draw_title(section, L"Buttons (none)");
            CRect plot = plot_rect(section);
            dc->DrawText(L"No Button usage in descriptor",
                         &plot,
                         DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        } else {
            draw_title(section, L"Buttons 1-" + std::to_wstring(button_count));

            CRect plot = plot_rect(section);
            int cols = 4;
            int rows = static_cast<int>((button_count + cols - 1) / cols);
            if (button_count <= 8) {
                cols = 4;
                rows = static_cast<int>((button_count + cols - 1) / cols);
            } else if (button_count > 16) {
                cols = 6;
                rows = static_cast<int>((button_count + cols - 1) / cols);
                if (rows > 4) {
                    cols = 8;
                    rows = static_cast<int>((button_count + cols - 1) / cols);
                }
            }
            int spacing_x = 8;
            int spacing_y = 8;
            const int role_gap = 2;
            const int role_h = 12;
            int diameter_w = (plot.Width() - spacing_x * (cols - 1)) / cols;
            int diameter_h = (plot.Height() - spacing_y * (rows - 1) - rows * (role_h + role_gap)) / rows;
            int diameter = (diameter_w < diameter_h) ? diameter_w : diameter_h;
            if (diameter < 10) {
                diameter = 10;
            }
            if (diameter > 36) {
                diameter = 36;
            }

            int grid_w = cols * diameter + (cols - 1) * spacing_x;
            int cell_h = diameter + role_gap + role_h;
            int grid_h = rows * cell_h + (rows - 1) * spacing_y;
            int grid_left = plot.left + ((plot.Width() > grid_w) ? ((plot.Width() - grid_w) / 2) : 0);
            int grid_top = plot.top + ((plot.Height() > grid_h) ? ((plot.Height() - grid_h) / 2) : 0);

            for (size_t i = 0; i < button_count; ++i) {
                int row = static_cast<int>(i / static_cast<size_t>(cols));
                int col = static_cast<int>(i % cols);
                int cell_x = grid_left + col * (diameter + spacing_x);
                int y = grid_top + row * (cell_h + spacing_y);
                CRect btn(cell_x, y, cell_x + diameter, y + diameter);
                if (btn.right > plot.right || btn.bottom > plot.bottom) {
                    continue;
                }

                bool pressed_btn = (i < state.buttons.size()) && (state.buttons[i] != 0);
                COLORREF color = pressed_btn ? RGB(56, 189, 248) : RGB(226, 232, 240);
                CBrush brush(color);
                CBrush* old_brush = dc->SelectObject(&brush);
                dc->Ellipse(btn);
                dc->SelectObject(old_brush);

                wchar_t num[4] = {};
                swprintf_s(num, L"%u", static_cast<unsigned int>(i + 1));
                dc->DrawText(num, &btn, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                const wchar_t* role = ButtonRoleLabel(i + 1);
                if (role && role[0] != 0) {
                    CRect role_rect(btn.left - 8, btn.bottom + role_gap, btn.right + 8, btn.bottom + role_gap + role_h);
                    dc->SetTextColor(RGB(71, 85, 105));
                    dc->DrawText(role, &role_rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                }
            }
        }
    }

    if (show_gamepad_panel) {
        CRect panel;
        CRect draw_area;
        if (CalcGamepadPanelRects(area, &panel, &draw_area)) {
            dc->FillSolidRect(panel, RGB(248, 250, 252));
            dc->DrawEdge(panel, EDGE_ETCHED, BF_RECT);

            CRect panel_title(panel.left + 8, panel.top + 2, panel.right - 8, panel.top + 22);
            dc->SelectObject(&m_uiFont);
            dc->SetTextColor(RGB(30, 41, 59));
            dc->DrawText(L"Gamepad Mapping", &panel_title, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            auto map_pt = [&](double nx, double ny) {
                return CPoint(draw_area.left + static_cast<int>(nx * static_cast<double>(draw_area.Width())),
                              draw_area.top + static_cast<int>(ny * static_cast<double>(draw_area.Height())));
            };
            auto btn_on = [&](size_t one_based) {
                if (one_based == 0 || one_based > state.buttons.size()) {
                    return false;
                }
                return state.buttons[one_based - 1] != 0;
            };

            bool dpad_up = false;
            bool dpad_down = false;
            bool dpad_left = false;
            bool dpad_right = false;
            bool l3_axis_x_present = state.x_present;
            bool l3_axis_y_present = state.y_present;
            int l3_axis_x_norm = state.x_norm;
            int l3_axis_y_norm = state.y_norm;

            if (m_swapDpadL3Enabled) {
                const int x_norm = state.x_present ? ClampNorm(state.x_norm) : 500;
                const int y_norm = state.y_present ? ClampNorm(state.y_norm) : 500;
                const int threshold = 140;
                dpad_left = (x_norm <= (500 - threshold));
                dpad_right = (x_norm >= (500 + threshold));
                dpad_up = (y_norm <= (500 - threshold));
                dpad_down = (y_norm >= (500 + threshold));

                l3_axis_x_present = state.pov_present;
                l3_axis_y_present = state.pov_present;
                l3_axis_x_norm = 500;
                l3_axis_y_norm = 500;
                if (state.pov_present && !state.pov_centered && state.pov_degrees >= 0) {
                    const double rad = (static_cast<double>(state.pov_degrees) - 90.0) * (kPi / 180.0);
                    l3_axis_x_norm = ClampNorm(static_cast<int>(500.0 + std::cos(rad) * 500.0));
                    l3_axis_y_norm = ClampNorm(static_cast<int>(500.0 + std::sin(rad) * 500.0));
                }
            } else if (state.pov_present && !state.pov_centered && state.pov_degrees >= 0) {
                int sector = ((state.pov_degrees + 22) % 360) / 45;
                switch (sector) {
                case 0:
                    dpad_up = true;
                    break;
                case 1:
                    dpad_up = true;
                    dpad_right = true;
                    break;
                case 2:
                    dpad_right = true;
                    break;
                case 3:
                    dpad_down = true;
                    dpad_right = true;
                    break;
                case 4:
                    dpad_down = true;
                    break;
                case 5:
                    dpad_down = true;
                    dpad_left = true;
                    break;
                case 6:
                    dpad_left = true;
                    break;
                case 7:
                    dpad_up = true;
                    dpad_left = true;
                    break;
                default:
                    break;
                }
            }

            const int short_side = (draw_area.Width() < draw_area.Height()) ? draw_area.Width() : draw_area.Height();
            const int marker_r = (short_side >= 260) ? 14 : 12;
            const int stick_outer_r = (short_side >= 260) ? 30 : 26;
            const int stick_move_r = (short_side >= 260) ? 18 : 15;
            const int stick_knob_r = (short_side >= 260) ? 10 : 9;

            auto draw_round_marker = [&](double nx, double ny, int radius, bool active, const wchar_t* label) {
                CPoint p = map_pt(nx, ny);
                COLORREF fill = active ? RGB(56, 189, 248) : RGB(226, 232, 240);
                COLORREF border = active ? RGB(14, 116, 144) : RGB(100, 116, 139);
                CPen pen(PS_SOLID, active ? 2 : 1, border);
                CBrush brush(fill);
                CPen* old_pen = dc->SelectObject(&pen);
                CBrush* old_brush = dc->SelectObject(&brush);
                dc->Ellipse(p.x - radius, p.y - radius, p.x + radius + 1, p.y + radius + 1);
                dc->SelectObject(old_brush);
                dc->SelectObject(old_pen);
                if (label && label[0] != 0) {
                    CRect label_rect(p.x - radius - 2, p.y - radius - 2, p.x + radius + 2, p.y + radius + 2);
                    dc->SetTextColor(active ? RGB(255, 255, 255) : RGB(30, 41, 59));
                    dc->DrawText(label, &label_rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                }
            };

            static CFont s_hint_font;
            static bool s_hint_font_ready = false;
            if (!s_hint_font_ready) {
                const int hint_h = -MulDiv(6, dc->GetDeviceCaps(LOGPIXELSY), 72);
                s_hint_font.CreateFontW(hint_h, 0, 0, 0, FW_NORMAL, FALSE, FALSE, 0, DEFAULT_CHARSET,
                                        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
                s_hint_font_ready = true;
            }

            auto draw_button_id_hint = [&](int anchor_x, int anchor_y, const wchar_t* text) {
                if (!text || text[0] == 0) {
                    return;
                }
                const int hint_w = 24;
                const int hint_h = 10;
                int x = anchor_x;
                int y = anchor_y;
                if (x + hint_w > draw_area.right - 2) {
                    x = draw_area.right - 2 - hint_w;
                }
                if (y + hint_h > draw_area.bottom - 2) {
                    y = draw_area.bottom - 2 - hint_h;
                }
                if (x < draw_area.left + 2) {
                    x = draw_area.left + 2;
                }
                if (y < draw_area.top + 2) {
                    y = draw_area.top + 2;
                }
                CRect r(x, y, x + hint_w, y + hint_h);
                CFont* old_font = dc->SelectObject(&s_hint_font);
                dc->SetTextColor(RGB(71, 85, 105));
                dc->DrawText(text, &r, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);
                dc->SelectObject(old_font);
            };

            auto draw_button_id_hint_near = [&](const CRect& target, const wchar_t* text, int y_adjust) {
                draw_button_id_hint(target.right + 2, target.bottom - 7 + y_adjust, text);
            };

            auto draw_rect_marker = [&](double nx, double ny, int w, int h, bool active, const wchar_t* label) {
                CPoint p = map_pt(nx, ny);
                CRect r(p.x - w / 2, p.y - h / 2, p.x + w / 2, p.y + h / 2);
                COLORREF fill = active ? RGB(56, 189, 248) : RGB(226, 232, 240);
                COLORREF border = active ? RGB(14, 116, 144) : RGB(100, 116, 139);
                CPen pen(PS_SOLID, active ? 2 : 1, border);
                CBrush brush(fill);
                CPen* old_pen = dc->SelectObject(&pen);
                CBrush* old_brush = dc->SelectObject(&brush);
                dc->RoundRect(r, CPoint(6, 6));
                dc->SelectObject(old_brush);
                dc->SelectObject(old_pen);
                if (label && label[0] != 0) {
                    dc->SetTextColor(active ? RGB(255, 255, 255) : RGB(30, 41, 59));
                    dc->DrawText(label, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                }
                return r;
            };

            auto draw_stick = [&](double cxn, double cyn, bool axis_x_present, int axis_x_norm,
                                  bool axis_y_present, int axis_y_norm, bool click_pressed, const wchar_t* label) {
                CPoint c = map_pt(cxn, cyn);
                CPen ring_pen(PS_SOLID, click_pressed ? 3 : 2, click_pressed ? RGB(249, 115, 22) : RGB(71, 85, 105));
                CBrush ring_brush(RGB(239, 239, 239));
                CPen* old_pen = dc->SelectObject(&ring_pen);
                CBrush* old_brush = dc->SelectObject(&ring_brush);
                dc->Ellipse(c.x - stick_outer_r, c.y - stick_outer_r, c.x + stick_outer_r + 1, c.y + stick_outer_r + 1);
                dc->SelectObject(old_brush);
                dc->SelectObject(old_pen);

                double dx = 0.0;
                double dy = 0.0;
                if (axis_x_present) {
                    dx = (static_cast<double>(ClampNorm(axis_x_norm)) - 500.0) / 500.0;
                }
                if (axis_y_present) {
                    dy = (static_cast<double>(ClampNorm(axis_y_norm)) - 500.0) / 500.0;
                }
                int px = c.x + static_cast<int>(dx * static_cast<double>(stick_move_r));
                int py = c.y + static_cast<int>(dy * static_cast<double>(stick_move_r));

                CBrush knob(click_pressed ? RGB(249, 115, 22) : RGB(56, 189, 248));
                CBrush* old_knob = dc->SelectObject(&knob);
                dc->Ellipse(px - stick_knob_r, py - stick_knob_r, px + stick_knob_r + 1, py + stick_knob_r + 1);
                dc->SelectObject(old_knob);

                if (label && label[0] != 0) {
                    CRect lr(c.x - 16, c.y + stick_outer_r + 3, c.x + 16, c.y + stick_outer_r + 19);
                    dc->SetTextColor(RGB(71, 85, 105));
                    dc->DrawText(label, &lr, DT_CENTER | DT_TOP | DT_SINGLELINE);
                }
                CRect stick_bounds(c.x - stick_outer_r, c.y - stick_outer_r, c.x + stick_outer_r + 1, c.y + stick_outer_r + 1);
                return stick_bounds;
            };

            CRect outline = draw_area;
            outline.DeflateRect(4, 2);
            CPen outline_pen(PS_SOLID, 1, RGB(203, 213, 225));
            CBrush outline_brush(RGB(248, 250, 252));
            CPen* old_outline_pen = dc->SelectObject(&outline_pen);
            CBrush* old_outline_brush = dc->SelectObject(&outline_brush);
            dc->RoundRect(outline, CPoint(18, 18));
            dc->SelectObject(old_outline_brush);
            dc->SelectObject(old_outline_pen);

            CRect l1_rect = draw_rect_marker(0.30, 0.17, 42, 16, btn_on(5), L"L1");
            CRect r1_rect = draw_rect_marker(0.70, 0.17, 42, 16, btn_on(6), L"R1");
            CRect l2_rect = draw_rect_marker(0.30, 0.24, 42, 16, btn_on(7), L"L2");
            CRect r2_rect = draw_rect_marker(0.70, 0.24, 42, 16, btn_on(8), L"R2");
            draw_button_id_hint_near(l1_rect, L"B5", -2);
            draw_button_id_hint_near(r1_rect, L"B6", -2);
            draw_button_id_hint_near(l2_rect, L"B7", -2);
            draw_button_id_hint_near(r2_rect, L"B8", -2);

            draw_rect_marker(0.20, 0.38, 18, 24, dpad_up, L"U");
            draw_rect_marker(0.20, 0.62, 18, 24, dpad_down, L"D");
            draw_rect_marker(0.12, 0.50, 24, 18, dpad_left, L"L");
            draw_rect_marker(0.28, 0.50, 24, 18, dpad_right, L"R");
            draw_round_marker(0.20, 0.50, 6, (dpad_up || dpad_down || dpad_left || dpad_right), L"");

            CRect l3_rect = draw_stick(0.34, 0.72,
                                       l3_axis_x_present, l3_axis_x_norm,
                                       l3_axis_y_present, l3_axis_y_norm,
                                       btn_on(11), L"L3");
            CRect r3_rect = draw_stick(0.66, 0.72, state.z_present, state.z_norm, state.rz_present, state.rz_norm, btn_on(12), L"R3");
            draw_button_id_hint_near(l3_rect, L"B11", 0);
            draw_button_id_hint_near(r3_rect, L"B12", 0);

            CRect sel_rect = draw_rect_marker(0.41, 0.50, 52, 20, btn_on(9), L"SELECT");
            CRect sta_rect = draw_rect_marker(0.59, 0.50, 62, 20, btn_on(10), L"START");
            draw_button_id_hint_near(sel_rect, L"B9", 0);
            draw_button_id_hint_near(sta_rect, L"B10", 0);

            // ABXY in common Xbox-style diamond: Y(top), A(bottom), X(left), B(right).
            CPoint x_pt = map_pt(0.70, 0.50);
            CPoint a_pt = map_pt(0.78, 0.58);
            CPoint b_pt = map_pt(0.86, 0.50);
            CPoint y_pt = map_pt(0.78, 0.42);
            draw_round_marker(0.70, 0.50, marker_r, btn_on(1), L"X");
            draw_round_marker(0.78, 0.58, marker_r, btn_on(2), L"A");
            draw_round_marker(0.86, 0.50, marker_r, btn_on(3), L"B");
            draw_round_marker(0.78, 0.42, marker_r, btn_on(4), L"Y");
            CRect x_rect(x_pt.x - marker_r, x_pt.y - marker_r, x_pt.x + marker_r + 1, x_pt.y + marker_r + 1);
            CRect a_rect(a_pt.x - marker_r, a_pt.y - marker_r, a_pt.x + marker_r + 1, a_pt.y + marker_r + 1);
            CRect b_rect(b_pt.x - marker_r, b_pt.y - marker_r, b_pt.x + marker_r + 1, b_pt.y + marker_r + 1);
            CRect y_rect(y_pt.x - marker_r, y_pt.y - marker_r, y_pt.x + marker_r + 1, y_pt.y + marker_r + 1);
            draw_button_id_hint_near(x_rect, L"B1", 0);
            draw_button_id_hint_near(a_rect, L"B2", 0);
            draw_button_id_hint_near(b_rect, L"B3", 0);
            draw_button_id_hint_near(y_rect, L"B4", 0);

            if (state.buttons.size() > 12) {
                int extra_pressed = 0;
                for (size_t i = 12; i < state.buttons.size(); ++i) {
                    if (state.buttons[i] != 0) {
                        ++extra_pressed;
                    }
                }
                if (extra_pressed > 0) {
                    wchar_t extra_buf[64] = {};
                    swprintf_s(extra_buf, L"+%d extra button(s)", extra_pressed);
                    CRect extra_label(draw_area.left + 8, draw_area.bottom - 18, draw_area.right - 8, draw_area.bottom - 2);
                    dc->SetTextColor(RGB(30, 41, 59));
                    dc->DrawText(extra_buf, &extra_label, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                }
            }
        }
    }
}

void CRxPanel::DrawRawMappingTable(CDC* dc, const CRect& area, const std::vector<std::wstring>& lines) {
    if (!dc || area.Width() <= 0 || area.Height() <= 0) {
        return;
    }

    CRect frame = area;
    frame.DeflateRect(2, 2);
    if (frame.Width() < 120 || frame.Height() < 40) {
        return;
    }

    dc->FillSolidRect(frame, RGB(248, 250, 252));
    dc->DrawEdge(frame, EDGE_ETCHED, BF_RECT);

    CRect titleRect(frame.left + 8, frame.top + 3, frame.right - 8, frame.top + 20);
    dc->SetBkMode(TRANSPARENT);
    dc->SelectObject(&m_uiFont);
    dc->SetTextColor(RGB(15, 23, 42));
    dc->DrawText(L"Raw Byte -> Field Mapping (payload[0..63])", &titleRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    CRect body(frame.left + 8, titleRect.bottom + 2, frame.right - 8, frame.bottom - 6);
    dc->SelectObject(&m_monoFont);
    TEXTMETRIC tm = {};
    dc->GetTextMetrics(&tm);
    int line_height = tm.tmHeight + 2;
    if (line_height <= 0) {
        line_height = 14;
    }
    int max_lines = body.Height() / line_height;
    if (max_lines < 1) {
        max_lines = 1;
    }

    if (lines.empty()) {
        dc->DrawText(L"(no mapping)", &body, DT_LEFT | DT_TOP | DT_SINGLELINE);
        return;
    }

    for (int i = 0; i < max_lines && i < static_cast<int>(lines.size()); ++i) {
        CRect row(body.left, body.top + i * line_height, body.right, body.top + (i + 1) * line_height);
        dc->DrawText(lines[static_cast<size_t>(i)].c_str(), &row, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
}

void CRxPanel::AppendRow(const std::array<unsigned char, 64>& payload, const std::wstring& timestamp) {
    Row row;
    row.data = payload;
    row.timestamp = timestamp;
    row.valid = true;
    m_rows.push_back(row);
    while (m_rows.size() > 10) {
        m_rows.pop_front();
    }
}

std::wstring CRxPanel::FormatTimestamp() const {
    SYSTEMTIME now = {};
    GetLocalTime(&now);
    wchar_t buffer[32] = {};
    swprintf_s(buffer, L"%02u:%02u:%02u.%03u", now.wHour, now.wMinute, now.wSecond, now.wMilliseconds);
    return buffer;
}

void CRxPanel::OnSaveLogClicked() {
    bool enabled = (m_saveCheck.GetCheck() == BST_CHECKED);
    m_logEnabled = enabled;
    m_logger.Enable(enabled);
}

void CRxPanel::OnTimestampClicked() {
    m_timestampEnabled = (m_timestampCheck.GetCheck() == BST_CHECKED);
    InvalidateRect(m_headerRect, FALSE);
    InvalidateRect(m_gridRect, FALSE);
}

void CRxPanel::OnClearClicked() {
    ClearData();
}

void CRxPanel::OnSwapDpadL3Clicked() {
    m_swapDpadL3Enabled = (m_swapDpadL3Check.GetCheck() == BST_CHECKED);
    InvalidateRect(m_visualRect, FALSE);
}

void CRxPanel::OnCopyRowClicked(UINT nID) {
    int row = static_cast<int>(nID) - kRxCopyBase;
    if (row < 0 || row >= kRowCount) {
        return;
    }
    if (row >= static_cast<int>(m_rows.size()) || !m_rows[static_cast<size_t>(row)].valid) {
        return;
    }
    const auto& data = m_rows[static_cast<size_t>(row)].data;
    std::wstring text = BytesToHex(data.data(), data.size());
    CopyTextToClipboard(text);
}

void CRxPanel::OnTimer(UINT_PTR nIDEvent) {
    if (nIDEvent == kUiTimerId) {
        ProcessPendingPayloads();
        return;
    }
    CWnd::OnTimer(nIDEvent);
}

bool CRxPanel::WaitForPayload(const std::vector<unsigned char>& payload, DWORD timeout_ms) {
    if (!m_rxEvent) {
        return false;
    }
    if (payload.size() < 64) {
        return false;
    }
    DWORD remaining = timeout_ms;
    DWORD start_tick = GetTickCount();
    while (remaining > 0) {
        DWORD wait = WaitForSingleObject(m_rxEvent, remaining);
        if (wait == WAIT_TIMEOUT) {
            return false;
        }
        if (wait != WAIT_OBJECT_0) {
            return false;
        }
        std::vector<unsigned char> last;
        EnterCriticalSection(&m_payloadLock);
        last = m_lastPayload;
        LeaveCriticalSection(&m_payloadLock);
        if (last.size() >= 64 && std::equal(payload.begin(), payload.begin() + 64, last.begin())) {
            return true;
        }
        DWORD now = GetTickCount();
        DWORD elapsed = now - start_tick;
        if (elapsed >= timeout_ms) {
            return false;
        }
        remaining = timeout_ms - elapsed;
    }
    return false;
}

bool CRxPanel::GetLastPayloadHex(std::wstring* out_text) {
    if (!out_text) {
        return false;
    }
    std::vector<unsigned char> payload;
    EnterCriticalSection(&m_payloadLock);
    payload = m_lastPayload;
    LeaveCriticalSection(&m_payloadLock);

    if (payload.empty()) {
        out_text->clear();
        return false;
    }
    *out_text = BytesToHex(payload.data(), payload.size());
    return true;
}

void CRxPanel::LogTxLine(const std::wstring& line) {
    if (line.empty()) {
        return;
    }
    WriteDirectionalLogLine(L"TX", line, m_timestampEnabled.load());
}

bool CRxPanel::GetLastPayload(std::vector<unsigned char>* out_payload) {
    if (!out_payload) {
        return false;
    }
    EnterCriticalSection(&m_payloadLock);
    *out_payload = m_lastPayload;
    LeaveCriticalSection(&m_payloadLock);

    if (out_payload->empty()) {
        out_payload->clear();
        return false;
    }
    return true;
}

bool CRxPanel::GetLatestJoystickSummary(std::wstring* out_text) {
    if (!out_text) {
        return false;
    }
    EnterCriticalSection(&m_payloadLock);
    *out_text = m_lastJoystickSummary;
    LeaveCriticalSection(&m_payloadLock);
    return !out_text->empty();
}

void CRxPanel::SetDescriptorPreview(const HidJoystickVisualState& state,
                                    const std::vector<std::wstring>& mapping_lines) {
    std::vector<std::wstring> lines = mapping_lines;
    if (lines.empty()) {
        lines.push_back(L"(waiting for descriptor mapping)");
    }
    std::wstring summary = BuildJoystickSummary(state);

    EnterCriticalSection(&m_payloadLock);
    m_lastVisualState = state;
    m_lastRawMappingLines = lines;
    m_lastJoystickSummary = summary;
    LeaveCriticalSection(&m_payloadLock);

    if (GetSafeHwnd()) {
        InvalidateRect(m_visualRect, FALSE);
        InvalidateRect(m_mappingRect, FALSE);
    }
}

bool CRxPanel::IsLogEnabled() const {
    return m_saveCheck.GetCheck() == BST_CHECKED;
}

bool CRxPanel::IsTimestampEnabled() const {
    return m_timestampCheck.GetCheck() == BST_CHECKED;
}

void CRxPanel::SetLogEnabled(bool enabled) {
    m_saveCheck.SetCheck(enabled ? BST_CHECKED : BST_UNCHECKED);
    m_logEnabled = enabled;
    m_logger.Enable(enabled);
}

void CRxPanel::SetTimestampEnabled(bool enabled) {
    m_timestampCheck.SetCheck(enabled ? BST_CHECKED : BST_UNCHECKED);
    m_timestampEnabled = enabled;
    InvalidateRect(m_headerRect, FALSE);
    InvalidateRect(m_gridRect, FALSE);
}

void CRxPanel::SetExternalGridMetrics(int start_x, int cell_base, int cell_extra, int row_height, int row_gap) {
    if (start_x <= 0 || cell_base <= 0) {
        return;
    }
    m_hasExternalGrid = true;
    m_externalStartX = start_x;
    m_externalCellBase = cell_base;
    m_externalCellExtra = cell_extra < 0 ? 0 : cell_extra;
    m_externalRowHeight = row_height;
    m_externalRowGap = row_gap;

    if (GetSafeHwnd()) {
        CRect rect;
        GetClientRect(&rect);
        LayoutControls(rect.Width(), rect.Height());
        Invalidate(FALSE);
    }
}

void CRxPanel::SetControlsEnabled(bool enabled) {
    m_saveCheck.EnableWindow(TRUE);
    m_timestampCheck.EnableWindow(TRUE);
    (void)enabled;
}

void CRxPanel::SetClearEnabled(bool enabled) {
    m_clearButton.EnableWindow(enabled);
}

void CRxPanel::ClearData() {
    m_rows.clear();
    EnterCriticalSection(&m_payloadLock);
    m_lastPayload.clear();
    m_lastJoystickSummary = L"X=0 Y=0 Z=0 Rz=0 POV=CENTER BTN=none";
    m_lastVisualState = HidJoystickVisualState{};
    m_lastRawMappingLines.clear();
    m_lastRawMappingLines.push_back(L"(waiting for descriptor mapping)");
    LeaveCriticalSection(&m_payloadLock);
    m_cleared = true;
    EnterCriticalSection(&m_pendingLock);
    m_pendingPayloads.clear();
    LeaveCriticalSection(&m_pendingLock);
    UpdateGridControls();
    SetRxState(RxIdle);
    Invalidate(FALSE);
}

void CRxPanel::ProcessPendingPayloads() {
    std::deque<std::array<unsigned char, 64>> batch;
    EnterCriticalSection(&m_pendingLock);
    if (!m_pendingPayloads.empty()) {
        batch.swap(m_pendingPayloads);
    }
    LeaveCriticalSection(&m_pendingLock);

    if (batch.empty()) {
        return;
    }

    bool timestamped = m_timestampEnabled.load();
    for (const auto& bytes : batch) {
        m_cleared = false;
        std::wstring timestamp;
        if (timestamped) {
            timestamp = FormatTimestamp();
        }
        AppendRow(bytes, timestamp);
    }

    UpdateGridControls();
    InvalidateRect(m_visualRect, FALSE);
    InvalidateRect(m_mappingRect, FALSE);
    InvalidateRect(m_gridRect, FALSE);
}

void CRxPanel::WriteDirectionalLogLine(const std::wstring& direction, const std::wstring& body, bool timestamped) {
    if (!m_logger.IsEnabled() || direction.empty() || body.empty()) {
        return;
    }
    std::wstring line;
    if (timestamped) {
        line = L"[" + FormatTimestamp() + L"] [" + direction + L"] [" + body + L"]";
    } else {
        line = L"[" + direction + L"] [" + body + L"]";
    }
    std::wstring error;
    m_logger.WriteLine(line, &error);
}

void CRxPanel::UpdateGridControls() {
    if (!m_cells[0][0].GetSafeHwnd()) {
        return;
    }
    size_t row_count = m_rows.size();
    for (int row = 0; row < kRowCount; ++row) {
        bool valid = row < static_cast<int>(row_count) && m_rows[static_cast<size_t>(row)].valid;
        if (m_copyButtons[row].GetSafeHwnd()) {
            m_copyButtons[row].EnableWindow(valid ? TRUE : FALSE);
        }
        for (int col = 0; col < kByteCount; ++col) {
            if (!valid) {
                m_cells[row][col].SetWindowTextW(L"");
                continue;
            }
            unsigned char value = m_rows[static_cast<size_t>(row)].data[static_cast<size_t>(col)];
            wchar_t buf[4] = {};
            swprintf_s(buf, L"%02X", value);
            m_cells[row][col].SetWindowTextW(buf);
        }
    }
}

void CRxPanel::SetRxState(int state) {
    if (m_rxState == state) {
        return;
    }
    m_rxState = state;
    InvalidateRect(m_headerRect, FALSE);
}

int CRxPanel::GetPreferredHeight() const {
    const int margin = 10;
    const int title_height = 16;
    const int gap = 8;
    const int top_height = 24;
    const int visual_height = 150;
    const int mapping_height = 92;

    int header_height = m_headerHeight;
    int row_height = m_rowHeight;
    int row_gap = m_rowGap;
    if (header_height <= 0 || row_height <= 0) {
        CClientDC dc(const_cast<CRxPanel*>(this));
        CFont* old = dc.SelectObject(const_cast<CFont*>(&m_monoFont));
        TEXTMETRIC tm = {};
        if (dc.GetTextMetrics(&tm)) {
            header_height = tm.tmHeight + 4;
            row_height = tm.tmHeight + 6;
        } else {
            header_height = 16;
            row_height = 24;
        }
        dc.SelectObject(old);
    }

    int total = margin + title_height;
    total += top_height + gap;
    total += visual_height + gap;
    total += mapping_height + gap;
    total += header_height + 4;
    total += row_height * 10 + row_gap * 9;
    return total;
}
