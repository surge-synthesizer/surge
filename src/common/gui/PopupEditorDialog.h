/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2020 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#pragma once

#if WINDOWS

#include <atlctrls.h>
#include <atlCRACK.h>
#include <algorithm>

class PopupEditorDialog : public CDialogImpl<PopupEditorDialog>
{
public:
   PopupEditorDialog::PopupEditorDialog()
   {
      irange = -1;
   }
   enum
   {
      IDD = IDD_EVALUE
   };
   enum
   {
      TextDataSize = 256
   };
   char textdata[TextDataSize];

   char prompt[TextDataSize], title[TextDataSize];
   int ivalue;
   float fvalue;
   int irange;
   bool updated;

   BEGIN_MSG_MAP_EX(PopupEditorDialog)
   COMMAND_HANDLER_EX(IDOK, BN_CLICKED, OnOK)
   COMMAND_HANDLER_EX(IDCANCEL, BN_CLICKED, OnCancel)
   MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
   END_MSG_MAP()

   LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
   {
      // this->CenterWindow(::GetActiveWindow());
      // Do some initialization code
      WTL::CEdit ce;
      ce.Attach(GetDlgItem(IDC_TEDIT));
      ce.AppendText(textdata);
      // ce.SetLimitText(16);
      ce.Detach();

      WTL::CStatic cs;
      cs.Attach(GetDlgItem(IDC_TPROMPT));
      cs.SetWindowText(prompt);
      // ce.SetLimitText(16);
      cs.Detach();

      SetWindowText( title );

      return 1;
   }

   inline void PopupEditorDialog::SetText(char* a)
   {
      strncpy(textdata, a, TextDataSize);
   }

   inline void PopupEditorDialog::SetValue(int a)
   {
      sprintf(textdata, "%i", a);
   }

   inline void PopupEditorDialog::SetFValue(float a)
   {
      sprintf(textdata, "%f", a);
   }

   inline void PopupEditorDialog::OnCancel(UINT uCode, int nID, HWND hWndCtl)
   {
      updated = false;
      EndDialog(nID);
   }

   inline void PopupEditorDialog::OnOK(UINT uCode, int nID, HWND hWndCtl)
   {
      WTL::CEdit ce;
      ce.Attach(GetDlgItem(IDC_TEDIT));
      memset(textdata, 0, TextDataSize);
      ce.GetLine(0, textdata, TextDataSize);
      ce.Detach();
      if (irange > 0) // try to convert string to integer
      {
         ivalue = std::min(irange - 1, atoi(textdata));
      }
      fvalue = (float)atof(textdata);

      updated = true;
      EndDialog(nID);
   }
};

#endif
