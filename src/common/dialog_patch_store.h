//-------------------------------------------------------------------------------------------------------
//
//	SURGE
//
//	Copyright 2004-2005 Claes Johanson
//
//-------------------------------------------------------------------------------------------------------
#pragma once

#include "storage.h"
#include <string>
using namespace std;

class CPatchStoreDialog : public CDialogImpl<CPatchStoreDialog>
{
public:	
	CPatchStoreDialog::CPatchStoreDialog()
	{

	}
	enum { IDD = IDD_SAVEPATCH };	
	bool updated;
	string name,category,comment,author;
	vector<patchlist_category> *patch_category;
	int startcategory;

	BEGIN_MSG_MAP_EX(CPatchStoreDialog)
		COMMAND_HANDLER_EX(IDOK,BN_CLICKED,OnOK)
		COMMAND_HANDLER_EX(IDCANCEL,BN_CLICKED,OnCancel)	
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)		
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, 
	LPARAM lParam, BOOL& bHandled)
	{		
		//this->CenterWindow(::GetActiveWindow());
		// Do some initialization code
		WTL::CEdit ce;		
		ce.Attach(GetDlgItem(IDC_NAME));
		ce.AppendText(name.c_str());			
		ce.Detach();

		ce.Attach(GetDlgItem(IDC_AUTHOR));
		ce.AppendText(author.c_str());			
		ce.Detach();
		
		ce.Attach(GetDlgItem(IDC_DESCRIPTION));
		ce.AppendText(comment.c_str());			
		ce.Detach();

		WTL::CComboBox ccb;
		ccb.Attach(GetDlgItem(IDC_CATEGORY));
		int n = patch_category->size();
		for(int i=startcategory; i<n; i++)
		{
			ccb.AddString(patch_category->at(i).name.c_str());
		}
		ccb.SelectString(-1,category.c_str());		
		ccb.Detach();

		return 1;
	}

	inline void CPatchStoreDialog::OnCancel ( UINT uCode, int nID, HWND hWndCtl )
	{				
		updated = false;
		EndDialog(nID);
	}

	inline void CPatchStoreDialog::OnOK ( UINT uCode, int nID, HWND hWndCtl )
	{	
		WTL::CEdit ce;
		WTL::CComboBox ccb;
		WTL::CListBox clb;	
		const int TextSize = 2048;
		char textdata[TextSize];
		
		ce.Attach(GetDlgItem(IDC_NAME));
		memset(textdata, 0, TextSize);
		ce.GetLine(0,textdata,TextSize);
		name = textdata;		
		ce.Detach();

		ce.Attach(GetDlgItem(IDC_AUTHOR));
		memset(textdata, 0, TextSize);
		ce.GetLine(0,textdata,TextSize);
		author = textdata;		
		ce.Detach();
		
		ce.Attach(GetDlgItem(IDC_DESCRIPTION));
		memset(textdata, 0, TextSize);
		ce.GetWindowTextA(textdata,TextSize);
		comment = textdata;
		ce.Detach();

		ccb.Attach(GetDlgItem(IDC_CATEGORY));
		//int sel = ccb.GetCurSel();
		memset(textdata, 0, TextSize);
		ccb.GetWindowTextA(textdata,TextSize);
		category = textdata;
		ccb.Detach();
		

		/*if ((sel>0)&&(sel<patch_category->size()))
		{
			category = patch_category->at(sel).name.c_str();
		};*/

		updated = true;
		EndDialog(nID);
	}	
};