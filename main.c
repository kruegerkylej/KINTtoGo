/*******************************************

 MAIN.CPP - The KINT Interpreter
	Copyright 2002-2008 Everett Kaser
	All rights reserved.

	Do not copy or redistribute without the
	author's explicit written permission.
 *******************************************/

/*********************  Header Files  *********************/

#include "main.h"

//char gump[25][80];
//int	gumpnum=0;

#if KINT_MS_WINDOWS
  char	OSctrl_cmdSTR[32];// = "Ctrl";
#endif
#if KINT_MAC
  char	OSctrl_cmdSTR[32] = "Cmd";
#endif

#if KINT_HELP
	extern	int	F6BREAK;
#endif

//********************************************************************
extern PBMB		kBuf, pbFirst;

//********************************************************************

#define	UM_GETPATH	(WM_USER+0)

#define	MAXMSGS		1024	// max messages queued at a time in KINTmsgs

//*********************************************************************
#if KINT_DEBUG

	BYTE	*DbgMem=NULL;		// pointer to memory mapped file for communicating with debugee
	HANDLE	hFileMapping=NULL;
	extern	int	KdbgHalted;

#endif
//*********************************************************************

#if KINT_DOUBLE_SIZE
  #define	KORDS(l)	((((long)(short)HIWORD(l)/2)<<16) | (((long)(short)LOWORD(l)/2)&0x0000FFFF))
  #define	KINT_WND_SIZEW	(KINT_POCKET_PC ? 488 : 648)
  #define	KINT_WND_SIZEH	(KINT_POCKET_PC ? 672 : 512)
#else
  #define	KORDS(l)	(l)
  #define	KINT_WND_SIZEW	(KINT_POCKET_PC ? 248 : 328)
  #define	KINT_WND_SIZEH	(KINT_POCKET_PC ? 352 : 272)
#endif

#if !KINT_MS_WINDOWS
  #define	TCHAR	char
  #define	TEXT(s)	(s)
#endif

#ifdef _UNICODE

	TCHAR		WideBuf[512];
	//************************************************************
	TCHAR *Widen(char *pS) {

	#if KINT_MS_WINDOWS
		mbstowcs(WideBuf, pS, 511);
	#else
		TCHAR *pW;

		pW = WideBuf;
		do {
			*pW++ = (TCHAR)((WORD)((BYTE)(*pS)));
		} while( *pS++ );
	#endif
		return &WideBuf[0];
	}
	//************************************************************
	void UnWiden(char *pS, TCHAR *pW) {

	#if KINT_MS_WINDOWS
		wcstombs(pS, pW, 511);
	#else
		do {
			*pS = (char)((BYTE)((WORD)(*pW++)));
		} while( *pS++ );
	#endif
	}

	#define	kscpy(d, s)		_tcscpy(d, s)
	#define	kscat(d, s)		_tcscat(d, s)
	#define	kslen(s)		_tcslen(s)
	#define	kscmp(d, s)		_tcscmp(d, s)
#else
	#define	Widen(s)		((char *)s)
	#define	UnWiden(s, pw)	strcpy(s, pw)
	#define	kscpy(d, s)		strcpy(d, s)
	#define	kscat(d, s)		strcat(d, s)
	#define	kslen(s)		strlen(s)
	#define	kscmp(d, s)		strcmp(d, s)
#endif

struct firstnext {
	struct firstnext	*pNext;		// pointer to next FIRSTNEXT structure
	BOOL	AppData;	// FALSE if file is in PROGRAM folder tree, TRUE if file is in AppDataPath tree
	BYTE	attrib;
	BYTE	fname[3];
};
typedef struct firstnext FIRSTNEXT;

/*******************  Global Variables ********************/

HANDLE		ghInstance;
HWND		hWndMain=NULL;
HDC			ddc, sdc;
HBITMAP		oldBMs, oldBMd;
HCURSOR		hCursorOld;
HPALETTE	kPal;

KE			mainKE;	// struct for main .KE program
DWORD		*KINTmsgs=NULL;
int			KINTmsgadd=0, KINTmsgrem=0;

int	bitspixel, planes, numcolors, sizepalette, colormode;

BYTE		ffdir;

int		PhysicalScreenW, PhysicalScreenH, PhysicalScreenSizeIndex, PhysicalWndW, PhysicalWndH;

WORD	MinFlag=FALSE, MusicPlaying=FALSE;

BOOL	ProgramReady=FALSE, InSetWindowPos=FALSE;
UINT	TimerID=0;

TCHAR	PgmPath[PATHLEN], TmpPath[PATHLEN];
TCHAR	PartPath[PATHLEN];	// used ONLY by GetFileName()!!!
TCHAR	AppDataPath[PATHLEN];	// where to store game user-modified files
TCHAR	EKSname[PATHLEN], GAMEname[PATHLEN];

BOOL	CopyAppData;

FIRSTNEXT	*pFirstNext=NULL, *pNextNext=NULL;

#if KINT_UNIX
TCHAR	CasePath[PATHLEN];		// used ONLY by KOpen() and KFindFirst()
TCHAR	*CasePtrs[10];			// used ONLY by KOpen() and KFindFirst()
char	CaseCnt;				// used ONLY by KOpen() and KFindFirst()
DWORD	CaseLoopLim, CaseLoop;	// used ONLY by KOpen() and KFindFirst()
#endif

TCHAR	szAppName[] = TEXT("KINT 3.0");
char	KEname[128];

int		CurCur=0, mainShowCmd;

#if KINT_PRINT
  DWORD	hPRT;
  #if KINT_MS_WINDOWS
	HFONT	hPRTFont=NULL;
	DOCINFO PRTdocinfo = {sizeof(DOCINFO), "EKS puzzle", NULL, NULL, 0};
	PAGESETUPDLG	psd;
	PRINTDLG		pdlg;
  #endif
#endif

#if KINT_MIDI
 UINT MCIwDeviceID;
 MCI_OPEN_PARMS mciOpenParms;
 MCI_PLAY_PARMS mciPlayParms;
 MCI_STATUS_PARMS mciStatusParms;
#endif

#if KINT_DESKTOP_WINDOWS
WINDOWPLACEMENT	WndPlace;
struct _finddata_t	fdat;
#endif

#if KINT_POCKET_PC
  static SHACTIVATEINFO	s_sai;
  WIN32_FIND_DATA 	fdat32;
#endif

struct	_stat	fstats;

//********************************************************************

LRESULT WINAPI MainWndProc(HWND, UINT, WPARAM, LPARAM);

#if BETA
//***********************************************************************************
void CenterWindow(HWND hwnd) {

	RECT	rcParent, rcDlg;
	int		xMid, yMid, xLeft, yTop;

	SetRect(&rcParent, 0, 0, PhysicalScreenW, PhysicalScreenH);
	// find ideal center point
	xMid = (rcParent.left + rcParent.right) / 2;
	yMid = (rcParent.top + rcParent.bottom) / 2;
	// find dialog's upper left based on that
	GetWindowRect(hwnd, &rcDlg);
	xLeft = xMid - (rcDlg.right-rcDlg.left) / 2;
	yTop = yMid - (rcDlg.bottom-rcDlg.top) / 2;
	// if the dialog is outside the screen, move it inside
	if( xLeft < 0 ) xLeft = 0;
	else if( xLeft+(rcDlg.right-rcDlg.left) >= PhysicalScreenW ) xLeft = PhysicalScreenW-(rcDlg.right-rcDlg.left);

	if( yTop < 0 ) yTop = 0;
	else if( yTop+(rcDlg.bottom-rcDlg.top) >= PhysicalScreenH ) yTop = PhysicalScreenH-(rcDlg.bottom-rcDlg.top);

	SetWindowPos(hwnd, NULL, xLeft, yTop, -1, -1, SWP_NOSIZE | SWP_NOZORDER);
}

//***********************************************************************************
LRESULT CALLBACK BetaTestDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {

	switch( uMsg ) {
	 case WM_INITDIALOG:
		CenterWindow(hDlg);
		return TRUE;
	 case WM_COMMAND:
		switch( wParam ) {
		 case IDCANCEL:
			EndDialog(hDlg, FALSE);
			break;
		 case IDOK:
			EndDialog(hDlg, TRUE);
			return TRUE;
		}
		break;
	}
	return FALSE;
}
#endif

#if KINT_POCKET_PC
//********************************************************************
void ShowTaskBar(int show) {

	HWND	hwnd;

	hwnd = FindWindow(TEXT("HHTaskbar"), NULL);
	if( hwnd!=NULL ) {
		if( show ) {
			SHFullScreen(hWndMain, SHFS_SHOWTASKBAR | SHFS_SHOWSIPBUTTON | SHFS_SHOWSTARTICON );
			ShowWindow(hwnd, SW_SHOW);
			MoveWindow(hWndMain, 0, MENU_HEIGHT, kBuf->logH, kBuf->logW-2*MENU_HEIGHT, TRUE);
		} else {
			SHFullScreen(hWndMain, SHFS_HIDETASKBAR | SHFS_HIDESIPBUTTON | SHFS_HIDESTARTICON);
			ShowWindow(hwnd, SW_HIDE);
			MoveWindow(hWndMain, 0, 0, kBuf->logH, kBuf->logW, TRUE);
		}
	}
}
#endif

//********************************************************************
// Sets AppDataPath[] to absolute path into which to store Game-specific user modified files
void SetAppDataPath() {

	TCHAR		*pEKS;
	ITEMIDLIST	*pidl;

// Examples of input:
//	AppDataPath	== "C:\My Documents"
//	PgmPath		== "C:\EKS\GAME\"
// Output would be:
//	AppDataPath	== "C:\My Documents\EKS\GAME\"

	if( NOERROR==SHGetSpecialFolderLocation(NULL, CSIDL_PERSONAL, &pidl) && SHGetPathFromIDList(pidl, AppDataPath) ) {
		// find last TWO components of PgmPath (EKS folder and game folder names)
		strcpy(EKSname, PgmPath);
		pEKS = EKSname + strlen(EKSname) - 1;
		if( *pEKS=='\\' ) *pEKS = 0;
		do --pEKS; while( *pEKS!='\\' );
		strcpy(GAMEname, pEKS+1);
		*pEKS = 0;
		do --pEKS; while( *pEKS!='\\' );
		strcpy(EKSname, pEKS+1);

		// add '\\' to end of AppData if not there (usually it's not)
		if( AppDataPath[strlen(AppDataPath)-1] != '\\' ) strcat(AppDataPath, "\\");
		// tack EKS folder name onto end of AppData name
		strcat(AppDataPath, EKSname);
		// create AppData\EKS folder if it doesn't exist
		_mkdir(AppDataPath);
		strcat(AppDataPath, "\\");
		strcat(AppDataPath, GAMEname);
		// create AppData\EKS\game folder if it doesn't exist
		_mkdir(AppDataPath);
		strcat(AppDataPath, "\\");

		CopyAppData = TRUE;
	} else {
		// error getting AppData, just try to use the game program folder
		strcpy(AppDataPath, PgmPath);

		CopyAppData = FALSE;
	}
}

//********************************************************************
// callback for enumerate windows to bring previous instance of this program to the foreground
// for each top-level window, get it's class name.  If it matches "our" class name, then
// send it a message to get a ptr to it's program path.  If it matches THIS program path,
// we've found the previous instance, and we bring it to the foreground.
BOOL CALLBACK FindPrevKintWnd(HWND hWnd, LPARAM lParam) {

	GetClassName(hWnd, TmpPath, PATHLEN);
	if( !kscmp(TmpPath, szAppName) ) {
		if( !kscmp((TCHAR *)SendMessage(hWnd, UM_GETPATH, 0, 0), PgmPath) ) {
			SetForegroundWindow(hWnd);
			return FALSE;
		}
	}
	return TRUE;
}

//*****************************************************************
// The following function translates lParam X/Y mouse cursor location
// appropriately to KASM/KINT X/Y coordinates, taking into account
// KINT_DOUBLE_SIZE and/or screen rotation.  KASM/KINT knowns NOTHING
// about screen rotation or double sizing, so coordinates get
// translated going in to KASM/KINT code, and then translated again
// coming out (when drawing on screen or other bitmap).  The BMB->orient
// variable contains 1 of 8 values (see MAIN.H) for the 8 possible orientations.
// An LPARAM, in MS Windows, is really a DWORD, the low-word of which is X
// and the high-word of which is Y.
LPARAM FixXY(LPARAM z) {

	long	x, y;
	DWORD	r;

	x = (long)(short)LOWORD(z);
	y = (long)(short)HIWORD(z);
#if KINT_DOUBLE_SIZE
	switch( kBuf->orient ) {
	 case BMO_NORMAL:
		r = (DWORD)((x/2) & 0x0000FFFF) | (((DWORD)(y/2))<<16);
		break;
	 case BMO_180:
		r = (DWORD)((kBuf->logW-1-x/2) & 0x0000FFFF) | (((DWORD)(kBuf->logH-1-y/2))<<16);
		break;
	 case BMO_90:
		r = (DWORD)((kBuf->logW-1-y/2) & 0x0000FFFF) | (((DWORD)(x/2))<<16);
		break;
	 case BMO_270:
		r = (DWORD)((y/2) & 0x0000FFFF) | (((DWORD)(kBuf->logH-1-x/2))<<16);
		break;
	 case BMO_FNORMAL:
		r = (DWORD)((kBuf->logW-1-x/2) & 0x0000FFFF) | (((DWORD)(y/2))<<16);
		break;
	 case BMO_F180:
		r = (DWORD)((x/2) & 0x0000FFFF) | (((DWORD)(kBuf->logH-1-y/2))<<16);
		break;
	 case BMO_F90:
		r = (DWORD)((kBuf->logW-1-y/2) & 0x0000FFFF) | (((DWORD)(kBuf->logH-1-x/2))<<16);
		break;
	 case BMO_F270:
		r = (DWORD)((y/2) & 0x0000FFFF) | (((DWORD)(x/2))<<16);
		break;
	 default:
		SOerr(kBuf);
		return 0;
	}
#else
	switch( kBuf->orient ) {
	 case BMO_NORMAL:
		r = (DWORD)((x) & 0x0000FFFF) | (((DWORD)(y))<<16);
		break;
	 case BMO_180:
		r = (DWORD)((kBuf->logW-1-x) & 0x0000FFFF) | (((DWORD)(kBuf->logH-1-y))<<16);
		break;
	 case BMO_90:
		r = (DWORD)((kBuf->logW-1-y) & 0x0000FFFF) | (((DWORD)(x))<<16);
		break;
	 case BMO_270:
		r = (DWORD)((y) & 0x0000FFFF) | (((DWORD)(kBuf->logH-1-x))<<16);
		break;
	 case BMO_FNORMAL:
		r = (DWORD)((kBuf->logW-1-x) & 0x0000FFFF) | (((DWORD)(y))<<16);
		break;
	 case BMO_F180:
		r = (DWORD)((x) & 0x0000FFFF) | (((DWORD)(kBuf->logH-1-y))<<16);
		break;
	 case BMO_F90:
		r = (DWORD)((kBuf->logW-1-y) & 0x0000FFFF) | (((DWORD)(kBuf->logH-1-x))<<16);
		break;
	 case BMO_F270:
		r = (DWORD)((y) & 0x0000FFFF) | (((DWORD)(x))<<16);
		break;
	 default:
		SOerr(kBuf);
		return 0;
	}
#endif
	return (LPARAM)r;
}

