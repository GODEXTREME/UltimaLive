/* Copyright(c) 2016 UltimaLive
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "ProgressBarDialog.h"

BOOL CALLBACK ProgressBarDialog::DialogProc (HWND, UINT message, WPARAM, LPARAM)
{
  switch(message)
  {
    case WM_INITDIALOG:
    {
    }
    break;

    default:
    {
      return false;
    }
  }
  return true;
}

HMODULE GetCurrentModuleHandle()
{
    HMODULE hMod = NULL;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
          reinterpret_cast<LPCWSTR>(&GetCurrentModuleHandle),
          &hMod);
     return hMod;
}

void ProgressBarDialog::setProgress(uint32_t progress)
{
  HWND hwndProgressBar = GetDlgItem(m_hDialog, IDC_PROGRESS1);
  SendMessage(hwndProgressBar, PBM_SETPOS, progress, 0);
}

ProgressBarDialog::ProgressBarDialog()
  : m_hDialog()
{
}

void ProgressBarDialog::setMessage(std::string message)
{
  HWND hwndProgressBarMessage = GetDlgItem(m_hDialog,  IDC_IMPORT_MESSAGE);
  SetWindowText(hwndProgressBarMessage, message.c_str());      
}

void ProgressBarDialog::show()
{
  m_hDialog = CreateDialog ((HINSTANCE)GetCurrentModuleHandle(), MAKEINTRESOURCE (IDD_DIALOG1), 0, ProgressBarDialog::DialogProc);
}

void ProgressBarDialog::hide()
{
  DestroyWindow(m_hDialog);
}