/*******************************************

 MAIN.H - The KINT Interpreter
	Copyright 2003 Everett Kaser
	All rights reserved.

	Do not copy or redistribute without the
	author's explicit written permission.
 *******************************************/

#define	BETA		1
#define BETAYEAR	2012	// year of expiry-1900, so 100=2000, 101=2001, etc.
#define BETAMONTH	12	// month of expiry, Jan=1 Feb=2 Mar=3 Apr=4 May=5 Jun=6 Jul=7 Aug=8 Sep=9 Oct=10 Nov=11 Dec=12
#define	BETADAY		15	// 1-31
#define	BETAKEY		0xA394CF12

#if BETA
  #define	KINT_DEBUG	0
#else
  #define	KINT_DEBUG	0
#endif

//*****************************************
//* VERSION TO BUILD **********************
										//*
#define	KINT_DESKTOP_WINDOWS		1	//*	// set to 1 for running on an MS Windows desktop/notebook
#define	KINT_POCKET_PC				0	//*	// set to 1 for running on an MS Pocket PC
#define	KINT_UNIX					0	//*

#define	KINT_MS_WINDOWS	(KINT_DESKTOP_WINDOWS || KINT_POCKET_PC)

#if KINT_DESKTOP_WINDOWS				//*
  #define	KINT_POCKET_PC_EMUL		0	//*	// set to 1 for emulating the PocketPC on a desktop
  #define	KINT_DOUBLE_SIZE		0	//*	// set to 1 to draw the emulated PocketPC screen double-size
#else									//*
  #define	KINT_POCKET_PC_EMUL		0	//*	// ALWAYS 0
  #define	KINT_DOUBLE_SIZE		0	//*	// ALWAYS 0
#endif									//*

#if KINT_POCKET_PC						//*
  #define	KINT_PRINT				0	//*	// ALWAYS 0
  #define	KINT_MIDI				0	//*	// ALWAYS 0
  #define	KINT_MAKE_COPY			0	//*	// ALWAYS 0
#else
  #define	KINT_PRINT				1	//*	// set to 1 to include the SysPrint code
  #define	KINT_MIDI				1	//*	// set to 1 to include the MIDI music playing code
  #define	KINT_MAKE_COPY			1	//*	// set to 1 to include the SysMakeSharewareCopy code
#endif

#define	KINT_HELP					0	//*	// set to 1 if this interpreter is for the HELP EDITOR
										//*
//* VERSION TO BUILD **********************
//*****************************************

#if KINT_MS_WINDOWS
  #define	PATHLEN	MAX_PATH
#else
  #define	PATHLEN 512
#endif

#if KINT_POCKET_PC
  #define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
  #define MENU_HEIGHT	26
#endif

#if KINT_MS_WINDOWS
  #include <windows.h>
  #include <stdio.h>
  #include <stdlib.h>
  #include <mmsystem.h>
  #include <tchar.h>
  #include <direct.h>
  #include <shlobj.h>
  #include <objidl.h>

  #if KINT_DESKTOP_WINDOWS
    #include <time.h>
    #include <io.h>
	#include <sys/types.h>
	#include <sys/stat.h>
  #endif

  #if KINT_POCKET_PC
    #include <aygshell.h>
    #include <sipapi.h>
  #endif

  #include "resource.h"
#else
  typedef	unsigned char	BYTE;
  typedef	unsigned short	WORD;
  typedef	unsigned long	DWORD;
  #define	NULL	0
  #define	FALSE	0
#endif

#include "kdbg.h"

#define	Kmin(a, b)	__min(a, b)
#define	Kmax(a, b)	__max(a, b)

// WM_SIZE defines for KINT
#define	K_SIZED		0
#define	K_MINIMIZED	1
#define	K_MAXIMIZED	2
#define	K_RESTORED	3

// BMB rotation/flip orient values
#define	BMO_NORMAL	0	// normal orientation
#define	BMO_90		1	// rotated counter-clockwise 90
#define	BMO_180		2	// rotated counter-clockwise 180
#define	BMO_270		3	// rotated counter-clockwise 270
#define	BMO_FNORMAL	4	// flipped left-right
#define	BMO_F90		5	// flipped & rotated 90
#define	BMO_F180	6	// flipped & rotated 180
#define	BMO_F270	7	// flipped & rotated 270

/*
#define	SO_WIDE		0	// non-flipped states must be even
#define	SO_WIDEFLIP	1	// flipped states must be odd
#define	SO_TALL		2
#define	SO_TALLFLIP	3
#define	SO_GET		4
#define	SO_FLIPPED	1	// for testing for "flipped" status
*/
typedef struct {
	BYTE		*pBits;
	BYTE		*pAND;
	WORD		linelen, orient, physW, physH, logW, logH;
	int			clipX1, clipX2, clipY1, clipY2;
#if KINT_MS_WINDOWS
	HBITMAP		hBitmap;
#endif
	void		*pPrev, *pNext;
} BMB, *PBMB;