//***********************************************************
// translate up/dn/lf/rt cursor for screen rotation
DWORD MapCursor(WPARAM wParam) {

	switch( kBuf->orient ) {
	 case BMO_NORMAL:
		break;
	 case BMO_180:
		if( wParam==VK_LEFT ) wParam = VK_RIGHT;
		else if( wParam==VK_RIGHT ) wParam = VK_LEFT;
		else if( wParam==VK_UP ) wParam = VK_DOWN;
		else if( wParam==VK_DOWN ) wParam = VK_UP;
		break;
	 case BMO_90:
		if( wParam==VK_LEFT ) wParam = VK_UP;
		else if( wParam==VK_RIGHT ) wParam = VK_DOWN;
		else if( wParam==VK_UP ) wParam = VK_RIGHT;
		else if( wParam==VK_DOWN ) wParam = VK_LEFT;
		break;
	 case BMO_270:
		if( wParam==VK_LEFT ) wParam = VK_DOWN;
		else if( wParam==VK_RIGHT ) wParam = VK_UP;
		else if( wParam==VK_UP ) wParam = VK_LEFT;
		else if( wParam==VK_DOWN ) wParam = VK_RIGHT;
		break;
	 case BMO_FNORMAL:
		if( wParam==VK_LEFT ) wParam = VK_RIGHT;
		else if( wParam==VK_RIGHT ) wParam = VK_LEFT;
		break;
	 case BMO_F180:
		if( wParam==VK_UP ) wParam = VK_DOWN;
		else if( wParam==VK_DOWN ) wParam = VK_UP;
		break;
	 case BMO_F90:
		if( wParam==VK_LEFT ) wParam = VK_DOWN;
		else if( wParam==VK_RIGHT ) wParam = VK_UP;
		else if( wParam==VK_UP ) wParam = VK_RIGHT;
		else if( wParam==VK_DOWN ) wParam = VK_LEFT;
		break;
	 case BMO_F270:
		if( wParam==VK_LEFT ) wParam = VK_UP;
		else if( wParam==VK_RIGHT ) wParam = VK_DOWN;
		else if( wParam==VK_UP ) wParam = VK_LEFT;
		else if( wParam==VK_DOWN ) wParam = VK_RIGHT;
		break;
	 default:
		SOerr(kBuf);
		return 0;
	}
	return (DWORD)wParam;
}

//********************************************************************
void AddMsg(WORD cmd, DWORD arg1, DWORD arg2, DWORD arg3) {

#if KINT_DEBUG
	if( KdbgHalted && DbgMem!=NULL && KDBG_HERE ) {
		if( cmd==KHDR_TIMER || cmd==KHDR_POINTER_MOVE || cmd==KHDR_IDLE ) return;
	}
#endif
	if( KINTmsgs!=NULL ) {
		KINTmsgs[KINTmsgadd*4] = (DWORD)cmd;
		KINTmsgs[KINTmsgadd*4+1] = arg1;
		KINTmsgs[KINTmsgadd*4+2] = arg2;
		KINTmsgs[KINTmsgadd*4+3] = arg3;
		++KINTmsgadd;
		if( KINTmsgadd >= MAXMSGS ) KINTmsgadd = 0;
		if( KINTmsgadd == KINTmsgrem ) {
			--KINTmsgadd;
			if( KINTmsgadd < 0 ) KINTmsgadd = MAXMSGS-1;
		}
	} else KEmsg(&mainKE, cmd, arg1, arg2, arg3);
}

//********************************************************************
void SendNextMsg() {

	if( KINTmsgs!=NULL && KINTmsgrem != KINTmsgadd ) {
		KEmsg(&mainKE, (WORD)KINTmsgs[KINTmsgrem*4], KINTmsgs[KINTmsgrem*4+1], KINTmsgs[KINTmsgrem*4+2], KINTmsgs[KINTmsgrem*4+3]);
		++KINTmsgrem;
		if( KINTmsgrem >= MAXMSGS ) KINTmsgrem = 0;
	}
}

//********************************************************************
// Main entry point for Windows programs
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpszCmdLine, int nCmdShow) {

	WNDCLASS	wc;
	MSG			msg;
	int			i;
	DWORD		IdleCount;
	HANDLE		hmutex;
#if KINT_PRINT
	HDC			hDC;
#endif
#if BETA
	KTIME	t;
#endif

	mainShowCmd = nCmdShow;		// make a copy, allow KASM code to change during startup
	ghInstance = hInstance;

// If no command line argument, get path program was run from and save in PgmPath
// ALL other file access by this program are relative to PgmPath.
	if( lpszCmdLine[0]==0 ) {
		i = GetModuleFileName((HMODULE)ghInstance, PgmPath, PATHLEN-1);
#if KINT_HELP
		strcpy(KEname, "help.ke");
#else
		strcpy(KEname, "main.ke");
#endif
	} else {
// if command line argument, it must be absolute path to the .KE file to run
// ALL other file access by this program is relative to that path
		kscpy(PgmPath, lpszCmdLine);
		i = kslen(PgmPath);
		while( i && PgmPath[i]!=TEXT('\\') && PgmPath[i]!=TEXT(':') ) --i;
		if( PgmPath[i]!=TEXT('\\') || PgmPath[i]!=TEXT(':') ) ++i;
		UnWiden(KEname, PgmPath+i);
	}


	while( i && PgmPath[i]!=TEXT('\\') && PgmPath[i]!=TEXT(':') ) --i;
	if( PgmPath[i]==TEXT('\\') || PgmPath[i]==TEXT(':') ) ++i;
	if( !i || PgmPath[i-1]!=TEXT('\\') ) PgmPath[i++] = TEXT('\\');
	PgmPath[i] = 0;

// see if a copy of this program is already running
// if so, just activate it and kill this copy,
// else let this copy proceed.

	// convert program folder path backslashes to underscores
	for(i=0; PgmPath[i]; i++) {
		if( PgmPath[i]=='\\' ) TmpPath[i] = '_';
		else TmpPath[i] = PgmPath[i];
	}
	TmpPath[i] = 0;
	// create a Windows mutex.  if it succeeds, this is the first instance of the program
	hmutex = CreateMutex(NULL, FALSE, TmpPath);
	// if it fails, there's another instance already running
	if( GetLastError()==ERROR_ALREADY_EXISTS ) {
		// find the other instance and bring it to the foreground, then terminate this instance
		EnumWindows((WNDENUMPROC)FindPrevKintWnd, 0);
		return FALSE;
	}

	// Set AppDataPath[] so we know where to create our files
	SetAppDataPath();

#if KINT_DEBUG
	hFileMapping = OpenFileMapping(FILE_MAP_WRITE, FALSE, "KDBG_FILE_MAP");
	if( hFileMapping!=NULL ) {
		DbgMem = (BYTE *)MapViewOfFile(hFileMapping, FILE_MAP_WRITE, 0, 0, 16384);
		if( DbgMem!=NULL ) {
			KINT_SIGNAL = KSIG_NONE;
			KINT_ACK = KSIG_NO_ACK;
			KINT_HERE = 1;
		}
	}
#endif

	if( NULL==(KINTmsgs=(DWORD *)malloc(MAXMSGS*16)) ) {
		SystemMessage("Failed to allocate MSG buf");
		exit(0);
	}

#if KINT_MS_WINDOWS
	i = GetKeyNameText(0x001D0001, OSctrl_cmdSTR, sizeof(OSctrl_cmdSTR));
//sprintf(TmpPath, "GetKeyNameText()=%d OSctrl_cmdSTR[]=%s", i, OSctrl_cmdSTR);
//SystemMessage(TmpPath);
#endif

// try to load the .KE file
	if( !LoadKEFile(KEname, &mainKE) ) exit(0);

// register our window class
	wc.lpfnWndProc		= MainWndProc;
	wc.cbClsExtra		= 0;
	wc.cbWndExtra		= 0;
	wc.hInstance		= hInstance;
	wc.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON));
	wc.hCursor			= NULL;//LoadCursor(NULL, IDC_ARROW);
	wc.lpszMenuName		= NULL;
	wc.lpszClassName	= szAppName;
#if KINT_POCKET_PC
    wc.style			= CS_HREDRAW | CS_VREDRAW; // CS_OWNDC????
    wc.hbrBackground	= (HBRUSH) GetStockObject(WHITE_BRUSH);
	RegisterClass(&wc);

#endif

#if KINT_DESKTOP_WINDOWS
	wc.style			= CS_OWNDC | CS_VREDRAW | CS_HREDRAW;
	wc.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	RegisterClass( &wc );
#endif

	SysStartup();
// create a color palette in case we're running in a 256-color mode
	MakePalette();

#if KINT_PRINT
// create the two "working" screen DCs (source DC and destination DC)
// these are used heavily in SYSFUNCS.CPP module for doing drawing to kBM and elsewhere
	hDC = GetDC(NULL);
	if( NULL==(sdc=CreateCompatibleDC(hDC)) ) {
		SystemMessage("Failed creating source DC");
		ReleaseDC(NULL, hDC);
		exit(0);
	}
	if( NULL==(ddc=CreateCompatibleDC(hDC)) ) {
		SystemMessage("Failed creating destination DC");
		ReleaseDC(NULL, hDC);
		exit(0);
	}
	ReleaseDC(NULL, hDC);
#endif

// create the default font and the kBM (screen) bitmap.
// the KASM code does ALL of it's drawing to kBM, a memory bitmap.
// when Windows needs to update the REAL screen, the WM_PAINT message
// in this program merely BitBlt's from kBM to the screen DC.
	InitScreen(0, 0);

// call the BEGIN_STARTUP code in the KE module (tell it we're starting up and
// about to create the main program window)
/****/	KEmsg(&mainKE, KHDR_BEGIN_STARTUP, 0, 0, 0);

// create main window
#if KINT_DESKTOP_WINDOWS
	hWndMain = CreateWindow(szAppName, szAppName, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, KINT_WND_SIZEW, KINT_WND_SIZEH, NULL, NULL, hInstance, NULL);
#endif

#if KINT_POCKET_PC
	hWndMain = CreateWindow(szAppName, szAppName, WS_VISIBLE,
		CW_USEDEFAULT, CW_USEDEFAULT, 240, 320, /*CW_USEDEFAULT, CW_USEDEFAULT,*/ NULL, NULL, hInstance, NULL);
#endif
	if( !hWndMain ) {
		SysCleanup();
		return FALSE;
	}
#if KINT_DEBUG
	if( DbgMem!=NULL ) KINT_HWND = (DWORD)hWndMain;
#endif

#if KINT_POCKET_PC
	ShowTaskBar(FALSE);
	InvalidateScreen(0, 0, 320, 240);
#endif

// call the END_STARTUP code in the KE module (tell it to finish setting up, and
// that the main program window has now been created.
/****/	KEmsg(&mainKE, KHDR_END_STARTUP, 0, 0, 0);

#if BETA
	if( IDOK!=DialogBox((HINSTANCE)ghInstance, MAKEINTRESOURCE(IDD_BETA), hWndMain, (DLGPROC)BetaTestDlgProc) ) {
		DestroyWindow(hWndMain);
		exit(0);
	}
	KGetTime(&t);
	if( t.y>=BETAYEAR && (t.mo>BETAMONTH || (t.mo==BETAMONTH && t.d>=BETADAY)) ) {
		SystemMessage("The BETA TEST period for this program has elapsed.\nPlease replace it with the official shareware or licensed version.\nVisit our webpage:  http://www.kaser.com");
		DestroyWindow(hWndMain);
		exit(0);
	}
#endif

// possibly cause the window to be shown on the display.
// 'mainShowCmd' may get set by the .KE code during KHDR_END_STARTUP using
// SysGetWindowPos and SysSetWindowPos calls.
	ShowWindow(hWndMain, mainShowCmd);

// send one IDLE message to the KASM code in case it needs to do anything once
// right up front after everything else has been initialized.
/****/	KEmsg(&mainKE, KHDR_IDLE, 0, 0, 0);

	ProgramReady = TRUE;

// this is the main "message pump" loop.  While no messages are waiting for the
// program and the IDLE call to the KASM code returns non-0, it keeps sending
// IDLE messages to the KASM code.  If the KASM code returns 0, then no more
// IDLE messages are sent until some Windows message arrives for the program.
	for(;;) {
		while( KINTmsgs!=NULL && KINTmsgadd != KINTmsgrem ) SendNextMsg();
		IdleCount = 0;
		// check to see if we can do idle work
/****/	while( !PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE) ) {
			if( !KEmsg(&mainKE, KHDR_IDLE, IdleCount++, 0, 0) ) break;
		}
		// either we have a message, or OnIdle returned false
		if( GetMessage(&msg, NULL, 0, 0)==TRUE ) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		} else break;
	}
	return msg.wParam;
}

