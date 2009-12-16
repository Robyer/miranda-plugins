/* 
Copyright (C) 2009 Ricardo Pescuma Domenecci

This is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with this file; see the file license.txt.  If
not, write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  
*/

#include "commons.h"

#define NUM_LINES 3


static INT_PTR CALLBACK DlgProcAccMgrUI(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
static int static_iaxc_callback(iaxc_event e, void *param);


IAXProto::IAXProto(const char *aProtoName, const TCHAR *aUserName)
{
	InitializeCriticalSection(&cs);

	reg_id = 0;
	hNetlibUser = 0;
	hCallStateEvent = 0;
	m_iDesiredStatus = m_iStatus = ID_STATUS_OFFLINE;

	m_tszUserName = mir_tstrdup(aUserName);
	m_szProtoName = mir_strdup(aProtoName);
	m_szModuleName = mir_strdup(aProtoName);

	static OptPageControl ctrls[] = { 
		{ NULL,	CONTROL_TEXT,		IDC_HOST,			"Host", NULL, 0, 0, 256 },
		{ NULL,	CONTROL_INT,		IDC_PORT,			"Port", 4569, 0, 1 },
		{ NULL,	CONTROL_TEXT,		IDC_USERNAME,		"Username", NULL, 0, 0, 16 },
		{ NULL,	CONTROL_PASSWORD,	IDC_PASSWORD,		"Password", NULL, 0, 0, 16 },
		{ NULL,	CONTROL_CHECKBOX,	IDC_SAVEPASSWORD,	"SavePassword", (BYTE) TRUE },
	};

	memmove(accountManagerCtrls, ctrls, sizeof(accountManagerCtrls));
	accountManagerCtrls[0].var = &opts.host;
	accountManagerCtrls[1].var = &opts.port;
	accountManagerCtrls[2].var = &opts.username;
	accountManagerCtrls[3].var = &opts.password;
	accountManagerCtrls[4].var = &opts.savePassword;

	LoadOpts(accountManagerCtrls, MAX_REGS(accountManagerCtrls), m_szModuleName);

	CreateProtoService(PS_CREATEACCMGRUI, &IAXProto::CreateAccMgrUI);
}


IAXProto::~IAXProto()
{
	Netlib_CloseHandle(hNetlibUser);

	mir_free(m_tszUserName);
	mir_free(m_szProtoName);
	mir_free(m_szModuleName);

	DeleteCriticalSection(&cs);
}


DWORD_PTR __cdecl IAXProto::GetCaps( int type, HANDLE hContact ) 
{
	switch(type) 
	{
		case PFLAGNUM_1:
			return 0;

		case PFLAGNUM_2:
			return PF2_ONLINE | PF2_SHORTAWAY | PF2_LONGAWAY | PF2_LIGHTDND | PF2_HEAVYDND 
					 | PF2_FREECHAT | PF2_OUTTOLUNCH | PF2_ONTHEPHONE | PF2_INVISIBLE | PF2_IDLE;

		case PFLAGNUM_3:
			return 0;

		case PFLAGNUM_4:
			return PF4_NOCUSTOMAUTH;

		case PFLAG_UNIQUEIDTEXT:
			return NULL;

		case PFLAG_UNIQUEIDSETTING:
			return NULL;

		case PFLAG_MAXLENOFMESSAGE:
			return 100;
	}

	return 0;
}


int __cdecl IAXProto::SetStatus( int iNewStatus ) 
{
	if (m_iStatus == iNewStatus) 
		return 0;
	m_iDesiredStatus = iNewStatus;

	if (m_iDesiredStatus == ID_STATUS_OFFLINE)
	{
		Disconnect();
	}
	else if (m_iStatus == ID_STATUS_OFFLINE)
	{
		Trace(_T("Connecting..."));

		BroadcastStatus(ID_STATUS_CONNECTING);

		TCHAR server_port[1024];
		mir_sntprintf(server_port, MAX_REGS(server_port), _T("%s:%d"), opts.host, opts.port);

		reg_id = iaxc_register(TcharToUtf8(opts.username), opts.password, TcharToUtf8(server_port));
		if (reg_id <= 0)
		{
			Error(TranslateT("Error registering with IAX"));
			BroadcastStatus(ID_STATUS_OFFLINE);
			return -1;
		}
	}
	else if (m_iStatus > ID_STATUS_OFFLINE)
	{
		BroadcastStatus(m_iDesiredStatus);
	}

	return 0; 
}


INT_PTR  __cdecl IAXProto::CreateAccMgrUI(WPARAM wParam, LPARAM lParam)
{
	return (INT_PTR) CreateDialogParam(hInst, MAKEINTRESOURCE(IDD_ACCMGRUI), 
										(HWND) lParam, DlgProcAccMgrUI, (LPARAM)this);
}


void IAXProto::BroadcastStatus(int newStatus)
{
	Trace(_T("BroadcastStatus %d"), newStatus);

	int oldStatus = m_iStatus;
	m_iStatus = newStatus;
	SendBroadcast(NULL, ACKTYPE_STATUS, ACKRESULT_SUCCESS, (HANDLE)oldStatus, m_iStatus);
}


void IAXProto::CreateProtoService(const char* szService, IAXServiceFunc serviceProc)
{
	char str[MAXMODULELABELLENGTH];

	mir_snprintf(str, sizeof(str), "%s%s", m_szModuleName, szService);
	::CreateServiceFunctionObj(str, (MIRANDASERVICEOBJ)*(void**)&serviceProc, this);
}


void IAXProto::CreateProtoService(const char* szService, IAXServiceFuncParam serviceProc, LPARAM lParam)
{
	char str[MAXMODULELABELLENGTH];
	mir_snprintf(str, sizeof(str), "%s%s", m_szModuleName, szService);
	::CreateServiceFunctionObjParam(str, (MIRANDASERVICEOBJPARAM)*(void**)&serviceProc, this, lParam);
}


HANDLE IAXProto::CreateProtoEvent(const char* szService)
{
	char str[MAXMODULELABELLENGTH];
	mir_snprintf(str, sizeof(str), "%s%s", m_szModuleName, szService);
	return ::CreateHookableEvent(str);
}


int IAXProto::SendBroadcast(HANDLE hContact, int type, int result, HANDLE hProcess, LPARAM lParam)
{
	ACKDATA ack = {0};
	ack.cbSize = sizeof(ACKDATA);
	ack.szModule = m_szModuleName;
	ack.hContact = hContact;
	ack.type = type;
	ack.result = result;
	ack.hProcess = hProcess;
	ack.lParam = lParam;
	return CallService(MS_PROTO_BROADCASTACK, 0, (LPARAM)&ack);
}


#define MESSAGE_TYPE_TRACE 0
#define MESSAGE_TYPE_INFO 1
#define MESSAGE_TYPE_ERROR 2


void IAXProto::Trace(TCHAR *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	ShowMessage(MESSAGE_TYPE_TRACE, fmt, args);

	va_end(args);
}


void IAXProto::Info(TCHAR *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	ShowMessage(MESSAGE_TYPE_INFO, fmt, args);

	va_end(args);
}


void IAXProto::Error(TCHAR *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	ShowMessage(MESSAGE_TYPE_ERROR, fmt, args);

	va_end(args);
}


void IAXProto::ShowMessage(int type, TCHAR *fmt, va_list args) 
{
	TCHAR buff[1024];
	if (type == MESSAGE_TYPE_TRACE)
		lstrcpy(buff, _T("TRACE: "));
	else if (type == MESSAGE_TYPE_INFO)
		lstrcpy(buff, _T("INFO : "));
	else
		lstrcpy(buff, _T("ERROR: "));

	mir_vsntprintf(&buff[7], MAX_REGS(buff)-7, fmt, args);

	OutputDebugString(buff);
	OutputDebugString(_T("\n"));
	CallService(MS_NETLIB_LOG, (WPARAM) hNetlibUser, (LPARAM) TcharToChar(buff).get());
}


void IAXProto::Disconnect()
{
	Trace(_T("Disconnecting..."));

	iaxc_dump_all_calls();

	if (reg_id > 0)
	{
		iaxc_unregister(reg_id);
		reg_id = 0;
	}

	BroadcastStatus(ID_STATUS_OFFLINE);
}


int IAXProto::levels_callback(iaxc_ev_levels &levels)
{
	return 0;
}


int IAXProto::text_callback(iaxc_ev_text &text)
{
	switch(text.type) 
	{
		case IAXC_TEXT_TYPE_STATUS:
		{
			Trace(_T("Status: %s"), Utf8ToTchar(text.message));
			return 1;
		}
		case IAXC_TEXT_TYPE_NOTICE:
		{
			Info(TranslateT("Notice: %s"), Utf8ToTchar(text.message));
			return 1;
		}
		case IAXC_TEXT_TYPE_ERROR:
		{
			Error(Utf8ToTchar(text.message));
			return 1;
		}
		case IAXC_TEXT_TYPE_FATALERROR:
		{
			Error(TranslateT("Fatal: %s"), Utf8ToTchar(text.message));
			Disconnect();
			return 1;
		}
		case IAXC_TEXT_TYPE_IAX:
		{
			Trace(_T("IAX: %s"), Utf8ToTchar(text.message));
			return 1;
		}
		default:
			return 0;
	}

	return 0;
}


int IAXProto::state_callback(iaxc_ev_call_state &call)
{
	bool outgoing = ((call.state & IAXC_CALL_STATE_OUTGOING) != 0);

	TCHAR buffer[256] = {0};
	TCHAR *number = NULL;

	if (!outgoing)
	{
		mir_sntprintf(buffer, MAX_REGS(buffer), _T("%s %s"), 
						Utf8ToTchar(call.remote_name).get(), 
						Utf8ToTchar(call.remote).get());
		lstrtrim(buffer);
		number = buffer;
	}

	Trace(_T("callNo %d state %d"), call.callNo, call.state);

	if (call.state & IAXC_CALL_STATE_FREE)
	{
		NotifyCall(call.callNo, VOICE_STATE_ENDED);
	}
	else if (call.state & IAXC_CALL_STATE_BUSY)
	{
		NotifyCall(call.callNo, VOICE_STATE_BUSY, NULL, number);
	}
	else if (call.state & IAXC_CALL_STATE_RINGING)
	{
		if (outgoing)
			NotifyCall(call.callNo, VOICE_STATE_CALLING, NULL, number);
		else
			NotifyCall(call.callNo, VOICE_STATE_RINGING, NULL, number);
	}
	else if (call.state & IAXC_CALL_STATE_SELECTED)
	{
		NotifyCall(call.callNo, VOICE_STATE_TALKING, NULL, number);
	}
	else
	{
		NotifyCall(call.callNo, VOICE_STATE_ON_HOLD, NULL, number);
	}

	return 0;
}


int IAXProto::netstats_callback(iaxc_ev_netstats &netstats)
{
	Trace(_T("netstats"));
	return 0;
}


int IAXProto::url_callback(iaxc_ev_url &url)
{
	Trace(_T("URL"));
	return 0;
}


int IAXProto::registration_callback(iaxc_ev_registration &reg)
{
	Trace(_T("registration_callback %d : %d"), reg.id, reg.reply);

	if (reg.id != reg_id)
		return 0;

	switch(reg.reply)
	{
		case IAXC_REGISTRATION_REPLY_ACK:
		{
			BroadcastStatus(m_iDesiredStatus > ID_STATUS_OFFLINE ? m_iDesiredStatus : ID_STATUS_ONLINE);

			if (reg.msgcount > 0) 
				Info(TranslateT("You have %d voicemail message(s)"), reg.msgcount);

			return 1;
		}
		case IAXC_REGISTRATION_REPLY_REJ:
		{
			Error(TranslateT("Registration rejected"));
			Disconnect();

			return 1;
		}
		case IAXC_REGISTRATION_REPLY_TIMEOUT:
		{
			Error(TranslateT("Registration timeout"));
			Disconnect();

			return 1;
		}
		default:
			return 0;
	}
}


int IAXProto::iaxc_callback(iaxc_event &e)
{
	switch(e.type) 
	{
		case IAXC_EVENT_LEVELS:			return levels_callback(e.ev.levels);
		case IAXC_EVENT_TEXT:			return text_callback(e.ev.text);
		case IAXC_EVENT_STATE:			return state_callback(e.ev.call);
		case IAXC_EVENT_NETSTAT:		return netstats_callback(e.ev.netstats);
		case IAXC_EVENT_URL:			return url_callback(e.ev.url);
		case IAXC_EVENT_REGISTRATION:	return registration_callback(e.ev.reg);
		default:						return 0;  // not handled
	}
}


static void CALLBACK ProcessIAXEvents(void *param)
{
	for(int i = 0; i < instances.getCount(); ++i)
	{
		IAXProto *proto = &instances[i];

		EnterCriticalSection(&proto->cs);

		for(std::vector<iaxc_event>::iterator it = proto->events.begin(); it != proto->events.end(); ++it)
			proto->iaxc_callback(*it);
		
		proto->events.clear();

		LeaveCriticalSection(&proto->cs);
	}
}


static int static_iaxc_callback(iaxc_event e, void *param)
{
	IAXProto *proto = (IAXProto *) param;

	EnterCriticalSection(&proto->cs);
	proto->events.push_back(e);
	LeaveCriticalSection(&proto->cs);

	CallFunctionAsync(&ProcessIAXEvents, NULL);

	return 0;
}



static INT_PTR CALLBACK DlgProcAccMgrUI(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	IAXProto *proto = (IAXProto *) GetWindowLongPtr(hwnd, GWLP_USERDATA);

	switch(msg) 
	{
		case WM_INITDIALOG: 
		{
			SetWindowLongPtr(hwnd, GWLP_USERDATA, lParam);
			proto = (IAXProto *) lParam;
			break;
		}

		case WM_NOTIFY:
		{
			if (((LPNMHDR)lParam)->code == (UINT)PSN_APPLY) 
			{
			}
			break;
		}
	}

	if (proto == NULL)
		return FALSE;

	return SaveOptsDlgProc(proto->accountManagerCtrls, MAX_REGS(proto->accountManagerCtrls), 
							proto->m_szModuleName, hwnd, msg, wParam, lParam);
}


int __cdecl IAXProto::OnEvent( PROTOEVENTTYPE iEventType, WPARAM wParam, LPARAM lParam ) 
{
	switch(iEventType) 
	{
		case EV_PROTO_ONLOAD:    
			return OnModulesLoaded(0, 0);

		case EV_PROTO_ONEXIT:    
			return OnPreShutdown(0, 0);

		case EV_PROTO_ONOPTIONS: 
			return OnOptionsInit(wParam, lParam);

		case EV_PROTO_ONRENAME:
		{	
			break;
		}
	}
	return 1;
}


int __cdecl IAXProto::OnModulesLoaded(WPARAM wParam, LPARAM lParam)
{
	TCHAR buffer[MAX_PATH]; 
	mir_sntprintf(buffer, MAX_REGS(buffer), TranslateT("%s plugin connections"), m_tszUserName);
		
	NETLIBUSER nl_user = {0};
	nl_user.cbSize = sizeof(nl_user);
	nl_user.flags = NUF_INCOMING | NUF_OUTGOING | NUF_TCHAR;
	nl_user.szSettingsModule = m_szModuleName;
	nl_user.ptszDescriptiveName = buffer;
	hNetlibUser = (HANDLE) CallService(MS_NETLIB_REGISTERUSER, 0, (LPARAM)&nl_user);

	if (!ServiceExists(MS_VOICESERVICE_REGISTER))
	{
		Error(TranslateT("IAX needs Voice Service plugin to work!"));
		return 1;
	}

	iaxc_set_event_callback(&static_iaxc_callback, this); 
	
	// TODO Handle network out port
	iaxc_set_preferred_source_udp_port(-1);
	if (iaxc_initialize(NUM_LINES))
	{
		Error(TranslateT("Failed to initialize iaxc lib"));
		return 1;
	}

	iaxc_set_formats(IAXC_FORMAT_SPEEX,
					 IAXC_FORMAT_ULAW|IAXC_FORMAT_ALAW|IAXC_FORMAT_GSM|IAXC_FORMAT_SPEEX|IAXC_FORMAT_ILBC);
	iaxc_set_speex_settings(1,-1,-1,0,8000,3);
	iaxc_set_silence_threshold(-99);

	if (iaxc_start_processing_thread())
	{
		Error(TranslateT("Failed to initialize iax threads"));
		return 1;
	}


	hCallStateEvent = CreateProtoEvent(PE_VOICE_CALL_STATE);

	VOICE_MODULE vm = {0};
	vm.cbSize = sizeof(vm);
	vm.name = m_szModuleName;
	vm.flags = VOICE_CAPS_CALL_STRING | VOICE_CAPS_CAN_HOLD;
	CallService(MS_VOICESERVICE_REGISTER, (WPARAM) &vm, 0);

	return 0;
}


int __cdecl IAXProto::OnOptionsInit(WPARAM wParam, LPARAM lParam)
{
	return 0;
}


int __cdecl IAXProto::OnPreShutdown(WPARAM wParam, LPARAM lParam)
{
	CallService(MS_VOICESERVICE_UNREGISTER, (WPARAM) m_szModuleName, 0);

	iaxc_stop_processing_thread();

	iaxc_shutdown();

	return 0;
}


void IAXProto::NotifyCall(int callNo, int state, HANDLE hContact, TCHAR *number)
{
	char tmp[16];

	VOICE_CALL vc = {0};
	vc.cbSize = sizeof(vc);
	vc.szModule = m_szModuleName;
	vc.id = itoa(callNo, tmp, 10);
	vc.flags = VOICE_TCHAR;
	vc.hContact = hContact;
	vc.ptszNumber = number;
	vc.state = state;

	NotifyEventHooks(hCallStateEvent, (WPARAM) &vc, 0);
}


int __cdecl IAXProto::VoiceCall(WPARAM wParam, LPARAM lParam)
{
	HANDLE hContact = (HANDLE) wParam;
	TCHAR *number = (TCHAR *) lParam;

	if (number == NULL || number[0] == 0)
		return 1;

	if (iaxc_first_free_call() < 0)
		return 2;

	int callNo = iaxc_call_ex(TcharToUtf8(number), NULL, NULL, FALSE);
	if (callNo < 0 || callNo >= NUM_LINES)
		return 3;

	NotifyCall(callNo, VOICE_STATE_CALLING, hContact, number);

	return 0;
}


int __cdecl IAXProto::VoiceAnswerCall(WPARAM wParam, LPARAM lParam)
{
	char *id = (char *) wParam;
	if (id == NULL || id[0] == 0)
		return 1;

	int callNo = atoi(id);
	if (callNo < 0 || callNo >= NUM_LINES)
		return 2;

	if (iaxc_select_call(callNo) < 0)
		return 3;

	return 0;
}


int __cdecl IAXProto::VoiceDropCall(WPARAM wParam, LPARAM lParam)
{
	char *id = (char *) wParam;
	if (id == NULL || id[0] == 0)
		return 1;

	int callNo = atoi(id);
	if (callNo < 0 || callNo >= NUM_LINES)
		return 2;

	if (iaxc_get_call_state(callNo) & IAXC_CALL_STATE_RINGING)
		iaxc_reject_call_number(callNo);
	else
		iaxc_dump_call_number(callNo);

	return 0;
}


int __cdecl IAXProto::VoiceHoldCall(WPARAM wParam, LPARAM lParam)
{
	char *id = (char *) wParam;
	if (id == NULL || id[0] == 0)
		return 1;

	int callNo = atoi(id);
	if (callNo < 0 || callNo >= NUM_LINES)
		return 2;

	if (callNo != iaxc_selected_call())
		return 3;

	iaxc_select_call(-1);
	NotifyCall(callNo, VOICE_STATE_ON_HOLD);
	return 0;
}


int __cdecl IAXProto::VoiceCallStringValid(WPARAM wParam, LPARAM lParam)
{
	TCHAR *number = (TCHAR *) wParam;
	
	if (number == NULL || number[0] == 0 || lstrlen(number) >= IAXC_EVENT_BUFSIZ)
		return 0;

	return 1;
}