typedef struct {
	WORD	y, mo, d, h, mi, s;
} KTIME;

#if KINT_DEBUG
	#define	DB_MAX_BP	50
#endif

typedef struct {
	BYTE	*pKode;				// ptr to memory block containing .KE
	DWORD	memlen;				// len of this .KE memory block
	DWORD	StackBase;			// offset in pKode of base of K stack for this .KE
	DWORD	reg4, reg5;			// save/init locations for SB, SP for this .KE

	DWORD	KEnum;
#if KINT_DEBUG
	DWORD	DB_BP[DB_MAX_BP];
	int		DB_BPcnt;
#endif
} KE;

//*****************
#include "pmach.h"
//*****************

//#if KINT_POCKET_PC
//  #define	stricmp(s, d)	_stricmp(s, d)
//#endif

// The following are the KASM/KINT file-open flag definitions.
// These are consistent with standard C library #define values
// but if this is being translated to another language, it's
// good to know what the KASM code is going to be passing you and
// what it means....
#define	O_RDBIN		0x8000	// read only, binary
#define	O_WRBIN		0x8002	// read/write, binary
#define	O_WRBINNEW	0x8302	// read/write, binary, create or truncate

// The following are the KASM/KINT file-open directories
#define	DIR_GAME		0
#define	DIR_UI			1
#define DIR_IMAGES		2
#define DIR_BACKGNDS	3
#define DIR_MUSIC		4
#define DIR_SOUNDS		5
#define	DIR_GAME_IMG	6
#define	DIR_UI_COMMON	7
#define	DIR_LOSSY		0x40
#define	DIR_NOEXT		0x80

void InitScreen(long minW, long minH);
void MakePalette();
void DestroyPalette();
void KillProgram();
void KINT_SetWindowPos(DWORD r11, DWORD r12, DWORD r13, DWORD r14, DWORD r15);
void KINT_GetWindowPos(DWORD *r11, DWORD *r12, DWORD *r13, DWORD *r14, DWORD *r15);
void PlayMusic(char *pMem);
void StopMusic();
void KPlaySound(char *pMem);
void KPlaySoundMem(char *pMem, BOOL nostop);
void StartTimer(DWORD timval);
void StopTimer();
void SystemMessage(char *pMem);
int GetSystemEOL(BYTE *pMem);
char *GetFileName(char *fname, char *xname);
void BuildPath(char *fname, BYTE dir, BOOL AppData);

DWORD Kopen(char *fname, int oflags, BYTE dir);
DWORD Kseek(DWORD fhandle, DWORD foffset, int origin);
void Kclose(DWORD fhandle);
DWORD Kstat(char *fname, BYTE dir, DWORD *modtim, BYTE *filtyp);
DWORD Keof(DWORD fhandle);
DWORD Kread(DWORD fhandle, BYTE *pMem, DWORD numbytes);
DWORD Kwrite(DWORD fhandle, BYTE *pMem, DWORD numbytes);
DWORD Kfindfirst(char *fname, BYTE *fbuf, BYTE *attrib, BYTE dir);
DWORD Kfindnext(BYTE *fbuf, BYTE *attrib);
void Kfindclose();
void Kdelete(char *fname, BYTE dir);
WORD KCopyFile(char *src, char *dst);
int KMakeFolder(char *fname);
void KDelFolder(char *fname);

void KGetTime(KTIME *pT);
void KTimeToString(BYTE *pMem);
DWORD KGetTicks();
void KSetCursor(WORD curtype);
void KSetTitle(BYTE *pMem);

PBMB CreateBMB(WORD w, WORD h, WORD sysBM);
void DeleteBMB(PBMB pX);
void InvalidateScreen(int x1, int y1, int x2, int y2);
void FlushScreen();

DWORD KPrintStart(DWORD *pw, DWORD *ph, DWORD *Xppi, DWORD *Yppi);
void KPrintStop();
BYTE KPrintBeginPage();
BYTE KPrintEndPage();
void KPrintText(int x, int y, char *str, DWORD clr);
void KPrintBMB(int xd, int yd, int wd, int hd, PBMB bmS, int xs, int ys, int ws, int hs);
void KPrintFont(WORD hlim, WORD flags, WORD *h, WORD *asc);
void KPrintLines(DWORD *pMem, WORD numpoints, DWORD clr);
void KPrintGetTextSize(char *str, DWORD *dw, DWORD *dh, DWORD *dasc);
void KPrintRect(int xd, int yd, int wd, int hd, DWORD fillcolor, DWORD edgecolor);
void KCaptureMouse(BOOL cap);
void KSleep(DWORD tmilli);
void KGetOSversion(BYTE *pMem);