/*
BYTE	DBuf[256];

//***********************************************************************
void WriteDebug(BYTE *p) {

	DWORD	hFile;

	if( -1==(hFile=(DWORD)_open("debug.log", O_WRBIN, _S_IREAD | _S_IWRITE)) ) {
		if( -1==(hFile=(DWORD)_open("debug.log", O_WRBINNEW, _S_IREAD | _S_IWRITE)) ) return;
	}
	Kseek(hFile, 0, SEEK_END);

	Kwrite(hFile, p, strlen(p));
	Kwrite(hFile, "\r\n", 2);
	Kclose(hFile);
}
*/
//********************************************************************************
// This is the main MS Windows message ("event") handling function.
LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam ) {

#if KINT_MS_WINDOWS
	HBITMAP			holdbm;
	HDC				hdc, bdc;
	PAINTSTRUCT		ps;
#endif
	LPARAM			tmpL;
	RECT			rect;

/*
	if( msg==WM_ACTIVATEAPP ) {
		msg = msg;
	}
	if( msg==WM_ACTIVATE ) {
		msg = msg;
	}
	if( msg!=WM_SETFOCUS && msg!=WM_KILLFOCUS && msg!=WM_PAINT && msg!=WM_ERASEBKGND
	  && msg!=WM_SHOWWINDOW && msg!=WM_SETCURSOR && msg!=WM_TIMER
	   && msg!=WM_WINDOWPOSCHANGING && msg!=WM_GETMINMAXINFO && msg!=WM_NCCALCSIZE
	    && msg!=WM_NCPAINT && msg!=WM_GETTEXT && msg!=WM_WINDOWPOSCHANGED
	    && msg!=WM_NCHITTEST && msg!=WM_NCACTIVATE && msg!=WM_MOUSEMOVE 
		&& msg!=WM_ACTIVATEAPP && msg!=WM_ACTIVATE
		 && msg!=WM_CREATE && msg!=WM_SETTEXT && msg!=WM_MOVE && msg!=WM_SIZE 
		 && msg!=WM_KEYUP && msg!=WM_KEYDOWN && msg!=WM_LBUTTONDOWN 
		 && msg!=WM_LBUTTONUP && msg!=WM_NCCREATE && msg!=WM_NCMOUSEMOVE
		) {
	  msg = msg;
	}
*/

	switch( msg ) {
	 case UM_GETPATH:
	// our own custom message to get the path to the other instances of
	// the program, so that we can figure out if programs by the same
	// name are actually the same program or not.  (ie, this same interpreter
	// might be used for several different games, in different folders.  While
	// the name of the EXE *MIGHT* be the same, the folders will be different,
	// so we check BOTH the program and AND the folder from which it was run.)
	// See the FindPrevKintWnd() function above.
		return (LRESULT)PgmPath;
#if KINT_DESKTOP_WINDOWS
	// For KINT, 0x8003 is keycode for MOUSE_WHEEL turned.
	 case 0x020A:	// WM_MOUSEWHEEL
		GetWindowRect(hWnd, &rect);
		tmpL = (((WORD)((int)((short)(HIWORD(lParam)))-(rect.top+GetSystemMetrics(SM_CYSIZEFRAME)+GetSystemMetrics(SM_CYCAPTION)+GetSystemMetrics(SM_CYBORDER))))<<16)
			 | (((WORD)((int)((short)(LOWORD(lParam)))-(rect.left+GetSystemMetrics(SM_CXSIZEFRAME)+GetSystemMetrics(SM_CXBORDER))))&0x0000FFFF);
/****/	AddMsg(KHDR_KEY_DOWN, 0x00008003, ((short)(HIWORD(wParam)))/120, FixXY(tmpL));
		return 0;
	// For KINT, 0x8001 is keycode for BACK mouse button, 0x8002 is keycode for FWD mouse button
	// 0x020B is the MS Windows message when an 'extended' mouse key is pressed.
	// 0x020C is the MS Windows message when an 'extended' mouse key is released.
	// wParam is 0x00010020 when BACK is pressed, 0x00010000 when BACK is released.
	// wParam is 0x00020040 when FWD is pressed,  0x00020000 when FWD is released.
	 case 0x020B:	// WM_XBUTTONDOWN
/****/	AddMsg(KHDR_KEY_DOWN, (wParam & 0x00010000)?0x00008001:0x00008002, wParam, FixXY(lParam));
		return 0;
	 case 0x020C:	// WM_XBUTTONUP
/****/	AddMsg(KHDR_KEY_UP, (wParam & 0x00010000) ?0x00008001:0x00008002, wParam, FixXY(lParam));
		return 0;
	 case 0x020E:	// WM_MOUSEHWHEEL mouse side scroll wParam=0xFF880000 for left, 0x00780000 for right
		GetWindowRect(hWnd, &rect);
		tmpL = (((WORD)((int)((short)(HIWORD(lParam)))-(rect.top+GetSystemMetrics(SM_CYSIZEFRAME)+GetSystemMetrics(SM_CYCAPTION)+GetSystemMetrics(SM_CYBORDER))))<<16)
			 | (((WORD)((int)((short)(LOWORD(lParam)))-(rect.left+GetSystemMetrics(SM_CXSIZEFRAME)+GetSystemMetrics(SM_CXBORDER))))&0x0000FFFF);
/****/	AddMsg(KHDR_KEY_DOWN, 0x00008004, ((short)(HIWORD(wParam)))/120, FixXY(tmpL));
		return 1;// this message must return TRUE to signal that it's been handled
	 case WM_MENUCHAR:
		return (MNC_CLOSE<<16);	// to avoid BEEP when holding ALT down and pressing the "hotkey"
	 case WM_SYSKEYDOWN:
		/* NOTE: "menu" (alt-xxx) keys set ARG3 to 1, all other keys (except mouse buttons) set it to 0 */
		if( (wParam>=0x30 && wParam<=0x39) || (wParam>=0x41 && wParam<=0x5A) ) {
// Windows doesn't send a WM_CHAR for ALT-alpha keys, which screws up the logic!
// So, we send it manually in those cases, to keep things consistent.
/****/		AddMsg(KHDR_ALPHAKEY_DOWN, (DWORD)wParam, (DWORD)(lParam&0x0ffff), 0);
/****/		AddMsg(KHDR_CHAR, (DWORD)wParam, (DWORD)(lParam&0x0ffff), 0);
		} else {
/****/		AddMsg(KHDR_KEY_DOWN, (DWORD)wParam, (DWORD)(lParam&0x0ffff), 1);
		}
		return 0;
	 case WM_SYSKEYUP:
		/* NOTE: "menu" (alt-xxx) keys set ARG3 to 1, all other keys (except mouse buttons) set it to 0 */
		if( (wParam>=0x30 && wParam<=0x39) || (wParam>=0x41 && wParam<=0x5A) ) {
			AddMsg(KHDR_ALPHAKEY_UP, (DWORD)wParam, (DWORD)(lParam&0x0ffff), 0);
		} else {
/****/		AddMsg(KHDR_KEY_UP, (DWORD)wParam, (DWORD)(lParam&0x0ffff), 1);
		}
		return 0;
	 case WM_DISPLAYCHANGE:
	// The resolution of the display has been changed.  Recreate
	// the "display" bitmap kBM, the defFont, and redraw the window.
		InitScreen(-1, -1);
		return 0;
#endif
#if KINT_POCKET_PC
	 case WM_CREATE:
		break;
	 case WM_ACTIVATE:
	 case WM_ENABLE:
	 case WM_SETTINGCHANGE:
	 case WM_SETFOCUS:
		ShowTaskBar(FALSE);
		break;
	 case WM_KILLFOCUS:
		ShowTaskBar(TRUE);
		break;
#endif
	 case WM_ERASEBKGND:
	// never erase background, because we always draw everything
		return TRUE;
#if KINT_MIDI
	 case MM_MCINOTIFY:
	// various MIDI messages, we only care about termination
		switch( wParam ) {
		 case MCI_NOTIFY_ABORTED:
		 case MCI_NOTIFY_FAILURE:
		 case MCI_NOTIFY_SUPERSEDED:
	 		break;
		 case MCI_NOTIFY_SUCCESSFUL:
			mciSendCommand(MCIwDeviceID, MCI_CLOSE, 0, 0);
			MusicPlaying = 0;
/****/		AddMsg(KHDR_MUSIC_DONE, 0, 0, 0);
			break;
		 default:
			break;
		}
		return 0;
#endif
	 case WM_SIZE:
	// the program window has been resized, minimized, or maximized

	// the following line is flush the msg queue, as it otherwise gets filled with
	// KHDR_TIMER messages and the KHDR_WINDOWSIZE messages never get through
	// because apparently the main window message pump is not active during window
	// resizing or something.
		if( ProgramReady && !InSetWindowPos ) while( KINTmsgs!=NULL && KINTmsgadd != KINTmsgrem ) SendNextMsg();
		if( wParam==SIZE_MINIMIZED ) {
			MinFlag = TRUE;
/****/		AddMsg(KHDR_WINDOW_HIDDEN, 1, 0, 0);
		} else {
			RECT	rc;

/****/		if( MinFlag==TRUE ) AddMsg(KHDR_WINDOW_HIDDEN, 0, 0, 0);
			GetClientRect(hWnd, &rc);
#if KINT_POCKET_PC || KINT_POCKET_PC_EMUL
			kBuf->logW = 320;
			kBuf->logH = 240;
#else
			if( kBuf->orient & BMO_90 ) {
				kBuf->logW = (WORD)(rc.bottom-rc.top);
				kBuf->logH = (WORD)(rc.right-rc.left);
			} else {
				kBuf->logW = (WORD)(rc.right-rc.left);
				kBuf->logH = (WORD)(rc.bottom-rc.top);
			}
#endif
/****/		if( !InSetWindowPos ) {
				if( kBuf->logW > kBuf->physW || kBuf->logH > kBuf->physH ) InitScreen(kBuf->logW, kBuf->logH);
				AddMsg(KHDR_WINDOW_SIZE, kBuf->logW, kBuf->logH, (wParam==SIZE_MAXIMIZED)?K_MAXIMIZED:((wParam==SIZE_RESTORED)?K_RESTORED:K_SIZED));
				if( ProgramReady ) SendNextMsg();
			}
			MinFlag = FALSE;
		}
		return 0;
	 case WM_MOVE:
	// the program window has been moved
/****/	if( !InSetWindowPos ) AddMsg(KHDR_WINDOW_MOVE, LOWORD(KORDS(lParam)), HIWORD(KORDS(lParam)), 0);
		return 0;
	 case WM_TIMER:
	// the timer time period has expired
/****/	if( ProgramReady ) AddMsg(KHDR_TIMER, 0, 0, 0);
		return 0;
	 case WM_KEYDOWN:
	// a keyboard key has been pressed
#if KINT_POCKET_PC_EMUL
		if( wParam==VK_F1 ) {
//			WORD w;

			kBuf->orient = (kBuf->orient+1)&7;
//			w = kBuf->logW;
//			kBuf->logW = kBuf->logH;
//			kBuf->logH = w;

			kBuf->clipX1 = kBuf->clipY1 = 0;
			kBuf->clipX2 = 320;
			kBuf->clipY2 = 240;
	/****/	AddMsg(KHDR_SCREEN_CHANGE, 0, 0, 0);
			InvalidateRect(hWndMain, NULL, FALSE);
			return 0;
		}
#endif

#if KINT_HELP
		if( wParam==VK_F6 ) F6BREAK = 1;
#endif
		if( (wParam>=0x30 && wParam<=0x39) || (wParam>=0x41 && wParam<=0x5A) ) {
			AddMsg(KHDR_ALPHAKEY_DOWN, (DWORD)wParam, (DWORD)(lParam&0x0ffff), 0);
		} else {
/****/		AddMsg(KHDR_KEY_DOWN, (DWORD)MapCursor(wParam), (DWORD)(lParam&0x0ffff), 0);
		}
		return 0;
	 case WM_KEYUP:
	// a keyboard key has been released
	 // KHDR_KEY_UP and KHDR_KEY_DOWN *MUST* be sent for at least SHIFT, CTL, ALT,
	 // TAB, and ENTER, or menus and dialogs will be broken (so far as the keyboard goes).
		if( (wParam>=0x30 && wParam<=0x39) || (wParam>=0x41 && wParam<=0x5A) ) {
			AddMsg(KHDR_ALPHAKEY_UP, (DWORD)wParam, (DWORD)(lParam&0x0ffff), 0);
		} else {
/****/		AddMsg(KHDR_KEY_UP, (DWORD)MapCursor(wParam), (DWORD)(lParam&0x0ffff), 0);
		}
		return 0;
	 case WM_CHAR:
	// The KHDR_ALPHAKEY_DOWN and KHDR_ALPHAKEY_UP events are necessary in some
	// circumstances to make menus and dialogs work right with the keyboard, but
	// can be lived without if an OS doesn't make it easy to do them.  HOWEVER....
	// KHDR_CHAR *MUST* be sent during "normal typing" in order for "edit" fields
	// to work!!!  In Microsoft Windows, when you press and release an alpha-key,
	// a WM_KEYDOWN is sent, then a WM_CHAR, then a WM_KEYUP.  The WM_CHAR is
	// a sent as a result of the TranslateMessage() call in the message pump loop
	// at the bottom of WinMain().
		if( wParam>=0x20 ) {
/****/		AddMsg(KHDR_CHAR, (DWORD)wParam, (DWORD)(lParam&0x0ffff), 0);
		}
		return 0;
	 case WM_MOUSEMOVE:
	// the mouse (pointer) has been moved
		CurCur = -1;	// preset cursor flag to "none set" so we know if KASM code sets it.
		tmpL = FixXY(lParam);
/****/	AddMsg(KHDR_POINTER_MOVE, (DWORD)(long)(short)(WORD)LOWORD(tmpL), (DWORD)(long)(short)(WORD)HIWORD(tmpL), wParam);
		if( CurCur==-1 ) KSetCursor(0);	// if KASM code didn't set cursor, force to "normal" cursor
		return 0;
	 case WM_LBUTTONDOWN:
	// the LEFT mouse button has been pressed
		if( GetActiveWindow() != hWnd ) SetActiveWindow(hWnd);
		// the above causes our window to become activated in the case
		// where the .KE program has done a mousecapture while another
		// program is the active app.
/****/	AddMsg(KHDR_KEY_DOWN, VK_LBUTTON, wParam, FixXY(lParam));
		return 0;
	 case WM_LBUTTONUP:
	// the LEFT mouse button has been released
/****/	AddMsg(KHDR_KEY_UP, VK_LBUTTON, wParam, FixXY(lParam));
		return 0;
	 case WM_MBUTTONDOWN:
	// the MIDDLE mouse button has been pressed
/****/	AddMsg(KHDR_KEY_DOWN, VK_MBUTTON, wParam, FixXY(lParam));
		return 0;
	 case WM_MBUTTONUP:
	// the MIDDLE mouse button has been released
/****/	AddMsg(KHDR_KEY_UP, VK_MBUTTON, wParam, FixXY(lParam));
		return 0;
	 case WM_RBUTTONDOWN:
	// the RIGHT mouse button has been pressed
/****/	AddMsg(KHDR_KEY_DOWN, VK_RBUTTON, wParam, FixXY(lParam));
		return 0;
	 case WM_RBUTTONUP:
	// the RIGHT mouse button has been released
/****/	AddMsg(KHDR_KEY_UP, VK_RBUTTON, wParam, FixXY(lParam));
		return 0;
	 case WM_PAINT:
	// the program window needs updating, repaint it from the kBM bitmap
	{
		RECT	rect;
//int g;
		hdc = BeginPaint(hWnd, &ps);

		SelectPalette(hdc, kPal, 0);
		RealizePalette(hdc);	// for 256 color displays

		SetBkColor(hdc, PALETTERGB(255,255,255));		
		SetTextColor(hdc, PALETTERGB(0,0,0));

		bdc=CreateCompatibleDC(hdc);
		holdbm = (HBITMAP)SelectObject(bdc, kBuf->hBitmap);
		SelectPalette(bdc, kPal, 0);
		RealizePalette(bdc);

		if( NULLREGION !=GetClipBox(hdc, &rect) ) {
#if KINT_DOUBLE_SIZE
			rect.left &= ~1;
			rect.top  &= ~1;
			rect.right = (rect.right+1) & ~1;
			rect.bottom= (rect.bottom+1) & ~1;
			StretchBlt(hdc, rect.left, rect.top, rect.right-rect.left, rect.bottom-rect.top,
						bdc, rect.left/2, rect.top/2, (rect.right-rect.left)/2, (rect.bottom-rect.top)/2, SRCCOPY);
#else
			BitBlt(hdc, rect.left, rect.top, rect.right-rect.left+1, rect.bottom-rect.top+1, bdc, rect.left, rect.top, SRCCOPY);
#endif
		}
		SelectObject(bdc, holdbm);
		DeleteDC(bdc);
//for(g=0; g<25; g++) TextOut(hdc, 0, g*20, gump[g], strlen(gump[g]));

		EndPaint(hWnd, &ps);
	}
		return 0;
	 case WM_CLOSE:
	// someone has requested the closing of the program.
	// if KEmsg(&mainKE, KHDR_QUERY_QUIT) returns 0, don't quit program
	//   else go ahead and quit
/****/	if( !KEmsg(&mainKE, KHDR_QUERY_QUIT, 0, 0, 0) ) return 0;
		break;
	 case WM_DESTROY:
	// kill the timer if the Kode failed to
		if( TimerID!=0 ) {
			KillTimer(hWndMain, TimerID);
			TimerID = 0;
		}

#if KINT_POCKET_PC
		ShowTaskBar(TRUE);
#endif

		if( KINTmsgs!=NULL ) {
			while( KINTmsgs!=NULL && KINTmsgadd != KINTmsgrem ) SendNextMsg();
			free(KINTmsgs);
			KINTmsgs = (DWORD *)(KINTmsgadd = KINTmsgrem = 0);
		}

	// the program is being killed, tell the KASM code so it can clean up
/****/	KEmsg(&mainKE, KHDR_SHUT_DOWN, 0, 0, 0);

#if KINT_DEBUG
		if( DbgMem!=NULL ) {
			if( KDBG_HERE ) {
				KINT_HWND = 0;
				KDBG_LOADKE_KENUM = (DWORD)mainKE.KEnum;
				Ksig(KSIG_UNLOADKE);
			}
			KINT_HERE = 0;
			UnmapViewOfFile(DbgMem);
			DbgMem = NULL;
		}
		if( hFileMapping!=NULL ) CloseHandle(hFileMapping);
#endif
		SysCleanup();
		DestroyPalette();

		free(mainKE.pKode);

#if KINT_PRINT
		DeleteDC(sdc);
		DeleteDC(ddc);
#endif
		PostQuitMessage(0);
		return 0;
	}
	return( DefWindowProc(hWnd, msg, wParam, lParam));
}

