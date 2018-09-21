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

class CMiniEditDialog : public CDialogImpl<CMiniEditDialog>
{
public:
   CMiniEditDialog::CMiniEditDialog()
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

   BEGIN_MSG_MAP_EX(CMiniEditDialog)
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

   inline void CMiniEditDialog::SetText(char* a)
   {
      strncpy(textdata, a, TextDataSize);
   }

   inline void CMiniEditDialog::SetValue(int a)
   {
      sprintf(textdata, "%i", a);
   }

   inline void CMiniEditDialog::SetFValue(float a)
   {
      sprintf(textdata, "%f", a);
   }

   inline void CMiniEditDialog::OnCancel(UINT uCode, int nID, HWND hWndCtl)
   {
      updated = false;
      EndDialog(nID);
   }

   inline void CMiniEditDialog::OnOK(UINT uCode, int nID, HWND hWndCtl)
   {
      WTL::CEdit ce;
      ce.Attach(GetDlgItem(IDC_TEDIT));
      memset(textdata, 0, TextDataSize);
      ce.GetLine(0, textdata, TextDataSize);
      ce.Detach();
      if (irange > 0) // try to convert string to integer
      {
         ivalue = min(irange - 1, atoi(textdata));
      }
      fvalue = (float)atof(textdata);

      updated = true;
      EndDialog(nID);
   }
};

#endif