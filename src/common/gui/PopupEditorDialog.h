//-------------------------------------------------------------------------------------------------------
//
//	Shortcircuit
//
//	Copyright 2004 Claes Johanson
//
//-------------------------------------------------------------------------------------------------------
#pragma once

#if !MAC

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