//**********************************************************************
//* Called from PMACH.CPP when the .KE wants the program to terminate
//* Do whatever's necessary to make the program terminate.
//*
void KillProgram() {

	DestroyWindow(hWndMain);
}

//***********************************************************************
void KINT_SetWindowPos(DWORD r11, DWORD r12, DWORD r13, DWORD r14, DWORD r15) {

	BOOL	t;

	t = InSetWindowPos;
	InSetWindowPos = TRUE;
#if KINT_DESKTOP_WINDOWS
	if( r12+r13+r14+r15==0 ) {
		WndPlace.length = sizeof(WINDOWPLACEMENT);
		GetWindowPlacement(hWndMain, &WndPlace);
		WndPlace.flags = 0;
		WndPlace.showCmd = SW_SHOWNORMAL;
		SetWindowPlacement(hWndMain, &WndPlace);

//		SetActiveWindow(hWndMain);
//		SetForegroundWindow(hWndMain);
//		SetWindowPos(hWndMain, HWND_TOP, 0,0,0,0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
	} else {
		WndPlace.length = sizeof(WINDOWPLACEMENT);
		WndPlace.flags = 0;
		mainShowCmd = WndPlace.showCmd = (UINT)r11;
		WndPlace.rcNormalPosition.left = (int)r15;
		WndPlace.rcNormalPosition.top  = (int)r14;
		WndPlace.rcNormalPosition.right= (int)r13+(int)r15;
		WndPlace.rcNormalPosition.bottom=(int)r12+(int)r14;
		WndPlace.ptMaxPosition.x = 0;
		WndPlace.ptMaxPosition.y = 0;
		SetWindowPlacement(hWndMain, &WndPlace);
	}
#endif
#if KINT_POCKET_PC
	SetWindowPos(hWndMain, 0, 0, 0, 240, 320, SWP_NOZORDER);
#endif
	InSetWindowPos = t;
}

//**************************************************************************
void KINT_GetWindowPos(DWORD *r11, DWORD *r12, DWORD *r13, DWORD *r14, DWORD *r15) {

#if KINT_DESKTOP_WINDOWS
	UINT	showCmd;

	WndPlace.length = sizeof(WINDOWPLACEMENT);
	GetWindowPlacement(hWndMain, &WndPlace);
	*r15 = (DWORD)(WndPlace.rcNormalPosition.left);
	*r14 = (DWORD)(WndPlace.rcNormalPosition.top);
	*r13 = (DWORD)(WndPlace.rcNormalPosition.right-WndPlace.rcNormalPosition.left);
	*r12 = (DWORD)(WndPlace.rcNormalPosition.bottom-WndPlace.rcNormalPosition.top);
	showCmd = WndPlace.showCmd;
	if( showCmd==SW_MINIMIZE ) showCmd = SW_RESTORE;
	if( showCmd==SW_SHOWMINIMIZED ) showCmd = SW_SHOWNORMAL;
	*r11 = (DWORD)showCmd;
#endif
#if KINT_POCKET_PC
	*r15 = 0;
	*r14 = 0;
	*r13 = 240;
	*r12 = 320;
	*r11 = SW_SHOWNORMAL;
#endif
}

//*********************************************************
void StopMusic() {

	if( MusicPlaying ) {
#if KINT_MIDI
		mciSendCommand(MCIwDeviceID, MCI_STOP, MCI_WAIT, 0);
		mciSendCommand(MCIwDeviceID, MCI_CLOSE, MCI_WAIT, 0);
#endif
		MusicPlaying = 0;
	}
}

//****************************************************************************
void PlayMusic(char *pMem) {

#if KINT_MIDI
	MCIERROR	dwReturn;

	StopMusic();	// stop any previously playing music

	BuildPath(pMem, DIR_MUSIC, FALSE);

// Open the device by specifying the device name and device element.
// MCI will attempt to choose the MIDI Mapper as the output port.
	mciOpenParms.dwCallback = 0;
	mciOpenParms.wDeviceID = 0;
    mciOpenParms.lpstrDeviceType = "sequencer";//NULL;
    mciOpenParms.lpstrElementName = (TCHAR *)TmpPath;
	mciOpenParms.lpstrAlias = NULL;
    dwReturn = mciSendCommand(0, MCI_OPEN, MCI_OPEN_TYPE | MCI_WAIT | MCI_OPEN_ELEMENT, (DWORD)(LPVOID) &mciOpenParms);
//    dwReturn = mciSendCommand(0, MCI_OPEN, MCI_WAIT | MCI_OPEN_ELEMENT, (DWORD)(LPVOID) &mciOpenParms);
    if( dwReturn ) return; // Failed to open device, bail out

// Begin playback. The window procedure function for the parent window
// will be notified with an MM_MCINOTIFY message when playback is
// complete. At that time, the window procedure closes the device.
    MCIwDeviceID = mciOpenParms.wDeviceID;
    mciPlayParms.dwCallback = (DWORD)(hWndMain);
	dwReturn = mciSendCommand(MCIwDeviceID, MCI_PLAY, MCI_NOTIFY, (DWORD)(LPVOID) &mciPlayParms);
	if( dwReturn ) {	// if error
        mciSendCommand(MCIwDeviceID, MCI_CLOSE, 0, 0);	// close MCI device
        return;	// and bail out
    }
	MusicPlaying = 1;
#else
	MusicPlaying = 0;
#endif
}

//*******************************************************************
void KPlaySound(char *pMem) {

	BuildPath(pMem, DIR_SOUNDS, FALSE);
// note: on this platform, the .WAV files are in the 'home' folder
	sndPlaySound(TmpPath, SND_ASYNC | SND_NODEFAULT);
}

//*******************************************************************
void KPlaySoundMem(char *pMem, BOOL nostop) {

	sndPlaySound(pMem, SND_MEMORY | SND_ASYNC | SND_NODEFAULT | (nostop?SND_NOSTOP:0));
}

//******************************************************************
void StartTimer(DWORD timval) {

// create periodic timer
	if( TimerID==0 ) TimerID = SetTimer(hWndMain, 1, (UINT)timval, 0);
}

//******************************************************************
void StopTimer() {

	if( TimerID!=0 ) KillTimer(hWndMain, TimerID);
	TimerID = 0;
}

//****************************************
void InitScreen(long minW, long minH) {

	int			redraw;
	HDC			hDC;
	WORD		o, ow, oh;

	if( minW==-1 ) {
		ow = kBuf->logW;
		oh = kBuf->logH;
		minW = minH = 0;
	} else {
		ow = (WORD)minW;
		oh = (WORD)minH;
	}
	hDC = GetDC(NULL);
	PhysicalScreenW = GetDeviceCaps(hDC, HORZRES);
	PhysicalScreenH = GetDeviceCaps(hDC, VERTRES);
	ReleaseDC(NULL, hDC);

	if( kBuf!=NULL ) {
		DeleteBMB(kBuf);
		redraw = 1;
	} else redraw = 0;

	if( PhysicalScreenW > minW ) minW = PhysicalScreenW;
	if( PhysicalScreenH > minH ) minH = PhysicalScreenH;

#if KINT_DESKTOP_WINDOWS
	if( 3*1024 > minW ) minW = 3*1024;
	if( 2*1024 > minH ) minH = 2*1024;
#endif

	kBuf = CreateBMB((WORD)minW, (WORD)minH, TRUE);
	if( NULL==kBuf ) {
		SystemMessage("Failed creating screen BITMAP");
		exit(0);
	}
#if KINT_DESKTOP_WINDOWS
		 if( kBuf->logW<800  || kBuf->logH<600 ) PhysicalScreenSizeIndex = 0;
	else if( kBuf->logW<1024 || kBuf->logH<768 ) PhysicalScreenSizeIndex = 1;
	else if( kBuf->logW<1152 || kBuf->logH<864 ) PhysicalScreenSizeIndex = 2;
	else if( kBuf->logW<1280 || kBuf->logH<1024) PhysicalScreenSizeIndex = 3;
	else if( kBuf->logW<1600 || kBuf->logH<1280) PhysicalScreenSizeIndex = 4;
	else PhysicalScreenSizeIndex = 5;
#endif

// initialize the ENTIRE screen bitmap to black
	o = kBuf->orient;
	kBuf->orient = BMO_NORMAL;
	RectBMB(0, 0, 0, kBuf->physW, kBuf->physH, 0x00000000, -1);
	kBuf->orient = o;

	kBuf->logW = ow;
	kBuf->logH = oh;

	if( redraw ) {
// call the SCREEN_CHANGE code in the KE module
/****/	AddMsg(KHDR_SCREEN_CHANGE, 0, 0, 0);
		SendNextMsg();
	}
}

//*****************************************************************
// The following palette is used for 256 color displays.
COLORREF	OurPalRGB[256];

BYTE	kLogPal[sizeof(LOGPALETTE)+(4096-1)*sizeof(PALETTEENTRY)];
BYTE	OurPal[sizeof(LOGPALETTE)+255*sizeof(PALETTEENTRY)] = {
			0, 3,	// version
			0, 1,	// number of entries
// 1 BLACK/WHITE
		0x00, 0x00, 0x00, 0x00,
		0x22, 0x22, 0x22, 0x00,
		0x33, 0x33, 0x33, 0x00,
		0x44, 0x44, 0x44, 0x00,
		0x55, 0x55, 0x55, 0x00,
		0x66, 0x66, 0x66, 0x00,
		0x77, 0x77, 0x77, 0x00,
		0x88, 0x88, 0x88, 0x00,
		0x99, 0x99, 0x99, 0x00,
		0xAA, 0xAA, 0xAA, 0x00,
		0xBB, 0xBB, 0xBB, 0x00,
		0xCC, 0xCC, 0xCC, 0x00,
		0xDD, 0xDD, 0xDD, 0x00,
		0xEE, 0xEE, 0xEE, 0x00,
		0xFF, 0xFF, 0xFF, 0x00,
// 2 BROWN
		0x28, 0x14, 0x00, 0x00,
		0x41, 0x23, 0x00, 0x00,
		0x5F, 0x32, 0x00, 0x00,
		0x82, 0x41, 0x00, 0x00,
		0xA0, 0x50, 0x00, 0x00,
		0xC3, 0x5F, 0x14, 0x00,
		0xE1, 0x73, 0x1E, 0x00,
		0xFF, 0x82, 0x32, 0x00,
		0xFF, 0x91, 0x41, 0x00,
		0xFF, 0xA0, 0x50, 0x00,
		0xFF, 0xAF, 0x5F, 0x00,
		0xFF, 0xBE, 0x73, 0x00,
		0xFF, 0xD2, 0x82, 0x00,
		0xFF, 0xE1, 0x91, 0x00,
		0xFF, 0xF0, 0xA0, 0x00,
// 3 BROWN
		0x32, 0x1E, 0x1E, 0x00,
		0x41, 0x20, 0x20, 0x00,
		0x5F, 0x30, 0x30, 0x00,
		0x82, 0x40, 0x40, 0x00,
		0xA0, 0x50, 0x50, 0x00,
		0xBE, 0x60, 0x60, 0x00,
		0xE1, 0x70, 0x70, 0x00,
		0xFF, 0x7F, 0x7F, 0x00,
		0xFF, 0x7F, 0x7F, 0x00,
		0xFF, 0x8E, 0x7F, 0x00,
		0xFF, 0x9F, 0x7F, 0x00,
		0xFF, 0xAF, 0x7F, 0x00,
		0xFF, 0xBF, 0x7F, 0x00,
		0xFF, 0xCF, 0x7F, 0x00,
		0xFF, 0xDF, 0x7F, 0x00,
// 4 BRICK RED
		0x28, 0x0D, 0x0D, 0x00,
		0x40, 0x15, 0x15, 0x00,
		0x60, 0x20, 0x20, 0x00,
		0x80, 0x2A, 0x2A, 0x00,
		0xA0, 0x35, 0x35, 0x00,
		0xC0, 0x40, 0x40, 0x00,
		0xE0, 0x4A, 0x4A, 0x00,
		0xFF, 0x55, 0x55, 0x00,
		0xFF, 0x67, 0x64, 0x00,
		0xFF, 0x6F, 0x74, 0x00,
		0xFF, 0x75, 0x84, 0x00,
		0xFF, 0x84, 0x9D, 0x00,
		0xFF, 0x94, 0xB7, 0x00,
		0xFF, 0x9F, 0xD1, 0x00,
		0xFF, 0xAE, 0xEA, 0x00,
// 5 RED TO ORANGE TO YELLOW
		0x90, 0x14, 0x00, 0x00,
		0xA0, 0x20, 0x00, 0x00,
		0xB0, 0x30, 0x00, 0x00,
		0xC0, 0x40, 0x00, 0x00,
		0xD0, 0x50, 0x00, 0x00,
		0xE0, 0x60, 0x00, 0x00,
		0xF0, 0x70, 0x00, 0x00,
		0xFF, 0x80, 0x00, 0x00,
		0xFF, 0x90, 0x00, 0x00,
		0xFF, 0xA0, 0x00, 0x00,
		0xFF, 0xB0, 0x00, 0x00,
		0xFF, 0xC0, 0x00, 0x00,
		0xFF, 0xD0, 0x00, 0x00,
		0xFF, 0xE0, 0x00, 0x00,
		0xFF, 0xF0, 0x00, 0x00,
// 6 RED
		0x28, 0x00, 0x00, 0x00,
		0x40, 0x00, 0x00, 0x00,
		0x60, 0x00, 0x00, 0x00,
		0x80, 0x00, 0x00, 0x00,
		0xA0, 0x00, 0x00, 0x00,
		0xC0, 0x00, 0x00, 0x00,
		0xE0, 0x00, 0x00, 0x00,
		0xFF, 0x00, 0x00, 0x00,
		0xFF, 0x28, 0x28, 0x00,
		0xFF, 0x40, 0x40, 0x00,
		0xFF, 0x60, 0x60, 0x00,
		0xFF, 0x80, 0x80, 0x00,
		0xFF, 0xA0, 0xA0, 0x00,
		0xFF, 0xC0, 0xC0, 0x00,
		0xFF, 0xE0, 0xE0, 0x00,
// 7 PURPLE
		0x28, 0x00, 0x28, 0x00,
		0x40, 0x00, 0x40, 0x00,
		0x60, 0x00, 0x60, 0x00,
		0x80, 0x00, 0x80, 0x00,
		0xA0, 0x00, 0xA0, 0x00,
		0xC0, 0x00, 0xC0, 0x00,
		0xE0, 0x00, 0xE0, 0x00,
		0xFF, 0x00, 0xFF, 0x00,
		0xFF, 0x28, 0xFF, 0x00,
		0xFF, 0x40, 0xFF, 0x00,
		0xFF, 0x60, 0xFF, 0x00,
		0xFF, 0x80, 0xFF, 0x00,
		0xFF, 0xA0, 0xFF, 0x00,
		0xFF, 0xC0, 0xFF, 0x00,
		0xFF, 0xE0, 0xFF, 0x00,
// 8 LIGHT PURPLE
		0x28, 0x14, 0x28, 0x00,
		0x40, 0x20, 0x40, 0x00,
		0x60, 0x30, 0x60, 0x00,
		0x80, 0x40, 0x80, 0x00,
		0xA0, 0x50, 0xA0, 0x00,
		0xC0, 0x60, 0xC0, 0x00,
		0xE0, 0x70, 0xE0, 0x00,
		0xFF, 0x7F, 0xFF, 0x00,
		0xFF, 0x94, 0xFF, 0x00,
		0xFF, 0xA0, 0xFF, 0x00,
		0xFF, 0xB0, 0xFF, 0x00,
		0xFF, 0xC0, 0xFF, 0x00,
		0xFF, 0xD0, 0xFF, 0x00,
		0xFF, 0xE0, 0xFF, 0x00,
		0xFF, 0xF0, 0xFF, 0x00,
// 9 DARK PURPLE
		0x28, 0x00, 0x50, 0x00,
		0x35, 0x05, 0x66, 0x00,
		0x42, 0x0A, 0x7C, 0x00,
		0x4F, 0x0F, 0x92, 0x00,
		0x5C, 0x14, 0xA8, 0x00,
		0x69, 0x19, 0xBE, 0x00,
		0x76, 0x1E, 0xD4, 0x00,
		0x83, 0x23, 0xEA, 0x00,
		0x90, 0x28, 0xFF, 0x00,
		0xA0, 0x40, 0xFF, 0x00,
		0xB0, 0x60, 0xFF, 0x00,
		0xC0, 0x80, 0xFF, 0x00,
		0xD0, 0xA0, 0xFF, 0x00,
		0xE0, 0xC0, 0xFF, 0x00,
		0xF0, 0xE0, 0xFF, 0x00,
// 10 BLUE
		0x00, 0x00, 0x28, 0x00,
		0x00, 0x00, 0x40, 0x00,
		0x00, 0x00, 0x60, 0x00,
		0x00, 0x00, 0x80, 0x00,
		0x00, 0x00, 0xA0, 0x00,
		0x00, 0x00, 0xC0, 0x00,
		0x00, 0x00, 0xE0, 0x00,
		0x00, 0x00, 0xFF, 0x00,
		0x0A, 0x28, 0xFF, 0x00,
		0x28, 0x4A, 0xFF, 0x00,
		0x46, 0x6A, 0xFF, 0x00,
		0x67, 0x8A, 0xFF, 0x00,
		0x87, 0xAA, 0xFF, 0x00,
		0xA7, 0xCA, 0xFF, 0x00,
		0xC7, 0xEB, 0xFF, 0x00,
// 11 DARK CYAN
		0x0F, 0x1E, 0x1E, 0x00,
		0x14, 0x23, 0x23, 0x00,
		0x19, 0x32, 0x32, 0x00,
		0x1E, 0x41, 0x41, 0x00,
		0x28, 0x50, 0x50, 0x00,
		0x32, 0x5F, 0x5F, 0x00,
		0x37, 0x73, 0x73, 0x00,
		0x41, 0x82, 0x82, 0x00,
		0x46, 0x91, 0x91, 0x00,
		0x50, 0xA0, 0xA0, 0x00,
		0x5A, 0xAF, 0xAF, 0x00,
		0x5F, 0xC3, 0xC3, 0x00,
		0x69, 0xD2, 0xD2, 0x00,
		0x73, 0xE1, 0xE1, 0x00,
		0x78, 0xF0, 0xF0, 0x00,
// 12 CYAN
		0x00, 0x28, 0x28, 0x00,
		0x00, 0x40, 0x40, 0x00,
		0x00, 0x60, 0x60, 0x00,
		0x00, 0x80, 0x80, 0x00,
		0x00, 0xA0, 0xA0, 0x00,
		0x00, 0xC0, 0xC0, 0x00,
		0x00, 0xE0, 0xE0, 0x00,
		0x00, 0xFF, 0xFF, 0x00,
		0x28, 0xFF, 0xFF, 0x00,
		0x40, 0xFF, 0xFF, 0x00,
		0x60, 0xFF, 0xFF, 0x00,
		0x80, 0xFF, 0xFF, 0x00,
		0xA0, 0xFF, 0xFF, 0x00,
		0xC0, 0xFF, 0xFF, 0x00,
		0xE0, 0xFF, 0xFF, 0x00,
// 13 GREEN
		0x00, 0x28, 0x00, 0x00,
		0x00, 0x40, 0x00, 0x00,
		0x00, 0x60, 0x00, 0x00,
		0x00, 0x80, 0x00, 0x00,
		0x00, 0xA0, 0x00, 0x00,
		0x00, 0xC0, 0x00, 0x00,
		0x00, 0xE0, 0x00, 0x00,
		0x00, 0xFF, 0x00, 0x00,
		0x28, 0xFF, 0x28, 0x00,
		0x40, 0xFF, 0x40, 0x00,
		0x60, 0xFF, 0x60, 0x00,
		0x80, 0xFF, 0x80, 0x00,
		0xA0, 0xFF, 0xA0, 0x00,
		0xC0, 0xFF, 0xC0, 0x00,
		0xE0, 0xFF, 0xE0, 0x00,
// 14 GREEN TO YELLOW
		0x0A, 0x23, 0x0A, 0x00,
		0x23, 0x41, 0x23, 0x00,
		0x32, 0x5F, 0x32, 0x00,
		0x41, 0x82, 0x41, 0x00,
		0x50, 0xA0, 0x50, 0x00,
		0x5F, 0xC3, 0x5F, 0x00,
		0x73, 0xE1, 0x73, 0x00,
		0x82, 0xFF, 0x82, 0x00,
		0x91, 0xFF, 0x6E, 0x00,
		0xA0, 0xFF, 0x5F, 0x00,
		0xB4, 0xFF, 0x50, 0x00,
		0xC3, 0xFF, 0x41, 0x00,
		0xD2, 0xFF, 0x32, 0x00,
		0xE1, 0xFF, 0x23, 0x00,
		0xF0, 0xFF, 0x0F, 0x00,
// 15 YELLOW
		0x28, 0x28, 0x00, 0x00,
		0x40, 0x40, 0x00, 0x00,
		0x60, 0x60, 0x00, 0x00,
		0x80, 0x80, 0x00, 0x00,
		0xA0, 0xA0, 0x00, 0x00,
		0xC0, 0xC0, 0x00, 0x00,
		0xE0, 0xE0, 0x00, 0x00,
		0xFF, 0xFF, 0x00, 0x00,
		0xFF, 0xFF, 0x28, 0x00,
		0xFF, 0xFF, 0x40, 0x00,
		0xFF, 0xFF, 0x60, 0x00,
		0xFF, 0xFF, 0x80, 0x00,
		0xFF, 0xFF, 0xA0, 0x00,
		0xFF, 0xFF, 0xC0, 0x00,
		0xFF, 0xFF, 0xE0, 0x00
	};

int		clrmap16[16]={0,16,32,48,64,80,104,120,136,152,168,184,200,224,240,255};

//***************************************************************************
void MakePalette() {

#if KINT_DESKTOP_WINDOWS
	int	r, g, b;
	HDC		hDC;
	
	hDC = GetDC(NULL);
	bitspixel = GetDeviceCaps(hDC, BITSPIXEL);
	planes = GetDeviceCaps(hDC, PLANES);
	numcolors = GetDeviceCaps(hDC, NUMCOLORS);
	sizepalette = GetDeviceCaps(hDC, SIZEPALETTE);
	ReleaseDC(NULL, hDC);

	if( bitspixel*planes==8 || sizepalette==256 ) colormode = 1;
	else if( bitspixel*planes>8 || numcolors>256 ) colormode = 2;
	else colormode = 0;

	if( colormode==2 ) {
		for(b=0; b<16; b++) {
			for(g=0; g<16; g++) {
				for(r=0; r<16; r++) {
					kLogPal[4+4*(256*b+16*g+r)+0] = clrmap16[r];
					kLogPal[4+4*(256*b+16*g+r)+1] = clrmap16[g];
					kLogPal[4+4*(256*b+16*g+r)+2] = clrmap16[b];
					kLogPal[4+4*(256*b+16*g+r)+3] = PC_NOCOLLAPSE;
				}
			}
		}
		kLogPal[2] = 0;
		kLogPal[3] = 16;
	} else {
		memcpy(kLogPal, OurPal, sizeof(OurPal));
	}
	kPal = CreatePalette((LPLOGPALETTE)kLogPal);
#else
	kPal = NULL;
#endif
}

//*******************************************************************
void DestroyPalette() {

#if KINT_DESKTOP_WINDOWS
	DeleteObject(kPal);
#endif
}

//********************************************************************
// Display the NULL-terminated message in 'pMem' using OS resources.
void SystemMessage(char *pMem) {

#if KINT_POCKET_PC
	ShowTaskBar(TRUE);
#endif

	MessageBox(NULL, Widen(pMem), TEXT("!"), MB_OK);

#if KINT_POCKET_PC
	ShowTaskBar(FALSE);
#endif
}

//*********************************************************************
// store the appropriate End-Of-Line sequence into buffer 'pMem' and
// return the number of bytes written to 'pMem'.
int GetSystemEOL(BYTE *pMem) {

#if KINT_MS_WINDOWS
	*pMem++ = '\r';
	*pMem++ = '\n';
	return 2;
#endif
#if KINT_UNIX
	*pMem++ = '\n';
	return 1;
#endif
}

//***************************************
PBMB CreateBMB(WORD w, WORD h, WORD sysBM) {

	PBMB	pX;

	pX = (PBMB)malloc(sizeof(BMB));
	if( pX!=NULL ) {
		pX->linelen = 4*((3*w+3)/4);	// force line of pixels to take even number of DWORDS
		pX->pBits = NULL;
		if( !sysBM && NULL==(pX->pBits=(BYTE *)malloc(pX->linelen*h)) ) {
			free(pX);
			pX = NULL;
		} else {
			pX->pAND = NULL;
			pX->pNext = (void *)pbFirst;
			if( pbFirst!=NULL ) pbFirst->pPrev = (void *)pX;
			pX->pPrev = NULL;
			pbFirst = pX;
			pX->hBitmap = NULL;
			pX->physW = w;
			pX->physH = h;
			pX->clipX1 = pX->clipY1 = 0;
			pX->clipX2 = (int)w;
			pX->clipY2 = (int)h;
			if( sysBM ) {
				BITMAPINFO			pbmi;

				pbmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
				pbmi.bmiHeader.biWidth = (long)w;
				pbmi.bmiHeader.biHeight = -(long)h;	// negative to make bitmap "top-down" addressing
				pbmi.bmiHeader.biPlanes = 1;
				pbmi.bmiHeader.biBitCount = 24;
				pbmi.bmiHeader.biSizeImage = pX->linelen*h;
				pbmi.bmiHeader.biXPelsPerMeter = pbmi.bmiHeader.biYPelsPerMeter = pbmi.bmiHeader.biClrUsed = pbmi.bmiHeader.biClrImportant = pbmi.bmiHeader.biCompression = 0;
#if KINT_POCKET_PC || KINT_POCKET_PC_EMUL
				pX->logW = 320;
				pX->logH = 240;
	#if KINT_POCKET_PC_EMUL
				pX->orient = BMO_NORMAL;
	#else
				pX->orient = BMO_90;
	#endif
				pX->clipX2 = (int)pX->logW;
				pX->clipY2 = (int)pX->logH;
#else
				pX->logW = w;
				pX->logH = h;
				pX->orient = BMO_NORMAL;
#endif
				if( NULL==(pX->hBitmap=CreateDIBSection(NULL, &pbmi, DIB_RGB_COLORS, (void **)&pX->pBits, NULL, 0)) ) {
					DeleteBMB(pX);
					pX = NULL;
				}
			} else {
				pX->logW = w;
				pX->logH = h;
				pX->orient = BMO_NORMAL;
			}
		}
	}
	return pX;
}

//***************************************
void DeleteBMB(PBMB pX) {

	if( pX!=NULL ) {
		if( pX->pAND!= NULL ) free(pX->pAND);
		if( pX->hBitmap != NULL ) DeleteObject(pX->hBitmap);
		else if( pX->pBits != NULL ) free(pX->pBits);
		if( pX==pbFirst ) {
			pbFirst = (PBMB)pX->pNext;
			if( pbFirst!=NULL ) pbFirst->pPrev = NULL;
		} else {
			if( pX->pNext!=NULL ) ((PBMB)pX->pNext)->pPrev = pX->pPrev;
			if( pX->pPrev!=NULL ) ((PBMB)pX->pPrev)->pNext = pX->pNext;
		}
		free(pX);
	}
}

//**********************************************************************
void InvalidateScreen(int x1, int y1, int x2, int y2) {

	int		t;
	RECT	rect;

	if( x1>x2 ) {
		t  = x1;
		x1 = x2;
		x2 = t;
	}
	if( y1>y2 ) {
		t  = y1;
		y1 = y2;
		y2 = t;
	}
#if KINT_DOUBLE_SIZE
	switch( kBuf->orient ) {
	 case BMO_NORMAL:
		rect.left = 2*x1;
		rect.top  = 2*y1;
		rect.right= 2*x2;
		rect.bottom=2*y2;
		break;
	 case BMO_180:
		rect.left = 2*(kBuf->logW-x2);
		rect.top  = 2*(kBuf->logH-y2);
		rect.right= 2*(kBuf->logW-x1);
		rect.bottom=2*(kBuf->logH-y1);
		break;
	 case BMO_90:
		rect.left = 2*y1;
		rect.top  = 2*(kBuf->logW-x2);
		rect.right= 2*y2;
		rect.bottom=2*(kBuf->logW-x1);
		break;
	 case BMO_270:
		rect.left = 2*(kBuf->logH-y2);
		rect.top  = 2*x1;
		rect.right= 2*(kBuf->logH-y1);
		rect.bottom=2*x2;
		break;
	 case BMO_FNORMAL:
		rect.left = 2*(kBuf->logW-x2);
		rect.top  = 2*y1;
		rect.right= 2*(kBuf->logW-x1);
		rect.bottom=2*y2;
		break;
	 case BMO_F180:
		rect.left = 2*x1;
		rect.top  = 2*(kBuf->logH-y2);
		rect.right= 2*x2;
		rect.bottom=2*(kBuf->logH-y1);
		break;
	 case BMO_F90:
		rect.left = 2*(kBuf->logH-y2);
		rect.top  = 2*(kBuf->logW-x2);
		rect.right= 2*(kBuf->logH-y1);
		rect.bottom=2*(kBuf->logW-x1);
		break;
	 case BMO_F270:
		rect.left = 2*y1;
		rect.top  = 2*x1;
		rect.right= 2*y2;
		rect.bottom=2*x2;
		break;
	 default:
		SOerr(kBuf);
		break;
	}
#else
	switch( kBuf->orient ) {
	 case BMO_NORMAL:
		rect.left = x1;
		rect.top  = y1;
		rect.right= x2;
		rect.bottom=y2;
		break;
	 case BMO_180:
		rect.left = kBuf->logW-x2;
		rect.top  = kBuf->logH-y2;
		rect.right= kBuf->logW-x1;
		rect.bottom=kBuf->logH-y1;
		break;
	 case BMO_90:
		rect.left = y1;
		rect.top  = kBuf->logW-x2;
		rect.right= y2;
		rect.bottom=kBuf->logW-x1;
		break;
	 case BMO_270:
		rect.left = kBuf->logH-y2;
		rect.top  = x1;
		rect.right= kBuf->logH-y1;
		rect.bottom=x2;
		break;
	 case BMO_FNORMAL:
		rect.left = kBuf->logW-x2;
		rect.top  = y1;
		rect.right= kBuf->logW-x1;
		rect.bottom=y2;
		break;
	 case BMO_F180:
		rect.left = x1;
		rect.top  = kBuf->logH-y2;
		rect.right= x2;
		rect.bottom=kBuf->logH-y1;
		break;
	 case BMO_F90:
		rect.left = kBuf->logH-y2;
		rect.top  = kBuf->logW-x2;
		rect.right= kBuf->logH-y1;
		rect.bottom=kBuf->logW-x1;
		break;
	 case BMO_F270:
		rect.left = y1;
		rect.top  = x1;
		rect.right= y2;
		rect.bottom=x2;
		break;
	 default:
		SOerr(kBuf);
		break;
	}
#endif
	InvalidateRect(hWndMain, &rect, FALSE);
}

//*********************************************************
// don't allow a program to access anything above the game directory,
// so don't allow absolute paths or .. or // or such.
char *GetFileName(char *fname, char *xname) {

	char	*p;

	if( fname[0]=='\\' ) return NULL; // fail miserably if they try absolute path, only allow SUB-dirs
	strcpy(PartPath, fname);
	for(p=PartPath; *p; p++) {
		if( (*p=='.' && *(p+1)=='.')
		 || (*p=='/' && *(p+1)=='/')
		 || (*p==':')
		) return NULL;	// fail miserably if they try some tricky directory BS, only allow SUB-dirs
		if( *p=='/' ) *p = '\\';	// DOS/Windows path conversion
	}
	strcat(PartPath, xname);	// stick desired file extension on end
	return PartPath;
}

//*********************************************************
//	In the Windows implementation, UI, IMAGES, and BACKGNDS files
//	are all .BMP files, and get that file extendsion added to them
//	automatically.  MUSIC files are assumed to be MIDI files and get .MID.
//	SOUNDS are assumed to be .WAV files, and get .WAV appended.
//	When porting this to another platform (or even under Windows),
//	there's absolutely NO reason why you can't use other graphics file
//	formats (so long as they're 'lossless', so as to preserve the exact
//	colors of the pixels, so that transparency works right), or other
//	music (like MP3) or sound file formats.  As long as you modify
//	the KINT code that opens and loads/plays the files, you can use
//	whatever file formats you want.  The KASM code doesn't care.
//
// This routine MUST build the full file path in 'TmpPath', as that's
// where the calling functions expect to find it.
//
void BuildPath(char *pMem, BYTE folder, BOOL AppData) {

	if( AppData ) kscpy(TmpPath, AppDataPath);
	else kscpy(TmpPath, PgmPath);

	switch( folder & 0x0F ) {
	 case DIR_GAME:	// GAME (home) folder
		kscat(TmpPath, Widen(GetFileName(pMem, "")));
		break;
	 case DIR_UI:
		kscat(TmpPath, TEXT("UI\\"));
		kscat(TmpPath, Widen(pMem));
		if( !(folder & DIR_NOEXT) ) {
			if( folder & DIR_LOSSY ) kscat(TmpPath, TEXT(".JPG"));
			else kscat(TmpPath, TEXT(".BMP"));
		}
		break;
	 case DIR_IMAGES:
		kscat(TmpPath, TEXT("IMAGES\\"));
		kscat(TmpPath, Widen(pMem));
		if( !(folder & DIR_NOEXT) ) {
			if( folder & DIR_LOSSY ) kscat(TmpPath, TEXT(".JPG"));
			else kscat(TmpPath, TEXT(".BMP"));
		}
		break;
	 case DIR_BACKGNDS:
		kscat(TmpPath, TEXT("BACKGNDS\\"));
		kscat(TmpPath, Widen(pMem));
		if( !(folder & DIR_NOEXT) ) {
			if( folder & DIR_LOSSY ) kscat(TmpPath, TEXT(".JPG"));
			else kscat(TmpPath, TEXT(".BMP"));
		}
		break;
	 case DIR_MUSIC:
	// note: in this implementation, all music files are in a "common" folder up one level from the 'home' folder
	// so that other games from Everett Kaser Software can share the same MIDI files.
		kscat(TmpPath, TEXT("..\\MUSIC\\"));
		kscat(TmpPath, Widen(pMem));
		if( !(folder & DIR_NOEXT) ) kscat(TmpPath, TEXT(".MID"));
		break;
	 case DIR_SOUNDS:
	// note: in this implementation, we store the WAV files in the 'home' folder
		kscat(TmpPath, Widen(GetFileName(pMem, "")));
		if( !(folder & DIR_NOEXT) ) kscat(TmpPath, TEXT(".WAV"));
		break;
	 case DIR_GAME_IMG:
	// note: this is a GRAPHICS file, located in the GAME folder.
		kscat(TmpPath, Widen(GetFileName(pMem, "")));
		if( !(folder & DIR_NOEXT) ) {
			if( folder & DIR_LOSSY ) kscat(TmpPath, TEXT(".JPG"));
			else kscat(TmpPath, TEXT(".BMP"));
		}
		break;
	 case DIR_UI_COMMON:
	// note: in this implementation, all UI_COMMON files are in a "common" folder up 
	// one level from the 'home' folder so that other games from Everett Kaser Software 
	// can share the same UI files.  This folder contains .UI files and lossyBM (.JPG) files
		kscat(TmpPath, TEXT("..\\COMMONUI\\"));
		kscat(TmpPath, Widen(pMem));
		if( !(folder & DIR_NOEXT) ) {
			if( folder & DIR_LOSSY ) kscat(TmpPath, TEXT(".JPG"));
			else kscat(TmpPath, TEXT(".BMP"));
		}
		break;
	 default:
		break;
	}
}

#if KINT_UNIX
//*********************************************************************************
// Set up CasePtrs/CaseCnt in case of case-sensitive OS
void InitCasePath() {

	CasePtrs[CaseCnt=0] = CasePath;
	CaseLoopLim = 3;
	if( TmpPath[0]!='/' && TmpPath[0]!='\\' ) ++CaseCnt;
	else CaseLoopLim = 1;

	for(p=TmpPath; *p; p++) {
		if( *p=='\\' ) *p = '/';	// DOS/Windows -> Unix path conversion
		if( *p=='/' || *p=='.' ) {
			// setup ptrs into CasePath where fname will be copied
			// during each iteration of trying raw, uppercase, and lowercase filenames
			CasePtr[CaseCnt++] = CasePath + (p-TmpPath+1);
			CaseLoopLim *= 3;
		}
	}
	CasePtr[CaseCnt] = CasePath + (p-TmpPath+1);
}

void GenCasePath() {

	char	*p;
	DWORD	partindex, partcase, remcase;

	strcpy(CasePath, TmpPath);
	for(partindex=0, remcase=CaseLoop; partindex<CaseCnt; partindex++) {
		partcase = remcase % 3;
		remcase /= 3;
		if( partcase ) {	// modify case
			if( --partcase ) {	// UPPER case this part
				for(p=CasePtrs[partindex]; p<CasePtrs[partindex+1]; p++) *p = toupper(*p);
			} else {	// LOWER case this part
				for(p=CasePtrs[partindex]; p<CasePtrs[partindex+1]; p++) *p = tolower(*p);
			}
		}
	}
}
#endif

//*********************************************************************************
// make a sub-folder of the AppData folder or a sub-folder thereof
// return non-zero if successful, else return zero if fail
int KMakeFolder(char *fname) {

	if( !kscmp(fname, TEXT("../COMMONUI")) ) {
		kscpy(TmpPath, AppDataPath);
		kscat(TmpPath, TEXT("..\\COMMONUI"));
	} else {
		BuildPath(fname, DIR_GAME, TRUE);
	}
	return CreateDirectory(TmpPath, NULL);
}

//*********************************************************************************
// Delete an empty folder (can NOT contain ANY files)
void KDelFolder(char *fname) {

	BuildPath(fname, DIR_GAME, TRUE);
#if KINT_MS_WINDOWS
	RemoveDirectory(TmpPath);
#endif

#if KINT_UNIX
	InitCasePath();
	for(CaseLoop=0; CaseLoop<CaseLoopLim; CaseLoop++) {
		GenCasePath();
// NOTE: change the RemoveDirectory() function for your brand of C-compiler/Unix/etc
// this assumes that it returns 0 if the folder deleted ok.
		if( !RemoveDirectory(CasePath) ) break;
	}
#endif
}

//*********************************************************************************
// return file handle if successful, else -1
DWORD Kopen(char *fname, int oflags, BYTE dir) {

	char	*p, tmpbuf[PATHLEN];

	// if O_WRBIN or O_WRBINNEW
		// if open(O_RDBIN) in AppData
			// close file
		// else if open(O_RDBIN) in PgmPath
			// close file
			// build AppData filename and copy to tmpbuf
			// KCopyFile(fname, tmpbuf);
		// open(oflags) in AppData
		// return handle
	// else
		// if open(O_RDBIN) in AppData
			// return handle
		// else if open(O_RDBIN) in PgmData
			// return handle
	// return -1

#if KINT_MS_WINDOWS	
	int		retval;

	#if KINT_POCKET_PC
		DWORD	dw1, dw2;

//GEEKDO: Modify this to match the structure of the desktop Windows open()
		BuildPath(fname, dir, FALSE);
		for(p=TmpPath; *p; p++) if( *p=='/' ) *p = '\\';	// DOS/Windows path conversion
		dw1 = GENERIC_READ;
		if( oflags & 0x0003 ) dw1 |= GENERIC_WRITE;
		dw2 = (oflags & 0x0300) ? CREATE_ALWAYS : OPEN_EXISTING;
		return (DWORD)CreateFile(TmpPath, dw1, 0, NULL, dw2, FILE_ATTRIBUTE_NORMAL, NULL);
	#else
		if( oflags==O_RDBIN ) {	// if just READING
			BuildPath(fname, dir, TRUE);	// try AppData first
			for(p=TmpPath; *p; p++) if( *p=='/' ) *p = '\\';	// DOS/Windows path conversion
			retval = _open(TmpPath, oflags, _S_IREAD | _S_IWRITE);
			if( retval==-1 ) {
				BuildPath(fname, dir, FALSE);	// try PgmPath second
				for(p=TmpPath; *p; p++) if( *p=='/' ) *p = '\\';	// DOS/Windows path conversion
				retval = _open(TmpPath, oflags, _S_IREAD | _S_IWRITE);
			}
		} else {				// if WRITING
			BuildPath(fname, dir, TRUE);	// try AppData first
			for(p=TmpPath; *p; p++) if( *p=='/' ) *p = '\\';	// DOS/Windows path conversion

			strcpy(tmpbuf, TmpPath);		// save AppData target name
			retval = _open(tmpbuf, O_RDBIN, _S_IREAD | _S_IWRITE);
			if( retval==-1 ) {
				// make sure target AppData path exists
				for(p=tmpbuf+strlen(tmpbuf)-1; p>tmpbuf && *p!='\\'; p--) ;	// find end of 'path' portion
				if( p > tmpbuf ) {
					char	*b[64];
					long	bcnt;

					bcnt = 0;
					b[bcnt] = p;
					*b[bcnt] = 0;	// cut off file name portion, just keep path portion
					++bcnt;
					while( -1==_stat(tmpbuf, &fstats) ) {	// find 'lowest' portion of path that already exists
						for(b[bcnt]=b[bcnt-1]-1; b[bcnt]>tmpbuf && *b[bcnt]!='\\'; --b[bcnt]) ;	// find next higher folder
						if( b[bcnt] <= tmpbuf ) break;
						*b[bcnt] = 0;
						++bcnt;
					}
					if( b[bcnt-1] > tmpbuf ) {
						while( bcnt>1 ) {			// create the rest of the path folders that didn't already exist
							--bcnt;
							*b[bcnt] = '\\';
							CreateDirectory(tmpbuf, NULL);
						}

					}
					*b[0] = '\\';
				}

				BuildPath(fname, dir, FALSE);	// try PgmPath second
				for(p=TmpPath; *p; p++) if( *p=='/' ) *p = '\\';	// DOS/Windows path conversion
				retval = _open(TmpPath, O_RDBIN, _S_IREAD | _S_IWRITE);
				if( retval != -1 ) {
					_close(retval);
					KCopyFile(fname, tmpbuf);
				}
			} else _close(retval);
			retval = _open(tmpbuf, oflags, _S_IREAD | _S_IWRITE);
		}
	#endif

	return retval;
#endif

#if KINT_UNIX
	DWORD	retval;

	BuildPath(fname, dir, FALSE);
	InitCasePath();
	// this loop tries opening every combination of modifications possible on the TmpPath
	// pathname, where the "parts" of the pathname are the directory names and filenames,
	// and any parts of those names that are separated by '.'  Each "part" of the pathname
	// can be:
	//	0	left alone (no modification)
	//	1	uppercased
	//	2	lowercased
	// 'CaseLoopLim' was set (above) to 3^n where 'n' is the number of "parts".  'loop' (below)
	// is then stepped from 0 to CaseLoopLim-1, and each count of 3 (ie, 'loop' is a base-3
	// value, and is plucked apart by mod-ing and dividing by 3) controls a separate 'part'
	// of the pathname.  Thus, this code will try opening the unmodified pathname FIRST,
	// then will step through all possible combinations.  Since KINT games rarely have more
	// than 1 subfolder, and the filename has but 1 (at most) '.' there's rarely more than
	// 3 parts to the pathname, rarely more than 4 parts, and probably never more than 5.
	// For example, accessing the MUSIC or COMMONUI folder files would have something like
	// "../MUSIC/HALLELUJ.MID" which would have 4 parts.  Accessing image sets would be
	// something like "IMAGES/DEFAULT.BMP" which would have 3 parts.  It would be an unusual
	// game that would have two levels of sub-levels of subfolders of the game folder (resulting
	// in 4 parts) and probably never one with three sub-levels resulting in 5 parts.  But just
	// in case (no pun intended...) the CasePtrs[] array is declared with 10 members, supporting
	// up to 9 pathname parts (since the 10th one would be used to point to the end of the pathname).
	// This means that there will usually be 9 (3x3) different cases to try, frequently 27 (3x3x3),
	// and sometimes 81 (3x3x3x3).  If a sub-folder had a sub-folder which had a sub-folder, 
	// there would be 243 (3x3x3x3x3) possibilities to check, a good reason never to go that deep. :-)
	for(CaseLoop=0; CaseLoop<CaseLoopLim; CaseLoop++) {
		GenCasePath();
		if( -1 != (retval=(DWORD)open(CasePath, oflags, _S_IREAD | _S_IWRITE)) ) return retval;
	}
#endif
}

//*********************************************************************************
// return file len if successful, else -1
DWORD Kstat(char *fname, BYTE dir, DWORD *modtim, BYTE *filtyp) {

	char	*p;
	int		retval;
	DWORD	len;

	BuildPath(fname, dir, TRUE);	// try AppData first
	for(p=TmpPath; *p; p++) if( *p=='/' ) *p = '\\';	// DOS/Windows path conversion

	if( -1==_stat(TmpPath, &fstats) ) {
		BuildPath(fname, dir, FALSE);	// try PgmPath second
		for(p=TmpPath; *p; p++) if( *p=='/' ) *p = '\\';	// DOS/Windows path conversion
		if( -1==_stat(TmpPath, &fstats) ) return 0xFFFFFFFF;
	}
	*modtim = fstats.st_mtime;
	if( (fstats.st_mode & S_IFMT)==S_IFDIR ) *filtyp = 2;
	else *filtyp = 1;

	retval = _open(TmpPath, O_RDBIN, _S_IREAD | _S_IWRITE);
	if( retval==-1 ) return 0xFFFFFFFF;

	len = _lseek(retval, 0, 2);	// 2 is defined in the KINT system as SEEK_END (usually same as C libs, but just in case...)

	_close(retval);

	return len;
}

//********************************************************************************
// In KINT/KASM, 'origin' has these values (in case your language is different):
//		SEEK_SET    0
//		SEEK_CUR    1
//		SEEK_END    2
// return new file pointer position or -1 if error
DWORD Kseek(DWORD fhandle, DWORD foffset, int origin) {

#if KINT_POCKET_PC
	DWORD	dw2;

	if( origin == 0 ) dw2 = FILE_BEGIN;
	else if( origin==1 ) dw2 = FILE_CURRENT;
	else dw2 = FILE_END;
	return SetFilePointer((HANDLE)fhandle, (long)foffset, NULL, dw2);
#else
	return _lseek((int)fhandle, (long)foffset, origin);
#endif
}

//*********************************************************************************
void Kclose(DWORD fhandle) {

#if KINT_POCKET_PC
	CloseHandle((HANDLE)fhandle);
#else
	_close((int)fhandle);
#endif
}

//********************************************************************************
//	return 1 if at EOF, 0 if not at EOF, -1 if error
DWORD Keof(DWORD fhandle) {

#if KINT_POCKET_PC
	return (DWORD)((SetFilePointer((HANDLE)fhandle, 0, NULL, FILE_CURRENT)>=GetFileSize((HANDLE)fhandle, NULL))?1:0);
#else
	return (DWORD)_eof((int)fhandle);
#endif
}

//******************************************************
DWORD Kread(DWORD fhandle, BYTE *pMem, DWORD numbytes) {

	DWORD	bytesread;

#if KINT_POCKET_PC
	ReadFile((HANDLE)fhandle, pMem, numbytes, &bytesread, NULL);
#else
	int		chunk, need;

	bytesread = 0;
	while( numbytes ) {
		if( numbytes<16384 ) need = (int)numbytes;
		else need = 16384;
		chunk = _read((int)fhandle, (void *)pMem, need);
		if( chunk==-1 ) {
			bytesread = -1;
			break;
		}
		if( chunk==0 ) break;
		bytesread += (DWORD)chunk;
		pMem += chunk;
		numbytes -= (DWORD)chunk;
	}
#endif
	return bytesread;
}

//*******************************************************
DWORD Kwrite(DWORD fhandle, BYTE *pMem, DWORD numbytes) {

	DWORD	byteswritten;

#if KINT_POCKET_PC
	WriteFile((HANDLE)fhandle, pMem, numbytes, &byteswritten, NULL);
#else
	int		chunk, need;

	byteswritten = 0;
	while( numbytes ) {
		if( numbytes<16384 ) need = (int)numbytes;
		else need = 16384;
		chunk = _write((int)fhandle, (void *)pMem, need);
		if( chunk==-1 ) {
			byteswritten = -1;
			break;
		}
		byteswritten += (DWORD)chunk;
		pMem += chunk;
		numbytes -= (DWORD)chunk;
	}
#endif
	return byteswritten;
}

//****************************************************
// Terminate the Kfindfirst()/Kfindnext() process
void Kfindclose() {

	FIRSTNEXT	*pNext;

	while( pFirstNext != NULL ) {
		pNext = pFirstNext->pNext;
		free(pFirstNext);
		pFirstNext = pNext;
	}
	pNextNext = NULL;
}

//**************************************
void Kff(BOOL AppData, BOOL stripEXT) {

	DWORD		ffhandle;
	TCHAR		*pChar;
	FIRSTNEXT	*pLast;
	BYTE		nbuf[PATHLEN];
	
#if KINT_MS_WINDOWS
	#if KINT_POCKET_PC
		ffhandle = (DWORD)FindFirstFile(TmpPath, &fdat32);

		if( ffhandle != 0xFFFFFFFF ) {
			do {
				// find JUST the filename portion
				for(pChar=fdat32.cFileName+kslen(fdat32.cFileName); pChar>fdat32.cFileName && *(pChar-1)!=TEXT('\\'); --pChar) ;
				UnWiden((char *)nbuf, pChar);
				if( stripEXT ) {

					BYTE	*pCh;

					for(pCh=nbuf+strlen(nbuf); pCh>nbuf && *pCh!='.'; --pCh) ;
					if( *pCh=='.' ) *pCh = '\0';
				}

				// if first one, just alloc a struct and go on
				if( pFirstNext==NULL ) {
					pFirstNext = pLast = (FIRSTNEXT *)malloc(sizeof(FIRSTNEXT)+kslen(fdat32.cFileName));
				// else if not first one, check for duplicate fname
				} else {
					for(pLast=pFirstNext; ; pLast=pLast->pNext) {
						// if duplicate fname, skip this one, don't add it, don't allocate, don't collect $200
						if( !stricmp(pLast->fname, nbuf) ) goto dupe;
						if( pLast->pNext==NULL ) break;
					}
					// new fname, alloc a struct and go on
					pLast->pNext = (FIRSTNEXT *)malloc(sizeof(FIRSTNEXT)+kslen(fdat32.cFileName));
					pLast = pLast->pNext;
				}

				pLast->pNext = NULL;
				pLast->AppData = AppData;
				pLast->attrib = (BYTE)((fdat32.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)?0x10:0x00);
				strcpy(pT->fname, nbuf);
dupe:;
			} while( !FindNextFile((HANDLE)ffhandle, &fdat32) );
			FindClose((HANDLE)ffhandle);
		}
	#else
		ffhandle = (DWORD)_findfirst(TmpPath, &fdat);

		if( ffhandle != 0xFFFFFFFF ) {
			do {
				// find JUST the filename portion
				for(pChar=fdat.name+strlen(fdat.name); pChar>fdat.name && *(pChar-1)!='\\'; --pChar) ;
				strcpy((char *)nbuf, pChar);
				if( stripEXT ) {	// if not DIR_GAME remove file extension
					for(pChar=(char *)nbuf+strlen((char *)nbuf); pChar>(char *)nbuf && *pChar!='.'; --pChar) ;
					if( *pChar=='.' ) *pChar = '\0';	// remove file extension
				}

				// if first one, just alloc a struct and go on
				if( pFirstNext==NULL ) {
					pFirstNext = pLast = (FIRSTNEXT *)malloc(sizeof(FIRSTNEXT)+kslen(fdat.name));
				// else if not first one, check for duplicate fname
				} else {
					for(pLast=pFirstNext; ; pLast=pLast->pNext) {
						// if duplicate fname, skip this one, don't add it, don't allocate, don't collect $200
						if( !stricmp(pLast->fname, nbuf) ) goto dupe;
						if( pLast->pNext==NULL ) break;
					}
					// new fname, alloc a struct and go on
					pLast->pNext = (FIRSTNEXT *)malloc(sizeof(FIRSTNEXT)+kslen(fdat.name));
					pLast = pLast->pNext;
				}

				pLast->pNext = NULL;
				pLast->AppData = AppData;
				pLast->attrib = (BYTE)(fdat.attrib);
				strcpy(pLast->fname, nbuf);
dupe:;
			} while( !_findnext((long)ffhandle, &fdat) );
			_findclose((long)ffhandle);
		}
	#endif
#endif

#if KINT_UNIX
	// NOTE: this won't find ALL files that match, if there are different
	// mixtures of upper/lower case in various parts of pathnames, only
	// those that match the "case mixture" of the first one found!!!!
	// Not good, but fixing it will require changes to KASM game codes.  Sigh.
	InitCasePath();
	for(CaseLoop=0; CaseLoop<CaseLoopLim; CaseLoop++) {
		GenCasePath();
// NOTE: replace this following code with whatever your brand of Unix requires.
		if( bad!=(ffhandle=Unixfindfirstfile(CasePath, &fdat)) ) {
		// find JUST the filename portion
			for(pChar=fdat.name+strlen(fdat.name); pChar>fdat.name && *(pChar-1)!='\\'; --pChar) ;
			strcpy((char *)fbuf, pChar);
			if( dir!=DIR_GAME && !(dir & DIR_NOEXT) ) {	// if not DIR_GAME remove file extension
				for(pChar=(char *)fbuf+strlen((char *)fbuf); pChar>(char *)fbuf && *pChar!='.'; --pChar) ;
				if( *pChar=='.' ) *pChar = '\0';	// remove file extension
			}
			break;
		}
	}
#endif
}

//******************************************************************
// Search for a filename (possibly containing '*' as wild card)
DWORD Kfindfirst(char *fname, BYTE *fbuf, BYTE *attrib, BYTE dir) {

	BOOL	stripEXT;

	Kfindclose();

	stripEXT = dir!=DIR_GAME && !(dir & DIR_NOEXT);

	if( CopyAppData ) {	// if there's an AppData directory tree, search it first
		BuildPath(fname, dir, TRUE);
		Kff(TRUE, stripEXT);
	}
	BuildPath(fname, dir, FALSE);	// now search the PgmPath directory tree
	Kff(FALSE, stripEXT);

	if( pFirstNext ) {
		*attrib = pFirstNext->attrib;
		strcpy(fbuf, pFirstNext->fname);
		pNextNext = pFirstNext->pNext;
		return 0;
	}

	return 0xFFFFFFFF;
}

//********************************************************************
// Search for next filename in sequence started by Kfindfirst()
DWORD Kfindnext(BYTE *fbuf, BYTE *attrib) {

	if( pNextNext ) {
		*attrib = pNextNext->attrib;
		strcpy(fbuf, pNextNext->fname);
		pNextNext = pNextNext->pNext;
		return 0;
	}

	return 0xFFFFFFFF;
}

//*************************************************
void Kdelete(char *fname, BYTE dir) {

	BuildPath(fname, dir, TRUE);
#if KINT_MS_WINDOWS
	#if KINT_POCKET_PC
		DeleteFile(TmpPath);
	#else
		_unlink(TmpPath);
	#endif
#endif
#if KINT_UNIX
	InitCasePath();
	for(CaseLoop=0; CaseLoop<CaseLoopLim; CaseLoop++) {
		GenCasePath();
// NOTE: change the _unlink() function for your brand of C-compiler/Unix/etc
// this assumes that it returns 0 if the file deleted ok.
		if( !_unlink(CasePath) ) break;
	}
#endif
}

//*****************************************************
// Only called from SysMakeShareCopy in PMACH.C and Kopen() in MAIN.C, so always copies from PgmPath
WORD KCopyFile(char *src, char *dst) {

#if KINT_DESKTOP_WINDOWS
	BuildPath(src, DIR_GAME, FALSE);
	return (WORD)CopyFile(TmpPath, Widen(dst), FALSE);
#else
	return FALSE;
#endif
}

//*****************************************************
void KGetTime(KTIME *pT) {

	SYSTEMTIME	curtime;

	GetLocalTime(&curtime);
	pT->y = (WORD)curtime.wYear;
	pT->mo= (WORD)curtime.wMonth;
	pT->d = (WORD)curtime.wDay;
	pT->h = (WORD)curtime.wHour;
	pT->mi= (WORD)curtime.wMinute;
	pT->s = (WORD)curtime.wSecond;
}

//******************************************************
void KTimeToString(BYTE *pMem) {

	SYSTEMTIME	curtime;

	GetLocalTime(&curtime);
	sprintf((char *)pMem, "%d-%d-%d %2.2d:%2.2d:%2.2d", curtime.wMonth, curtime.wDay, curtime.wYear, curtime.wHour, curtime.wMinute, curtime.wSecond);
}

//****************************************************************************
//		Return a millisecond count which doesn't necessarily bear
//		any resemblance to "real" time, but rather is a counter that is
//		a reasonably accurate "millisecond counter" that may eventually
//		roll over to 0, but which should not do so more often than once a day,
//		and who's count value continues upwards (until such time as a roll-over
//		might occur).  This is accomplished on the MS-Windows platform using
//		the GetTickCount() function.  Other platforms may choose to implement
//		this in whatever way seems easiest, while meeting the above criteria.
//		This counter value is only used for timing the speed of animations
//		and brief delays in actions.  So, reasonable accuracy down to 5 or 10
//		milliseconds would be very nice, and the roll-over (if and when it
//		occurs) should only cause (at most) a slight burp in any animation
//		or screen action that is going on at the moment (but from which it
//		should recover almost immediately).
//    NOTE: It is IMPORTANT that his count be relative accurate, as it is
//		used for generating the elapsed time count for solving puzzles!
//	It could be done getting the time of day, and subtracting that from the
//  time of day when the program started, but the Windows GetTickCount() function
//	works nicely, too.

DWORD KGetTicks() {return GetTickCount();}

// yield to OS for 'tm' milliseconds
//
// NOTE: we don't simply call Sleep() or SleepEx() here because under Win XP,
// doing so will allow re-entry into the main message pump at an unfortunate
// time which will allow things to get "out of sync" in the .KE code, which
// isn't good.  So, we just spin our wheels here, calling PeekMessage() to
// other processes (on Win 95/98/Me) a chance.
//
void KSleep(DWORD tm) {

	MSG			msg;
	DWORD		st;

	st = KGetTicks();
	while( st+tm > KGetTicks() ) PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE);	// just keep calling system if we're asleep, do nothing else
}

//**********************************************************
void KSetCursor(WORD curtype) {
//	curtype = 0 for normal, 1 for wait, 2 for hotlink

	CurCur = curtype;
	// CurCur is used in the MainWnd WM_MOUSEMOVE code in MAIN.CPP to update the cursor
#if KINT_HELP
	if( CurCur==7 ) SetCursor(LoadCursor(NULL, IDC_SIZEALL));
	else if( CurCur==6 ) SetCursor(LoadCursor(NULL, IDC_SIZENESW));
	else if( CurCur==5 ) SetCursor(LoadCursor(NULL, IDC_SIZENWSE));
	else if( CurCur==4 ) SetCursor(LoadCursor(NULL, IDC_SIZEWE));
	else if( CurCur==3 ) SetCursor(LoadCursor(NULL, IDC_SIZENS));
	else 
#endif
#if KINT_DESKTOP_WINDOWS
	if( CurCur==2 ) SetCursor(LoadCursor(NULL, IDC_UPARROW));
	else 
#endif
	if( CurCur==1 ) SetCursor(LoadCursor(NULL, IDC_WAIT));
	else SetCursor(LoadCursor(NULL, IDC_ARROW));
}

//**********************************************************
void KSetTitle(BYTE *pMem) {

	SetWindowText(hWndMain, Widen((char *)pMem));
}

//***********************************************************
// Return 0 if fail, non-0 if succeed (and set pw, ph, Xppi, Yppi)
// Get everything setup for printing which is to follow
DWORD KPrintStart(DWORD *pw, DWORD *ph, DWORD *Xppi, DWORD *Yppi) {

/*
typedef struct tagPD {  // pd 
    DWORD     lStructSize; 
    HWND      hwndOwner; 
    HANDLE    hDevMode; 
    HANDLE    hDevNames; 
    HDC       hDC; 
    DWORD     Flags; 
    WORD      nFromPage; 
    WORD      nToPage; 
    WORD      nMinPage; 
    WORD      nMaxPage; 
    WORD      nCopies; 
    HINSTANCE hInstance; 
    DWORD     lCustData; 
    LPPRINTHOOKPROC lpfnPrintHook; 
    LPSETUPHOOKPROC lpfnSetupHook; 
    LPCTSTR    lpPrintTemplateName; 
    LPCTSTR    lpSetupTemplateName; 
    HANDLE    hPrintTemplate; 
    HANDLE    hSetupTemplate; 
} PRINTDLG; 
*/ 
	HDC			hdc;

	char	*pNames;

	pdlg.lStructSize = sizeof(PRINTDLG);
	pdlg.hwndOwner = hWndMain;
	pdlg.hDevMode  = NULL;
	pdlg.hDevNames = NULL;
	pdlg.Flags =	  PD_RETURNDC
				| PD_NOPAGENUMS
				| PD_NOSELECTION
				| PD_NOWARNING
				| PD_USEDEVMODECOPIESANDCOLLATE 
				 ;
	pdlg.hInstance = NULL;//ghInstance;
	pdlg.lCustData = 0;
	pdlg.lpfnPrintHook = NULL;
	pdlg.lpfnSetupHook = NULL;
	pdlg.lpPrintTemplateName = NULL;
	pdlg.lpSetupTemplateName = NULL;
	pdlg.hPrintTemplate = 0;
	pdlg.hSetupTemplate = 0;
	if( PrintDlg(&pdlg) ) {
		pNames = (char *)GlobalLock(pdlg.hDevNames);
		hdc = CreateDC(	pNames + ((DEVNAMES *)pNames)->wDriverOffset,
						pNames + ((DEVNAMES *)pNames)->wDeviceOffset, NULL, NULL);
		GlobalUnlock(pdlg.hDevNames);

		*pw = (DWORD)(long)GetDeviceCaps(hdc, HORZRES);
		*ph = (DWORD)(long)GetDeviceCaps(hdc, VERTRES);
		*Xppi = (DWORD)(long)GetDeviceCaps(hdc, LOGPIXELSX);
		*Yppi = (DWORD)(long)GetDeviceCaps(hdc, LOGPIXELSY);
		if( StartDoc(hdc, &PRTdocinfo) <= 0 ) {
			DeleteDC(hdc);
			hdc = NULL;
		}
	} else {
//		sprintf(TmpPath, "ERROR: %d", CommDlgExtendedError());
//		MessageBox(NULL, TmpPath, "DEBUG", MB_OK);

		hdc = NULL;
	}

	hPRT = (DWORD)hdc;
	return hPRT;
}

//*******************************************************
// Shutdown and clean up after printing, we're all through
void KPrintStop() {

#if KINT_PRINT
  #if KINT_MS_WINDOWS
	if( hPRT!=0 ) {
		EndDoc((HDC)hPRT);
		DeleteDC((HDC)hPRT);
		hPRT = 0;
	}
	if( hPRTFont!=NULL ) DeleteObject(hPRTFont);
  #endif
#endif
}

//*****************************************************
//	return non-0 if successful, 0 if fail
BYTE KPrintBeginPage() {

#if KINT_PRINT
  #if KINT_MS_WINDOWS
	return (StartPage((HDC)hPRT) > 0);
  #else
	return 0;
  #endif
#else
	return 0;
#endif
}

//*****************************************************
//	return non-0 if successful, 0 if fail
BYTE KPrintEndPage() {

#if KINT_PRINT
  #if KINT_MS_WINDOWS
	return (EndPage((HDC)hPRT) > 0);
  #else
	return 0;
  #endif
#else
	return 0;
#endif
}

//******************************************************
void KPrintText(int x, int y, char *str, DWORD clr) {

#if KINT_PRINT
  #if KINT_MS_WINDOWS
	TCHAR	*pT;
	HFONT	hOldFont;

	SetBkMode((HDC)hPRT, TRANSPARENT);
	SetTextColor((HDC)hPRT, (COLORREF)clr);
	if( hPRTFont!=NULL ) hOldFont = SelectObject((HDC)hPRT, hPRTFont);
	SetTextAlign((HDC)hPRT, TA_LEFT | TA_TOP);
	SelectPalette((HDC)hPRT, kPal, 0);
	RealizePalette((HDC)hPRT);
	pT = Widen(str);
	TextOut((HDC)hPRT, x, y, pT, kslen(pT));
	if( hPRTFont!=NULL ) SelectObject((HDC)hPRT, hOldFont);
  #endif
#endif
}

//***********************************************************
void KPrintGetTextSize(char *str, DWORD *dw, DWORD *dh, DWORD *dasc) {

#if KINT_PRINT
  #if KINT_MS_WINDOWS
	TCHAR		*pT;
	TEXTMETRIC	tm;
	SIZE		txtsize;
	HFONT		hOldFont;

	if( hPRTFont!=NULL ) hOldFont = SelectObject((HDC)hPRT, hPRTFont);
	SetTextAlign((HDC)hPRT, TA_LEFT | TA_TOP);
	pT = Widen(str);
	GetTextMetrics((HDC)hPRT, &tm);
	GetTextExtentPoint32((HDC)hPRT, pT, kslen(pT), &txtsize);
	*dw = txtsize.cx;
	*dh = txtsize.cy;
	*dasc=tm.tmAscent;
	if( hPRTFont!=NULL ) SelectObject((HDC)hPRT, hOldFont);
  #endif
#endif
}

//**************************************************************************
void KPrintBMB(int xd, int yd, int wd, int hd, PBMB bmS, int xs, int ys, int ws, int hs) {

#if KINT_PRINT
  #if KINT_MS_WINDOWS
	int	oldstretchs, oldstretchd;
	PBMB	bmP;

	if( bmS==0 ) {
		bmP = kBuf;
	} else {
		bmP = CreateBMB((WORD)ws, (WORD)hs, TRUE);
		BltBMB(bmP, 0, 0, bmS, xs, ys, ws, hs, (int)FALSE, (int)FALSE);
		xs = ys = 0;
	}

	oldBMs = SelectObject(sdc, bmP->hBitmap);
	SelectPalette(sdc, kPal, 0);
	RealizePalette(sdc);

	oldstretchs = SetStretchBltMode(sdc, STRETCH_DELETESCANS); 
	oldstretchd = SetStretchBltMode(ddc, STRETCH_DELETESCANS); 

	StretchBlt((HDC)hPRT, xd, yd, wd, hd, sdc, xs, ys, ws, hs, SRCCOPY);

	SetStretchBltMode(sdc, oldstretchs);
	SetStretchBltMode(ddc, oldstretchd);
	SelectObject(sdc, oldBMs);
	if( bmP!=kBuf ) DeleteBMB(bmP);
  #endif
#endif
}

//**************************************************************************
void KPrintRect(int xd, int yd, int wd, int hd, DWORD fillcolor, DWORD edgecolor) {

#if KINT_PRINT
  #if KINT_MS_WINDOWS

	HPEN	p, op;
	POINT	lp[5];
	int		x, y;

	if( fillcolor!=-1 && edgecolor!=-1 ) {
		p = CreatePen(PS_SOLID, 1, (COLORREF)edgecolor);
		op = (HPEN)SelectObject((HDC)hPRT, p);
	
		lp[0].x = lp[3].x = lp[4].x = xd;
		lp[0].y = lp[1].y = lp[4].y = yd;
		lp[1].x = lp[2].x = xd+wd-1;
		lp[2].y = lp[3].y = yd+hd-1;
		Polyline((HDC)hPRT, lp, 5);

		SelectObject((HDC)hPRT, op);
		DeleteObject(p);

		++xd;
		++yd;
		--wd;--wd;
		--hd;--hd;

		p = CreatePen(PS_SOLID, 1, (COLORREF)fillcolor);
		op = (HPEN)SelectObject((HDC)hPRT, p);
		
		if( wd>hd ) {
			lp[0].x = xd;
			lp[1].x = xd+wd-1;
			for(y=yd; hd; ++y, --hd) {
				lp[0].y = lp[1].y = y;
				Polyline((HDC)hPRT, lp, 2);
			}
		} else {
			lp[0].y = yd;
			lp[1].y = yd+hd-1;
			for(x=xd; wd; ++x, --wd) {
				lp[0].x = lp[1].x = x;
				Polyline((HDC)hPRT, lp, 2);
			}
		}

		SelectObject((HDC)hPRT, op);
		DeleteObject(p);
	} else if( fillcolor==-1 ) {
		p = CreatePen(PS_SOLID, 1, (COLORREF)edgecolor);
		op = (HPEN)SelectObject((HDC)hPRT, p);
	
		lp[0].x = lp[3].x = lp[4].x = xd;
		lp[0].y = lp[1].y = lp[4].y = yd;
		lp[1].x = lp[2].x = xd+wd-1;
		lp[2].y = lp[3].y = yd+hd-1;
		Polyline((HDC)hPRT, lp, 5);

		SelectObject((HDC)hPRT, op);
		DeleteObject(p);
	} else {
		p = CreatePen(PS_SOLID, 1, (COLORREF)fillcolor);
		op = (HPEN)SelectObject((HDC)hPRT, p);
		
		if( wd>hd ) {
			lp[0].x = xd;
			lp[1].x = xd+wd-1;
			for(y=yd; hd; ++y, --hd) {
				lp[0].y = lp[1].y = y;
				Polyline((HDC)hPRT, lp, 2);
			}
		} else {
			lp[0].y = yd;
			lp[1].y = yd+hd-1;
			for(x=xd; wd; ++x, --wd) {
				lp[0].x = lp[1].x = x;
				Polyline((HDC)hPRT, lp, 2);
			}
		}

		SelectObject((HDC)hPRT, op);
		DeleteObject(p);
	}
  #endif
#endif
}

//************************************************************************************
void KPrintFont(WORD hlim, WORD flags, WORD *h, WORD *asc) {

#if KINT_PRINT
  #if KINT_MS_WINDOWS
	int			logpixy;
	LOGFONT		sFont;
	HFONT		hOldFont;
	TEXTMETRIC	tm;

	if( hPRTFont ) DeleteObject(hPRTFont);

	logpixy = GetDeviceCaps((HDC)hPRT, LOGPIXELSY);
	sFont.lfWidth = 0;
	sFont.lfEscapement = sFont.lfOrientation = 0;
	sFont.lfWeight = (flags & 2)?FW_BOLD:FW_NORMAL;
	sFont.lfItalic = (flags & 4)?TRUE:FALSE;
	sFont.lfUnderline = FALSE;
	sFont.lfStrikeOut = FALSE;
	sFont.lfCharSet = ANSI_CHARSET;
	sFont.lfOutPrecision = OUT_DEFAULT_PRECIS;
	sFont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
	sFont.lfQuality = 2;//PROOF_QUALITY;
	if( flags & 1 ) {
		sFont.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
		strcpy(sFont.lfFaceName, "Courier New");
	} else {
		sFont.lfPitchAndFamily = VARIABLE_PITCH | FF_MODERN;
		strcpy(sFont.lfFaceName, "Tahoma");
	}
	sFont.lfHeight = -(hlim*logpixy/72);
	hPRTFont = CreateFontIndirect(&sFont);

	hOldFont = SelectObject((HDC)hPRT, hPRTFont);
	GetTextMetrics((HDC)hPRT, &tm);
	SelectObject((HDC)hPRT, hOldFont);
	*h = (WORD)tm.tmHeight;
	*asc = (WORD)tm.tmAscent;
  #endif
#endif
}

//*********************************************************
void KPrintLines(DWORD *pMem, WORD numpoints, DWORD clr) {

#if KINT_PRINT
  #if KINT_MS_WINDOWS
	HPEN	hPen, holdPen;
	int		i, *pI;

	hPen = CreatePen(PS_SOLID, 1, clr);
	holdPen = SelectObject((HDC)hPRT, hPen);

	if( NULL!=(pI=(int *)malloc(2*numpoints*sizeof(int))) ) {
		for(i=0; i<(int)numpoints; i++) {
			pI[2*i] = (int)(long)(*pMem++);
			pI[2*i+1] = (int)(long)(*pMem++);
		}
		Polyline((HDC)hPRT, (POINT *)pI, (int)numpoints);
		free(pI);
	}
	SelectObject((HDC)hPRT, holdPen);
	DeleteObject(hPen);
  #endif
#endif
}

//**********************************************************************
void FlushScreen() {

	UpdateWindow(hWndMain);
}

//****************************************************************
void KCaptureMouse(BOOL cap) {

	if( cap ) SetCapture(hWndMain);
	else ReleaseCapture();
}

//****************************************************************
// This function can return as much (well, up to 512 bytes including NUL)
// or as little information as you want regarding the current OS.  This
// information is ONLY used for display purposes within the Help, and
// does NOT affect the program execution in any other way.
void KGetOSversion(BYTE *pMem) {

/*typedef struct _OSVERSIONINFO{ 
    DWORD dwOSVersionInfoSize; 
    DWORD dwMajorVersion; 
    DWORD dwMinorVersion; 
    DWORD dwBuildNumber; 
    DWORD dwPlatformId; 
    TCHAR szCSDVersion[ 128 ]; 
} OSVERSIONINFO; 
*/

	OSVERSIONINFO	osinfo;

	osinfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	if( GetVersionEx(&osinfo) ) {
		sprintf(pMem, "MS Windows ID:%u Version:%u.%u Build:%u (%s)",
			osinfo.dwPlatformId,
			osinfo.dwMajorVersion, osinfo.dwMinorVersion,
			osinfo.dwBuildNumber,
			osinfo.szCSDVersion);
	} else strcpy(pMem, "OS unknown");
}

//***************
//* END OF FILE *
//***************
