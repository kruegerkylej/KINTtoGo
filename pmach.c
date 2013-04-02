/*******************************************

 PMACH.CPP - The KINT Interpreter
	Copyright 2003 Everett Kaser
	All rights reserved.

	Do not copy or redistribute without the
	author's explicit written permission.
 *******************************************/

//*********************************
// INCLUDES

//#define	CHECKWORDALIGN

#include "main.h"

#ifdef CHECKWORDALIGN
	unsigned long savePC;
	#define	defgetmemw
#endif

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <math.h>

#if BETA
  #if KINT_DESKTOP_WINDOWS
	#include <time.h>
  #endif
  BYTE	Expired=1;
#endif

#define	GEEK_TRACE
#define	GEEK_TRACE2
#define	GEEK_TRACE3

#ifdef GEEK_TRACE
  #define TBMAX		512
	DWORD	TraceBuf[TBMAX], TBptr;
#endif
#ifdef GEEK_TRACE2
  #define TBMAX2	512
	DWORD	TraceBuf2[TBMAX2], TBptr2;
#endif
#ifdef GEEK_TRACE3
  #define TBMAX3	512
	DWORD	TraceBuf3[TBMAX3], TBptr3;
#endif

#if KINT_HELP
	int	F6BREAK=0;
#endif

#if KINT_DEBUG
	#include	<io.h>
	#include	<fcntl.h>
	#include	<ctype.h>

	extern BYTE	*DbgMem;
	extern HWND	hWndMain;

	char	dbuf[8192];

	int		KdbgHalted=0;

#endif

//************************************************************
//* PORTING NOTES
//* -------------
//* 1) In this code, a BYTE is 8 bits, a WORD is 16 bits,
//*		a DWORD is 32 bits, all unsigned.  A 'char' is 8 bits,
//*		a 'short' is 16 bits, a long is 32 bits, all signed.
//* 2) Multi-byte values in the KODEs are stored in little-endian
//*		order (what's used on current INTEL processors, the primary
//*		current target platform.  If you're porting this to a
//*		platform that uses a big-endian processor, you'll need to
//*		VERY carefully modify the multi-byte operations to grab
//*		the bytes in the correct order. See the GETREG/SETREG and
//*		GETMEM/SETMEM macros in pmach.h.
//* 3) This implementation knows that memory pointers are DWORDS
//*		and uses that fact to place them directly into "registers"
//*		R0, R1, and R2.  (The .KE code only saves and loads these
//*		values, NEVER does arithmetic on them.)  If this gets ported
//*		to a machine that uses (for example) 8-byte pointers, then
//*		those pointers would need to be saved in a table or structure,
//*		and a HANDLE (or index) would be placed in R0, R1, and R2
//*		in place of the actual memory pointer.  Then, the actual
//*		memory pointer would have to be fetched anytime it was needed
//*		by the execution of the .KE kodes.  Places where this is
//*		a concern, I've tried to mark with "WARN: PTR4".
//*		The same applies to MS Windows HANDLES (such as HDCs, etc).
//* 4) Wherever possible, I've tried to use as simple, lowest-level
//*		C-library functions as possible in order to keep the code
//*		as portable as possible, so that folks doing ports don't
//*		have to modify any more code than necessary.
//* 5) Kode (at this point) assumes that the screen size in
//*		pixels ranges from 640x480 to 1600x1280.  Larger (or
//*		smaller sizes may be supported, but that gives you a
//*		range that you may have to "scale to" if your actual
//*		display size is greatly different (like 10-20 years
//*		from now :-).
//* 5a) Support for Pocket PC screen size of 240x320 has been added
//*
//************************************************************

//************************************************************
// EXTERNALS
extern TCHAR	PgmPath[PATHLEN], TmpPath[PATHLEN], AppDataPath[PATHLEN];
extern int		PhysicalScreenW, PhysicalScreenH, PhysicalScreenSizeIndex;
extern WORD		MusicPlaying;
extern int		CurCur;
extern KE		mainKE;
extern char		OSctrl_cmdSTR[];

//************************************************************
// STRUCTURES
typedef struct {
	DWORD	BkColor;
	DWORD	TextColor;
	WORD	Font;
} OLDMODES;

#if KINT_MS_WINDOWS

#else
	typedef struct tagBITMAPFILEHEADER { // bmfh 
			WORD    bfType; 
			DWORD   bfSize; 
			WORD    bfReserved1; 
			WORD    bfReserved2; 
			DWORD   bfOffBits; 
	} BITMAPFILEHEADER; 

	typedef struct tagBITMAPINFOHEADER{ // bmih 
	   DWORD  biSize; 
	   long   biWidth; 
	   long   biHeight; 
	   WORD   biPlanes; 
	   WORD   biBitCount;
	   DWORD  biCompression; 
	   DWORD  biSizeImage; 
	   long   biXPelsPerMeter; 
	   long   biYPelsPerMeter; 
	   DWORD  biClrUsed; 
	   DWORD  biClrImportant; 
	} BITMAPINFOHEADER; 
 
	typedef struct tagRGBQUAD { // rgbq 
		BYTE    rgbBlue; 
		BYTE    rgbGreen; 
		BYTE    rgbRed; 
		BYTE    rgbReserved; 
	} RGBQUAD; 

	typedef struct tagBITMAPINFO { // bmi 
	   BITMAPINFOHEADER bmiHeader; 
	   RGBQUAD          bmiColors[1]; 
	} BITMAPINFO; 
#endif

//*******************************************************************************
// Global general variables, common to all .KE programs running at any given time
BYTE	Kregs[256]; // 4*32 + 2*48 + 1*32 = 256 (32 DWORDS, 48 WORDS, 32 BYTES)
DWORD	result;	// MSbit, EQ, LSbit (ie, CPU flags for the KCPU, see NOTE above "ExecKE()")
BYTE	signedmath;
BYTE	killpgm=0;

//********************************************************************************
// Global graphics variables, common to all .KE programs running at any given time
PBMB		kBuf, pbFirst=NULL;

#define		MAXFONTS	100
BYTE		*pFontData;	// ptr to malloc'd RAM for fonts
WORD		NumFonts;	// number of fonts in pFontData
UNALIGNED DWORD		*pFontTable;	// ptr to table of offsets to fonts in pFontData
BYTE		FontSelection[MAXFONTS];	// used in prioritizing fonts in SysGetFont

DWORD		LineStyle=0xFFFFFFFF;

DWORD		CurTextColor, CurBackColor;
WORD		CurFont;
WORD		AlphaBlendD=0, AlphaBlendS=255;	// 0-255: 0 keeps NONE, 255 keeps ALL, in between keeps some (set by SysAlphaBlend)

OLDMODES	Moldie[MAXFONTS];
int			MoldCnt=0;

BYTE		TmpFname[PATHLEN];

int			TdBMW, TdBMH, TsBMW, TsBMH;
int			TdclipX1, TdclipX2, TdclipY1, TdclipY2;
int			TsclipX1, TsclipX2, TsclipY1, TsclipY2;
int			TdstepX, TdstepY, TsstepX, TsstepY, TastepX, TastepY;
BYTE		*Tdpbits, *Tspbits, *Tapbits;

#if KINT_MAKE_COPY
char		*pNxtSrc=NULL;
char		*pMSCmem=NULL;
DWORD		swllen;
#endif

//************************************************************
//* Read in .KE, allocate memory for it, initialize ptrs
//*		Return TRUE if successful, FALSE if not.
//*
int	LoadKEFile(char *pnameKE, KE *pKE) {

	DWORD	fh, readcnt;
	BYTE	*pB;
	DWORD	remains, zerosize;
#if BETA
  #if KINT_POCKET_PC
	WIN32_FIND_DATA	fd;
	HANDLE	hFind;
	SYSTEMTIME	sysTime;
  #else
	unsigned char	ffmore;
	char		far *p;
	FILETIME	ft, localTime;
	SYSTEMTIME	sysTime;
	int			bd, bm, by;
	WIN32_FIND_DATA	pTemp;
	WIN32_FIND_DATA	m_pFoundInfo;
	WIN32_FIND_DATA	m_pNextInfo;
	HANDLE	m_hContext;
	char	m_strRoot[256];

	memset(&m_pFoundInfo, 0, sizeof(m_pFoundInfo));
  #endif
#endif

// Open .KE file
	if( -1==(fh=Kopen(pnameKE, O_RDBIN, DIR_GAME)) ) {
		SystemMessage(".KE file not found");
		return FALSE;
	}
// Read .KE header
	if( 20!=(readcnt=Kread(fh, (BYTE *)TmpFname, 20)) ) {
		SystemMessage("Error reading .KE file");
		Kclose(fh);
		return FALSE;
	}
	if( strcmp((char *)TmpFname, "KASMB") ) {
		SystemMessage("Not a .KE file");
		Kclose(fh);
		return FALSE;
	}
#if BETA
	if( !((BYTE)(TmpFname[KHDR_MAJOR_INTERP_REV]) & 0x80) ||  (BYTE)(TmpFname[KHDR_MAJOR_INTERP_REV])>INTERP_MAJOR_REV || ((BYTE)(TmpFname[KHDR_MAJOR_INTERP_REV])==INTERP_MAJOR_REV && (BYTE)(TmpFname[KHDR_MINOR_INTERP_REV])>INTERP_MINOR_REV) ) {
//char xxx[128];
//sprintf(xxx, "KE:%u.%u EXE:%ud.%ud", TmpFname[KHDR_MAJOR_INTERP_REV], TmpFname[KHDR_MINOR_INTERP_REV], INTERP_MAJOR_REV, INTERP_MINOR_REV);
//SystemMessage(xxx);
		SystemMessage("This .KE file requires a newer KINT.EXE");
		Kclose(fh);
		return FALSE;
	}
#else
	if( ((BYTE)TmpFname[KHDR_MAJOR_INTERP_REV] & 0x80)
	 || ((BYTE)TmpFname[KHDR_MAJOR_INTERP_REV] > INTERP_MAJOR_REV)
	 || ((BYTE)TmpFname[KHDR_MAJOR_INTERP_REV]==INTERP_MAJOR_REV
		 && (BYTE)TmpFname[KHDR_MINOR_INTERP_REV]>INTERP_MINOR_REV) ) {
		SystemMessage("This .KE file requires a newer KINT.EXE");
		Kclose(fh);
		return FALSE;
	}
#endif

// Get size of initialized code/data in file
	remains = GETMEMD(TmpFname+KHDR_CODE_SIZE);
	zerosize = GETMEMD(TmpFname+KHDR_ZERODATA_SIZE);
// Save pointer to base of stack
	pKE->StackBase = pKE->reg4 = pKE->reg5 = remains + zerosize;
// Get size of memory required (init code/data size + uninit data size + stack size)
	pKE->memlen = remains + zerosize + GETMEMD(TmpFname+KHDR_STACK_SIZE);
// Allocate .KE memory
	if( NULL==(pKE->pKode=(BYTE *)malloc(pKE->memlen)) ) {
		SystemMessage("Error allocating .KE memory");
		Kclose(fh);
		return FALSE;
	}
// Copy first 20 bytes that we already read, and setup ptr for rest of reading
	memcpy(pKE->pKode, TmpFname, 20);
	pB = pKE->pKode + 20;
	remains -= 20;
// Read rest of .KE file into memory
	readcnt = Kread(fh, pB, remains);
	Kclose(fh);
	if( readcnt!=remains ) return FALSE;
// Initialize uninitialized area to 0.
// Make sure that your memset takes at least a 4-byte value for "length".
// Otherwise, you'll need to modify this next line to properly zero-out
// all of the ZERODATA area, which may be longer than 64K bytes.
	memset(pKE->pKode+GETMEMD(TmpFname+KHDR_CODE_SIZE), 0, zerosize);

#if BETA
	Expired =  (*((DWORD *)(pKE->pKode+5*sizeof(DWORD)))!=BETAKEY);

  #if KINT_POCKET_PC
	hFind = FindFirstFile(TEXT("\\TEMP\\*.*"), &fd);
	if( hFind!=INVALID_HANDLE_VALUE ) {
		do {
			FileTimeToSystemTime(&fd.ftLastAccessTime, &sysTime);
			if( sysTime.wYear>BETAYEAR
			 || (sysTime.wYear==BETAYEAR && sysTime.wMonth>BETAMONTH)
			 || (sysTime.wYear==BETAYEAR && sysTime.wMonth==BETAMONTH && sysTime.wDay>BETADAY) ) {
				Expired = 1;
			}
		} while( FindNextFile(hFind, &fd));
		FindClose(hFind);
	}
	hFind = FindFirstFile(TEXT("\\*.*"), &fd);
	if( hFind!=INVALID_HANDLE_VALUE ) {
		do {
			FileTimeToSystemTime(&fd.ftLastAccessTime, &sysTime);
			if( sysTime.wYear>BETAYEAR
			 || (sysTime.wYear==BETAYEAR && sysTime.wMonth>BETAMONTH)
			 || (sysTime.wYear==BETAYEAR && sysTime.wMonth==BETAMONTH && sysTime.wDay>BETADAY) ) {
				Expired = 1;
			}
		} while( FindNextFile(hFind, &fd));
		FindClose(hFind);
	}
  #else
	bd = BETADAY;
	bm = BETAMONTH;
	by = BETAYEAR;

	GetTempPath(512, TmpPath);
//SystemMessage(TmpPath);
	p = TmpPath+strlen(TmpPath)-1;
	if( *p!='\\' ) *p = '\\';
	*++p = '\0';
	strcat(TmpPath, "*.*");

	strcpy(m_pNextInfo.cFileName, TmpPath);
	m_hContext = FindFirstFile(TmpPath, &m_pNextInfo);

	if( m_hContext == INVALID_HANDLE_VALUE ) {
		DWORD dwTemp = GetLastError();
		ffmore = 0;
	} else {
		if( _fullpath(m_strRoot, TmpPath, 256) == NULL) {
			FindClose(m_hContext);
			ffmore = 0;
		} else {
			// find the last forward or backward slash
			char * pstrBack  = strrchr(m_strRoot, '\\');
			char * pstrFront = strrchr(m_strRoot, '/');

			if( pstrFront != NULL || pstrBack != NULL ) {
				if( pstrFront == NULL ) pstrFront = m_strRoot;
				if( pstrBack == NULL ) pstrBack = m_strRoot;
				// from the start to the last slash is the root
				if( pstrFront >= pstrBack ) *pstrFront = '\0';
				else *pstrBack = '\0';
			}
			ffmore = 1;
		}
	}
	while( ffmore ) {
		memcpy(&pTemp, &m_pFoundInfo, sizeof(WIN32_FIND_DATA));
		memcpy(&m_pFoundInfo, &m_pNextInfo, sizeof(WIN32_FIND_DATA));
		memcpy(&m_pNextInfo, &pTemp, sizeof(WIN32_FIND_DATA));

		ffmore = FindNextFile(m_hContext, &m_pNextInfo)?1:0;

		ft = m_pFoundInfo.ftLastWriteTime;

			// first convert file time (UTC time) to local time
		FileTimeToLocalFileTime(&ft, &localTime);
			// then convert that time to system time
		FileTimeToSystemTime(&localTime, &sysTime);

		if( sysTime.wYear >= by && sysTime.wMonth >= bm && sysTime.wDay >= bd ) {
				Expired = 1;
		}
	}
  #endif
#endif
#if KINT_DEBUG
	pKE->DB_BP[0] = 0;
	pKE->DB_BPcnt = 1;
	if( DbgMem!=NULL && KDBG_HERE ) {
		strcpy((char *)KINT_LOADKE_NAME, pnameKE);
		strcpy((char *)KINT_LOADKE_PATH, PgmPath);
		Ksig(KSIG_LOADKE);
		while( KDBG_ACK != KSIG_NO_ACK ) ;
		pKE->KEnum = KDBG_LOADKE_KENUM;
	}
#endif
	return TRUE;
}

//***************************************************************
//* This routine is called to send a "message" to a .KE module
//* entry point, with:
//* R32 = cmd (offset into pKode...see KHDR_ #defines in pmach.h)
//*	R15 = arg1
//*	R14 = arg2
//*	R13 = arg3
//*
WORD KEmsg(KE *pKE, WORD cmd, DWORD arg1, DWORD arg2, DWORD arg3) {

	WORD	retval;

	SETREGD(REG_0, pKE->pKode);	// init code block base register
#if BETA
	if( !Expired || cmd==KHDR_BEGIN_STARTUP || cmd==KHDR_END_STARTUP ) SETREGD(REG_3, GETMEMD(pKE->pKode+cmd));	// init PC
#else
	SETREGD(REG_3, GETMEMD(pKE->pKode+cmd));	// init PC
#endif
	SETREGD(REG_4, pKE->reg4);	// init stack base frame ptr
	SETREGD(REG_5, pKE->reg5);	// init stack top ptr

	SETREGW(REG_32, cmd);		// set cmd
	SETREGD(REG_15, arg1);		//    and args into CPU regs for Kode
	SETREGD(REG_14, arg2);
	SETREGD(REG_13, arg3);

// call K interpreter
	retval = ExecKE(pKE);

// save KE specific CPU registers
	pKE->reg4 = GETREGD(REG_4);
	pKE->reg5 = GETREGD(REG_5);

// if Kode did a TERM, kill the program, but ONLY do it ONCE!
// (The Kode may call TERM more than once due to "shut down" Kode calls)
	if( killpgm==1 ) {
		killpgm |= 2;
		KillProgram();
	}
	return retval;
}

#if KINT_DEBUG
//************************************************
void UpdateBrkpts(KE *pKE) {

	int		i;

	if( DbgMem!=NULL ) {
		KDBG_BRKPT_CNT = (WORD)pKE->DB_BPcnt;
		for(i=0; i<DB_MAX_BP; i++) {
			if( i<pKE->DB_BPcnt ) {
				*((DWORD *)(KDBG_BRKPT+4*i)) = pKE->DB_BP[i];
			} else {
				*((DWORD *)(KDBG_BRKPT+4*i)) = 0;
			}
		}
	}
}
#endif

//************************************************
//* The K interpreter
//*		Event Kode must put return value into R39 before HALTing
//*
//* NOTE: "result" contains the KCPU "flags" for equality,
//*		least-significant bit, and most-significant bit.
//*		The "result" flags are set by these instructions:
//*			SHL SHR ROL ROR SUB B2I "RET 0" and "RET 1"
//*			AND XOR CMP INC DEC NOT NEG TST FCMP
//*		When the operation is a WORD or BYTE, the result
//*		is first cast to a SHORT or a CHAR before being
//*		cast to a DWORD in order to make sure that the
//*		most-significant bit gets propagated correctly.
//*
//*		The instructions that are affected by the 'signedmath' flag are:
//*			SHR, DIV, MOD, SUB, CMP
//*		The f--- floating point opcodes are ALWAYS signed operations.

//DWORD opcnts[256];
//DWORD minstack=64*1024;
//DWORD entryr7, entryr3;

WORD ExecKE(KE *pKE) {

	WORD	*PC, opcode;
	BYTE	*pMem, *pKode;
	DWORD	dw, dw2;
	WORD	w, w2, dr;
	BYTE	b;
	float	f1;

//entryr7 = GETREGD(REG_7);
//entryr3 = GETREGD(REG_3);

	pKode = pKE->pKode;
	PC = (WORD *)(pKode + GETREGD(REG_3));	// get local ptr to PC code
#if KINT_DEBUG
	UpdateBrkpts(pKE);
#endif

#if KINT_HELP
	for( ; !F6BREAK; ) {
#else
	for( ;; ) {
#endif
#if KINT_DEBUG
		if( DbgMem!=NULL ) {
			if( KDBG_HERE ) {
				BYTE	run=0, regsdone=0;
				int		i;
				WORD	dbgop;

				KINT_KENUM = pKE->KEnum;
				while( !run ) {
					if( KDBG_TOGGLE_BRKPT ) {
						if( KDBG_TOGGLE_BRKPT==-1 ) {
							dbgop = *((WORD *)(pKE->pKode + GETREGD(REG_3)));
							if( dbgop==0xFE69 || dbgop==0xFE6A ) {
								pKE->DB_BP[0] = GETREGD(REG_3)+8;
							} else pKE->DB_BP[0] = GETREGD(REG_3)+6;
						} else {
							for(i=1; i<pKE->DB_BPcnt; i++) {
								if( pKE->DB_BP[i]==KDBG_TOGGLE_BRKPT ) {
									for(; i<pKE->DB_BPcnt; i++) pKE->DB_BP[i] = pKE->DB_BP[i+1];
									i = -1;
									break;
								}
							}
							if( i>=pKE->DB_BPcnt && pKE->DB_BPcnt<DB_MAX_BP ) {
								pKE->DB_BP[pKE->DB_BPcnt++] = KDBG_TOGGLE_BRKPT;
							}
						}
						UpdateBrkpts(pKE);
						KDBG_TOGGLE_BRKPT = 0;
					}

					if( KDBG_STATE==KDBG_RUN ) {
						for(i=0; i<pKE->DB_BPcnt; i++) {
							if( pKE->DB_BP[i]==GETREGD(REG_3) ) {
								KDBG_STATE = KDBG_BRK;
								if( i==0 ) pKE->DB_BP[0] = 0;
								break;
							}
						}
					}
					run = (KDBG_STATE==KDBG_BRK) ? 0 : 1;
					if( KDBG_STATE==KDBG_STEP ) {
						KDBG_STATE = KDBG_BRK;
					}

					if( !run ) {
						MSG		msg;

						if( (!regsdone || KDBG_GETMEM) && KDBG_GETMEM_BLOCK<3 && GETREGD(4*KDBG_GETMEM_BLOCK) ) {	// avoid mem fault if == 0
							memcpy(KINT_MEMORY, (BYTE *)GETREGD(4*KDBG_GETMEM_BLOCK) + KDBG_MEM_ADDR, 128);
							KDBG_GETMEM = 0;
							Ksig(KSIG_UPDATE);
							while( KDBG_ACK!=KSIG_NO_ACK ) ;
						}
						if( !regsdone ) {
							SETREGD(REG_3, (DWORD)((BYTE *)PC - pKode));
							memcpy(KINT_REGS, Kregs, 256);
							KINT_PC_MEM = *((WORD *)(pKE->pKode + GETREGD(REG_3)));
							KINT_FLAGS = result;
							KINT_SIGNED_MATH = (DWORD)signedmath;
							regsdone = 1;
							Ksig(KSIG_UPDATE);
							while( KDBG_ACK!=KSIG_NO_ACK ) ;
							SetForegroundWindow((HWND)KDBG_HWND);
						}
						KdbgHalted = 1;
						if( PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE) ) {
							if( GetMessage(&msg, NULL, 0, 0)==TRUE ) {
								TranslateMessage(&msg);
								DispatchMessage(&msg);
							}
						}
						KdbgHalted = 0;
					}
				}
				if( regsdone ) {
					memcpy(Kregs, KINT_REGS, 256);
					PC = (WORD *)(pKode + GETREGD(REG_3));
					result = KINT_FLAGS;
					signedmath = (BYTE)KINT_SIGNED_MATH;
					if( hWndMain!=NULL ) SetForegroundWindow(hWndMain);
				}
			}
		}
#endif

#ifdef GEEK_TRACE3
	TraceBuf3[TBptr3++] = (DWORD)((BYTE *)PC-pKode);
	if( TBptr3>=TBMAX3 ) TBptr3 = 0;
	if( *((DWORD *)(pKode+0x70)) != 0x0276 ) {
pKode = pKode;
	}
#endif
		opcode = GETMEMW(PC);

#ifdef CHECKWORDALIGN
	savePC = (BYTE *)PC-pKode;
#endif

		PC++;
		switch( (opcode>>8) & 0x00FF ) {
		 case 0x00:	// unused
		 case 0x01:	// unused
		 case 0x02:	// unused
			break;
		 case 0x03:	// SHL b,b
			dr = opcode & 0x00FF;
			result = (DWORD)(SETREGB(dr, GETREGB(dr)<<(BYTE)(GETREGB(GETMEMW(PC)) & 0x0007)));
			PC++;
			result |= (result<<24);
			break;
		 case 0x04:	// SHL w,b
			dr = opcode & 0x00FF;
			result = (DWORD)(SETREGW(dr, GETREGW(dr)<<(BYTE)(GETREGB(GETMEMW(PC)) & 0x000F)));
			PC++;
			result |= (result<<16);
			break;
		 case 0x05:	// SHL d,b
			dr = opcode & 0x00FF;
			result = SETREGD(dr, GETREGD(dr)<<(BYTE)(GETREGB(GETMEMW(PC)) & 0x001F));
			PC++;
			break;
		 case 0x06:	// SHR b,b
			dr = opcode & 0x00FF;
			if( signedmath ) result = (DWORD)(char)(SETREGB(dr, (char)(GETREGB(dr))>>(BYTE)(GETREGB(GETMEMW(PC)) & 0x07)));
			else result = (DWORD)(char)(SETREGB(dr, GETREGB(dr)>>(BYTE)(GETREGB(GETMEMW(PC)) & 0x07)));
			PC++;
			result |= (result<<24);
			break;
		 case 0x07:	// SHR w,b
			dr = opcode & 0x00FF;
			if( signedmath ) result = (DWORD)(short)(SETREGW(dr, (short)(GETREGW(dr))>>(BYTE)(GETREGB(GETMEMW(PC)) & 0x000F)));
			else result = (DWORD)(short)(SETREGW(dr, GETREGW(dr)>>(BYTE)(GETREGB(GETMEMW(PC)) & 0x000F)));
			PC++;
			result |= (result<<16);
			break;
		 case 0x08:	// SHR d,b
			dr = opcode & 0x00FF;
			if( signedmath ) result = SETREGD(dr, (long)(GETREGD(dr))>>(BYTE)(GETREGB(GETMEMW(PC)) & 0x001F));
			else result = SETREGD(dr, GETREGD(dr)>>(BYTE)(GETREGB(GETMEMW(PC)) & 0x001F));
			PC++;
			break;
		 case 0x09: // ROL b,b
			dr = opcode & 0x00FF;
			w = (WORD)(GETREGB(GETMEMW(PC)) & 0x07);
			PC++;
			b = GETREGB(dr);
			while( w-- ) {
				if( b & 0x80 ) b = (b << 1) | 1;
				else b <<= 1;
			}
			result = (DWORD)(char)SETREGB(dr, b);
			result |= (result<<24);
			break;
		 case 0x0A:	// ROL w,b
			dr = opcode & 0x00FF;
			b = GETREGB(GETMEMW(PC)) & 0x1F;
			PC++;
			w = GETREGW(dr);
			while( b-- ) {
				if( w & 0x8000 ) w = (w<<1) | 1;
				else w <<= 1;
			}
			result = (DWORD)(short)SETREGW(dr, w);
			result |= (result<<24);
			break;
		 case 0x0B:	// ROL d,b
			dr = opcode & 0x00FF;
			dw = GETREGD(dr);
			w = GETREGB(GETMEMW(PC)) & 0x1F;
			PC++;
			while( w-- ) {
				dw2 = dw;
				dw <<= 1;
				if( dw2 & 0x80000000 ) dw |= 1;
			}
			result = SETREGD(dr, dw);
			break;
		 case 0x0C:	// ROR b,b
			dr = opcode & 0x00FF;
			w = (WORD)(GETREGB(GETMEMW(PC)) & 0x07);
			PC++;
			b = GETREGB(dr);
			while( w-- ) {
				if( b & 1 ) b = (b >> 1) | 0x80;
				else b >>= 1;
			}
			result = (DWORD)(char)SETREGB(dr, b);
			result |= (result<<24);
			break;
		 case 0x0D:	// ROR w,b
			dr = opcode & 0x00FF;
			b = GETREGB(GETMEMW(PC)) & 0x001F;
			PC++;
			w = GETREGW(dr);
			while( b-- ) {
				if( w & 1 ) w = (w >> 1) | 0x8000;
				else w >>= 1;
			}
			result = (DWORD)(short)SETREGW(dr, w);
			result |= (result<<16);
			break;
		 case 0x0E:	// ROR d,b
			dr = opcode & 0x00FF;
			dw = GETREGD(dr);
			w = GETREGB(GETMEMW(PC)) & 0x1F;
			PC++;
			while( w-- ) {
				dw2 = dw;
				dw >>= 1;
				if( dw2 & 1 ) dw |= 0x80000000;
			}
			result = SETREGD(dr, dw);
			break;
		 case 0x0F:	// ADD b,b
			dr = opcode & 0x00FF;
			SETREGB(dr, GETREGB(dr)+GETREGB(GETMEMW(PC)));
			PC++;
			break;
		 case 0x10:	// ADD w,w
			dr = opcode & 0x00FF;
			SETREGW(dr, GETREGW(dr)+GETREGW(GETMEMW(PC)));
			PC++;
			break;
		 case 0x11:	// ADD d,d
			dr = opcode & 0x00FF;
			SETREGD(dr, GETREGD(dr)+GETREGD(GETMEMW(PC)));
			PC++;
			break;
		 case 0x12:	// SUB b,b
			dr = opcode & 0x00FF;
			if( signedmath ) result = (DWORD)((long)(((long)((char)GETREGB(dr)))-((long)((char)GETREGB(GETMEMW(PC))))));
			else result = (DWORD)GETREGB(dr)-(DWORD)GETREGB(GETMEMW(PC));
			PC++;
			SETREGB(dr, (BYTE)result);
			break;
		 case 0x13:	// SUB w,w
			dr = opcode & 0x00FF;
			if( signedmath ) result = (DWORD)((long)(((long)((short)GETREGW(dr)))-((long)((short)GETREGW(GETMEMW(PC))))));
			else result = (DWORD)GETREGW(dr)-(DWORD)GETREGW(GETMEMW(PC));
			PC++;
			SETREGW(dr, (WORD)result);
			break;
		 case 0x14:	// SUB d,d
			dr = opcode & 0x00FF;
			if( signedmath ) result = SETREGD(dr, (long)(GETREGD(dr)) - (long)GETREGD(GETMEMW(PC)));
			else result = SETREGD(dr, GETREGD(dr) - GETREGD(GETMEMW(PC)));
			PC++;
			break;
		 case 0x15:	// MUL b,b
			dr = opcode & 0x00FF;
			SETREGB(dr, GETREGB(dr)*GETREGB(GETMEMW(PC)));
			PC++;
			break;
		 case 0x16:	// MUL w,w
			dr = opcode & 0x00FF;
			SETREGW(dr, GETREGW(dr)*GETREGW(GETMEMW(PC)));
			PC++;
			break;
		 case 0x17:	// MUL d,d
			dr = opcode & 0x00FF;
			SETREGD(dr, GETREGD(dr)*GETREGD(GETMEMW(PC)));
			PC++;
			break;
		 case 0x18:	// DIV b,b
			dr = opcode & 0x00FF;
			if( signedmath ) SETREGB(dr, (char)(GETREGB(dr))/(char)GETREGB(GETMEMW(PC)));
			else SETREGB(dr, GETREGB(dr)/GETREGB(GETMEMW(PC)));
			PC++;
			break;
		 case 0x19:	// DIV w,w
			dr = opcode & 0x00FF;
			if( signedmath ) SETREGW(dr, (short)(GETREGW(dr))/(short)GETREGW(GETMEMW(PC)));
			else SETREGW(dr, GETREGW(dr)/GETREGW(GETMEMW(PC)));
			PC++;
			break;
		 case 0x1A:	// DIV d,d
			dr = opcode & 0x00FF;
			if( signedmath ) SETREGD(dr, (long)(GETREGD(dr))/(long)(GETREGD(GETMEMW(PC))));
			else SETREGD(dr, GETREGD(dr)/GETREGD(GETMEMW(PC)));
			PC++;
			break;
		 case 0x1B:	// MOD b,b
			dr = opcode & 0x00FF;
			if( signedmath ) SETREGB(dr, (char)(GETREGB(dr))%(char)GETREGB(GETMEMW(PC)));
			else SETREGB(dr, GETREGB(dr)%GETREGB(GETMEMW(PC)));
			PC++;
			break;
		 case 0x1C:	// MOD w,w
			dr = opcode & 0x00FF;
			if( signedmath ) SETREGW(dr, (short)(GETREGW(dr))%(short)GETREGW(GETMEMW(PC)));
			else SETREGW(dr, GETREGW(dr)%GETREGW(GETMEMW(PC)));
			PC++;
			break;
		 case 0x1D:	// MOD d,d
			dr = opcode & 0x00FF;
			if( signedmath ) SETREGD(dr, (long)(GETREGD(dr))%(long)(GETREGD(GETMEMW(PC))));
			else SETREGD(dr, GETREGD(dr)%GETREGD(GETMEMW(PC)));
			PC++;
			break;
		 case 0x1E:	// OR b,b
			dr = opcode & 0x00FF;
			SETREGB(dr, GETREGB(dr) | GETREGB(GETMEMW(PC)));
			PC++;
			break;
		 case 0x1F:	// OR w,w
			dr = opcode & 0x00FF;
			SETREGW(dr, GETREGW(dr) | GETREGW(GETMEMW(PC)));
			PC++;
			break;
		 case 0x20:	// OR d,d
			dr = opcode & 0x00FF;
			SETREGD(dr, GETREGD(dr) | GETREGD(GETMEMW(PC)));
			PC++;
			break;
		 case 0x21:	// AND b,b
			dr = opcode & 0x00FF;
			result = (DWORD)((long)(char)GETREGB(dr) & (long)(char)GETREGB(GETMEMW(PC)));
			PC++;
			SETREGB(dr, (BYTE)result);
			break;
		 case 0x22:	// AND w,w
			dr = opcode & 0x00FF;
			result = (DWORD)((long)(short)GETREGW(dr) & (long)(short)GETREGW(GETMEMW(PC)));
			PC++;
			SETREGW(dr, (WORD)result);
			break;
		 case 0x23:	// AND d,d
			dr = opcode & 0x00FF;
			result = SETREGD(dr, GETREGD(dr) & GETREGD(GETMEMW(PC)));
			PC++;
			break;
		 case 0x24:	// XOR b,b
			dr = opcode & 0x00FF;
			result = (DWORD)(char)SETREGB(dr, GETREGB(dr) ^ GETREGB(GETMEMW(PC)));
			PC++;
			result |= (result<<24);
			break;
		 case 0x25:	// XOR w,w
			dr = opcode & 0x00FF;
			result = (DWORD)(short)SETREGW(dr, GETREGW(dr) ^ GETREGW(GETMEMW(PC)));
			PC++;
			result |= (result<<16);
			break;
		 case 0x26:	// XOR d,d
			dr = opcode & 0x00FF;
			result = SETREGD(dr, GETREGD(dr) ^ GETREGD(GETMEMW(PC)));
			PC++;
			break;
		 case 0x27:	// CMP b,b
			if( signedmath ) result = (DWORD)((long)(((long)(char)GETREGB(opcode & 0x00FF))-((long)(char)(GETREGB(GETMEMW(PC))))));
			else result = (DWORD)((DWORD)((long)((short)((char)((BYTE)GETREGB(opcode & 0x00FF)))))-(DWORD)((long)((short)((char)((BYTE)GETREGB(GETMEMW(PC)))))));
			PC++;
			break;
		 case 0x28:	// CMP w,w
			if( signedmath ) result = (DWORD)((long)(((long)(short)GETREGW(opcode & 0x00FF))-((long)(short)(GETREGW(GETMEMW(PC))))));
			else result = (DWORD)GETREGW(opcode & 0x00FF)-(DWORD)GETREGW(GETMEMW(PC));
			PC++;
			break;
		 case 0x29:	// CMP d,d
			if( signedmath ) result = (long)(GETREGD(opcode & 0x00FF)) - (long)(GETREGD(GETMEMW(PC)));
			else result = (DWORD)GETREGD(opcode & 0x00FF) - (DWORD)GETREGD(GETMEMW(PC));
			PC++;
			break;
		 case 0x2A:	// TST b,b
			result = (DWORD)(char)(GETREGB(opcode & 0x00FF) & GETREGB(GETMEMW(PC)));
			PC++;
			result |= (result<<24);
			break;
		 case 0x2B:	// TST w,w
			result = (DWORD)(short)(GETREGW(opcode & 0x00FF) & GETREGW(GETMEMW(PC)));
			PC++;
			result |= (result<<16);
			break;
		 case 0x2C:	// TST d,d
			result = GETREGD(opcode & 0x00FF) & GETREGD(GETMEMW(PC));
			PC++;
			break;
		 case 0x2D:	// XCH b,b
			dr = opcode & 0x00FF;
			b = GETREGB(dr);
			w = GETMEMW(PC);
			PC++;
			SETREGB(dr, GETREGB(w));
			SETREGB(w, b);
			break;
		 case 0x2E:	// XCH w,w
			dr = opcode & 0x00FF;
			w2 = GETREGW(dr);
			w = GETMEMW(PC);
			PC++;
			SETREGW(dr, GETREGW(w));
			SETREGW(w, w2);
			break;
		 case 0x2F:	// XCH d,d
			dr = opcode & 0x00FF;
			dw = GETREGD(dr);
			w = GETMEMW(PC);
			PC++;
			SETREGD(dr, GETREGD(w));
			SETREGD(w, dw);
			break;
		 case 0x30:	// LDR b,b
			SETREGB(opcode & 0x00FF, GETREGB(GETMEMW(PC)));
			PC++;
			break;
		 case 0x31:	// LDR w,w
			SETREGW(opcode & 0x00FF, GETREGW(GETMEMW(PC)));
			PC++;
			break;
		 case 0x32:	// LDR d,d
			SETREGD(opcode & 0x00FF, GETREGD(GETMEMW(PC)));
			PC++;
			break;
		 case 0x33:	// LDE d,b
			SETREGD(opcode & 0x00FF, (DWORD)(BYTE)GETREGB(GETMEMW(PC)));
			PC++;
			break;
		 case 0x34:	// LDE d,w
			SETREGD(opcode & 0x00FF, (DWORD)(WORD)GETREGW(GETMEMW(PC)));
			PC++;
			break;
		 case 0x35:	// LDE w,b
			SETREGW(opcode & 0x00FF, (WORD)GETREGB(GETMEMW(PC)));
			PC++;
			break;
		 case 0x36:	// ABIT w,b
			dr = opcode & 0x00FF;
			if( dr<=REG_96 ) {
				w2 = GETMEMW(PC);
				PC++;
				w  = GETREGW(dr);
				for(; w; w>>=1, ++w2) if( w & 1 ) SETREGB(w2, 1+GETREGB(w2));
			}
			break;
		 case 0x37:	// ABIT b,b
			w2 = GETMEMW(PC);
			PC++;
			b  = GETREGB(opcode & 0x00FF);
			for(; b; b>>=1, ++w2) if( b & 1 ) SETREGB(w2, 1+GETREGB(w2));
			break;
		 case 0x38:	// LD0 b,d
			SETREGB(opcode & 0x00FF, GETMEMB(pKode + GETREGD(GETMEMW(PC))));
			PC++;
			break;
		 case 0x39:	// LD0 w,d
			SETREGW(opcode & 0x00FF, GETMEMW((WORD *)(pKode + GETREGD(GETMEMW(PC)))));
			PC++;
			break;
		 case 0x3A:	// LD0 d,d
			SETREGD(opcode & 0x00FF, GETMEMD(pKode + GETREGD(GETMEMW(PC))));
			PC++;
			break;
		 case 0x3B:	// LD1 b,d
			SETREGB(opcode & 0x00FF, GETMEMB(GETREGD(REG_1) + GETREGD(GETMEMW(PC))));
			PC++;
			break;
		 case 0x3C:	// LD1 w,d
			SETREGW(opcode & 0x00FF, GETMEMW((WORD *)(GETREGD(REG_1) + GETREGD(GETMEMW(PC)))));
			PC++;
			break;
		 case 0x3D:	// LD1 d,d
			SETREGD(opcode & 0x00FF, GETMEMD(GETREGD(REG_1) + GETREGD(GETMEMW(PC))));
			PC++;
			break;
		 case 0x3E:	// LD2 b,d
			SETREGB(opcode & 0x00FF, GETMEMB(GETREGD(REG_2) + GETREGD(GETMEMW(PC))));
			PC++;
			break;
		 case 0x3F:	// LD2 w,d
			SETREGW(opcode & 0x00FF, GETMEMW((WORD *)(GETREGD(REG_2) + GETREGD(GETMEMW(PC)))));
			PC++;
			break;
		 case 0x40:	// LD2 d,d
			SETREGD(opcode & 0x00FF, GETMEMD(GETREGD(REG_2) + GETREGD(GETMEMW(PC))));
			PC++;
			break;
		 case 0x41:	// LDS b,d
			SETREGB(opcode & 0x00FF, GETMEMB(pKode + GETREGD(REG_4) + GETREGD(GETMEMW(PC))));
			PC++;
			break;
		 case 0x42:	// LDS w,d
			SETREGW(opcode & 0x00FF, GETMEMW((WORD *)(pKode + GETREGD(REG_4) + GETREGD(GETMEMW(PC)))));
			PC++;
			break;
		 case 0x43:	// LDS d,d
			SETREGD(opcode & 0x00FF, GETMEMD(pKode + GETREGD(REG_4) + GETREGD(GETMEMW(PC))));
			PC++;
			break;
		 case 0x44:	// ST0 b,d
			SETMEMB(pKode + GETREGD(GETMEMW(PC)), GETREGB(opcode & 0x00FF));
			PC++;
			break;
		 case 0x45:	// ST0 w,d
			SETMEMW((WORD *)(pKode + GETREGD(GETMEMW(PC))), GETREGW(opcode & 0x00FF));
			PC++;
			break;
		 case 0x46:	// ST0 d,d
			SETMEMD(pKode + GETREGD(GETMEMW(PC)), GETREGD(opcode & 0x00FF));
			PC++;
			break;
		 case 0x47:	// ST1 b,d
			SETMEMB(GETREGD(REG_1) + GETREGD(GETMEMW(PC)), GETREGB(opcode & 0x00FF));
			PC++;
			break;
		 case 0x48:	// ST1 w,d
			SETMEMW((WORD *)(GETREGD(REG_1) + GETREGD(GETMEMW(PC))), GETREGW(opcode & 0x00FF));
			PC++;
			break;
		 case 0x49:	// ST1 d,d
			SETMEMD(GETREGD(REG_1) + GETREGD(GETMEMW(PC)), GETREGD(opcode & 0x00FF));
			PC++;
			break;
		 case 0x4A:	// ST2 b,d
			SETMEMB(GETREGD(REG_2) + GETREGD(GETMEMW(PC)), GETREGB(opcode & 0x00FF));
			PC++;
			break;
		 case 0x4B:	// ST2 w,d
			SETMEMW((WORD *)(GETREGD(REG_2) + GETREGD(GETMEMW(PC))), GETREGW(opcode & 0x00FF));
			PC++;
			break;
		 case 0x4C:	// ST2 d,d
			SETMEMD(GETREGD(REG_2) + GETREGD(GETMEMW(PC)), GETREGD(opcode & 0x00FF));
			PC++;
			break;
		 case 0x4D:	// STS b,d
			SETMEMB(pKode + GETREGD(REG_4) + GETREGD(GETMEMW(PC)), GETREGB(opcode & 0x00FF));
			PC++;
			break;
		 case 0x4E:	// STS w,d
			SETMEMW((WORD *)(pKode + GETREGD(REG_4) + GETREGD(GETMEMW(PC))), GETREGW(opcode & 0x00FF));
			PC++;
			break;
		 case 0x4F:	// STS d,d
			SETMEMD(pKode + GETREGD(REG_4) + GETREGD(GETMEMW(PC)), GETREGD(opcode & 0x00FF));
			PC++;
			break;
		 case 0x50:	// PU0 b,d
			w = GETMEMW(PC);
			PC++;
			SETMEMB(pKode + GETREGD(w), GETREGB(opcode & 0x00FF));
			SETREGD(w, GETREGD(w)+1);
			break;
		 case 0x51:	// PU0 w,d
			w = GETMEMW(PC);
			PC++;
			SETMEMW((WORD *)(pKode + GETREGD(w)), GETREGW(opcode & 0x00FF));
			SETREGD(w, GETREGD(w)+2);
			break;
		 case 0x52:	// PU0 d,d
			w = GETMEMW(PC);
			PC++;
			SETMEMD(pKode + GETREGD(w), GETREGD(opcode & 0x00FF));
			SETREGD(w, GETREGD(w)+4);
			break;
		 case 0x53:	// PO0 b,d
			w = GETMEMW(PC);
			PC++;
			SETREGD(w, GETREGD(w)-1);
			SETREGB(opcode & 0x00FF, GETMEMB(pKode + GETREGD(w)));
			break;
		 case 0x54:	// PO0 w,d
			w = GETMEMW(PC);
			PC++;
			SETREGD(w, GETREGD(w)-2);
			SETREGW(opcode & 0x00FF, GETMEMW((WORD *)(pKode + GETREGD(w))));
			break;
		 case 0x55:	// PO0 d,d
			w = GETMEMW(PC);
			PC++;
			SETREGD(w, GETREGD(w)-4);
			SETREGD(opcode & 0x00FF, GETMEMD(pKode + GETREGD(w)));
			break;
		 case 0x56:	// memory ops
		 case 0x57:	// memory ops
/*
56/57op
0101011cciiiaaaa
cc = chunk size (0=byte, 1=word, 2=dword)
iii = instruction:
	000	MOV
	001 ADD
	010 SUB
	011 CMP
	100 TST
	101 AND
	110 OR
	111 XOR
aaaa = addressing mode
							WORD1	WORD2	WORD3	WORD4	WORD5	WORD6	WORD7	WORD8		regs	meof	imm
	0000	regl,[regh]			56/57op reg/reg													2		0		0
	0001	regl,[regh+off]		56/57op reg/reg	off----------									2		1		0
	0010	regl,[mem]			56/57op reg		mem----------									1		1		0
	0011	regl,[regh+mem+off]	56/57op reg/reg	mem----------	off----------					2		2		0
	0100	regl,[regh+reg2]	56/57op reg/reg	reg												3		0		0
	0101	[regl],regh			56/57op reg/reg													2		0		0
	0110	[regl],imm			56/57op reg		imm-- ???????									1		0		1
	0111	[regl+off],regh		56/57op reg/reg	off----------									2		0		1
	1000	[regl+off],imm		56/57op reg		off----------	imm-- ???????					1		1		1
	1001	[mem],regh			56/57op reg		mem----------									1		1		0
	1010	[mem],imm			56/57op mem----------	imm-- ???????							0		1		1
	1011	[regl+mem+off],regh	56/57op reg/reg	mem----------	off----------					2		2		0
	1100	[regl+mem+off],imm	56/57op reg		mem----------	off----------	imm-- ???????	1		2		1
	1101	[regl+regh],reg2	56/57op reg/reg	reg												3		0		0
	1110	[regl+regh],imm		56/57op reg/reg imm-- ???????									2		0		1
	1111	LEA/PEA
*/
			w = GETMEMW(PC);		// in all but two cases, there's a reg/reg word following the opcode, so get it
			PC++;					// we'll skip back in the two cases where reg/reg isn't there
			dr = (opcode & 0x0180)>>7;	// get chunk size (0=byte, 1=word, 2=dword)
			switch( opcode & 0x000F ) {
			 case 0:	//regl,[regh]			56/57op reg/reg
				if( !dr ) b = GETMEMB(pKode + GETREGD((w>>8) & 0x00FF));
				else if( dr==1 ) w2 = GETMEMW(pKode + GETREGD((w>>8) & 0x00FF));
				else dw2 = GETMEMD(pKode + GETREGD((w>>8) & 0x00FF));
				w &= 0x00FF;
				break;
			 case 1:	//regl,[regh+off]		56/57op reg/reg	off----------
				if( !dr ) b = GETMEMB(pKode + GETREGD((w>>8) & 0x00FF) + GETMEMD(PC));
				else if( dr==1 ) w2 = GETMEMW(pKode + GETREGD((w>>8) & 0x00FF) + GETMEMD(PC));
				else dw2 = GETMEMD(pKode + GETREGD((w>>8) & 0x00FF) + GETMEMD(PC));
				PC += 2;
				w &= 0x00FF;
				break;
			 case 2:	//regl,[mem]			56/57op reg		mem----------
				if( !dr ) b = GETMEMB(pKode + GETMEMD(PC));
				else if( dr==1 ) w2 = GETMEMW(pKode + GETMEMD(PC));
				else dw2 = GETMEMD(pKode + GETMEMD(PC));
				PC += 2;
				w &= 0x00FF;
				break;
			 case 3:	//regl,[mem+regh+off]	56/57op reg/reg	mem----------	off----------
				dw = GETMEMD(PC);
				PC += 2;
				if( !dr ) b = GETMEMB(pKode + dw + GETREGD((w>>8) & 0x00FF) + GETMEMD(PC));
				else if( dr==1 ) w2 = GETMEMW(pKode + dw + GETREGD((w>>8) & 0x00FF) + GETMEMD(PC));
				else dw2 = GETMEMD(pKode + dw + GETREGD((w>>8) & 0x00FF) + GETMEMD(PC));
				PC += 2;
				w &= 0x00FF;
				break;
			 case 4:	//regl,[regh+reg2]		56/57op reg/reg	reg
				if( !dr ) b = GETMEMB(pKode + GETREGD((w>>8) & 0x00FF) + GETREGD(GETMEMW(PC)));
				else if( dr==1 ) w2 = GETMEMW(pKode + GETREGD((w>>8) & 0x00FF) + GETREGD(GETMEMW(PC)));
				else dw2 = GETMEMD(pKode + GETREGD((w>>8) & 0x00FF) + GETREGD(GETMEMW(PC)));
				PC++;
				w &= 0x00FF;
				break;
			 case 5:	//[regl],regh			56/57op reg/reg
				if( !dr ) b = GETREGB((w>>8) & 0x00FF);
				else if( dr==1 ) w2 = GETREGW((w>>8) & 0x00FF);
				else dw2 = GETREGD((w>>8) & 0x00FF);
				pMem = pKode + GETREGD(w & 0x00FF);
				break;
			 case 6:	//[regl],imm			56/57op reg		imm-- ???????
				if( !dr ) b = (BYTE)GETMEMW(PC);
				else if( dr==1 ) w2 = GETMEMW(PC);
				else {
					dw2 = GETMEMD(PC);
					PC++;
				}
				PC++;
				pMem = pKode + GETREGD(w & 0x00FF);
				break;
			 case 7:	//[regl+off],regh		56/57op reg/reg	off----------
				if( !dr ) b = GETREGB((w>>8) & 0x00FF);
				else if( dr==1 ) w2 = GETREGW((w>>8) & 0x00FF);
				else dw2 = GETREGD((w>>8) & 0x00FF);
				pMem = pKode + GETREGD(w & 0x00FF) + GETMEMD(PC);
				PC += 2;
				break;
			 case 8:	//[regl+off],imm		56/57op reg		off----------	imm-- ???????
				pMem = pKode + GETREGD(w & 0x00FF) + GETMEMD(PC);
				PC += 2;
				if( !dr ) b = (BYTE)GETMEMW(PC);
				else if( dr==1 ) w2 = GETMEMW(PC);
				else {
					dw2 = GETMEMD(PC);
					PC++;
				}
				PC++;
				break;
			 case 9:	//[mem],regh			56/57op reg		mem----------
				if( !dr ) b = GETREGB(w & 0x00FF);
				else if( dr==1 ) w2 = GETREGW(w & 0x00FF);
				else dw2 = GETREGD(w & 0x00FF);
				pMem = pKode + GETMEMD(PC);
				PC += 2;
				break;
			 case 10:	//[mem],imm			56/57op mem----------	imm-- ???????
				--PC;
				pMem = pKode + GETMEMD(PC);
				PC += 2;
				if( !dr ) b = (BYTE)GETMEMW(PC);
				else if( dr==1 ) w2 = GETMEMW(PC);
				else {
					dw2 = GETMEMD(PC);
					PC++;
				}
				PC++;
				break;
			 case 11:	//[mem+regl+off],regh	56/57op reg/reg	mem----------	off----------
				if( !dr ) b = GETREGB((w>>8) & 0x00FF);
				else if( dr==1 ) w2 = GETREGW((w>>8) & 0x00FF);
				else dw2 = GETREGD((w>>8) & 0x00FF);
				pMem = pKode + GETREGD(w & 0x00FF) + GETMEMD(PC);
				PC += 2;
				pMem += GETMEMD(PC);
				PC += 2;
				break;
			 case 12:	//[mem+regl+off],imm	56/57op reg		mem----------	off----------	imm-- ???????
				dw = GETMEMD(PC);
				PC += 2;
				pMem = pKode + dw + GETMEMD(PC) + GETREGD(w & 0x00FF);
				PC += 2;
				if( !dr ) b = (BYTE)GETMEMW(PC);
				else if( dr==1 ) w2 = GETMEMW(PC);
				else {
					dw2 = GETMEMD(PC);
					PC++;
				}
				PC++;
				break;
			 case 13:	//[regl+regh],reg2		56/57op reg/reg	reg
				pMem = pKode + GETREGD((w>>8) & 0x00FF) + GETREGD(w & 0x00FF);
				if( !dr ) b = GETREGB(GETMEMW(PC));
				else if( dr==1 ) w2 = GETREGW(GETMEMW(PC));
				else dw2 = GETREGD(GETMEMW(PC));
				PC++;
				break;
			 case 14:	//[regl+regh],imm		56/57op reg/reg	imm-- ???????
				pMem = pKode + GETREGD((w>>8) & 0x00FF) + GETREGD(w & 0x00FF);
				if( !dr ) b = (BYTE)GETMEMW(PC);
				else if( dr==1 ) w2 = GETMEMW(PC);
				else {
					dw2 = GETMEMD(PC);
					PC++;
				}
				PC++;
				break;
			 case 15:	// LEA d,[aaa]
/*
	For LEA, the 0x56/0x57 opcodes are laid out like this:

	0101 011u uaaa 1111

	where 'uu' is:
		00	LEA
		01	PEA
		10	reserved for future use
		11	reserved for future use
	and 'aaa' is the addressing mode:
		000	[reg]
		001	[reg+off]
		010	[mem]
		011	[reg+mem+off]
		100	[reg+reg]
		101	unused
		110 unused
		111 unused
*/
				if( (opcode & 0x0180)==0 ) {	// LEA
					switch( opcode & 0x0070 ) {
					 case 0x0000:	// LEA d,[regt]			0x560F REGt:REGd
						SETREGD(w & 0x00FF, GETREGD((w>>8) & 0x00FF));
						break;
					 case 0x0010:	// LEA d,[regt+off]		0x561F REGt:REGd offDWORD
						SETREGD(w & 0x00FF, GETREGD((w>>8) & 0x00FF) + GETMEMD(PC));
						PC += 2;
						break;
					 case 0x0020:	// LEA d,[mem]			0x562F memDWORD
						SETREGD(w & 0x00FF, GETMEMD(PC));
						PC += 2;
						break;
					 case 0x0030:	// LEA d,[regt+mem+off]	0x563F REGt:REGd memDWORD offDWORD
						dw2 = GETMEMD(PC);
						PC += 2;
						SETREGD(w & 0x00FF, GETREGD((w>>8) & 0x00FF) + dw2 + GETMEMD(PC));
						PC += 2;
						break;
					 case 0x0040:	// LEA d,[regt1+regt2]	0x564F REGt1:REGd REGt2
						SETREGD(w & 0x00FF, GETREGD((w>>8) & 0x00FF) + GETREGD(GETMEMW(PC++)));
						break;
					 case 0x0050:	// unused
					 case 0x0060:	// unused
					 case 0x0070:	// unused
						break;
					}
					goto leabreak;
				} else if( (opcode & 0x0180)==0x0080 ) {	// PEA (push effective address onto C R7 stack)
					switch( opcode & 0x0070 ) {
					 case 0x0000:	// PEA [regt]			0x568F 0:REGt
						SETREGD(REG_7, GETREGD(REG_7)-4);
						SETMEMD(pKode + GETREGD(REG_7), GETREGD(w));
						break;
					 case 0x0010:	// PEA [regt+off]		0x569F 0:REGt offDWORD
						SETREGD(REG_7, GETREGD(REG_7)-4);
						SETMEMD(pKode + GETREGD(REG_7), GETREGD(w) + GETMEMD(PC));
						PC += 2;
						break;
					 case 0x0020:	// PEA [mem]			0x56AF memDWORD
						--PC;	// put back "register" WORD
						SETREGD(REG_7, GETREGD(REG_7)-4);
						SETMEMD(pKode + GETREGD(REG_7), GETMEMD(PC));
						PC += 2;
						break;
					 case 0x0030:	// PEA [regt+mem+off]	0x56BF 0:REGt memDWORD offDWORD
						SETREGD(REG_7, GETREGD(REG_7)-4);
						dw2 = GETMEMD(PC);
						PC += 2;
						SETMEMD(pKode + GETREGD(REG_7), GETREGD(w) + dw2 + GETMEMD(PC));
						PC += 2;
						break;
					 case 0x0040:	// PEA [regt1+regt2]	0x56CF REGt2:REGt1
						SETREGD(REG_7, GETREGD(REG_7)-4);
						SETMEMD(pKode + GETREGD(REG_7), GETREGD(w & 0x00FF) + GETREGD((w>>8) & 0x00FF));
						break;
					 case 0x0050:	// unused
					 case 0x0060:	// unused
					 case 0x0070:	// unused
						break;
					}
					goto leabreak;
				}
			}
			// w  = target reg OR pMem = target memory
			// b or w2 or dw2 = second arg
			dr = (opcode & 0x000F) > 4;	// dr = 0 if reg target, non-0 if mem target
			switch( (opcode >> 4) & 0x001F ) {
			 case 0:	//BYTE MOV
				if( dr ) SETMEMB(pMem, b);
				else SETREGB(w, b);
				break;
			 case 1:	//BYTE ADD
				if( dr ) SETMEMB(pMem, GETMEMB(pMem)+b);
				else SETREGB(w, GETREGB(w)+b);
				break;
			 case 2:	//BYTE SUB
				if( signedmath ) {
					if( dr ) SETMEMB(pMem, (result=(DWORD)((long)(((long)((char)GETMEMB(pMem)))-((long)((char)b))))));
					else SETREGB(w, (result=(DWORD)((long)(((long)((char)GETREGB(w)))-((long)((char)b))))));
				} else {
					if( dr ) SETMEMB(pMem, (result=(DWORD)GETMEMB(pMem)-(DWORD)b));
					else SETREGB(w, (result=(DWORD)GETREGB(w)-(DWORD)b));
				}
				break;
			 case 3:	//BYTE CMP
				if( signedmath ) {
					if( dr ) result = (DWORD)((long)(((long)((char)GETMEMB(pMem)))-((long)((char)b))));
					else result = (DWORD)((long)(((long)((char)GETREGB(w)))-((long)((char)b))));
				} else {
					if( dr ) result = (DWORD)GETMEMB(pMem)-(DWORD)b;
					else result = (DWORD)GETREGB(w)-(DWORD)b;
				}
				break;
			 case 4:	//BYTE TST
				if( dr ) result = (DWORD)(char)(GETMEMB(pMem) & b);
				else result = (DWORD)(char)(GETREGB(w) & b);
				result |= (result<<24);
				break;
			 case 5:	//BYTE AND
				if( dr ) SETMEMB(pMem, (BYTE)(result = (DWORD)((long)(char)GETMEMB(pMem) & (long)(char)b)));
				else SETREGB(w, (BYTE)(result = (DWORD)((long)(char)GETREGB(w) & (long)(char)b)));
				break;
			 case 6:	//BYTE OR
				if( dr ) SETMEMB(pMem, GETMEMB(pMem) | b);
				else SETREGB(w, GETREGB(w) | b);
				break;
			 case 7:	//BYTE XOR
				if( dr ) SETMEMB(pMem, (BYTE)(result = (DWORD)((long)(char)GETMEMB(pMem) ^ (long)(char)b)));
				else SETREGB(w, (BYTE)(result = (DWORD)((long)(char)GETREGB(w) ^ (long)(char)b)));
				break;
			 case 8:	//WORD MOV
				if( dr ) SETMEMW(pMem, w2);
				else SETREGW(w, w2);
				break;
			 case 9:	//WORD ADD
				if( dr ) SETMEMW(pMem, GETMEMW(pMem)+w2);
				else SETREGW(w, GETREGW(w)+w2);
				break;
			 case 10:	//WORD SUB
				if( signedmath ) {
					if( dr ) SETMEMW(pMem, (WORD)(result=(DWORD)((long)(((long)((short)GETMEMW(pMem)))-((long)((short)w2))))));
					else SETREGW(w, (WORD)(result=(DWORD)((long)(((long)((short)GETREGW(w)))-((long)((short)w2))))));
				} else {
					if( dr ) SETMEMW(pMem, (WORD)(result=(DWORD)GETMEMW(pMem)-(DWORD)w2));
					else SETREGW(w, (WORD)(result=(DWORD)GETREGW(w)-(DWORD)w2));
				}
				break;
			 case 11:	//WORD CMP
				if( signedmath ) {
					if( dr ) result = (DWORD)((long)(((long)((short)GETMEMW(pMem)))-((long)((short)w2))));
					else result = (DWORD)((long)(((long)((short)GETREGW(w)))-((long)((short)w2))));
				} else {
					if( dr ) result = (DWORD)GETMEMW(pMem)-(DWORD)w2;
					else result = (DWORD)GETREGW(w)-(DWORD)w2;
				}
				break;
			 case 12:	//WORD TST
				if( dr ) result = (DWORD)(short)(GETMEMW(pMem) & w2);
				else result = (DWORD)(short)(GETREGW(w) & w2);
				result |= (result<<24);
				break;
			 case 13:	//WORD AND
				if( dr ) SETMEMW(pMem, (WORD)(result = (DWORD)((long)(short)GETMEMW(pMem) & (long)(short)w2)));
				else SETREGW(w, (WORD)(result = (DWORD)((long)(short)GETREGW(w) & (long)(short)w2)));
				break;
			 case 14:	//WORD OR
				if( dr ) SETMEMW(pMem, GETMEMW(pMem) | w2);
				else SETREGW(w, GETREGW(w) | w2);
				break;
			 case 15:	//WORD XOR
				if( dr ) SETMEMW(pMem, (WORD)(result = (DWORD)((long)(short)GETMEMW(pMem) ^ (long)(short)w2)));
				else SETREGW(w, (WORD)(result = (DWORD)((long)(short)GETREGW(w) ^ (long)(short)w2)));
				break;
			 case 16:	//DWORD MOV
				if( dr ) SETMEMD(pMem, dw2);
				else SETREGD(w, dw2);
				break;
			 case 17:	//DWORD ADD
				if( dr ) SETMEMD(pMem, GETMEMD(pMem)+dw2);
				else SETREGD(w, GETREGD(w)+dw2);
				break;
			 case 18:	//DWORD SUB
				if( signedmath ) {
					if( dr ) SETMEMD(pMem, (result=(DWORD)((long)GETMEMD(pMem)-(long)dw2)));
					else SETREGD(w, (result=(DWORD)((long)GETREGD(w)-(long)dw2)));
				} else {
					if( dr ) SETMEMD(pMem, (result=GETMEMD(pMem)-dw2));
					else SETREGD(w, (result=GETREGD(w)-dw2));
				}
				break;
			 case 19:	//DWORD CMP
				if( signedmath ) {
					if( dr ) result = (DWORD)((long)GETMEMD(pMem)-(long)dw2);
					else result = (DWORD)((long)GETREGD(w)-(long)dw2);
				} else {
					if( dr ) result = GETMEMD(pMem)-dw2;
					else result = GETREGD(w)-dw2;
				}
				break;
			 case 20:	//DWORD TST
				if( dr ) result = GETMEMD(pMem) & dw2;
				else result = GETREGD(w) & dw2;
				break;
			 case 21:	//DWORD AND
				if( dr ) SETMEMD(pMem, (result = GETMEMD(pMem) & dw2));
				else SETREGD(w, (result = GETREGD(w) & dw2));
				break;
			 case 22:	//DWORD OR
				if( dr ) SETMEMD(pMem, GETMEMD(pMem) | dw2);
				else SETREGD(w, GETREGD(w) | dw2);
				break;
			 case 23:	//DWORD XOR
				if( dr ) SETMEMD(pMem, (result = GETMEMD(pMem) ^ dw2));
				else SETREGD(w, (result = GETREGD(w) ^ dw2));
				break;
			}
leabreak:
			break;
		 case 0x58:	// LD0 b,d,=dval
			w = GETMEMW(PC);
			PC++;
			SETREGB(opcode & 0x00FF, GETMEMB(pKode + GETREGD(w) + GETMEMD(PC)));
			PC += 2;
			break;
		 case 0x59:	// LD0 w,d,=dval
			w = GETMEMW(PC);
			PC++;
			SETREGW(opcode & 0x00FF, GETMEMW((WORD *)(pKode + GETREGD(w) + GETMEMD(PC))));
			PC += 2;
			break;
		 case 0x5A:	// LD0 d,d,=dval
			w = GETMEMW(PC);
			PC++;
			SETREGD(opcode & 0x00FF, GETMEMD(pKode + GETREGD(w) + GETMEMD(PC)));
			PC += 2;
			break;
		 case 0x5B:	// LD1 b,d,=dval
			w = GETMEMW(PC);
			PC++;
			SETREGB(opcode & 0x00FF, GETMEMB(GETREGD(REG_1) + GETREGD(w) + GETMEMD(PC)));
			PC += 2;
			break;
		 case 0x5C:	// LD1 w,d,=dval
			w = GETMEMW(PC);
			PC++;
			SETREGW(opcode & 0x00FF, GETMEMW((WORD *)(GETREGD(REG_1) + GETREGD(w) + GETMEMD(PC))));
			PC += 2;
			break;
		 case 0x5D:	// LD1 d,d,=dval
			w = GETMEMW(PC);
			PC++;
			SETREGD(opcode & 0x00FF, GETMEMD(GETREGD(REG_1) + GETREGD(w) + GETMEMD(PC)));
			PC += 2;
			break;
		 case 0x5E:	// LD2 b,d,=dval
			w = GETMEMW(PC);
			PC++;
			SETREGB(opcode & 0x00FF, GETMEMB(GETREGD(REG_2) + GETREGD(w) + GETMEMD(PC)));
			PC += 2;
			break;
		 case 0x5F:	// LD2 w,d,=dval
			w = GETMEMW(PC);
			PC++;
			SETREGW(opcode & 0x00FF, GETMEMW((WORD *)(GETREGD(REG_2) + GETREGD(w) + GETMEMD(PC))));
			PC += 2;
			break;
		 case 0x60:	// LD2 d,d,=dval
			w = GETMEMW(PC);
			PC++;
			SETREGD(opcode & 0x00FF, GETMEMD(GETREGD(REG_2) + GETREGD(w) + GETMEMD(PC)));
			PC += 2;
			break;
		 case 0x61:	// LDS b,d,=dval
			w = GETMEMW(PC);
			PC++;
			SETREGB(opcode & 0x00FF, GETMEMB(pKode + GETREGD(REG_4) + GETREGD(w) + GETMEMD(PC)));
			PC += 2;
			break;
		 case 0x62:	// LDS w,d,=dval
			w = GETMEMW(PC);
			PC++;
			SETREGW(opcode & 0x00FF, GETMEMW((WORD *)(pKode + GETREGD(REG_4) + GETREGD(w) + GETMEMD(PC))));
			PC += 2;
			break;
		 case 0x63:	// LDS d,d,=dval
			w = GETMEMW(PC);
			PC++;
			SETREGD(opcode & 0x00FF, GETMEMD(pKode + GETREGD(REG_4) + GETREGD(w) + GETMEMD(PC)));
			PC += 2;
			break;
		 case 0x64:	// ST0 b,d,=dval
			w = GETMEMW(PC);
			PC++;
			SETMEMB(pKode + GETREGD(w) + GETMEMD(PC), GETREGB(opcode & 0x00FF));
			PC += 2;
			break;
		 case 0x65:	// ST0 w,d,=dval
			w = GETMEMW(PC);
			PC++;
			SETMEMW((WORD *)(pKode + GETREGD(w) + GETMEMD(PC)), GETREGW(opcode & 0x00FF));
			PC += 2;
			break;
		 case 0x66:	// ST0 d,d,=dval
			w = GETMEMW(PC);
			PC++;
			SETMEMD(pKode + GETREGD(w) + GETMEMD(PC), GETREGD(opcode & 0x00FF));
			PC += 2;
			break;
		 case 0x67:	// ST1 b,d,=dval
			w = GETMEMW(PC);
			PC++;
			SETMEMB(GETREGD(REG_1) + GETREGD(w) + GETMEMD(PC), GETREGB(opcode & 0x00FF));
			PC += 2;
			break;
		 case 0x68:	// ST1 w,d,=dval
			w = GETMEMW(PC);
			PC++;
			SETMEMW((WORD *)(GETREGD(REG_1) + GETREGD(w) + GETMEMD(PC)), GETREGW(opcode & 0x00FF));
			PC += 2;
			break;
		 case 0x69:	// ST1 d,d,=dval
			w = GETMEMW(PC);
			PC++;
			SETMEMD(GETREGD(REG_1) + GETREGD(w) + GETMEMD(PC), GETREGD(opcode & 0x00FF));
			PC += 2;
			break;
		 case 0x6A:	// ST2 b,d,=dval
			w = GETMEMW(PC);
			PC++;
			SETMEMB(GETREGD(REG_2) + GETREGD(w) + GETMEMD(PC), GETREGB(opcode & 0x00FF));
			PC += 2;
			break;
		 case 0x6B:	// ST2 w,d,=dval
			w = GETMEMW(PC);
			PC++;
			SETMEMW((WORD *)(GETREGD(REG_2) + GETREGD(w) + GETMEMD(PC)), GETREGW(opcode & 0x00FF));
			PC += 2;
			break;
		 case 0x6C:	// ST2 d,d,=dval
			w = GETMEMW(PC);
			PC++;
			SETMEMD(GETREGD(REG_2) + GETREGD(w) + GETMEMD(PC), GETREGD(opcode & 0x00FF));
			PC += 2;
			break;
		 case 0x6D:	// STS b,d,=dval
			w = GETMEMW(PC);
			PC++;
			dw= GETMEMD(PC);
			PC += 2;
			SETMEMB(pKode + GETREGD(REG_4) + GETREGD(w) + dw, GETREGB(opcode & 0x00FF));
			break;
		 case 0x6E:	// STS w,d,=dval
			w = GETMEMW(PC);
			PC++;
			dw= GETMEMD(PC);
			PC += 2;
			SETMEMW((WORD *)(pKode + GETREGD(REG_4) + GETREGD(w) + dw), GETREGW(opcode & 0x00FF));
			break;
		 case 0x6F:	// STS d,d,=dval
			w = GETMEMW(PC);
			PC++;
			dw= GETMEMD(PC);
			PC += 2;
			SETMEMD(pKode + GETREGD(REG_4) + GETREGD(w) + dw, GETREGD(opcode & 0x00FF));
			break;
		 case 0x70:	// PU0 b,d,=dval
			w = GETMEMW(PC);
			PC++;
			dw= GETMEMD(PC);
			PC += 2;
			SETMEMB(pKode + GETREGD(w) +dw, GETREGB(opcode & 0x00FF));
			SETREGD(w, GETREGD(w)+1);
			break;
		 case 0x71:	// PU0 w,d,=dval
			w = GETMEMW(PC);
			PC++;
			dw= GETMEMD(PC);
			PC += 2;
			SETMEMW((WORD *)(pKode + GETREGD(w) +dw), GETREGW(opcode & 0x00FF));
			SETREGD(w, GETREGD(w)+2);
			break;
		 case 0x72:	// PU0 d,d,=dval
			w = GETMEMW(PC);
			PC++;
			dw= GETMEMD(PC);
			PC += 2;
			SETMEMD(pKode + GETREGD(w) +dw, GETREGD(opcode & 0x00FF));
			SETREGD(w, GETREGD(w)+4);
			break;
		 case 0x73:	// PO0 b,d,=dval
			w = GETMEMW(PC);
			PC++;
			dw= GETMEMD(PC);
			PC += 2;
			SETREGD(w, GETREGD(w)-1);
			SETREGB(opcode & 0x00FF, GETMEMB(pKode + GETREGD(w) + dw));
			break;
		 case 0x74:	// PO0 w,d,=dval
			w = GETMEMW(PC);
			PC++;
			dw= GETMEMD(PC);
			PC += 2;
			SETREGD(w, GETREGD(w)-2);
			SETREGW(opcode & 0x00FF, GETMEMW((WORD *)(pKode + GETREGD(w) + dw)));
			break;
		 case 0x75:	// PO0 d,d,=dval
			w = GETMEMW(PC);
			PC++;
			dw= GETMEMD(PC);
			PC += 2;
			SETREGD(w, GETREGD(w)-4);
			SETREGD(opcode & 0x00FF, GETMEMD(pKode + GETREGD(w) + dw));
			break;
		 case 0x76:	// LSA d,=dval
			SETREGD(opcode & 0x00FF, GETREGD(REG_4) + (int)(short)(WORD)GETMEMW(PC));
			PC++;
			break;
		 case 0x77:	// B2I r,r (byte-2-index dst-cnt-reg,src-bit-test-reg
			dr = opcode & 0x00FF;
			w = GETMEMW(PC);
			PC++;
			if( w<0x0080 ) dw = GETREGD(w);
			else if( w<0x00E0 ) dw = (DWORD)GETREGW(w);
			else dw = (DWORD)GETREGB(w);
			if( dw ) {
				result = 0;
				while( !(dw & 1) ) {
					++result;
					dw >>= 1;
				}
			} else result = 0xFFFFFFFF;
			if( dr<0x0080 ) SETREGD(opcode & 0x00FF, result);
			else if( dr<0x00E0 ) SETREGW(opcode & 0x00FF, (WORD)result);
			else SETREGB(opcode & 0x00FF, (BYTE)result);
			break;
		 case 0x78:	// CLR b,b
			dr = opcode & 0x00FF;
			memset(Kregs+dr, 0, GETMEMW(PC)+1-dr);
			PC++;
			break;
		 case 0x79:	// PUR b,b
			dr = opcode & 0x00FF;
			w = GETMEMW(PC)+1-dr;
			PC++;
			dw = GETREGD(REG_5);
			memcpy(pKode + dw, Kregs+dr, w);
			SETREGD(REG_5, dw+w);
			break;
		 case 0x7A:	// POR b,b
			dr = opcode & 0x00FF;
			w = GETMEMW(PC)+1-dr;
			PC++;
			dw = GETREGD(REG_5)-w;
			SETREGD(REG_5, dw);
			memcpy(Kregs+dr, pKode+dw, w);
			break;
		 case 0x7B:	// unused
		 case 0x7C:	// unused
			break;
		 case 0x7D:	// SAF b
			dw = result & 0x00000001;
			if( result==0 ) dw |= 2;
			if( result & 0x80000000 ) dw |= 4;
			if( signedmath ) dw |= 8;
			SETREGB(opcode & 0x00FF, (BYTE)dw);
			break;
		 case 0x7E:	// REF b
			b = GETREGB(opcode & 0x00FF);
			result = ((~b) & 2) | (b & 1);
			if( b & 4 ) result |= 0x80000000;
			signedmath = (b & 8)?1:0;
			break;
		 case 0x7F:	// RET b,=0
#ifdef GEEK_TRACE2
	if( TBptr2 ) --TBptr2;
#endif
			SETREGD(REG_5, GETREGD(REG_5)-4);	// pop return address
			PC = (WORD *)(pKode + GETMEMD(pKode + GETREGD(REG_5)));
			result = (DWORD)SETREGB(opcode & 0x00FF, 0);
			break;
		 case 0x80:	// RET w,=0
#ifdef GEEK_TRACE2
	if( TBptr2 ) --TBptr2;
#endif
			SETREGD(REG_5, GETREGD(REG_5)-4);	// pop return address
			PC = (WORD *)(pKode + GETMEMD(pKode + GETREGD(REG_5)));
			result = (DWORD)SETREGW(opcode & 0x00FF, 0);
			break;
		 case 0x81:	// RET d,=0
#ifdef GEEK_TRACE2
	if( TBptr2 ) --TBptr2;
#endif
			SETREGD(REG_5, GETREGD(REG_5)-4);	// pop return address
			PC = (WORD *)(pKode + GETMEMD(pKode + GETREGD(REG_5)));
			result = (DWORD)SETREGD(opcode & 0x00FF, 0);
			break;
		 case 0x82:	// RET b,=1
#ifdef GEEK_TRACE2
	if( TBptr2 ) --TBptr2;
#endif
			SETREGD(REG_5, GETREGD(REG_5)-4);	// pop return address
			PC = (WORD *)(pKode + GETMEMD(pKode + GETREGD(REG_5)));
			result = (DWORD)SETREGB(opcode & 0x00FF, 1);
			break;
		 case 0x83:	// RET w,=1
#ifdef GEEK_TRACE2
	if( TBptr2 ) --TBptr2;
#endif
			SETREGD(REG_5, GETREGD(REG_5)-4);	// pop return address
			PC = (WORD *)(pKode + GETMEMD(pKode + GETREGD(REG_5)));
			result = (DWORD)SETREGW(opcode & 0x00FF, 1);
			break;
		 case 0x84:	// RET d,=1
#ifdef GEEK_TRACE2
	if( TBptr2 ) --TBptr2;
#endif
			SETREGD(REG_5, GETREGD(REG_5)-4);	// pop return address
			PC = (WORD *)(pKode + GETMEMD(pKode + GETREGD(REG_5)));
			result = (DWORD)SETREGD(opcode & 0x00FF, 1);
			break;
		 case 0x85:	// INC b
			dr = opcode & 0x00FF;
			result = (DWORD)(char)SETREGB(dr, GETREGB(dr) + 1);
			result |= (result<<24);
			break;
		 case 0x86:	// INC w
			dr = opcode & 0x00FF;
			result = (DWORD)(short)SETREGW(dr, GETREGW(dr) + 1);
			result |= (result<<16);
			break;
		 case 0x87:	// INC d
			dr = opcode & 0x00FF;
			result = SETREGD(dr, GETREGD(dr) + 1);
			break;
		 case 0x88:	// DEC b
			dr = opcode & 0x00FF;
			result = (DWORD)(char)SETREGB(dr, GETREGB(dr) - 1);
			result |= (result<<24);
			break;
		 case 0x89:	// DEC w
			dr = opcode & 0x00FF;
			result = (DWORD)(short)SETREGW(dr, GETREGW(dr) - 1);
			result |= (result<<16);
			break;
		 case 0x8A:	// DEC d
			dr = opcode & 0x00FF;
			result = SETREGD(dr, GETREGD(dr) - 1);
			break;
		 case 0x8B:	// NOT b
			dr = opcode & 0x00FF;
			result = (DWORD)(char)SETREGB(dr, ~GETREGB(dr));
			result |= (result<<24);
			break;
		 case 0x8C:	// NOT w
			dr = opcode & 0x00FF;
			result = (DWORD)(short)SETREGW(dr, ~GETREGW(dr));
			result |= (result<<16);
			break;
		 case 0x8D:	// NOT d
			dr = opcode & 0x00FF;
			result = SETREGD(dr, ~GETREGD(dr));
			break;
		 case 0x8E:	// NEG b
			dr = opcode & 0x00FF;
			result = (DWORD)(char)SETREGB(dr, -(char)GETREGB(dr));
			result |= (result<<24);
			break;
		 case 0x8F:	// NEG w
			dr = opcode & 0x00FF;
			result = (DWORD)(short)SETREGW(dr, -(short)GETREGW(dr));
			result |= (result<<16);
			break;
		 case 0x90:	// NEG d
			dr = opcode & 0x00FF;
			result = SETREGD(dr, -(long)GETREGD(dr));
			break;
		 case 0x91:	// PUSH b	NOTE: we actually push a WORD to keep the stack WORD aligned
			SETMEMW((WORD *)(pKode + GETREGD(REG_5)), (WORD)(BYTE)GETREGB(opcode & 0x00FF));
			SETREGD(REG_5, GETREGD(REG_5)+2);
			break;
		 case 0x92:	// PUSH w
			SETMEMW((WORD *)(pKode + GETREGD(REG_5)), GETREGW(opcode & 0x00FF));
			SETREGD(REG_5, GETREGD(REG_5)+2);
			break;
		 case 0x93:	// PUSH d
			SETMEMD(pKode + GETREGD(REG_5), GETREGD(opcode & 0x00FF));
			SETREGD(REG_5, GETREGD(REG_5)+4);
			break;
		 case 0x94:	// POP b	NOTE: we actually pop a WORD, as that's what was pushed
			SETREGD(REG_5, GETREGD(REG_5)-2);
			SETREGB(opcode & 0x00FF, GETMEMB(pKode + GETREGD(REG_5)));
			break;
		 case 0x95:	// POP w
			SETREGD(REG_5, GETREGD(REG_5)-2);
			SETREGW(opcode & 0x00FF, GETMEMW((WORD *)(pKode + GETREGD(REG_5))));
			break;
		 case 0x96:	// POP d
			SETREGD(REG_5, GETREGD(REG_5)-4);
			SETREGD(opcode & 0x00FF, GETMEMD(pKode + GETREGD(REG_5)));
			break;
		 case 0x97:	// CBW w
//			w = GETREGW(opcode & 0x00FF);
//			SETREGW(opcode & 0x00FF, (w & 0x0080)?(w | 0xFF00):(w & 0x00FF));
			SETREGW(opcode & 0x00FF, (WORD)(short)(char)GETREGB(opcode & 0x00FF));
			break;
		 case 0x98:	// LD0 b,=dval
			SETREGB(opcode & 0x00FF, GETMEMB(pKode + GETMEMD(PC)));
			PC += 2;
			break;
		 case 0x99:	// LD0 w,=dval
			SETREGW(opcode & 0x00FF, GETMEMW((WORD *)(pKode + GETMEMD(PC))));
			PC += 2;
			break;
		 case 0x9A:	// LD0 d,=dval
			SETREGD(opcode & 0x00FF, GETMEMD(pKode + GETMEMD(PC)));
			PC += 2;
			break;
		 case 0x9B:	// LD1 b,=dval
			SETREGB(opcode & 0x00FF, GETMEMB(GETREGD(REG_1) + GETMEMD(PC)));
			PC += 2;
			break;
		 case 0x9C:	// LD1 w,=dval
			SETREGW(opcode & 0x00FF, GETMEMW((WORD *)(GETREGD(REG_1) + GETMEMD(PC))));
			PC += 2;
			break;
		 case 0x9D:	// LD1 d,=dval
			SETREGD(opcode & 0x00FF, GETMEMD(GETREGD(REG_1) + GETMEMD(PC)));
			PC += 2;
			break;
		 case 0x9E:	// LD2 b,=dval
			SETREGB(opcode & 0x00FF, GETMEMB(GETREGD(REG_2) + GETMEMD(PC)));
			PC += 2;
			break;
		 case 0x9F:	// LD2 w,=dval
			SETREGW(opcode & 0x00FF, GETMEMW((WORD *)(GETREGD(REG_2) + GETMEMD(PC))));
			PC += 2;
			break;
		 case 0xA0:	// LD2 d,=dval
			SETREGD(opcode & 0x00FF, GETMEMD(GETREGD(REG_2) + GETMEMD(PC)));
			PC += 2;
			break;
		 case 0xA1:	// LDS b,=dval
			SETREGB(opcode & 0x00FF, GETMEMB(pKode + GETREGD(REG_4) + (DWORD)((long)((short)GETMEMW(PC)))));
			PC++;
			break;
		 case 0xA2:	// LDS w,=dval
			SETREGW(opcode & 0x00FF, GETMEMW((WORD *)(pKode + GETREGD(REG_4) + (DWORD)((long)((short)GETMEMW(PC))))));
			PC++;
			break;
		 case 0xA3:	// LDS d,=dval
			SETREGD(opcode & 0x00FF, GETMEMD(pKode + GETREGD(REG_4) + (DWORD)((long)((short)GETMEMW(PC)))));
			PC++;
			break;
		 case 0xA4:	// ST0 b,=dval
			SETMEMB(pKode + GETMEMD(PC), GETREGB(opcode & 0x00FF));
			PC += 2;
			break;
		 case 0xA5:	// ST0 w,=dval
			SETMEMW((WORD *)(pKode + GETMEMD(PC)), GETREGW(opcode & 0x00FF));
			PC += 2;
			break;
		 case 0xA6:	// ST0 d,=dval
			SETMEMD(pKode + GETMEMD(PC), GETREGD(opcode & 0x00FF));
			PC += 2;
			break;
		 case 0xA7:	// ST1 b,=dval
			SETMEMB(GETREGD(REG_1) + GETMEMD(PC), GETREGB(opcode & 0x00FF));
			PC += 2;
			break;
		 case 0xA8:	// ST1 w,=dval
			SETMEMW((WORD *)(GETREGD(REG_1) + GETMEMD(PC)), GETREGW(opcode & 0x00FF));
			PC += 2;
			break;
		 case 0xA9:	// ST1 d,=dval
			SETMEMD(GETREGD(REG_1) + GETMEMD(PC), GETREGD(opcode & 0x00FF));
			PC += 2;
			break;
		 case 0xAA:	// ST2 b,=dval
			SETMEMB(GETREGD(REG_2) + GETMEMD(PC), GETREGB(opcode & 0x00FF));
			PC += 2;
			break;
		 case 0xAB:	// ST2 w,=dval
			SETMEMW((WORD *)(GETREGD(REG_2) + GETMEMD(PC)), GETREGW(opcode & 0x00FF));
			PC += 2;
			break;
		 case 0xAC:	// ST2 d,=dval
			SETMEMD(GETREGD(REG_2) + GETMEMD(PC), GETREGD(opcode & 0x00FF));
			PC += 2;
			break;
		 case 0xAD:	// STS b,=dval
			SETMEMB(pKode + GETREGD(REG_4) + (DWORD)((long)((short)GETMEMW(PC))), GETREGB(opcode & 0x00FF));
			PC++;
			break;
		 case 0xAE:	// STS w,=dval
			SETMEMW((WORD *)(pKode + GETREGD(REG_4) + (DWORD)((long)((short)GETMEMW(PC)))), GETREGW(opcode & 0x00FF));
			PC++;
			break;
		 case 0xAF:	// STS d,=dval
			SETMEMD(pKode + GETREGD(REG_4) + (DWORD)((long)((short)GETMEMW(PC))), GETREGD(opcode & 0x00FF));
			PC++;
			break;
		 case 0xB0:	// IJMP b,=dval
			dw2 = (DWORD)GETREGB(opcode & 0x00FF);
			goto ijmp;
		 case 0xB1:	// IJMP w,=dval
			dw2 = (DWORD)GETREGW(opcode & 0x00FF);
			goto ijmp;
		 case 0xB2:	// IJMP d,=dval
			dw2 = GETREGD(opcode & 0x00FF);
ijmp:
			pMem = pKode + GETMEMD(PC);	// don't bother to advance PC, we're jumping
			dw = GETMEMD(pMem);	// get # of entries in table
			if( dw2>dw ) dw2 = dw;
			++dw2;	// also skip "number of entries" DWORD
			pMem += 4*dw2;	// skip to the entry (default if past end of table)
			PC = (WORD *)(pKode + GETMEMD(pMem));
			break;
		 case 0xB3:	// KJMP b,=dval
			dw2 = (DWORD)GETREGB(opcode & 0x00FF);
			goto kjmp;
		 case 0xB4:	// KJMP w,=dval
			dw2 = (DWORD)GETREGW(opcode & 0x00FF);
			goto kjmp;
		 case 0xB5:	// KJMP d,=dval
			dw2 = GETREGD(opcode & 0x00FF);
kjmp:
			pMem = pKode + GETMEMD(PC);	// don't bother to advance PC, we're jumping
			dw = GETMEMD(pMem);	// get # of entries in table
			pMem += 4;
			while( dw-- ) {
				if( dw2==GETMEMD(pMem) ) {
					pMem += 4;	// skip value, point at jmp address
					break;
				}
				pMem += 8;	// skip value & jump address
			}
			PC = (WORD *)(pKode + GETMEMD(pMem));
			break;
		 case 0xB6:	// JSR d,=dval
#ifdef GEEK_TRACE2
	if( TBptr2+1 < TBMAX2 ) TraceBuf2[TBptr2++] = (DWORD)((BYTE *)PC-pKode);
#endif
			dw = GETREGD(opcode & 0x00FF) + GETMEMD(PC);
			PC += 2;
			SETMEMD(pKode + GETREGD(REG_5), (DWORD)((BYTE *)PC - pKode));	// push return address
			SETREGD(REG_5, GETREGD(REG_5)+4);
			PC = (WORD *)(pKode + dw);
			break;
		 case 0xB7:	// JSI d,=dval
#ifdef GEEK_TRACE2
	if( TBptr2+1 < TBMAX2 ) TraceBuf2[TBptr2++] = (DWORD)((BYTE *)PC-pKode);
#endif
			dw = GETMEMD(pKode + (GETREGD(opcode & 0x00FF) + GETMEMD(PC)));
			PC += 2;
			SETMEMD(pKode + GETREGD(REG_5), (DWORD)((BYTE *)PC - pKode));	// push return address
			SETREGD(REG_5, GETREGD(REG_5)+4);
			PC = (WORD *)(pKode + dw);
			break;
		 case 0xB8:	// CBD d
//			dw = GETREGD(opcode & 0x00FF);
//			SETREGD(opcode & 0x00FF, (dw & 0x00000080)?(dw | 0xFFFFFF00):(dw & 0x000000FF));
			SETREGD(opcode & 0x00FF, (DWORD)(long)(char)GETREGB(opcode & 0x00FF));
			break;
		 case 0xB9:	// CWD d
//			dw = GETREGD(opcode & 0x00FF);
//			SETREGD(opcode & 0x00FF, (dw & 0x00008000)?(dw | 0xFFFF0000):(dw & 0x0000FFFF));
			SETREGD(opcode & 0x00FF, (DWORD)(long)(short)GETREGW(opcode & 0x00FF));
			break;
		 case 0xBA:	// LDI b,=wval
			SETREGB(opcode & 0x00FF, GETMEMW(PC));
			PC++;
			break;
		 case 0xBB:	// LDI w,=wval
			SETREGW(opcode & 0x00FF, GETMEMW(PC));
			PC++;
			break;
		 case 0xBC:	// LDI d,=dval
			SETREGD(opcode & 0x00FF, GETMEMD(PC));
			PC += 2;
			break;
		 case 0xBD:	// SHL b,=wval
			dr = opcode & 0x00FF;
			result = (DWORD)(char)SETREGB(dr, GETREGB(dr)<<(BYTE)(GETMEMW(PC) & 0x0007));
			PC++;
			result |= (result<<24);
			break;
		 case 0xBE:	// SHL w,=wval
			dr = opcode & 0x00FF;
			result = SETREGW(dr, GETREGW(dr)<<(BYTE)(GETMEMW(PC) & 0x000F));
			PC++;
			result |= (result<<16);
			break;
		 case 0xBF:	// SHL d,=wval
			dr = opcode & 0x00FF;
			result = SETREGD(dr, GETREGD(dr)<<(BYTE)(GETMEMW(PC) & 0x001F));
			PC++;
			break;
		 case 0xC0:	// SHR b,=wval
			dr = opcode & 0x00FF;
			if( signedmath ) result = (DWORD)(char)(SETREGB(dr, (char)GETREGB(dr)>>(BYTE)(GETMEMW(PC) & 0x0007)));
			else result = (DWORD)(char)(SETREGB(dr, GETREGB(dr)>>(BYTE)(GETMEMW(PC) & 0x0007)));
			PC++;
			result |= (result<<24);
			break;
		 case 0xC1:	// SHR w,=wval
			dr = opcode & 0x00FF;
			if( signedmath ) result = (DWORD)(short)(SETREGW(dr, (short)(GETREGW(dr))>>(BYTE)(GETMEMW(PC) & 0x000F)));
			else result = (DWORD)(short)(SETREGW(dr, GETREGW(dr)>>(BYTE)(GETMEMW(PC) & 0x000F)));
			PC++;
			result |= (result<<16);
			break;
		 case 0xC2:	// SHR d,=wval
			dr = opcode & 0x00FF;
			if( signedmath ) result = SETREGD(dr, (long)GETREGD(dr)>>(BYTE)(GETMEMW(PC) & 0x001F));
			else result = SETREGD(dr, GETREGD(dr)>>(BYTE)(GETMEMW(PC) & 0x001F));
			PC++;
			break;
		 case 0xC3:	// ROL b,=wval
			dr = opcode & 0x00FF;
			w = (GETMEMW(PC) & 0x0007);
			PC++;
			b = GETREGB(dr);
			while( w-- ) {
				if( b & 0x80 ) b = (b << 1) | 1;
				else b <<= 1;
			}
			result = (DWORD)(char)SETREGB(dr, b);
			result |= (result<<24);
			break;
		 case 0xC4:	// ROL w,=wval
			dr = opcode & 0x00FF;
			b = (BYTE)GETMEMW(PC) & 0x001F;
			PC++;
			w = GETREGW(dr);
			while( b-- ) {
				if( w & 0x8000 ) w = (w << 1) | 1;
				else w <<= 1;
			}
			result = (DWORD)(short)SETREGW(dr, w);
			result |= (result<<16);
			break;
		 case 0xC5:	// ROL d,=wval
			dr = opcode & 0x00FF;
			dw = GETREGD(dr);
			w = GETMEMW(PC) & 0x1F;
			PC++;
			while( w-- ) {
				dw2 = dw;
				dw <<= 1;
				if( dw2 & 0x80000000 ) dw |= 1;
			}
			result = SETREGD(dr, dw);
			break;
		 case 0xC6:	// ROR b,=wval
			dr = opcode & 0x00FF;
			w = GETMEMW(PC) & 0x0007;
			PC++;
			b = GETREGB(dr);
			while( w-- ) {
				if( b & 1 ) b = (b >> 1) | 0x80;
				else b >>= 1;
			}
			result = (DWORD)(char)SETREGB(dr, b);
			result |= (result<<24);
			break;
		 case 0xC7:	// ROR w,=wval
			dr = opcode & 0x00FF;
			b = (BYTE)GETMEMW(PC) & 0x001F;
			PC++;
			w = GETREGW(dr);
			while( b-- ) {
				if( w & 1 ) w = (w >> 1) | 0x8000;
				else w >>= 1;
			}
			result = (DWORD)(short)SETREGW(dr, w);
			result |= (result<<16);
			break;
		 case 0xC8:	// ROR d,=wval
			dr = opcode & 0x00FF;
			dw = GETREGD(dr);
			w = GETMEMW(PC) & 0x1F;
			PC++;
			while( w-- ) {
				dw2 = dw;
				dw >>= 1;
				if( dw2 & 1 ) dw |= 0x80000000;
			}
			result = SETREGD(dr, dw);
			break;
		 case 0xC9:	// ADD b,=val
			dr = opcode & 0x00FF;
			SETREGB(dr, GETREGB(dr)+(BYTE)GETMEMW(PC));
			PC++;
			break;
		 case 0xCA:	// ADD w,=val
			dr = opcode & 0x00FF;
			SETREGW(dr, GETREGW(dr)+GETMEMW(PC));
			PC++;
			break;
		 case 0xCB:	// ADD d,=val
			dr = opcode & 0x00FF;
			SETREGD(dr, GETREGD(dr) + GETMEMD(PC));
			PC += 2;
			break;
		 case 0xCC:	// SUB b,=val
			dr = opcode & 0x00FF;
			if( signedmath ) result = (DWORD)((long)(((long)((char)GETREGB(dr)))-((long)((char)((BYTE)GETMEMW(PC))))));
			else result = (DWORD)GETREGB(dr)-(DWORD)GETMEMB(PC);
			PC++;
			SETREGB(dr, (BYTE)result);
			break;
		 case 0xCD:	// SUB w,=val
			dr = opcode & 0x00FF;
			if( signedmath ) result = (DWORD)((long)(((long)((short)GETREGW(dr)))-((long)((short)(GETMEMW(PC))))));
			else result = (DWORD)GETREGW(dr)-(DWORD)GETMEMW(PC);
			PC++;
			SETREGW(dr, (WORD)result);
			break;
		 case 0xCE:	// SUB d,=val
			dr = opcode & 0x00FF;
			if( signedmath ) result = SETREGD(dr, (long)(GETREGD(dr)) - (long)(GETMEMD(PC)));
			else result = SETREGD(dr, GETREGD(dr) - GETMEMD(PC));
			PC += 2;
			break;
		 case 0xCF:	// MUL b,=val
			dr = opcode & 0x00FF;
			b = (BYTE)GETMEMW(PC);
			PC++;
			SETREGB(dr, GETREGB(dr)*b);
			break;
		 case 0xD0:	// MUL w,=val
			dr = opcode & 0x00FF;
			SETREGW(dr, GETREGW(dr)*GETMEMW(PC));
			PC++;
			break;
		 case 0xD1:	// MUL d,=val
			dr = opcode & 0x00FF;
			SETREGD(dr, GETREGD(dr) * GETMEMD(PC));
			PC += 2;
			break;
		 case 0xD2:	// DIV b,=val
			dr = opcode & 0x00FF;
			b = (BYTE)GETMEMW(PC);
			PC++;
			if( signedmath ) SETREGB(dr, (char)(GETREGB(dr))/(char)b);
			else SETREGB(dr, GETREGB(dr)/b);
			break;
		 case 0xD3:	// DIV w,=val
			dr = opcode & 0x00FF;
			w2 = GETMEMW(PC);
			PC++;
			if( signedmath ) SETREGW(dr, (short)(GETREGW(dr))/(short)w2);
			else SETREGW(dr, GETREGW(dr)/w2);
			break;
		 case 0xD4:	// DIV d,=val
			dr = opcode & 0x00FF;
			if( signedmath ) SETREGD(dr, (long)(GETREGD(dr)) / (long)(GETMEMD(PC)));
			else SETREGD(dr, GETREGD(dr) / GETMEMD(PC));
			PC += 2;
			break;
		 case 0xD5:	// MOD b,=val
			dr = opcode & 0x00FF;
			if( signedmath ) SETREGB(dr, (char)(GETREGB(dr))%(char)((BYTE)GETMEMW(PC)));
			else SETREGB(dr, GETREGB(dr)%((BYTE)GETMEMW(PC)));
			PC++;
			break;
		 case 0xD6:	// MOD w,=val
			dr = opcode & 0x00FF;
			if( signedmath ) SETREGW(dr, (short)(GETREGW(dr))%(short)((WORD)GETMEMW(PC)));
			else SETREGW(dr, (WORD)GETREGW(dr)%((WORD)GETMEMW(PC)));
			PC++;
			break;
		 case 0xD7:	// MOD d,=val
			dr = opcode & 0x00FF;
			if( signedmath ) SETREGD(dr, (long)(GETREGD(dr)) % (long)(GETMEMD(PC)));
			else SETREGD(dr, GETREGD(dr) % GETMEMD(PC));
			PC += 2;
			break;
		 case 0xD8:	// OR b,=val
			dr = opcode & 0x00FF;
			SETREGB(dr, GETREGB(dr) | (BYTE)GETMEMW(PC));
			PC++;
			break;
		 case 0xD9:	// OR w,=val
			dr = opcode & 0x00FF;
			SETREGW(dr, GETREGW(dr) | GETMEMW(PC));
			PC++;
			break;
		 case 0xDA:	// OR d,=val
			dr = opcode & 0x00FF;
			SETREGD(dr, GETREGD(dr) | GETMEMD(PC));
			PC += 2;
			break;
		 case 0xDB:	// AND b,=val
			dr = opcode & 0x00FF;
			result = (DWORD)(char)SETREGB(dr, GETREGB(dr) & (BYTE)GETMEMW(PC));
			PC++;
			result |= (result<<24);
			break;
		 case 0xDC:	// AND w,=val
			dr = opcode & 0x00FF;
			result = (DWORD)(short)SETREGW(dr, GETREGW(dr) & GETMEMW(PC));
			PC++;
			result |= (result<<16);
			break;
		 case 0xDD:	// AND d,=val
			dr = opcode & 0x00FF;
			result = SETREGD(dr, GETREGD(dr) & GETMEMD(PC));
			PC += 2;
			break;
		 case 0xDE:	// XOR b,=val
			dr = opcode & 0x00FF;
			result = (DWORD)(char)SETREGB(dr, GETREGB(dr) ^ (BYTE)GETMEMW(PC));
			PC++;
			result |= (result<<24);
			break;
		 case 0xDF:	// XOR w,=val
			dr = opcode & 0x00FF;
			result = (DWORD)(short)SETREGW(dr, GETREGW(dr) ^ GETMEMW(PC));
			PC++;
			result |= (result<<16);
			break;
		 case 0xE0:	// XOR d,=val
			dr = opcode & 0x00FF;
			result = SETREGD(dr, GETREGD(dr) ^ GETMEMD(PC));
			PC += 2;
			break;
		 case 0xE1:	// CMP b,=val
			if( signedmath ) result = (DWORD)((long)(((long)(char)GETREGB(opcode & 0x00FF))-((long)(char)(GETMEMW(PC)))));
			else result = ((DWORD)(BYTE)GETREGB(opcode & 0x00FF))-((DWORD)(BYTE)GETMEMW(PC));
			PC++;
			break;
		 case 0xE2:	// CMP w,=val
			if( signedmath ) result = (DWORD)((long)(((long)(short)GETREGW(opcode & 0x00FF))-((long)(short)(GETMEMW(PC)))));
			else result = (DWORD)(WORD)GETREGW(opcode & 0x00FF)-(DWORD)(WORD)GETMEMW(PC);
			PC++;
			break;
		 case 0xE3:	// CMP d,=val
			if( signedmath ) result = (long)(GETREGD(opcode & 0x00FF)) - (long)(GETMEMD(PC));
			else result = (DWORD)GETREGD(opcode & 0x00FF) - (DWORD)GETMEMD(PC);
			PC += 2;
			break;
		 case 0xE4:	// TST b,=val
			result = (DWORD)(char)(GETREGB(opcode & 0x00FF) & (BYTE)GETMEMW(PC));
			PC++;
			result |= (result<<24);
			break;
		 case 0xE5:	// TST w,=val
			result = (DWORD)(short)(GETREGW(opcode & 0x00FF) & GETMEMW(PC));
			PC++;
			result |= (result<<16);
			break;
		 case 0xE6:	// TST d,=val
			result = GETREGD(opcode & 0x00FF) & GETMEMD(PC);
			PC += 2;
			break;
		 case 0xE7:	// JMP =val
			dw = GETMEMD(PC);
			PC = (WORD *)(pKode + dw);
			break;
		 case 0xE8:	// JE  +offset (0 to 255 WORDS from CP)
			if( !result ) PC += (DWORD)(opcode & 0x01FF);
			break;
		 case 0xE9:	// JE  -offset (-256 to -1 WORDS from CP)
			if( !result ) PC += (DWORD)(opcode & 0x01FF) | 0xFFFFFE00;
			break;
		 case 0xEA:	// JNE +offset
			if( result ) PC += (DWORD)(opcode & 0x01FF);
			break;
		 case 0xEB:	// JNE -offset
			if( result ) PC += (DWORD)(opcode & 0x01FF) | 0xFFFFFE00;
			break;
		 case 0xEC:	// JL  +offset
			if( result & 0x80000000 ) PC += (DWORD)(opcode & 0x01FF);
			break;
		 case 0xED:	// JL  -offset
			if( result & 0x80000000 ) PC += (DWORD)(opcode & 0x01FF) | 0xFFFFFE00;
			break;
		 case 0xEE:	// JLE +offset
			if( !result || (result & 0x80000000) ) PC += (DWORD)(opcode & 0x01FF);
			break;
		 case 0xEF:	// JLE -offset
			if( !result || (result & 0x80000000) ) PC += (DWORD)(opcode & 0x01FF) | 0xFFFFFE00;
			break;
		 case 0xF0:	// JG  +offset
			if( result && !(result & 0x80000000) ) PC += (DWORD)(opcode & 0x01FF);
			break;
		 case 0xF1:	// JG  -offset
			if( result && !(result & 0x80000000) ) PC += (DWORD)(opcode & 0x01FF) | 0xFFFFFE00;
			break;
		 case 0xF2:	// JGE +offset
			if( !(result & 0x80000000) ) PC += (DWORD)(opcode & 0x01FF);
			break;
		 case 0xF3:	// JGE -offset
			if( !(result & 0x80000000) ) PC += (DWORD)(opcode & 0x01FF) | 0xFFFFFE00;
			break;
		 case 0xF4:	// JEV +offset
			if( !(result & 1) ) PC += (DWORD)(opcode & 0x01FF);
			break;
		 case 0xF5:	// JEV -offset
			if( !(result & 1) ) PC += (DWORD)(opcode & 0x01FF) | 0xFFFFFE00;
			break;
		 case 0xF6:	// JOD +offset
			if( result & 1 ) PC += (DWORD)(opcode & 0x01FF);
			break;
		 case 0xF7:	// JOD -offset
			if( result & 1 ) PC += (DWORD)(opcode & 0x01FF) | 0xFFFFFE00;
			break;
		 case 0xF8:	// JMP +offset
			PC += (DWORD)(opcode & 0x01FF);
			break;
		 case 0xF9:	// JMP -offset
			PC += (DWORD)(opcode & 0x01FF) | 0xFFFFFE00;
			break;
		 case 0xFA:	// SysFunc
			SysFunc(pKE, (WORD)(opcode & 0x00FF));
			break;
		 case 0xFB:	// JSR =dval and LJxx =dval and JMP[]
			dw = GETMEMD(PC);
			PC += 2;
			if( GETMEMB(pKode+KHDR_MAJOR_INTERP_REV)<3 ) opcode &= 0xFF00;	// force lower byte to 0 if code assembled for < 3.0 KINT
			switch( opcode & 0x00FF ) {
			 case 0x00:	// JSR =dval
#ifdef GEEK_TRACE2
	if( TBptr2+1 < TBMAX2 ) TraceBuf2[TBptr2++] = (DWORD)((BYTE *)PC-pKode);
#endif
				SETMEMD(pKode + GETREGD(REG_5), (DWORD)((BYTE *)PC - pKode));	// push return address
				SETREGD(REG_5, GETREGD(REG_5)+4);
				PC = (WORD *)(pKode + dw);
				break;
			 case 0x02:	// JMP [reg]
				PC -= 2;
				PC = (WORD *)(pKode + GETMEMD(pKode + GETREGD(GETMEMW(PC))));
				break;
			 case 0x03:	// JMP [reg+off]
				PC -= 2;
				dw2 = GETREGD(GETMEMW(PC));
				PC++;
				PC = (WORD *)(pKode + GETMEMD(pKode + dw2 + GETMEMD(PC)));
				break;
			 case 0x04:	// JMP [off]
				PC -= 2;
				PC = (WORD *)(pKode + GETMEMD(pKode + GETMEMD(PC)));
				break;
			 case 0x05:	// JMP [reg+off+off]
				PC -= 2;
				dw2 = GETREGD(GETMEMW(PC));
				PC++;
				dw2 += GETMEMD(PC);
				PC += 2;
				PC = (WORD *)(pKode + GETMEMD(pKode + dw2 + GETMEMD(PC)));
				break;
			 case 0x06:	// JMP [reg+reg]
				PC -= 2;
				w = GETMEMW(PC);
				PC = (WORD *)(pKode + GETMEMD(pKode + GETREGD(w & 0x00FF) + GETREGD((w>>8)&0x00FF)));
				break;
			 case 0x08:	// LJE
				if( !result ) PC = (WORD *)(pKode + dw);
				break;
			 case 0x09:	// LJNE
				if( result ) PC = (WORD *)(pKode + dw);
				break;
			 case 0x0A:	// LJL
				if( result & 0x80000000 ) PC = (WORD *)(pKode + dw);
				break;
			 case 0x0B:	// LJLE
				if( !result || (result & 0x80000000) ) PC = (WORD *)(pKode + dw);
				break;
			 case 0x0C:	// LJG
				if( result && !(result & 0x80000000) ) PC = (WORD *)(pKode + dw);
				break;
			 case 0x0D:	// LJGE
				if( !(result & 0x80000000) ) PC = (WORD *)(pKode + dw);
				break;
			 case 0x0E:	// LJEV
				if( !(result & 1) ) PC = (WORD *)(pKode + dw);
				break;
			 case 0x0F:	// LJOD
				if( result & 1 ) PC = (WORD *)(pKode + dw);
				break;
			}
			break;
		 case 0xFC:	// JSI =dval
#ifdef GEEK_TRACE2
	if( TBptr2+1 < TBMAX2 ) TraceBuf2[TBptr2++] = (DWORD)((BYTE *)PC-pKode);
#endif
			dw = GETMEMD(pKode + GETMEMD(PC));
			PC += 2;
			SETMEMD(pKode + GETREGD(REG_5), (DWORD)((BYTE *)PC - pKode));	// push return address
			SETREGD(REG_5, GETREGD(REG_5)+4);
			PC = (WORD *)(pKode + dw);
			break;
		 case 0xFD:	// RET
#ifdef GEEK_TRACE2
	if( TBptr2 ) --TBptr2;
#endif
			SETREGD(REG_5, GETREGD(REG_5)-4);	// pop return address
			PC = (WORD *)(pKode + GETMEMD(pKode + GETREGD(REG_5)));
			break;
		 case 0xFE:	// misc
			switch( opcode & 0x00FF ) {
			 case 0x0000:	// BRK
#if KINT_DEBUG
				if( DbgMem!=NULL && KDBG_HERE ) {
					Ksig(KSIG_BRK);
					while( KDBG_ACK!=KSIG_NO_ACK ) ;
				}
#endif
				break;
			 case 0x0001:	// NOP
				break;
			 case 0x0002:	// TERM
				if( pKE==&mainKE ) killpgm |= 1;	// set flag we need to kill the program, but ONLY "main.ke" can do this! (otherwise, just HALT)
// NOTE: case TERM falls through into case HALT
			 case 0x0003:	// HALT
				--PC;	// move PC back to TERM instruction just for safety
				SETREGD(REG_3, (DWORD)((BYTE *)PC - pKode));	// update PC in Kregs
				goto stopint;
			 case 0x0004:	// AFR
				// pu0 R4,R5
				SETMEMD(pKode + GETREGD(REG_5), GETREGD(REG_4));
				SETREGD(REG_5, GETREGD(REG_5)+4);
				// ldr R4,R5
				SETREGD(REG_4, GETREGD(REG_5));
				// add R5,=*PC++
				SETREGD(REG_5, GETREGD(REG_5) + GETMEMW(PC));
				PC++;
				break;
			 case 0x0005:	// DFR
				// ldr R5,R4
				SETREGD(REG_5, GETREGD(REG_4)-4);	// minus the "pop" that's coming up
				// po0 R4,R5
				SETREGD(REG_4, GETMEMD(pKode+GETREGD(REG_5)));
				break;
			 case 0x0006:	// USM
				signedmath = 0;
				break;
			 case 0x0007:	// SM
				signedmath = 1;
				break;
			 case 0x0010:	// LDCM b,b
				w = GETMEMW(PC);
				PC++;
				SETREGB(w & 0x00FF, GETREGB(w & 0x00FF)-1);
				SETREGB((w>>8) & 0x00FF, GETREGB(GETREGB(w & 0x00FF)));
				break;
			 case 0x0011:	// LDCM w,b
				w = GETMEMW(PC);
				PC++;
				SETREGB(w & 0x00FF, GETREGB(w & 0x00FF)-2);
				SETREGW((w>>8) & 0x00FF, GETREGW(GETREGB(w & 0x00FF)));
				break;
			 case 0x0012:	// LDCM d,b
				w = GETMEMW(PC);
				PC++;
				SETREGB(w & 0x00FF, GETREGB(w & 0x00FF)-4);
				SETREGD((w>>8) & 0x00FF, GETREGD(GETREGB(w & 0x00FF)));
				break;
			 case 0x0013:	// LDC b,b
				w = GETMEMW(PC);
				PC++;
				SETREGB((w>>8) & 0x00FF, GETREGB(GETREGB(w & 0x00FF)));
				break;
			 case 0x0014:	// LDC w,b
				w = GETMEMW(PC);
				PC++;
				SETREGW((w>>8) & 0x00FF, GETREGW(GETREGB(w & 0x00FF)));
				break;
			 case 0x0015:	// LDC d,b
				w = GETMEMW(PC);
				PC++;
				SETREGD((w>>8) & 0x00FF, GETREGD(GETREGB(w & 0x00FF)));
				break;
			 case 0x0016:	// LDCP b,b
				w = GETMEMW(PC);
				PC++;
				SETREGB((w>>8) & 0x00FF, GETREGB(GETREGB(w & 0x00FF)));
				SETREGB(w & 0x00FF, GETREGB(w & 0x00FF)+1);
				break;
			 case 0x0017:	// LDCP w,b
				w = GETMEMW(PC);
				PC++;
				SETREGW((w>>8) & 0x00FF, GETREGW(GETREGB(w & 0x00FF)));
				SETREGB(w & 0x00FF, GETREGB(w & 0x00FF)+2);
				break;
			 case 0x0018:	// LDCP d,b
				w = GETMEMW(PC);
				PC++;
				SETREGD((w>>8) & 0x00FF, GETREGD(GETREGB(w & 0x00FF)));
				SETREGB(w & 0x00FF, GETREGB(w & 0x00FF)+4);
				break;
			 case 0x0019:	// STCM b,b
				w = GETMEMW(PC);
				PC++;
				SETREGB(w & 0x00FF, GETREGB(w & 0x00FF)-1);
				SETREGB(GETREGB(w & 0x00FF), GETREGB((w>>8) & 0x00FF));
				break;
			 case 0x001A:	// STCM w,b
				w = GETMEMW(PC);
				PC++;
				SETREGB(w & 0x00FF, GETREGB(w & 0x00FF)-2);
				SETREGW(GETREGB(w & 0x00FF), GETREGW((w>>8) & 0x00FF));
				break;
			 case 0x001B:	// STCM d,b
				w = GETMEMW(PC);
				PC++;
				SETREGB(w & 0x00FF, GETREGB(w & 0x00FF)-4);
				SETREGD(GETREGB(w & 0x00FF), GETREGD((w>>8) & 0x00FF));
				break;
			 case 0x001C:	// STC b,b
				w = GETMEMW(PC);
				PC++;
				SETREGB(GETREGB(w & 0x00FF), GETREGB((w>>8) & 0x00FF));
				break;
			 case 0x001D:	// STC w,b
				w = GETMEMW(PC);
				PC++;
				SETREGW(GETREGB(w & 0x00FF), GETREGW((w>>8) & 0x00FF));
				break;
			 case 0x001E:	// STC d,b
				w = GETMEMW(PC);
				PC++;
				SETREGD(GETREGB(w & 0x00FF), GETREGD((w>>8) & 0x00FF));
				break;
			 case 0x001F:	// STCP b,b
				w = GETMEMW(PC);
				PC++;
				SETREGB(GETREGB(w & 0x00FF), GETREGB((w>>8) & 0x00FF));
				SETREGB(w & 0x00FF, GETREGB(w & 0x00FF)+1);
				break;
			 case 0x0020:	// STCP w,b
				w = GETMEMW(PC);
				PC++;
				SETREGW(GETREGB(w & 0x00FF), GETREGW((w>>8) & 0x00FF));
				SETREGB(w & 0x00FF, GETREGB(w & 0x00FF)+2);
				break;
			 case 0x0021:	// STCP d,b
				w = GETMEMW(PC);
				PC++;
				SETREGD(GETREGB(w & 0x00FF), GETREGD((w>>8) & 0x00FF));
				SETREGB(w & 0x00FF, GETREGB(w & 0x00FF)+4);
				break;



			 case 0x0022:	// PUB imm,-R7
			 case 0x0023:	// PUW imm,-R7
				SETREGD(REG_7, GETREGD(REG_7)-2);
				SETMEMW(pKode + GETREGD(REG_7), GETMEMW(PC));
				PC++;
				break;
			 case 0x0024:	// PUD imm,-R7
				SETREGD(REG_7, GETREGD(REG_7)-4);
				SETMEMD(pKode + GETREGD(REG_7), GETMEMD(PC));
				PC += 2;
				break;
			 case 0x0025:	// PUSH [reg]B,-R7
				SETREGD(REG_7, GETREGD(REG_7)-2);
				SETMEMW(pKode + GETREGD(REG_7), (WORD)(BYTE)GETMEMB(pKode + GETREGD(GETMEMW(PC))));
				PC++;
				break;
			 case 0x0026:	// PUSH [reg+mem/off]B,-R7
				SETREGD(REG_7, GETREGD(REG_7)-2);
				dw2 = GETREGD(GETMEMW(PC));
				PC++;
				SETMEMW(pKode + GETREGD(REG_7), (WORD)GETMEMB(pKode + dw2 + GETMEMD(PC)));
				PC += 2;
				break;
			 case 0x0027:	// PUSH [mem]B,-R7
				SETREGD(REG_7, GETREGD(REG_7)-2);
				SETMEMW(pKode + GETREGD(REG_7), (WORD)GETMEMB(pKode + GETMEMD(PC)));
				PC += 2;
				break;
			 case 0x0028:	// PUSH [reg+mem+off]B,-R7
				SETREGD(REG_7, GETREGD(REG_7)-2);
				dw2 = GETREGD(GETMEMW(PC));
				PC++;
				dw2 += GETMEMD(PC);
				PC += 2;
				SETMEMW(pKode + GETREGD(REG_7), (WORD)GETMEMB(pKode + dw2 + GETMEMD(PC)));
				PC += 2;
				break;
			 case 0x0029:	// PUSH [reg+reg]B,-R7
				SETREGD(REG_7, GETREGD(REG_7)-2);
				w = GETMEMW(PC);
				PC++;
				SETMEMW(pKode + GETREGD(REG_7), (WORD)GETMEMB(pKode + GETREGD(w & 0x00FF) + GETREGD((w>>8)&0x00FF)));
				break;
			 case 0x002A:	// PUSH [reg]W,-R7
				SETREGD(REG_7, GETREGD(REG_7)-2);
				SETMEMW(pKode + GETREGD(REG_7), GETMEMW(pKode + GETREGD(GETMEMW(PC))));
				PC++;
				break;
			 case 0x002B:	// PUSH [reg+mem/off]W,-R7
				SETREGD(REG_7, GETREGD(REG_7)-2);
				dw2 = GETREGD(GETMEMW(PC));
				PC++;
				SETMEMW(pKode + GETREGD(REG_7), GETMEMW(pKode + dw2 + GETMEMD(PC)));
				PC += 2;
				break;
			 case 0x002C:	// PUSH [mem]W,-R7
				SETREGD(REG_7, GETREGD(REG_7)-2);
				SETMEMW(pKode + GETREGD(REG_7), GETMEMW(pKode + GETMEMD(PC)));
				PC += 2;
				break;
			 case 0x002D:	// PUSH [reg+mem+off]W,-R7
				SETREGD(REG_7, GETREGD(REG_7)-2);
				dw2 = GETREGD(GETMEMW(PC));
				PC++;
				dw2 += GETMEMD(PC);
				PC += 2;
				SETMEMW(pKode + GETREGD(REG_7), GETMEMW(pKode + dw2 + GETMEMD(PC)));
				PC += 2;
				break;
			 case 0x002E:	// PUSH [reg+reg]W,-R7
				SETREGD(REG_7, GETREGD(REG_7)-2);
				w = GETMEMW(PC);
				PC++;
				SETMEMW(pKode + GETREGD(REG_7), GETMEMW(pKode + GETREGD(w & 0x00FF) + GETREGD((w>>8)&0x00FF)));
				break;
			 case 0x002F:	// PUSH [reg]D,-R7
				SETREGD(REG_7, GETREGD(REG_7)-4);
				SETMEMD(pKode + GETREGD(REG_7), GETMEMD(pKode + GETREGD(GETMEMW(PC))));
				PC++;
				break;
			 case 0x0030:	// PUSH [reg+mem/off]D,-R7
				SETREGD(REG_7, GETREGD(REG_7)-4);
				dw2 = GETREGD(GETMEMW(PC));
				PC++;
				SETMEMD(pKode + GETREGD(REG_7), GETMEMD(pKode + dw2 + GETMEMD(PC)));
				PC += 2;
				break;
			 case 0x0031:	// PUSH [mem]D,-R7
				SETREGD(REG_7, GETREGD(REG_7)-4);
				SETMEMD(pKode + GETREGD(REG_7), GETMEMD(pKode + GETMEMD(PC)));
				PC += 2;
				break;
			 case 0x0032:	// PUSH [reg+mem+off]D,-R7
				SETREGD(REG_7, GETREGD(REG_7)-4);
				dw2 = GETREGD(GETMEMW(PC));
				PC++;
				dw2 += GETMEMD(PC);
				PC += 2;
				SETMEMD(pKode + GETREGD(REG_7), GETMEMD(pKode + dw2 + GETMEMD(PC)));
				PC += 2;
				break;
			 case 0x0033:	// PUSH [reg+reg]D,-R7
				SETREGD(REG_7, GETREGD(REG_7)-4);
				w = GETMEMW(PC);
				PC++;
				SETMEMD(pKode + GETREGD(REG_7), GETMEMD(pKode + GETREGD(w & 0x00FF) + GETREGD((w>>8)&0x00FF)));
				break;
			 case 0x0034:	// POP [reg]B,R7+
				SETMEMB(pKode + GETREGD(GETMEMW(PC)), GETMEMB(pKode + GETREGD(REG_7)));
				PC++;
				SETREGD(REG_7, GETREGD(REG_7)-2);
				break;
			 case 0x0035:	// POP [reg+mem/off]B,R7+
				dw2 = GETREGD(GETMEMW(PC));
				PC++;
				SETMEMB(pKode + dw2 + GETMEMD(PC), GETMEMB(pKode + GETREGD(REG_7)));
				PC += 2;
				SETREGD(REG_7, GETREGD(REG_7)+2);
				break;
			 case 0x0036:	// POP [mem]B,R7+
				SETMEMB(pKode + GETMEMD(PC), GETMEMB(pKode + GETREGD(REG_7)));
				PC += 2;
				SETREGD(REG_7, GETREGD(REG_7)+2);
				break;
			 case 0x0037:	// POP [reg+mem+off]B,R7+
				dw2 = GETREGD(GETMEMW(PC));
				PC++;
				dw2 += GETMEMD(PC);
				PC += 2;
				SETMEMB(pKode + dw2 + GETMEMD(PC), GETMEMB(pKode + GETREGD(REG_7)));
				PC += 2;
				SETREGD(REG_7, GETREGD(REG_7)+2);
				break;
			 case 0x0038:	// POP [reg+reg]B,R7+
				w = GETMEMW(PC);
				PC++;
				SETMEMB(pKode + GETREGD(w & 0x00FF) + GETREGD((w>>8)&0x00FF), GETMEMB(pKode + GETREGD(REG_7)));
				SETREGD(REG_7, GETREGD(REG_7)+2);
				break;
			 case 0x0039:	// POP [reg]W,R7+
				SETMEMW(pKode + GETREGD(GETMEMW(PC)), GETMEMW(pKode + GETREGD(REG_7)));
				PC++;
				SETREGD(REG_7, GETREGD(REG_7)+2);
				break;
			 case 0x003A:	// POP [reg+mem/off]W,R7+
				dw2 = GETREGD(GETMEMW(PC));
				PC++;
				SETMEMW(pKode + dw2 + GETMEMD(PC), GETMEMW(pKode + GETREGD(REG_7)));
				PC += 2;
				SETREGD(REG_7, GETREGD(REG_7)+2);
				break;
			 case 0x003B:	// POP [mem]W,R7+
				SETMEMW(pKode + GETMEMD(PC), GETMEMW(pKode + GETREGD(REG_7)));
				PC += 2;
				SETREGD(REG_7, GETREGD(REG_7)+2);
				break;
			 case 0x003C:	// POP [reg+mem+off]W,R7+
				dw2 = GETREGD(GETMEMW(PC));
				PC++;
				dw2 += GETMEMD(PC);
				PC += 2;
				SETMEMW(pKode + dw2 + GETMEMD(PC), GETMEMW(pKode + GETREGD(REG_7)));
				PC += 2;
				SETREGD(REG_7, GETREGD(REG_7)+2);
				break;
			 case 0x003D:	// POP [reg+reg]W,R7+
				w = GETMEMW(PC);
				PC++;
				SETMEMW(pKode + GETREGD(w & 0x00FF) + GETREGD((w>>8)&0x00FF), GETMEMW(pKode + GETREGD(REG_7)));
				SETREGD(REG_7, GETREGD(REG_7)+2);
				break;
			 case 0x003E:	// POP [reg]D,R7+
				SETMEMD(pKode + GETREGD(GETMEMW(PC)), GETMEMD(pKode + GETREGD(REG_7)));
				PC++;
				SETREGD(REG_7, GETREGD(REG_7)+4);
				break;
			 case 0x003F:	// POP [reg+mem/off]D,R7+
				dw2 = GETREGD(GETMEMW(PC));
				PC++;
				SETMEMD(pKode + dw2 + GETMEMD(PC), GETMEMD(pKode + GETREGD(REG_7)));
				PC += 2;
				SETREGD(REG_7, GETREGD(REG_7)+4);
				break;
			 case 0x0040:	// POP [mem]D,R7+
				SETMEMD(pKode + GETMEMD(PC), GETMEMD(pKode + GETREGD(REG_7)));
				PC += 2;
				SETREGD(REG_7, GETREGD(REG_7)+4);
				break;
			 case 0x0041:	// POP [reg+mem+off]D,R7+
				dw2 = GETREGD(GETMEMW(PC));
				PC++;
				dw2 += GETMEMD(PC);
				PC += 2;
				SETMEMD(pKode + dw2 + GETMEMD(PC), GETMEMD(pKode + GETREGD(REG_7)));
				PC += 2;
				SETREGD(REG_7, GETREGD(REG_7)+4);
				break;
			 case 0x0042:	// POP [reg+reg]D,R7+
				w = GETMEMW(PC);
				PC++;
				SETMEMD(pKode + GETREGD(w & 0x00FF) + GETREGD((w>>8)&0x00FF), GETMEMD(pKode + GETREGD(REG_7)));
				SETREGD(REG_7, GETREGD(REG_7)+4);
				break;
			 case 0x0043:	// INC [reg]B
				pMem = pKode + GETREGD(GETMEMW(PC));
				SETMEMB(pMem, GETMEMB(pMem)+1);
				PC++;
				break;
			 case 0x0044:	// INC [reg+mem/off]B
				dw2 = GETREGD(GETMEMW(PC));
				PC++;
				pMem = pKode + dw2 + GETMEMD(PC);
				SETMEMB(pMem, GETMEMB(pMem)+1);
				PC += 2;
				break;
			 case 0x0045:	// INC [mem]B
				pMem = pKode + GETMEMD(PC);
				SETMEMB(pMem, GETMEMB(pMem)+1);
				PC += 2;
				break;
			 case 0x0046:	// INC [reg+mem+off]B
				dw2 = GETREGD(GETMEMW(PC));
				PC++;
				dw2 += GETMEMD(PC);
				PC += 2;
				pMem = pKode + dw2 + GETMEMD(PC);
				SETMEMB(pMem, GETMEMB(pMem)+1);
				PC += 2;
				break;
			 case 0x0047:	// INC [reg+reg]B
				w = GETMEMW(PC);
				PC++;
				pMem = pKode + GETREGD(w & 0x00FF) + GETREGD((w>>8)&0x00FF);
				SETMEMB(pMem, GETMEMB(pMem)+1);
				break;
			 case 0x0048:	// INC [reg]W
				pMem = pKode + GETREGD(GETMEMW(PC));
				SETMEMW(pMem, GETMEMW(pMem)+1);
				PC++;
				break;
			 case 0x0049:	// INC [reg+mem/off]W
				dw2 = GETREGD(GETMEMW(PC));
				PC++;
				pMem = pKode + dw2 + GETMEMD(PC);
				SETMEMW(pMem, GETMEMW(pMem)+1);
				PC += 2;
				break;
			 case 0x004A:	// INC [mem]W
				pMem = pKode + GETMEMD(PC);
				SETMEMW(pMem, GETMEMW(pMem)+1);
				PC += 2;
				break;
			 case 0x004B:	// INC [reg+mem+off]W
				dw2 = GETREGD(GETMEMW(PC));
				PC++;
				dw2 += GETMEMD(PC);
				PC += 2;
				pMem = pKode + dw2 + GETMEMD(PC);
				SETMEMW(pMem, GETMEMW(pMem)+1);
				PC += 2;
				break;
			 case 0x004C:	// INC [reg+reg]W
				w = GETMEMW(PC);
				PC++;
				pMem = pKode + GETREGD(w & 0x00FF) + GETREGD((w>>8)&0x00FF);
				SETMEMW(pMem, GETMEMW(pMem)+1);
				break;
			 case 0x004D:	// INC [reg]D
				pMem = pKode + GETREGD(GETMEMW(PC));
				SETMEMD(pMem, GETMEMD(pMem)+1);
				PC++;
				break;
			 case 0x004E:	// INC [reg+mem/off]D
				dw2 = GETREGD(GETMEMW(PC));
				PC++;
				pMem = pKode + dw2 + GETMEMD(PC);
				SETMEMD(pMem, GETMEMD(pMem)+1);
				PC += 2;
				break;
			 case 0x004F:	// INC [mem]D
				pMem = pKode + GETMEMD(PC);
				SETMEMD(pMem, GETMEMD(pMem)+1);
				PC += 2;
				break;
			 case 0x0050:	// INC [reg+mem+off]D
				dw2 = GETREGD(GETMEMW(PC));
				PC++;
				dw2 += GETMEMD(PC);
				PC += 2;
				pMem = pKode + dw2 + GETMEMD(PC);
				SETMEMD(pMem, GETMEMD(pMem)+1);
				PC += 2;
				break;
			 case 0x0051:	// INC [reg+reg]D
				w = GETMEMW(PC);
				PC++;
				pMem = pKode + GETREGD(w & 0x00FF) + GETREGD((w>>8)&0x00FF);
				SETMEMD(pMem, GETMEMD(pMem)+1);
				break;
			 case 0x0052:	// DEC [reg]B
				pMem = pKode + GETREGD(GETMEMW(PC));
				SETMEMB(pMem, GETMEMB(pMem)-1);
				PC++;
				break;
			 case 0x0053:	// DEC [reg+mem/off]B
				dw2 = GETREGD(GETMEMW(PC));
				PC++;
				pMem = pKode + dw2 + GETMEMD(PC);
				SETMEMB(pMem, GETMEMB(pMem)-1);
				PC += 2;
				break;
			 case 0x0054:	// DEC [mem]B
				pMem = pKode + GETMEMD(PC);
				SETMEMB(pMem, GETMEMB(pMem)-1);
				PC += 2;
				break;
			 case 0x0055:	// DEC [reg+mem+off]B
				dw2 = GETREGD(GETMEMW(PC));
				PC++;
				dw2 += GETMEMD(PC);
				PC += 2;
				pMem = pKode + dw2 + GETMEMD(PC);
				SETMEMB(pMem, GETMEMB(pMem)-1);
				PC += 2;
				break;
			 case 0x0056:	// DEC [reg+reg]B
				w = GETMEMW(PC);
				PC++;
				pMem = pKode + GETREGD(w & 0x00FF) + GETREGD((w>>8)&0x00FF);
				SETMEMB(pMem, GETMEMB(pMem)-1);
				break;
			 case 0x0057:	// DEC [reg]W
				pMem = pKode + GETREGD(GETMEMW(PC));
				SETMEMW(pMem, GETMEMW(pMem)-1);
				PC++;
				break;
			 case 0x0058:	// DEC [reg+mem/off]W
				dw2 = GETREGD(GETMEMW(PC));
				PC++;
				pMem = pKode + dw2 + GETMEMD(PC);
				SETMEMW(pMem, GETMEMW(pMem)-1);
				PC += 2;
				break;
			 case 0x0059:	// DEC [mem]W
				pMem = pKode + GETMEMD(PC);
				SETMEMW(pMem, GETMEMW(pMem)-1);
				PC += 2;
				break;
			 case 0x005A:	// DEC [reg+mem+off]W
				dw2 = GETREGD(GETMEMW(PC));
				PC++;
				dw2 += GETMEMD(PC);
				PC += 2;
				pMem = pKode + dw2 + GETMEMD(PC);
				SETMEMW(pMem, GETMEMW(pMem)-1);
				PC += 2;
				break;
			 case 0x005B:	// DEC [reg+reg]W
				w = GETMEMW(PC);
				PC++;
				pMem = pKode + GETREGD(w & 0x00FF) + GETREGD((w>>8)&0x00FF);
				SETMEMW(pMem, GETMEMW(pMem)-1);
				break;
			 case 0x005C:	// DEC [reg]D
				pMem = pKode + GETREGD(GETMEMW(PC));
				SETMEMD(pMem, GETMEMD(pMem)-1);
				PC++;
				break;
			 case 0x005D:	// DEC [reg+mem/off]D
				dw2 = GETREGD(GETMEMW(PC));
				PC++;
				pMem = pKode + dw2 + GETMEMD(PC);
				SETMEMD(pMem, GETMEMD(pMem)-1);
				PC += 2;
				break;
			 case 0x005E:	// DEC [mem]D
				pMem = pKode + GETMEMD(PC);
				SETMEMD(pMem, GETMEMD(pMem)-1);
				PC += 2;
				break;
			 case 0x005F:	// DEC [reg+mem+off]D
				dw2 = GETREGD(GETMEMW(PC));
				PC++;
				dw2 += GETMEMD(PC);
				PC += 2;
				pMem = pKode + dw2 + GETMEMD(PC);
				SETMEMD(pMem, GETMEMD(pMem)-1);
				PC += 2;
				break;
			 case 0x0060:	// DEC [reg+reg]D
				w = GETMEMW(PC);
				PC++;
				pMem = pKode + GETREGD(w & 0x00FF) + GETREGD((w>>8)&0x00FF);
				SETMEMD(pMem, GETMEMD(pMem)-1);
				break;


			 case 0x0061:	// PUSC b,-R7	NOTE: we actually push a WORD to keep the stack WORD aligned
				SETREGD(REG_7, GETREGD(REG_7)-2);
				SETMEMW((WORD *)(pKode + GETREGD(REG_7)), (WORD)(BYTE)GETREGB(GETMEMW(PC++)));
				break;
			 case 0x0062:	// PUSC w,-R7
				SETREGD(REG_7, GETREGD(REG_7)-2);
				SETMEMW((WORD *)(pKode + GETREGD(REG_7)), GETREGW(GETMEMW(PC++)));
				break;
			 case 0x0063:	// PUSC d,-R7
				SETREGD(REG_7, GETREGD(REG_7)-4);
				SETMEMD(pKode + GETREGD(REG_7), GETREGD(GETMEMW(PC++)));
				break;
			 case 0x0064:	// POPC b,R7+	NOTE: we actually pop a WORD, as that's what was pushed
				SETREGB(GETMEMW(PC++), GETMEMB(pKode + GETREGD(REG_7)));
				SETREGD(REG_7, GETREGD(REG_7)+2);
				break;
			 case 0x0065:	// POPC w,R7+
				SETREGW(GETMEMW(PC++), GETMEMW((WORD *)(pKode + GETREGD(REG_7))));
				SETREGD(REG_7, GETREGD(REG_7)+2);
				break;
			 case 0x0066:	// POPC d,R7+
				SETREGD(GETMEMW(PC++), GETMEMD(pKode + GETREGD(REG_7)));
				SETREGD(REG_7, GETREGD(REG_7)+4);
				break;
			 case 0x0067:	// JSRC =dval
#ifdef GEEK_TRACE
	if( TBptr+1 < TBMAX ) TraceBuf[TBptr++] = (DWORD)((BYTE *)PC-pKode);
#endif
				SETREGD(REG_7, GETREGD(REG_7)-4);
				dw = GETMEMD(PC);
				PC += 2;
				SETMEMD(pKode + GETREGD(REG_7), (DWORD)((BYTE *)PC - pKode));	// push return address
				PC = (WORD *)(pKode + dw);
				break;
			 case 0x0068:	// JSIC =dval
#ifdef GEEK_TRACE
	if( TBptr+1 < TBMAX ) TraceBuf[TBptr++] = (DWORD)((BYTE *)PC-pKode);
#endif
				SETREGD(REG_7, GETREGD(REG_7)-4);
				dw = GETMEMD(pKode + GETMEMD(PC));
				PC += 2;
				SETMEMD(pKode + GETREGD(REG_7), (DWORD)((BYTE *)PC - pKode));	// push return address
				PC = (WORD *)(pKode + dw);
				break;
			 case 0x0069:	// JSRC d,=dval
#ifdef GEEK_TRACE
	if( TBptr+1 < TBMAX ) TraceBuf[TBptr++] = (DWORD)((BYTE *)PC-pKode);
#endif
				SETREGD(REG_7, GETREGD(REG_7)-4);
				dw = (DWORD)GETMEMW(PC++);
				dw = GETREGD(dw) + GETMEMD(PC);
				PC += 2;
				SETMEMD(pKode + GETREGD(REG_7), (DWORD)((BYTE *)PC - pKode));	// push return address
				PC = (WORD *)(pKode + dw);
				break;
			 case 0x006A:	// JSIC d,=dval
#ifdef GEEK_TRACE
	if( TBptr+1 < TBMAX ) TraceBuf[TBptr++] = (DWORD)((BYTE *)PC-pKode);
#endif
				SETREGD(REG_7, GETREGD(REG_7)-4);
				dw = (DWORD)GETMEMW(PC++);
				dw = GETMEMD(pKode + (GETREGD(dw) + GETMEMD(PC)));
				PC += 2;
				SETMEMD(pKode + GETREGD(REG_7), (DWORD)((BYTE *)PC - pKode));	// push return address
				PC = (WORD *)(pKode + dw);
				break;
			 case 0x006B:	// RETC
#ifdef GEEK_TRACE
	if( TBptr ) --TBptr;
#endif

				PC = (WORD *)(pKode + GETMEMD(pKode + GETREGD(REG_7)));
				SETREGD(REG_7, GETREGD(REG_7)+4);	// pop return address
				break;
			 case 0x0070:	// FNEG
				w = GETMEMW(PC++);
				dw = GETREGD(w);
				f1 = *((float *)&dw);
				f1 = -f1;
				SETREGD(w, *((DWORD *)&f1));
				break;
			 case 0x0071:	// FADD
				w = GETMEMW(PC++);
				dw = GETREGD(w & 0x00FF);
				f1 = *((float *)&dw);
				dw = GETREGD((w>>8) & 0x00FF);
				f1 += *((float *)&dw);
				SETREGD(w & 0x00FF, *((DWORD *)&f1));
				break;
			 case 0x0072:	// FSUB
				w = GETMEMW(PC++);
				dw = GETREGD(w & 0x00FF);
				f1 = *((float *)&dw);
				dw = GETREGD((w>>8) & 0x00FF);
				f1 -= *((float *)&dw);
				SETREGD(w & 0x00FF, *((DWORD *)&f1));
				break;
			 case 0x0073:	// FMUL
				w = GETMEMW(PC++);
				dw = GETREGD(w & 0x00FF);
				f1 = *((float *)&dw);
				dw = GETREGD((w>>8) & 0x00FF);
				f1 *= *((float *)&dw);
				SETREGD(w & 0x00FF, *((DWORD *)&f1));
				break;
			 case 0x0074:	// FDIV
				w = GETMEMW(PC++);
				dw = GETREGD(w & 0x00FF);
				f1 = *((float *)&dw);
				dw = GETREGD((w>>8) & 0x00FF);
				f1 /= *((float *)&dw);
				SETREGD(w & 0x00FF, *((DWORD *)&f1));
				break;
			 case 0x0075:	// FF2I
				w = GETMEMW(PC++);
				dw = GETREGD(w);
				f1 = *((float *)&dw);
				SETREGD(w, (DWORD)(long)f1);
				break;
			 case 0x0076:	// FI2F
				w = GETMEMW(PC++);
				f1 = (float)(long)GETREGD(w);
				SETREGD(w, *((DWORD *)&f1));
				break;
			 case 0x0077:	// FCMP
				w = GETMEMW(PC++);
				dw = GETREGD(w & 0x00FF);
				f1 = *((float *)&dw);
				dw = GETREGD((w>>8) & 0x00FF);
				f1 -= *((float *)&dw);
				// NOTE: FCMP only sets GT, GE, LT, LE, EQ, NEQ.  "Even/odd" is undefined for FCMP.
				result = (f1<0) ? 0x80000000 : ((f1==0) ? 0 : 1);
				break;


			 case 0x0078:	// PUF [r]
			 case 0x0079:	// PUF [r][off]
			 case 0x007A:	// PUF [mem]
			 case 0x007B:	// PUF [r][mem][off]
			 case 0x007C:	// PUF [r][r]
			 case 0x007D:	// PUF r
			 case 0x007E:	// PUF imm
			 case 0x007F:	// PUF 0
				*(DWORD *)(pKode+20) = *(DWORD *)(pKode+24);
				*(DWORD *)(pKode+24) = *(DWORD *)(pKode+28);
				*(DWORD *)(pKode+28) = *(DWORD *)(pKode+32);
				*(DWORD *)(pKode+32) = *(DWORD *)(pKode+36);
				*(DWORD *)(pKode+36) = *(DWORD *)(pKode+40);
				*(DWORD *)(pKode+40) = *(DWORD *)(pKode+44);
				*(DWORD *)(pKode+44) = *(DWORD *)(pKode+48);
				switch( opcode & 0x00FF ) {
				 case 0x0078:	// PUF [r]
					pMem = pKode + GETREGD(GETMEMW(PC));
					PC++;
					*(DWORD *)(pKode+48) = GETMEMD(pMem);
					break;
				 case 0x0079:	// PUF [r][off]
					dw2 = GETREGD(GETMEMW(PC));
					PC++;
					pMem = pKode + dw2 + GETMEMD(PC);
					PC += 2;
					*(DWORD *)(pKode+48) = GETMEMD(pMem);
					break;
				 case 0x007A:	// PUF [mem]
					pMem = pKode + GETMEMD(PC);
					PC += 2;
					*(DWORD *)(pKode+48) = GETMEMD(pMem);
					break;
				 case 0x007B:	// PUF [r][mem][off]
					dw2 = GETREGD(GETMEMW(PC));
					PC++;
					dw2 += GETMEMD(PC);
					PC += 2;
					pMem = pKode + dw2 + GETMEMD(PC);
					PC += 2;
					*(DWORD *)(pKode+48) = GETMEMD(pMem);
					break;
				 case 0x007C:	// PUF [r][r]
					w = GETMEMW(PC);
					PC++;
					pMem = pKode + GETREGD(w & 0x00FF) + GETREGD((w>>8)&0x00FF);
					*(DWORD *)(pKode+48) = GETMEMD(pMem);
					break;
				 case 0x007D:	// PUF r
					*(DWORD *)(pKode+48) = GETREGD(GETMEMW(PC));
					PC++;
					break;
				 case 0x007E:	// PUF imm
					*(DWORD *)(pKode+48) = GETMEMD(PC);
					PC += 2;
					break;
				 case 0x007F:	// PUF 0
					*(float *)(pKode+48) = 0.0;
					break;
				}
				break;
			 case 0x0080:	// POF [r]
			 case 0x0081:	// POF [r][off]
			 case 0x0082:	// POF [mem]
			 case 0x0083:	// POF [r][mem][off]
			 case 0x0084:	// POF [r][r]
			 case 0x0085:	// POF r
			 case 0x0086:	// POF once (discard)
			 case 0x0087:	// POF twice (discard)
				dw = *(DWORD *)(pKode+48);
				*(DWORD *)(pKode+48) = *(DWORD *)(pKode+44);
				*(DWORD *)(pKode+44) = *(DWORD *)(pKode+40);
				*(DWORD *)(pKode+40) = *(DWORD *)(pKode+36);
				*(DWORD *)(pKode+36) = *(DWORD *)(pKode+32);
				*(DWORD *)(pKode+32) = *(DWORD *)(pKode+28);
				*(DWORD *)(pKode+28) = *(DWORD *)(pKode+24);
				*(DWORD *)(pKode+24) = *(DWORD *)(pKode+20);
				switch( opcode & 0x00FF ) {
				 case 0x0080:	// POF [r]
					pMem = pKode + GETREGD(GETMEMW(PC));
					PC++;
					SETMEMD(pMem, dw);
					break;
				 case 0x0081:	// POF [r][off]
					dw2 = GETREGD(GETMEMW(PC));
					PC++;
					pMem = pKode + dw2 + GETMEMD(PC);
					PC += 2;
					SETMEMD(pMem, dw);
					break;
				 case 0x0082:	// POF [mem]
					pMem = pKode + GETMEMD(PC);
					PC += 2;
					SETMEMD(pMem, dw);
					break;
				 case 0x0083:	// POF [r][mem][off]
					dw2 = GETREGD(GETMEMW(PC));
					PC++;
					dw2 += GETMEMD(PC);
					PC += 2;
					pMem = pKode + dw2 + GETMEMD(PC);
					PC += 2;
					SETMEMD(pMem, dw);
					break;
				 case 0x0084:	// POF [r][r]
					w = GETMEMW(PC);
					PC++;
					pMem = pKode + GETREGD(w & 0x00FF) + GETREGD((w>>8)&0x00FF);
					SETMEMD(pMem, dw);
					break;
				 case 0x0085:	// POF r
					SETREGD(GETMEMW(PC), dw);
					PC++;
					break;
				 case 0x0086:	// POF ONCE (DISCARD)
					break;
				 case 0x0087:	// POF TWICE (DISCARD)
fpop:
					*(DWORD *)(pKode+48) = *(DWORD *)(pKode+44);
					*(DWORD *)(pKode+44) = *(DWORD *)(pKode+40);
					*(DWORD *)(pKode+40) = *(DWORD *)(pKode+36);
					*(DWORD *)(pKode+36) = *(DWORD *)(pKode+32);
					*(DWORD *)(pKode+32) = *(DWORD *)(pKode+28);
					*(DWORD *)(pKode+28) = *(DWORD *)(pKode+24);
					*(DWORD *)(pKode+24) = *(DWORD *)(pKode+20);
					break;
				}
				break;
			 case 0x0088:	// ADF
				*(float *)(pKode+44) += *(float *)(pKode+48);
				goto fpop;
			 case 0x0089:	// SUF
				*(float *)(pKode+44) -= *(float *)(pKode+48);
				goto fpop;
			 case 0x008A:	// MUF
				*(float *)(pKode+44) *= *(float *)(pKode+48);
				goto fpop;
			 case 0x008B:	// DIF
				*(float *)(pKode+44) /= *(float *)(pKode+48);
				goto fpop;
			 case 0x008C:	// CMF
				f1 = *((float *)(pKode+44)) - *((float *)(pKode+48));
				// NOTE: CMF only sets GT, GE, LT, LE, EQ, NEQ.  "Even/odd" is undefined for CMF.
				result = (f1<0) ? 0x80000000 : ((f1==0) ? 0 : 1);
				*(DWORD *)(pKode+48) = *(DWORD *)(pKode+40);	// pop (discard) 2 values
				*(DWORD *)(pKode+44) = *(DWORD *)(pKode+36);
				*(DWORD *)(pKode+40) = *(DWORD *)(pKode+32);
				*(DWORD *)(pKode+36) = *(DWORD *)(pKode+28);
				*(DWORD *)(pKode+32) = *(DWORD *)(pKode+24);
				*(DWORD *)(pKode+28) = *(DWORD *)(pKode+20);
				break;
			 case 0x008D:	// NEF
				*(float *)(pKode+48) = -*(float *)(pKode+48);
				break;
			 case 0x008E:	// I2F
				*(DWORD *)(pKode+20) = *(DWORD *)(pKode+24);
				*(DWORD *)(pKode+24) = *(DWORD *)(pKode+28);
				*(DWORD *)(pKode+28) = *(DWORD *)(pKode+32);
				*(DWORD *)(pKode+32) = *(DWORD *)(pKode+36);
				*(DWORD *)(pKode+36) = *(DWORD *)(pKode+40);
				*(DWORD *)(pKode+40) = *(DWORD *)(pKode+44);
				*(DWORD *)(pKode+44) = *(DWORD *)(pKode+48);
				*(float *)(pKode+48) = (float)(long)GETREGD(GETMEMW(PC++));
				break;
			 case 0x008F:	// F2I
				w = GETMEMW(PC++);
				f1 = *(float *)(pKode+48);
				SETREGD(w, (DWORD)(long)f1);
				goto fpop;





			 case 0x00AA:	// NEG [reg]B
				pMem = pKode + GETREGD(GETMEMW(PC));
				SETMEMB(pMem, -(char)GETMEMB(pMem));
				PC++;
				break;
			 case 0x00AB:	// NEG [reg+mem/off]B
				dw2 = GETREGD(GETMEMW(PC));
				PC++;
				pMem = pKode + dw2 + GETMEMD(PC);
				SETMEMB(pMem, -(char)GETMEMB(pMem));
				PC += 2;
				break;
			 case 0x00AC:	// NEG [mem]B
				pMem = pKode + GETMEMD(PC);
				SETMEMB(pMem, -(char)GETMEMB(pMem));
				PC += 2;
				break;
			 case 0x00AD:	// NEG [reg+mem+off]B
				dw2 = GETREGD(GETMEMW(PC));
				PC++;
				dw2 += GETMEMD(PC);
				PC += 2;
				pMem = pKode + dw2 + GETMEMD(PC);
				SETMEMB(pMem, -(char)GETMEMB(pMem));
				PC += 2;
				break;
			 case 0x00AE:	// NEG [reg+reg]B
				w = GETMEMW(PC);
				PC++;
				pMem = pKode + GETREGD(w & 0x00FF) + GETREGD((w>>8)&0x00FF);
				SETMEMB(pMem, -(char)GETMEMB(pMem));
				break;
			 case 0x00AF:	// NEG [reg]W
				pMem = pKode + GETREGD(GETMEMW(PC));
				SETMEMW(pMem, -(short)GETMEMW(pMem));
				PC++;
				break;
			 case 0x00B0:	// NEG [reg+mem/off]W
				dw2 = GETREGD(GETMEMW(PC));
				PC++;
				pMem = pKode + dw2 + GETMEMD(PC);
				SETMEMW(pMem, -(short)GETMEMW(pMem));
				PC += 2;
				break;
			 case 0x00B1:	// NEG [mem]W
				pMem = pKode + GETMEMD(PC);
				SETMEMW(pMem, -(short)GETMEMW(pMem));
				PC += 2;
				break;
			 case 0x00B2:	// NEG [reg+mem+off]W
				dw2 = GETREGD(GETMEMW(PC));
				PC++;
				dw2 += GETMEMD(PC);
				PC += 2;
				pMem = pKode + dw2 + GETMEMD(PC);
				SETMEMW(pMem, -(short)GETMEMW(pMem));
				PC += 2;
				break;
			 case 0x00B3:	// NEG [reg+reg]W
				w = GETMEMW(PC);
				PC++;
				pMem = pKode + GETREGD(w & 0x00FF) + GETREGD((w>>8)&0x00FF);
				SETMEMW(pMem, -(short)GETMEMW(pMem));
				break;
			 case 0x00B4:	// NEG [reg]D
				pMem = pKode + GETREGD(GETMEMW(PC));
				SETMEMD(pMem, -(long)GETMEMD(pMem));
				PC++;
				break;
			 case 0x00B5:	// NEG [reg+mem/off]D
				dw2 = GETREGD(GETMEMW(PC));
				PC++;
				pMem = pKode + dw2 + GETMEMD(PC);
				SETMEMD(pMem, -(long)GETMEMD(pMem));
				PC += 2;
				break;
			 case 0x00B6:	// NEG [mem]D
				pMem = pKode + GETMEMD(PC);
				SETMEMD(pMem, -(long)GETMEMD(pMem));
				PC += 2;
				break;
			 case 0x00B7:	// NEG [reg+mem+off]D
				dw2 = GETREGD(GETMEMW(PC));
				PC++;
				dw2 += GETMEMD(PC);
				PC += 2;
				pMem = pKode + dw2 + GETMEMD(PC);
				SETMEMD(pMem, -(long)GETMEMD(pMem));
				PC += 2;
				break;
			 case 0x00B8:	// NEG [reg+reg]D
				w = GETMEMW(PC);
				PC++;
				pMem = pKode + GETREGD(w & 0x00FF) + GETREGD((w>>8)&0x00FF);
				SETMEMD(pMem, -(long)GETMEMD(pMem));
				break;
			 case 0x00B9:	// NOT [reg]B
				pMem = pKode + GETREGD(GETMEMW(PC));
				SETMEMB(pMem, ~(char)GETMEMB(pMem));
				PC++;
				break;
			 case 0x00BA:	// NOT [reg+mem/off]B
				dw2 = GETREGD(GETMEMW(PC));
				PC++;
				pMem = pKode + dw2 + GETMEMD(PC);
				SETMEMB(pMem, ~(char)GETMEMB(pMem));
				PC += 2;
				break;
			 case 0x00BB:	// NOT [mem]B
				pMem = pKode + GETMEMD(PC);
				SETMEMB(pMem, ~(char)GETMEMB(pMem));
				PC += 2;
				break;
			 case 0x00BC:	// NOT [reg+mem+off]B
				dw2 = GETREGD(GETMEMW(PC));
				PC++;
				dw2 += GETMEMD(PC);
				PC += 2;
				pMem = pKode + dw2 + GETMEMD(PC);
				SETMEMB(pMem, ~(char)GETMEMB(pMem));
				PC += 2;
				break;
			 case 0x00BD:	// NOT [reg+reg]B
				w = GETMEMW(PC);
				PC++;
				pMem = pKode + GETREGD(w & 0x00FF) + GETREGD((w>>8)&0x00FF);
				SETMEMB(pMem, ~(char)GETMEMB(pMem));
				break;
			 case 0x00BE:	// NOT [reg]W
				pMem = pKode + GETREGD(GETMEMW(PC));
				SETMEMW(pMem, ~(short)GETMEMW(pMem));
				PC++;
				break;
			 case 0x00BF:	// NOT [reg+mem/off]W
				dw2 = GETREGD(GETMEMW(PC));
				PC++;
				pMem = pKode + dw2 + GETMEMD(PC);
				SETMEMW(pMem, ~(short)GETMEMW(pMem));
				PC += 2;
				break;
			 case 0x00C0:	// NOT [mem]W
				pMem = pKode + GETMEMD(PC);
				SETMEMW(pMem, ~(short)GETMEMW(pMem));
				PC += 2;
				break;
			 case 0x00C1:	// NOT [reg+mem+off]W
				dw2 = GETREGD(GETMEMW(PC));
				PC++;
				dw2 += GETMEMD(PC);
				PC += 2;
				pMem = pKode + dw2 + GETMEMD(PC);
				SETMEMW(pMem, ~(short)GETMEMW(pMem));
				PC += 2;
				break;
			 case 0x00C2:	// NOT [reg+reg]W
				w = GETMEMW(PC);
				PC++;
				pMem = pKode + GETREGD(w & 0x00FF) + GETREGD((w>>8)&0x00FF);
				SETMEMW(pMem, ~(short)GETMEMW(pMem));
				break;
			 case 0x00C3:	// NOT [reg]D
				pMem = pKode + GETREGD(GETMEMW(PC));
				SETMEMD(pMem, ~(long)GETMEMD(pMem));
				PC++;
				break;
			 case 0x00C4:	// NOT [reg+mem/off]D
				dw2 = GETREGD(GETMEMW(PC));
				PC++;
				pMem = pKode + dw2 + GETMEMD(PC);
				SETMEMD(pMem, ~(long)GETMEMD(pMem));
				PC += 2;
				break;
			 case 0x00C5:	// NOT [mem]D
				pMem = pKode + GETMEMD(PC);
				SETMEMD(pMem, ~(long)GETMEMD(pMem));
				PC += 2;
				break;
			 case 0x00C6:	// NOT [reg+mem+off]D
				dw2 = GETREGD(GETMEMW(PC));
				PC++;
				dw2 += GETMEMD(PC);
				PC += 2;
				pMem = pKode + dw2 + GETMEMD(PC);
				SETMEMD(pMem, ~(long)GETMEMD(pMem));
				PC += 2;
				break;
			 case 0x00C7:	// NOT [reg+reg]D
				w = GETMEMW(PC);
				PC++;
				pMem = pKode + GETREGD(w & 0x00FF) + GETREGD((w>>8)&0x00FF);
				SETMEMD(pMem, ~(long)GETMEMD(pMem));
				break;

			 case 0x00C8:	// TST [reg]B
				pMem = pKode + GETREGD(GETMEMW(PC));
				PC++;
				result = (DWORD)(char)GETMEMB(pMem);
				result |= (result<<24);
				break;
			 case 0x00C9:	// TST [reg+mem/off]B
				dw2 = GETREGD(GETMEMW(PC));
				PC++;
				pMem = pKode + dw2 + GETMEMD(PC);
				PC += 2;
				result = (DWORD)(char)GETMEMB(pMem);
				result |= (result<<24);
				break;
			 case 0x00CA:	// TST [mem]B
				pMem = pKode + GETMEMD(PC);
				PC += 2;
				result = (DWORD)(char)GETMEMB(pMem);
				result |= (result<<24);
				break;
			 case 0x00CB:	// TST [reg+mem+off]B
				dw2 = GETREGD(GETMEMW(PC));
				PC++;
				dw2 += GETMEMD(PC);
				PC += 2;
				pMem = pKode + dw2 + GETMEMD(PC);
				PC += 2;
				result = (DWORD)(char)GETMEMB(pMem);
				result |= (result<<24);
				break;
			 case 0x00CC:	// TST [reg+reg]B
				w = GETMEMW(PC);
				PC++;
				pMem = pKode + GETREGD(w & 0x00FF) + GETREGD((w>>8)&0x00FF);
				result = (DWORD)(char)GETMEMB(pMem);
				result |= (result<<24);
				break;
			 case 0x00CD:	// TST [reg]W
				pMem = pKode + GETREGD(GETMEMW(PC));
				PC++;
				result = (DWORD)(short)GETMEMW(pMem);
				result |= (result<<16);
				break;
			 case 0x00CE:	// TST [reg+mem/off]W
				dw2 = GETREGD(GETMEMW(PC));
				PC++;
				pMem = pKode + dw2 + GETMEMD(PC);
				PC += 2;
				result = (DWORD)(short)GETMEMW(pMem);
				result |= (result<<16);
				break;
			 case 0x00CF:	// TST [mem]W
				pMem = pKode + GETMEMD(PC);
				PC += 2;
				result = (DWORD)(short)GETMEMW(pMem);
				result |= (result<<16);
				break;
			 case 0x00D0:	// TST [reg+mem+off]W
				dw2 = GETREGD(GETMEMW(PC));
				PC++;
				dw2 += GETMEMD(PC);
				PC += 2;
				pMem = pKode + dw2 + GETMEMD(PC);
				PC += 2;
				result = (DWORD)(short)GETMEMW(pMem);
				result |= (result<<16);
				break;
			 case 0x00D1:	// TST [reg+reg]W
				w = GETMEMW(PC);
				PC++;
				pMem = pKode + GETREGD(w & 0x00FF) + GETREGD((w>>8)&0x00FF);
				result = (DWORD)(short)GETMEMW(pMem);
				result |= (result<<16);
				break;
			 case 0x00D2:	// TST [reg]D
				pMem = pKode + GETREGD(GETMEMW(PC));
				PC++;
				result = GETMEMD(pMem);
				break;
			 case 0x00D3:	// TST [reg+mem/off]D
				dw2 = GETREGD(GETMEMW(PC));
				PC++;
				pMem = pKode + dw2 + GETMEMD(PC);
				PC += 2;
				result = GETMEMD(pMem);
				break;
			 case 0x00D4:	// TST [mem]D
				pMem = pKode + GETMEMD(PC);
				PC += 2;
				result = GETMEMD(pMem);
				break;
			 case 0x00D5:	// TST [reg+mem+off]D
				dw2 = GETREGD(GETMEMW(PC));
				PC++;
				dw2 += GETMEMD(PC);
				PC += 2;
				pMem = pKode + dw2 + GETMEMD(PC);
				PC += 2;
				result = GETMEMD(pMem);
				break;
			 case 0x00D6:	// TST [reg+reg]D
				w = GETMEMW(PC);
				PC++;
				pMem = pKode + GETREGD(w & 0x00FF) + GETREGD((w>>8)&0x00FF);
				result = GETMEMD(pMem);
				break;
			}
			break;
		 case 0xFF:	// unused
			break;
		}
/*
		if( pKode[0x317c6]==0x1b ) {
			if( geekset ) {
				if( pKode[0x317c8]==0x5b ) {
					geekset = geekset;
				}
			} else geekset = 1;
		}
*/
		SETREGD(REG_3, (DWORD)((BYTE *)PC - pKode));	// update PC in Kregs
	}
stopint:
#if KINT_HELP
	F6BREAK = 0;
#endif

//if( GETREGD(REG_7) != entryr7 ) {
// entryr7 = entryr7;
//}
	return GETREGW(REG_39);
}


//*************************************************************
//*************************************************************
// HARDWARE-INDEPENDENT GRAPHICS FUNCTIONS
//*************************************************************
//*************************************************************

int SaveBMB(PBMB bm, char *fname, BYTE dir) {

// NOTE: If porting to non-MS Windows platform, use
//	typedefs for BITMAPFILEHEADER and BITMAPINFO at top of this file
	BITMAPFILEHEADER	bmfh;
	BITMAPINFOHEADER	bmih;
	BYTE				*pB;
	DWORD				hFile;
	int					h, bmw;

	if( -1==(hFile=Kopen(fname, O_WRBINNEW, dir)) ) return FALSE;

	bmw = 4*((3*bm->physW+3)/4);
	bmfh.bfType = 0x4D42;	// 'BM'
	bmfh.bfSize = sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFO)+bmw*bm->physH-4;
	bmfh.bfReserved1 = bmfh.bfReserved2 = 0;
	bmfh.bfOffBits = sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER);

	bmih.biSize = sizeof(BITMAPINFOHEADER);
	bmih.biWidth = bm->physW;
	bmih.biHeight = bm->physH;
	bmih.biPlanes = 1;
	bmih.biBitCount = 24;
	bmih.biCompression = 0;
	bmih.biSizeImage = bmw*bm->physH;
	bmih.biXPelsPerMeter = 2947;
	bmih.biYPelsPerMeter = 2947;
	bmih.biClrUsed = 0;
	bmih.biClrImportant = 0;

	if( sizeof(BITMAPFILEHEADER)!=Kwrite(hFile, (BYTE *)&bmfh, sizeof(BITMAPFILEHEADER)) ) {
		Kclose(hFile);
		return FALSE;
	}
	if( sizeof(BITMAPINFOHEADER)!=Kwrite(hFile, (BYTE *)&bmih, sizeof(BITMAPINFOHEADER)) ) {
		Kclose(hFile);
		return FALSE;
	}
	pB = bm->pBits+(bm->physH-1)*bm->linelen;
	for(h=0; h<bm->physH; h++) {
		if( bm->linelen!=Kwrite(hFile, pB, bm->linelen) ) {
			Kclose(hFile);
			return FALSE;
		}
		pB -= bm->linelen;
	}
	Kclose(hFile);

	return TRUE;
}

//*************************************************************
// Load a MS Windows/OS2 .BMP format file into a device-independent
// memory array.  BMP file can be 8-bit (256 color / palettized)
// or 24-bit (RGB pixel values), but can NOT be compressed, and is
// assumed to be in the "bottom-up" format (bmiHeader.biHeight > 0).
// LoadBMB supports TWO transparency colors.  If first pixel
// coordinates (transX1, transY1) are (-1,-1) then the bitmap has
// no transparency color.  If it's a valid pixel coordinate, then
// transX2 and transY2 can either be (-1,-1) for only ONE transparency
// color OR the coordinates of a second color pixel.
PBMB LoadBMB(char *fname, int transX1, int transY1, int transX2, int transY2, BYTE dir) {

// NOTE: If porting to non-MS Windows platform, use
//	typedefs for BITMAPFILEHEADER and BITMAPINFO at top of this file
	BITMAPFILEHEADER	bmfh;
	BITMAPINFO			*pbmi;
	DWORD				dwInfoSize, dwBitsSize, BytesRead;
	PBMB				pX;
	BYTE				*pB, *pL, *pO, *pA;
	DWORD				hFile;
	int					x, y, w, h, bitcnt, r, g, b;
	BYTE				ci, tR1, tG1, tB1, tR2, tG2, tB2, tran;
	DWORD				linlen;

// first try to load the graphics file, and if that fails,
// then if the file is supposed to be in the GAME folder,
// add .BMP and try to load again.  This allows games to
// place .BMP files in the GAME folder and access them,
// but not have them show up in the users directory listing
// as a BMP file.
	// Open the file: read access, prohibit write access
	hFile = Kopen(fname, O_RDBIN, dir);
	if( -1 == hFile ) {
		if( dir==DIR_GAME ) {
			strcpy((char *)TmpFname, fname);
			strcat((char *)TmpFname, ".BMP");
			hFile = Kopen((char *)TmpFname, O_RDBIN, dir);
			if( -1 == hFile ) return NULL;
		}
	}

	// Read in the BITMAPFILEHEADER
	BytesRead = Kread(hFile, (BYTE *)&bmfh, sizeof(BITMAPFILEHEADER));
	if( BytesRead!=sizeof(BITMAPFILEHEADER) || (bmfh.bfType != *(WORD *)"BM") ) {
		Kclose(hFile);
		return NULL;
	}
	// Allocate memory for the information structure & read it in
	dwInfoSize = bmfh.bfOffBits - sizeof (BITMAPFILEHEADER);

	if( NULL==(pbmi=(struct tagBITMAPINFO *)malloc(dwInfoSize)) ) {
		Kclose(hFile);
		return NULL;
	}
	BytesRead = Kread(hFile, (BYTE *)pbmi, dwInfoSize);
	if( BytesRead != dwInfoSize )	{
		Kclose(hFile);
		free(pbmi);
		return NULL;
	}
	if( pbmi->bmiHeader.biBitCount<8 || pbmi->bmiHeader.biCompression!=0 ) {	// don't handle < 256 color bitmaps OR compressed bitmaps
		Kclose(hFile);
		free(pbmi);
		return NULL;
	}

	// Create the DIB
	if( NULL==(pX=CreateBMB((WORD)(pbmi->bmiHeader.biWidth), (WORD)(pbmi->bmiHeader.biHeight), FALSE)) ) {
		Kclose(hFile);
		free(pbmi);
		return NULL;
	}
	if( transX1>=0 && transX1<(int)pX->physW && transY1>=0 && transY1<(int)pX->physH ) {
		if( NULL==(pX->pAND=(BYTE *)malloc(pX->physW*pX->physH)) ) {
			DeleteBMB(pX);
			Kclose(hFile);
			free(pbmi);
			return NULL;
		}
		tran = 1;	// we're doing a transparent (masked) bitmap
	} else tran = 0;

	// allocate temp storage for .BMP file RGB/palette-index bytes
	dwBitsSize = bmfh.bfSize - bmfh.bfOffBits;
	if( NULL==(pB=(BYTE *)malloc(dwBitsSize)) ) {
		DeleteBMB(pX);
		Kclose(hFile);
		free(pbmi);
		return NULL;
	}

	BytesRead = Kread(hFile, pB, dwBitsSize);
	Kclose(hFile);
	if( BytesRead != dwBitsSize ) {
		free(pB);
		DeleteBMB(pX);
		free(pbmi);
		return NULL;
	}

	w = pbmi->bmiHeader.biWidth;
	h = pbmi->bmiHeader.biHeight;
	bitcnt = pbmi->bmiHeader.biBitCount - 8;	// it's either 8 or 24, so 0 or non-0
	if( pbmi->bmiHeader.biSizeImage==0 ) {
		pbmi->bmiHeader.biSizeImage = 4*((w*(bitcnt?3:1)+3)/4)*h;
	}
	linlen = pbmi->bmiHeader.biSizeImage / h;
	if( tran ) {	// if transparent (masked) bitmap, get transparency color(s)
		if( bitcnt ) {	// 24-bit bitmap
			pL = pB + linlen*(h-transY1-1) + 3*transX1;
			tB1 = *pL++;
			tG1 = *pL++;
			tR1 = *pL++;
			if( transX2>=0 && transX2<(int)pX->physW && transY2>=0 && transY2<(int)pX->physH ) {
				pL = pB + linlen*(h-transY2-1) + 3*transX2;
				tB2 = *pL++;
				tG2 = *pL++;
				tR2 = *pL++;
			} else {
				tB2 = tB1;
				tG2 = tG1;
				tR2 = tR1;
			}
		} else {		// 8-bit bitmap
			pL = pB + linlen*(h-transY1-1) + transX1;
			ci = *pL;
			tR1 = pbmi->bmiColors[ci].rgbRed;
			tG1 = pbmi->bmiColors[ci].rgbGreen;
			tB1 = pbmi->bmiColors[ci].rgbBlue;
			if( transX2>=0 && transX2<(int)pX->physW && transY2>=0 && transY2<(int)pX->physH ) {
				pL = pB + linlen*(h-transY2-1) + transX2;
				ci = *pL;
				tR2 = pbmi->bmiColors[ci].rgbRed;
				tG2 = pbmi->bmiColors[ci].rgbGreen;
				tB2 = pbmi->bmiColors[ci].rgbBlue;
			} else {
				tB2 = tB1;
				tG2 = tG1;
				tR2 = tR1;
			}
		}
	}
	for(y=0; y<h; y++) {
		pL = pB + linlen*(h-y-1);	// common .BMPs are inverted vertically
		pO = pX->pBits + pX->linelen * y;
		if( tran ) pA = pX->pAND + pX->physW * y;
		for(x=0; x<w; x++) {
			if( bitcnt ) {	// 24-bit
				b = *pL++;
				g = *pL++;
				r = *pL++;
			} else {		// 8-bit
				ci = *pL++;
				b = pbmi->bmiColors[ci].rgbBlue;
				g = pbmi->bmiColors[ci].rgbGreen;
				r = pbmi->bmiColors[ci].rgbRed;
			}
			// convert any 0x00818181 (middle-gray) pixels to 0x00808080
			// so that 0x00818181 is guaranteed to never be used in customer-created bitmaps
			if( (tR1!=0x81 || tG1!=0x81 || tB1!=0x81) && r==0x81 && g==0x81 && b==0x81 ) r = g = b = 0x80;

			*pO++ = b;
			*pO++ = g;
			*pO++ = r;
			if( tran ) *pA++ = ((r==tR1 && g==tG1 && b==tB1) || (r==tR2 && g==tG2 && b==tB2)) ? 255 : 0;
		}
	}
	free(pB);
	free(pbmi);

	return pX;
}

//*********************************************************
void CreateMaskBMB(PBMB bm, DWORD trans1, DWORD trans2) {

	BYTE	*pD, *pDST, *pA, tR1, tG1, tB1, tR2, tG2, tB2, r, g, b;
	int		w, h;

	if( bm->pAND==NULL ) {
		if( NULL==(bm->pAND=(BYTE *)malloc(bm->physW*bm->physH)) ) return;
	}
	tR1 = (BYTE)(trans1 & 0x00FF);
	tG1 = (BYTE)((trans1>>8) & 0x00FF);
	tB1 = (BYTE)((trans1>>16) & 0x00FF);
	tR2 = (BYTE)(trans2 & 0x00FF);
	tG2 = (BYTE)((trans2>>8) & 0x00FF);
	tB2 = (BYTE)((trans2>>16) & 0x00FF);
	pD = bm->pBits;
	pA = bm->pAND;
	for(h=bm->physH; h; pD += bm->linelen, --h) {
		pDST = pD;
		for(w=bm->physW; w; --w) {
			b = *pDST++;
			g = *pDST++;
			r = *pDST++;
			*pA++ = ((r==tR1 && g==tG1 && b==tB1) || (r==tR2 && g==tG2 && b==tB2)) ? 255 : 0;
		}
	}
}


//*********************************************************
void SOerr(PBMB kB) {

	char	xxx[32];

	sprintf(xxx, "ERR SO: %d %8.8X", kB->orient, *((DWORD *)(Kregs+3*4)));
	SystemMessage(xxx);
}

//***************************************************************
void SetupBMD(PBMB *bmD) {

	PBMB	bD;

	bD = *bmD;
	if( bD==0 ) bD = kBuf;
	TdBMW = bD->logW;
	TdBMH = bD->logH;
	TdclipX1 = bD->clipX1;
	TdclipY1 = bD->clipY1;
	TdclipX2 = bD->clipX2;
	TdclipY2 = bD->clipY2;
	switch( bD->orient ) {
	 case BMO_NORMAL:
		TdstepX	= 3;
		TdstepY = bD->linelen;
		Tdpbits = bD->pBits;
		break;
	 case BMO_90:
		TdstepX	= -bD->linelen;
		TdstepY = 3;
		Tdpbits = bD->pBits + (TdBMW-1)*bD->linelen;
		break;
	 case BMO_180:
		TdstepX	= -3;
		TdstepY = -bD->linelen;
		Tdpbits = bD->pBits + (TdBMW-1)*3 + (TdBMH-1)*bD->linelen;
		break;
	 case BMO_270:
		TdstepX	= bD->linelen;
		TdstepY = -3;
		Tdpbits = bD->pBits + (TdBMH-1)*3;
		break;
	 case BMO_FNORMAL:
		TdstepX	= -3;
		TdstepY = bD->linelen;
		Tdpbits = bD->pBits + (TdBMW-1)*3;
		break;
	 case BMO_F90:
		TdstepX	= -bD->linelen;
		TdstepY = -3;
		Tdpbits = bD->pBits + (TdBMH-1)*3 + (TdBMW-1)*bD->linelen;
		break;
	 case BMO_F180:
		TdstepX	= 3;
		TdstepY = -bD->linelen;
		Tdpbits = bD->pBits + (TdBMH-1)*bD->linelen;
		break;
	 case BMO_F270:
		TdstepX	= bD->linelen;
		TdstepY = 3;
		Tdpbits = bD->pBits;
		break;
	 default:
		SOerr(bD);
		return;
	}
	*bmD = bD;
}


void SetupBMS(PBMB *bmS) {

	PBMB	bS;

	bS = *bmS;
	if( bS==0 ) bS = kBuf;
	TsBMW = bS->logW;
	TsBMH = bS->logH;
	TsclipX1 = bS->clipX1;
	TsclipY1 = bS->clipY1;
	TsclipX2 = bS->clipX2;
	TsclipY2 = bS->clipY2;
	switch( bS->orient ) {
	 case BMO_NORMAL:
		TsstepX	= 3;
		TsstepY = bS->linelen;
		Tspbits = bS->pBits;
		TastepX	= 1;
		TastepY = bS->physW;
		Tapbits = bS->pAND;
		break;
	 case BMO_180:
		TsstepX	= -3;
		TsstepY = -bS->linelen;
		Tspbits = bS->pBits + (TsBMW-1)*3 + (TsBMH-1)*bS->linelen;
		TastepX	= -1;
		TastepY = -bS->physW;
		Tapbits = bS->pAND + (TsBMW-1) + (TsBMH-1)*bS->physW;
		break;
	 case BMO_90:
		TsstepX	= -bS->linelen;
		TsstepY = 3;
		Tspbits = bS->pBits + (TsBMW-1)*bS->linelen;
		TastepX	= -bS->physW;
		TastepY = 1;
		Tapbits = bS->pAND + (TsBMW-1)*bS->physW;
		break;
	 case BMO_270:
		TsstepX	= bS->linelen;
		TsstepY = -3;
		Tspbits = bS->pBits + (TsBMH-1)*3;
		TastepX	= bS->physW;
		TastepY = -1;
		Tapbits = bS->pAND + (TsBMH-1);
		break;
	 case BMO_FNORMAL:
		TsstepX	= -3;
		TsstepY = bS->linelen;
		Tspbits = bS->pBits + (TsBMW-1)*3;
		TastepX	= -1;
		TastepY = bS->physW;
		Tapbits = bS->pAND + TsBMW-1;
		break;
	 case BMO_F90:
		TsstepX	= -bS->linelen;
		TsstepY = -3;
		Tspbits = bS->pBits + (TsBMH-1)*3 + (TsBMW-1)*bS->linelen;
		TastepX	= bS->physW;
		TastepY = -1;
		Tapbits = bS->pAND + (TsBMH-1) + (TsBMW-1)*bS->physW;
		break;
	 case BMO_F180:
		TsstepX	= 3;
		TsstepY = -bS->linelen;
		Tspbits = bS->pBits + (TsBMH-1)*bS->linelen;
		TastepX	= 1;
		TastepY = -bS->physW;
		Tapbits = bS->pAND + (TsBMH-1)*bS->physW;
		break;
	 case BMO_F270:
		TsstepX	= bS->linelen;
		TsstepY = 3;
		Tspbits = bS->pBits;
		TastepX	= bS->physW;
		TastepY = 1;
		Tapbits = bS->pAND;
		break;
	 default:
		SOerr(bS);
		return;
	}
	*bmS = bS;
}

//**************************************************************************
// Convert bitmap to gray-scale values
// To convert 24-bit RGB values to gray scale, set each of R,G,B to the
// same value, that value being 29.9% of the original Red, 58.7% of the
// original Green, and 11.4% of the original Blue.
// light is from -255 to +255, and is a "percentage" of previous value added to the previous value for each pixel.
void GrayBMB(PBMB bm, int x, int y, int w, int h, int light, int contrast) {

	BYTE	*pD, *pDST, *pA, *pAND;
	int		tw, th, v;

	SetupBMS(&bm);

	if( x<TsclipX1 ) {
		w -= (TsclipX1-x);
		x = TsclipX1;
	}
	if( y<TsclipY1 ) {
		h -= (TsclipY1-y);
		y = TsclipY1;
	}
	if( (x+w)>=TsclipX2 ) w = TsclipX2-x;
	if( (y+h)>=TsclipY2 ) h = TsclipY2-y;
	if( w<=0 || h<=0 ) return;
	pD = Tspbits + TsstepY*y + TsstepX*x;
	if( bm->pAND ) pA = Tapbits + TastepY*y + TastepX*x;
	else pA = NULL;
	for(th=h; th; pD += TsstepY, --th) {
		pDST = pD;
		pAND = pA;
		for(tw=w; tw; pDST += TsstepX, --tw) {
			if( bm->pAND==NULL || (bm->pAND!=NULL && *pAND==0) ) {
				v = ((int)*pDST*114 + (int)*(pDST+1)*587 + (int)*(pDST+2)*299)/1000;
				if( contrast ) {
					v += ((v-128)*contrast)/255;
				}
				if( light ) {
					v += (light*v)/255;
					if( v>255 ) v = 255;
					if( v<0 ) v = 0;
				}
				*pDST = *(pDST+1) = *(pDST+2) = (BYTE)v;
			}
			if( pAND != NULL ) pAND += TastepX;
		}
		if( pA != NULL ) pA += TastepY;
	}
	if( bm==kBuf ) InvalidateScreen(x, y, x+w, y+h);
}

//*****************************************************************************
// hue and sat are values from 0-255, and if >= 0 simply replace the previous values for each pixel.
// light is from -255 to +255, and is a "percentage" of previous value added to the previous value for each pixel.
void ColorizeBMB(PBMB bm, int x, int y, int w, int h, int hue, int sat, int light, int contrast) {

	BYTE	*pD, *pDST, *pA, *pAND;
	int		tw, th, r, g, b;
	int		mx, mn, v, s, hh, rc, gc, bc, f, p, q, t;

	SetupBMS(&bm);

	if( x<TsclipX1 ) {
		w -= (TsclipX1-x);
		x = TsclipX1;
	}
	if( y<TsclipY1 ) {
		h -= (TsclipY1-y);
		y = TsclipY1;
	}
	if( (x+w)>=TsclipX2 ) w = TsclipX2-x;
	if( (y+h)>=TsclipY2 ) h = TsclipY2-y;
	if( w<=0 || h<=0 ) return;
	pD = Tspbits + TsstepY*y + TsstepX*x;
	if( bm->pAND ) pA = Tapbits + TastepY*y + TastepX*x;
	else pA = NULL;
	for(th=h; th; pD += TsstepY, --th) {
		pDST = pD;
		pAND = pA;
		for(tw=w; tw; pDST += TsstepX, --tw) {
			if( bm->pAND==NULL || (bm->pAND!=NULL && *pAND==0) ) {
				r = (int)*(pDST+2);
				g = (int)*(pDST+1);
				b = (int)*pDST;
				mx= (r>=g && r>=b)?r:((g>=r && g>=b)?g:b);
				mn= (r<=g && r<=b)?r:((g<=r && g<=b)?g:b);
				v = mx;	// value
				if( mx ) s = 255*(mx-mn)/mx;	// saturation
				else s = 0;
				if( s==0 ) hh = 0;
				else {
					rc = 255*(mx-r)/(mx-mn);
					gc = 255*(mx-g)/(mx-mn);
					bc = 255*(mx-b)/(mx-mn);

					if( r==mx ) hh = (bc-gc)/6;
					else if( g==mx ) hh = 85+(rc-bc)/6;
					else hh = 171+(gc-rc)/6;
				}
				if( hue>=0 ) hh = hue;
				hh &= 255;

				if( sat>=0 ) s = sat;
				if( s<0 ) s = 0;
				s &= 255;

				if( contrast ) {
					v += ((v-128)*contrast)/255;
				}
				if( light ) {
					v += (light*v)/255;
				}
				if( v>255 ) v = 255;
				else if( v<0 ) v = 0;

				if( s==0 ) {
					*(pDST+2) = *(pDST+1) = *pDST = (BYTE)v;
				} else {
					if( hh>255 ) hh = 0;
					f = 6*(hh % 42);
					hh= hh / 42;
					if( hh>5 || hh<0 ) hh = 0;
					p = (v*(255-s))/255;
					q = (f==0)?v:((v*(255*246-s*f))/(255*246));
					t = (v*(255*246-(s*(246-f))))/(255*266);
					switch( hh ) {
					 case 0:
						*(pDST+2) = (BYTE)v;
						*(pDST+1) = (BYTE)t;
						*(pDST+0) = (BYTE)p;
						break;
					 case 1:
						*(pDST+2) = (BYTE)q;
						*(pDST+1) = (BYTE)v;
						*(pDST+0) = (BYTE)p;
						break;
					 case 2:
						*(pDST+2) = (BYTE)p;
						*(pDST+1) = (BYTE)v;
						*(pDST+0) = (BYTE)t;
						break;
					 case 3:
						*(pDST+2) = (BYTE)p;
						*(pDST+1) = (BYTE)q;
						*(pDST+0) = (BYTE)v;
						break;
					 case 4:
						*(pDST+2) = (BYTE)t;
						*(pDST+1) = (BYTE)p;
						*(pDST+0) = (BYTE)v;
						break;
					 case 5:
						*(pDST+2) = (BYTE)v;
						*(pDST+1) = (BYTE)p;
						*(pDST+0) = (BYTE)q;
						break;
					}
				}
			}
			if( pAND != NULL ) pAND += TastepX;
		}
		if( pA != NULL ) pA += TastepY;
	}
	if( bm==kBuf ) InvalidateScreen(x, y, x+w, y+h);
}

//**************************************************************************
// Block-transfer a region from one bitmap to another.
// If either bitmap handle is NULL, redirect it to the screen bitmap
// In the source (masked) bitmap, the pBits memory block has 3 bytes
// per pixel containing the RGB information to be forced into the
// destination.  If 'masked' is TRUE, then the pAND memory block has 
// 1 byte per pixel that is 255 to leave the destination untouched at 
// that pixel, 0 to force the RGB pBits pixel color.  (If 'masked' is
// FALSE, then the pAND is ignored and all source pixel values are forced
// into the destination.
void BltBMB(PBMB bmD, int xd, int yd, PBMB bmS, int xs, int ys, int ws, int hs, int masked, int invert) {

	BYTE	*pDST, *pSRC, *pAND, *pD, *pS, *pA;
	int		x, y, ix;

	SetupBMD(&bmD);
	SetupBMS(&bmS);
	if( bmS->pAND==NULL ) masked = 0;	// if no mask, don't try to use it

	if( xs < TsclipX1 ) {
		if( (ws -= TsclipX1-xs)<=0 ) return;	// if src is clear off the left of source bitmap
		xd += TsclipX1-xs;
		xs = TsclipX1;
	}
	ix = xs+ws-TsclipX2;
	if( ix>0 ) {
		if( (ws -= ix)<=0 ) return;	// if src is clear off right of src bitmap
	}
	if( xd < TdclipX1 ) {
		if( (ws -= TdclipX1-xd)<=0 ) return;	// if dest is CLEAR off the left of dest bitmap
		xs += TdclipX1-xd;
		xd = TdclipX1;
	}
	ix = xd+ws-TdclipX2;
	if( ix>0 ) {
		if( (ws -= ix)<=0 ) return;	// if dest is clear off right of dst bitmap
	}
	if( ys < TsclipY1 ) {
		if( (hs -= TsclipY1-ys)<=0 ) return;	// if src is clear off top of source bitmap
		yd += TsclipY1-ys;
		ys = TsclipY1;
	}
	ix = ys+hs-TsclipY2;
	if( ix>0 ) {
		if( (hs -= ix)<=0 ) return;	// if src is clear off bottom of src bitmap
	}
	if( yd < TdclipY1 ) {
		if( (hs -= TdclipY1-yd)<=0 ) return;	// if dest is CLEAR off top of dst bitmap
		ys += TdclipY1-yd;
		yd = TdclipY1;
	}
	ix = yd+hs-TdclipY2;
	if( ix>0 ) {
		if( (hs -= ix)<=0 ) return;	// if dst is clear off bottom of dst bitmap
	}
	pS = Tspbits + TsstepY*ys + TsstepX*xs;
	pD = Tdpbits + TdstepY*yd + TdstepX*xd;
	if( AlphaBlendD ) {
		if( masked ) {
			if( invert ) {
				pA = Tapbits + TastepY*ys + TastepX*xs;
				for(y=0; y<hs; y++) {
					pAND = pA;
					pSRC = pS;
					pDST = pD;
					for(x=0; x<ws; x++) {
						if( *pAND == 0 ) {	// if AND mask has a hole, paint the SRC into the DST
							*pDST	  = (BYTE)(((WORD)(*pDST)     * AlphaBlendD + (WORD)(0xFF ^ *pSRC)     * AlphaBlendS + 127)/255);
							*(pDST+1) = (BYTE)(((WORD)(*(pDST+1)) * AlphaBlendD + (WORD)(0xFF ^ *(pSRC+1)) * AlphaBlendS + 127)/255);
							*(pDST+2) = (BYTE)(((WORD)(*(pDST+2)) * AlphaBlendD + (WORD)(0xFF ^ *(pSRC+2)) * AlphaBlendS + 127)/255);
						}
						pSRC += TsstepX;
						pAND += TastepX;
						pDST += TdstepX;		// move dest ptr to next pixel
					}
					pA += TastepY;
					pS += TsstepY;
					pD += TdstepY;
				}
			} else {
				pA = Tapbits + TastepY*ys + TastepX*xs;
				for(y=0; y<hs; y++) {
					pAND = pA;
					pSRC = pS;
					pDST = pD;
					for(x=0; x<ws; x++) {
						if( *pAND == 0 ) {	// if AND mask has a hole, paint the SRC into the DST
							*pDST = (BYTE)(((WORD)(*pDST) * AlphaBlendD + (WORD)(*pSRC) * AlphaBlendS + 127)/255);
							*(pDST+1) = (BYTE)(((WORD)(*(pDST+1)) * AlphaBlendD + (WORD)(*(pSRC+1)) * AlphaBlendS + 127)/255);
							*(pDST+2) = (BYTE)(((WORD)(*(pDST+2)) * AlphaBlendD + (WORD)(*(pSRC+2)) * AlphaBlendS + 127)/255);
						}
						pSRC += TsstepX;
						pAND += TastepX;
						pDST += TdstepX;		// move dest ptr to next pixel
					}
					pA += TastepY;
					pS += TsstepY;
					pD += TdstepY;
				}
			}
		} else {
			if( invert ) {
				for(y=0; y<hs; y++) {
					pSRC = pS;
					pDST = pD;
					for(x=0; x<ws; x++) {
						*pDST	  = (BYTE)(((WORD)(   *pDST)  * AlphaBlendD + (WORD)(0xFF ^ *pSRC)     * AlphaBlendS + 127)/255);
						*(pDST+1) = (BYTE)(((WORD)(*(pDST+1)) * AlphaBlendD + (WORD)(0xFF ^ *(pSRC+1)) * AlphaBlendS + 127)/255);
						*(pDST+2) = (BYTE)(((WORD)(*(pDST+2)) * AlphaBlendD + (WORD)(0xFF ^ *(pSRC+2)) * AlphaBlendS + 127)/255);
						pSRC += TsstepX;
						pDST += TdstepX;
					}
					pS += TsstepY;
					pD += TdstepY;
				}
			} else {
				for(y=0; y<hs; y++) {
					pSRC = pS;
					pDST = pD;
					for(x=0; x<ws; x++) {
						*pDST = (BYTE)(((WORD)(*pDST) * AlphaBlendD + (WORD)(*pSRC) * AlphaBlendS + 127)/255);
						*(pDST+1) = (BYTE)(((WORD)(*(pDST+1)) * AlphaBlendD + (WORD)(*(pSRC+1)) * AlphaBlendS + 127)/255);
						*(pDST+2) = (BYTE)(((WORD)(*(pDST+2)) * AlphaBlendD + (WORD)(*(pSRC+2)) * AlphaBlendS + 127)/255);
						pSRC += TsstepX;
						pDST += TdstepX;
					}
					pS += TsstepY;
					pD += TdstepY;
				}
			}
		}
	} else {
		if( masked ) {
			if( invert ) {
				pA = Tapbits + TastepY*ys + TastepX*xs;
				for(y=0; y<hs; y++) {
					pAND = pA;
					pSRC = pS;
					pDST = pD;
					for(x=0; x<ws; x++) {
						if( *pAND == 0 ) {	// if AND mask has a hole, paint the SRC into the DST
							*pDST	  = 0xFF ^ *pSRC;
							*(pDST+1) = 0xFF ^ *(pSRC+1);
							*(pDST+2) = 0xFF ^ *(pSRC+2);
						}
						pSRC += TsstepX;
						pAND += TastepX;
						pDST += TdstepX;		// move dest ptr to next pixel
					}
					pA += TastepY;
					pS += TsstepY;
					pD += TdstepY;
				}
			} else {
				pA = Tapbits + TastepY*ys + TastepX*xs;
				for(y=0; y<hs; y++) {
					pAND = pA;
					pSRC = pS;
					pDST = pD;
					for(x=0; x<ws; x++) {
						if( *pAND == 0 ) {	// if AND mask has a hole, paint the SRC into the DST
							*pDST = *pSRC;
							*(pDST+1) = *(pSRC+1);
							*(pDST+2) = *(pSRC+2);
						}
						pSRC += TsstepX;
						pAND += TastepX;
						pDST += TdstepX;		// move dest ptr to next pixel
					}
					pA += TastepY;
					pS += TsstepY;
					pD += TdstepY;
				}
			}
		} else {
			if( invert ) {
				for(y=0; y<hs; y++) {
					pSRC = pS;
					pDST = pD;
					for(x=0; x<ws; x++) {
						*pDST	  = 0xFF ^ *pSRC;
						*(pDST+1) = 0xFF ^ *(pSRC+1);
						*(pDST+2) = 0xFF ^ *(pSRC+2);
						pSRC += TsstepX;
						pDST += TdstepX;
					}
					pS += TsstepY;
					pD += TdstepY;
				}
			} else {
				for(y=0; y<hs; y++) {
					pSRC = pS;
					pDST = pD;
					for(x=0; x<ws; x++) {
						*pDST = *pSRC;
						*(pDST+1) = *(pSRC+1);
						*(pDST+2) = *(pSRC+2);
						pSRC += TsstepX;
						pDST += TdstepX;
					}
					pS += TsstepY;
					pD += TdstepY;
				}
			}
		}
	}
	if( bmD==kBuf ) InvalidateScreen(xd, yd, xd+ws, yd+hs);
}

//**************************************************************************
// Stretch or compress the source bitmap into the destination bitmap
// duplicating or removing pixels (pixel-resize)

// A *LOT* of redundant code in StretchBMB, but it's done to keep the
// routine as fast as possible under varying conditions of AlphaBlending or not,
// stretching or compressing or not. At least you shouldn't have to touch
// this code during ports! :-)

void StretchBMB(PBMB bmD, int xd, int yd, int wd, int hd, PBMB bmS, int xs, int ys, int ws, int hs, int masked) {

	BYTE	*pDST, *pSRC, *pAND, *pD, *pS, *pA;
	int		ErrX, ErrY, Ycnt, Xcnt;
	PBMB	bmDO;
	int		Xdest, Ydest;

	if( wd==0 || hd==0 || ws==0 || hs==0 ) return;
	if( wd==ws && hd==hs ) {
		BltBMB(bmD, xd, yd, bmS, xs, ys, ws, hs, masked, FALSE);
		return;
	}
	bmDO = bmD;
	SetupBMD(&bmD);
	if( bmS==0 ) bmS = kBuf;
	SetupBMS(&bmS);
	if( bmS->pAND==NULL ) masked = 0;	// if no mask, don't try to use it

	if( AlphaBlendD ) {
		if( wd>=ws && hd>=hs ) {	// stretch X and stretch Y
			Xdest = xd;
			ErrX  = ws>>1;
			pD = Tdpbits + TdstepY*yd + TdstepX*xd;
			pS = Tspbits + TsstepY*ys + TsstepX*xs;
			pA = Tapbits + TastepY*ys + TastepX*xs;
			for(Xcnt=wd; Xcnt; Xcnt--) {
				if( Xdest>=TdclipX1 && Xdest<TdclipX2 ) {
					Ydest = yd;
					ErrY  = hs>>1;
					pDST  = pD;
					pSRC  = pS;
					pAND  = pA;
					for(Ycnt=hd; Ycnt; Ycnt--) {
						if( Ydest>=TdclipY1 && Ydest<TdclipY2 && (!masked || (masked && *pAND==0)) ) {
							*pDST = (BYTE)(((WORD)(*pDST) * AlphaBlendD + (WORD)(*pSRC) * AlphaBlendS + 127)/255);
							*(pDST+1) = (BYTE)(((WORD)(*(pDST+1)) * AlphaBlendD + (WORD)(*(pSRC+1)) * AlphaBlendS + 127)/255);
							*(pDST+2) = (BYTE)(((WORD)(*(pDST+2)) * AlphaBlendD + (WORD)(*(pSRC+2)) * AlphaBlendS + 127)/255);
						}
						ErrY += hs;
						if( ErrY>hd ) {
							pSRC += TsstepY;
							pAND += TastepY;	// when masking, SRC bitmap is memory, and not rotated.
												// if not masking, pAND is never used, so don't care.
							ErrY -= hd;
						}
						++Ydest;
						pDST += TdstepY;
					}
				}
				ErrX += ws;
				if( ErrX>wd ) {
					pS += TsstepX;
					pA += TastepX;
					ErrX -= wd;
				}
				++Xdest;
				pD += TdstepX;
			}
		} else if( ws>=wd && hs>=hd ) {	// compress X and compress Y
			Xdest = xd;
			ErrX  = ws>>1;
			pD = Tdpbits + TdstepY*yd + TdstepX*xd;
			pS = Tspbits + TsstepY*ys + TsstepX*xs;
			pA = Tapbits + TastepY*ys + TastepX*xs;
			for(Xcnt=ws; Xcnt; Xcnt--) {
				ErrX += wd;
				if( ErrX>ws ) {
					if( Xdest>=TdclipX1 && Xdest<TdclipX2 ) {
						Ydest = yd;
						ErrY  = hs>>1;
						pDST  = pD;
						pSRC  = pS;
						pAND  = pA;
						for(Ycnt=hs; Ycnt; Ycnt--) {
							ErrY += hd;
							if( ErrY>hs ) {
								if( Ydest>=TdclipY1 && Ydest<TdclipY2 && (!masked || (masked && *pAND==0)) ) {
									*pDST = (BYTE)(((WORD)(*pDST) * AlphaBlendD + (WORD)(*pSRC) * AlphaBlendS + 127)/255);
									*(pDST+1) = (BYTE)(((WORD)(*(pDST+1)) * AlphaBlendD + (WORD)(*(pSRC+1)) * AlphaBlendS + 127)/255);
									*(pDST+2) = (BYTE)(((WORD)(*(pDST+2)) * AlphaBlendD + (WORD)(*(pSRC+2)) * AlphaBlendS + 127)/255);
								}
								ErrY -= hs;
								++Ydest;
								pDST += TdstepY;
							}
							pSRC += TsstepY;
							pAND += TastepY;
						}
					}
					ErrX -= ws;
					++Xdest;
					pD += TdstepX;
				}
				pS += TsstepX;
				pA += TastepX;
			}
		} else if( wd>=ws && hd<=hs ) {	// stretch X and compress Y
			Xdest = xd;
			ErrX  = ws>>1;
			pD = Tdpbits + TdstepY*yd + TdstepX*xd;
			pS = Tspbits + TsstepY*ys + TsstepX*xs;
			pA = Tapbits + TastepY*ys + TastepX*xs;
			for(Xcnt=wd; Xcnt; Xcnt--) {
				if( Xdest>=TdclipX1 && Xdest<TdclipX2 ) {
					Ydest = yd;
					ErrY  = hs>>1;
					pDST  = pD;
					pSRC  = pS;
					pAND  = pA;
					for(Ycnt=hs; Ycnt; Ycnt--) {
						ErrY += hd;
						if( ErrY>hs ) {
							if( Ydest>=TdclipY1 && Ydest<TdclipY2 && (!masked || (masked && *pAND==0)) ) {
								*pDST = (BYTE)(((WORD)(*pDST) * AlphaBlendD + (WORD)(*pSRC) * AlphaBlendS + 127)/255);
								*(pDST+1) = (BYTE)(((WORD)(*(pDST+1)) * AlphaBlendD + (WORD)(*(pSRC+1)) * AlphaBlendS + 127)/255);
								*(pDST+2) = (BYTE)(((WORD)(*(pDST+2)) * AlphaBlendD + (WORD)(*(pSRC+2)) * AlphaBlendS + 127)/255);
							}
							ErrY -= hs;
							++Ydest;
							pDST += TdstepY;
						}
						pSRC += TsstepY;
						pAND += TastepY;
					}
				}
				ErrX += ws;
				if( ErrX>wd ) {
					pS += TsstepX;
					pA += TastepX;
					ErrX -= wd;
				}
				++Xdest;
				pD += TdstepX;
			}
		} else if( ws>=wd && hs<=hd ) {	// compress X and stretch Y
			Xdest = xd;
			ErrX  = ws>>1;
			pD = Tdpbits + TdstepY*yd + TdstepX*xd;
			pS = Tspbits + TsstepY*ys + TsstepX*xs;
			pA = Tapbits + TastepY*ys + TastepX*xs;
			for(Xcnt=ws; Xcnt; Xcnt--) {
				ErrX += wd;
				if( ErrX>ws ) {
					if( Xdest>=TdclipX1 && Xdest<TdclipX2 ) {
						Ydest = yd;
						ErrY  = hs>>1;
						pDST  = pD;
						pSRC  = pS;
						pAND  = pA;
						for(Ycnt=hd; Ycnt; Ycnt--) {
							if( Ydest>=TdclipY1 && Ydest<TdclipY2 && (!masked || (masked && *pAND==0)) ) {
								*pDST = (BYTE)(((WORD)(*pDST) * AlphaBlendD + (WORD)(*pSRC) * AlphaBlendS + 127)/255);
								*(pDST+1) = (BYTE)(((WORD)(*(pDST+1)) * AlphaBlendD + (WORD)(*(pSRC+1)) * AlphaBlendS + 127)/255);
								*(pDST+2) = (BYTE)(((WORD)(*(pDST+2)) * AlphaBlendD + (WORD)(*(pSRC+2)) * AlphaBlendS + 127)/255);
							}
							ErrY += hs;
							if( ErrY>hd ) {
								pSRC += TsstepY;
								pA += TastepY;
								ErrY -= hd;
							}
							++Ydest;
							pDST += TdstepY;
						}
					}
					ErrX -= ws;
					++Xdest;
					pD += TdstepX;
				}
				pS += TsstepX;
				pA += TastepX;
			}
		}
	} else {
		if( wd>=ws && hd>=hs ) {	// stretch X and stretch Y
			Xdest = xd;
			ErrX  = ws>>1;
			pD = Tdpbits + TdstepY*yd + TdstepX*xd;
			pS = Tspbits + TsstepY*ys + TsstepX*xs;
			pA = Tapbits + TastepY*ys + TastepX*xs;
			for(Xcnt=wd; Xcnt; Xcnt--) {
				if( Xdest>=TdclipX1 && Xdest<TdclipX2 ) {
					Ydest = yd;
					ErrY  = hs>>1;
					pDST  = pD;
					pSRC  = pS;
					pAND  = pA;
					for(Ycnt=hd; Ycnt; Ycnt--) {
						if( Ydest>=TdclipY1 && Ydest<TdclipY2 && (!masked || (masked && *pAND==0)) ) {
							*pDST = *pSRC;
							*(pDST+1) = *(pSRC+1);
							*(pDST+2) = *(pSRC+2);
						}
						ErrY += hs;
						if( ErrY>hd ) {
							pSRC += TsstepY;
							pAND += TastepY;	// when masking, SRC bitmap is memory, and not rotated.
												// if not masking, pAND is never used, so don't care.
							ErrY -= hd;
						}
						++Ydest;
						pDST += TdstepY;
					}
				}
				ErrX += ws;
				if( ErrX>wd ) {
					pS += TsstepX;
					pA += TastepX;
					ErrX -= wd;
				}
				++Xdest;
				pD += TdstepX;
			}
		} else if( ws>=wd && hs>=hd ) {	// compress X and compress Y
			Xdest = xd;
			ErrX  = ws>>1;
			pD = Tdpbits + TdstepY*yd + TdstepX*xd;
			pS = Tspbits + TsstepY*ys + TsstepX*xs;
			pA = Tapbits + TastepY*ys + TastepX*xs;
			for(Xcnt=ws; Xcnt; Xcnt--) {
				ErrX += wd;
				if( ErrX>ws ) {
					if( Xdest>=TdclipX1 && Xdest<TdclipX2 ) {
						Ydest = yd;
						ErrY  = hs>>1;
						pDST  = pD;
						pSRC  = pS;
						pAND  = pA;
						for(Ycnt=hs; Ycnt; Ycnt--) {
							ErrY += hd;
							if( ErrY>hs ) {
								if( Ydest>=TdclipY1 && Ydest<TdclipY2 && (!masked || (masked && *pAND==0)) ) {
									*pDST = *pSRC;
									*(pDST+1) = *(pSRC+1);
									*(pDST+2) = *(pSRC+2);
								}
								ErrY -= hs;
								++Ydest;
								pDST += TdstepY;
							}
							pSRC += TsstepY;
							pAND += TastepY;
						}
					}
					ErrX -= ws;
					++Xdest;
					pD += TdstepX;
				}
				pS += TsstepX;
				pA += TastepX;
			}
		} else if( wd>=ws && hd<=hs ) {	// stretch X and compress Y
			Xdest = xd;
			ErrX  = ws>>1;
			pD = Tdpbits + TdstepY*yd + TdstepX*xd;
			pS = Tspbits + TsstepY*ys + TsstepX*xs;
			pA = Tapbits + TastepY*ys + TastepX*xs;
			for(Xcnt=wd; Xcnt; Xcnt--) {
				if( Xdest>=TdclipX1 && Xdest<TdclipX2 ) {
					Ydest = yd;
					ErrY  = hs>>1;
					pDST  = pD;
					pSRC  = pS;
					pAND  = pA;
					for(Ycnt=hs; Ycnt; Ycnt--) {
						ErrY += hd;
						if( ErrY>hs ) {
							if( Ydest>=TdclipY1 && Ydest<TdclipY2 && (!masked || (masked && *pAND==0)) ) {
								*pDST = *pSRC;
								*(pDST+1) = *(pSRC+1);
								*(pDST+2) = *(pSRC+2);
							}
							ErrY -= hs;
							++Ydest;
							pDST += TdstepY;
						}
						pSRC += TsstepY;
						pAND += TastepY;
					}
				}
				ErrX += ws;
				if( ErrX>wd ) {
					pS += TsstepX;
					pA += TastepX;
					ErrX -= wd;
				}
				++Xdest;
				pD += TdstepX;
			}
		} else if( ws>=wd && hs<=hd ) {	// compress X and stretch Y
			Xdest = xd;
			ErrX  = ws>>1;
			pD = Tdpbits + TdstepY*yd + TdstepX*xd;
			pS = Tspbits + TsstepY*ys + TsstepX*xs;
			pA = Tapbits + TastepY*ys + TastepX*xs;
			for(Xcnt=ws; Xcnt; Xcnt--) {
				ErrX += wd;
				if( ErrX>ws ) {
					if( Xdest>=TdclipX1 && Xdest<TdclipX2 ) {
						Ydest = yd;
						ErrY  = hs>>1;
						pDST  = pD;
						pSRC  = pS;
						pAND  = pA;
						for(Ycnt=hd; Ycnt; Ycnt--) {
							if( Ydest>=TdclipY1 && Ydest<TdclipY2 && (!masked || (masked && *pAND==0)) ) {
								*pDST = *pSRC;
								*(pDST+1) = *(pSRC+1);
								*(pDST+2) = *(pSRC+2);
							}
							ErrY += hs;
							if( ErrY>hd ) {
								pSRC += TsstepY;
								pA += TastepY;
								ErrY -= hd;
							}
							++Ydest;
							pDST += TdstepY;
						}
					}
					ErrX -= ws;
					++Xdest;
					pD += TdstepX;
				}
				pS += TsstepX;
				pA += TastepX;
			}
		}
	}
	if( bmD==kBuf ) InvalidateScreen(xd, yd, xd+wd, yd+hd);
}

//**************************************************************************
// Compress the source bitmap into the destination bitmap
// averaging source pixels to generate destination pixels (smooth resize)

// NOTE!!!! Does NOT do Alpha-blending or masked bitmaps!!!

#define	TRANPOINT	0.5

void CompressBMB(PBMB bmD, int xd, int yd, int wd, int hd, PBMB bmS, int xs, int ys, int ws, int hs, int masked) {

	BYTE	*pDST, *pSRC, *pD, *pS, *pSS, *pA, *pAND, *pAA;
	int		Ycnt, Xcnt, Ydest, Xdest, srcXcnt, srcYcnt, srcXcnt2, srcYcnt2;
	PBMB	bmDO, hTmpBM;
	double	wPPP, hPPP, wP, hP;	// pixels-per-pixels, ratio of ws/wd and hs/hd
	double	fYfirst, fXfirst, fysoff, fxsoff, r, g, b, d, m, a, s, transd;
	double	fY2, fX2, hMag, wMag;

	if( wd<=0 || hd<=0 || ws<=0 || hs<=0 ) return;
	if( wd<ws && hd>hs ) {
		hTmpBM = CreateBMB((WORD)wd, (WORD)hs, FALSE);
		CompressBMB(hTmpBM, 0, 0, wd, hs, bmS, xs, ys, ws, hs, masked);
		CompressBMB(bmD, xd, yd, wd, hd, hTmpBM, 0, 0, wd, hs, masked);
		DeleteBMB(hTmpBM);
		return;
	} else if( wd>ws && hd<hs ) {
		hTmpBM = CreateBMB((WORD)ws, (WORD)hd, FALSE);
		CompressBMB(hTmpBM, 0, 0, ws, hd, bmS, xs, ys, ws, hs, masked);
		CompressBMB(bmD, xd, yd, wd, hd, hTmpBM, 0, 0, ws, hd, masked);
		DeleteBMB(hTmpBM);
		return;
	}
	if( wd==ws && hd==hs ) {
		BltBMB(bmD, xd, yd, bmS, xs, ys, ws, hs, masked, FALSE);
		return;
	}
	bmDO = bmD;
	SetupBMD(&bmD);
	if( bmS==0 ) bmS = kBuf;
	SetupBMS(&bmS);

	if( bmS->pAND==NULL ) masked = 0;	// if no mask, don't try to use it

// ************* STREEEETTTTTTTTTTTTTCCCCCCCCCCCCHHHHHHHHHHHHHHHH
	if( wd>=ws && hd>=hs ) {	// stretch X and stretch Y
		wPPP = (double)ws/(double)wd;
		hPPP = (double)hs/(double)hd;
// wPPP and hPPP are the fractional width and height (respectively) of a SRC pixel which maps to a full DST pixel
		wMag = (double)wd/(double)ws;
		hMag = (double)hd/(double)hs;
// wMag and hMag are the amounts by which the fractional part of a SRC pixel must be magnified to make a full DST pixel

		pD = Tdpbits + TdstepY*yd + TdstepX*xd;
		pS = Tspbits + TsstepY*ys + TsstepX*xs;
		if( masked ) pA = Tapbits + TastepY*ys + TastepX*xs;
		else pA = NULL;

		hP = 0;
// hP and wP are the Y (height) and X (width) offsets into the current source pixel of the remaining portion of that
//	source pixel that will be used in the next DST pixel(s).  hP and wP are always >=0 and <1.0.

// fYfirst is the portion of the "first" SRC pixel that will be used, multiplied by hMag.
// fY2 is the portion of the "second" SRC pixel (if any) that will be used, multiplied by hMag.
// ditto for fXfirst and fX2.

		for(Ycnt=srcYcnt=0, Ydest=yd; Ycnt<hd; Ycnt++, Ydest++) {
			while( hP>=1.0 ) {
				hP -= 1.0;
				pS += TsstepY;
				if( pA != NULL ) pA += TastepY;
				++srcYcnt;
			}
			fY2 = hP+hPPP-1.0;
			if( fY2<=0 ) {
				fYfirst = hPPP*hMag;
				fY2 = 0;
			} else {
				fYfirst = (hPPP-fY2)*hMag;
				fY2 *= hMag;
			}

			pDST = pD + TdstepY*Ycnt;
			pSRC = pS;
			pAND = pA;

			wP = 0;
			if( Ydest>=TdclipY1 && Ydest<TdclipY2 ) {
				for(Xcnt=srcXcnt=0, Xdest=xd; Xcnt<wd; Xcnt++, Xdest++) {
					while( wP>=1.0 ) {
						wP -= 1.0;
						pSRC += TsstepX;
						if( pAND != NULL ) pAND += TastepX;
						++srcXcnt;
					}
					fX2 = wP+wPPP-1.0;
					if( fX2<=0 ) {
						fXfirst = wPPP*wMag;
						fX2 = 0;
					} else {
						fXfirst = (wPPP-fX2)*wMag;
						fX2 *= wMag;
					}

					if( Xdest>=TdclipX1 && Xdest<TdclipX2 ) {
						if( fY2==0 || (srcYcnt+1)>=hs ) {
				// Y dst is controlled entirely by 1 src row
							if( fX2==0 || (srcXcnt+1)>=ws ) {
						// Y dst is controlled entirely by 1 src col (ie, one src pixel)
								if( !masked || pAND==NULL || (masked && *pAND==0) ) {
									*pDST = *pSRC;
									*(pDST+1) = *(pSRC+1);
									*(pDST+2) = *(pSRC+2);
									if( *pDST==0x81 && *(pDST+1)==0x81 && *(pDST+2)==0x81 ) {	// don't allow stretching to generate mask color
										*pDST = 0x80;
										*(pDST+1) = 0x80;
										*(pDST+2) = 0x80;
									}
								}
							} else {
						// Y dst is controlled by 2 src cols (pixels) on same src row
								if( !masked || pAND==NULL ) {
									*pDST = (BYTE)((double)(*pSRC)*fXfirst + (double)(*(pSRC+TsstepX))*fX2);
									*(pDST+1) = (BYTE)((double)(*(pSRC+1))*fXfirst + (double)(*(pSRC+TsstepX+1))*fX2);
									*(pDST+2) = (BYTE)((double)(*(pSRC+2))*fXfirst + (double)(*(pSRC+TsstepX+2))*fX2);
									if( *pDST==0x81 && *(pDST+1)==0x81 && *(pDST+2)==0x81 ) {	// don't allow stretching to generate mask color
										*pDST = 0x80;
										*(pDST+1) = 0x80;
										*(pDST+2) = 0x80;
									}
								} else if( (*pAND + *(pAND+TastepX))==0 ) {
									r = 1.0;
									s = 0;
									if( srcYcnt ) {
										s += ((*(pAND-TastepY))?(0.5*fXfirst):0);
										s += ((*(pAND-TastepY+TastepX))?(0.5*fX2):0);
										r += 0.5;
									}
									if( srcYcnt<hs-1 ) {
										s += ((*(pAND+TastepY))?(0.5*fXfirst):0);
										s += ((*(pAND+TastepY+TastepX))?(0.5*fX2):0);
										r += 0.5;
									}
									if( s/r<=TRANPOINT ) {
										*pDST = (BYTE)((double)(*pSRC)*fXfirst + (double)(*(pSRC+TsstepX))*fX2);
										*(pDST+1) = (BYTE)((double)(*(pSRC+1))*fXfirst + (double)(*(pSRC+TsstepX+1))*fX2);
										*(pDST+2) = (BYTE)((double)(*(pSRC+2))*fXfirst + (double)(*(pSRC+TsstepX+2))*fX2);
										if( *pDST==0x81 && *(pDST+1)==0x81 && *(pDST+2)==0x81 ) {	// don't allow stretching to generate mask color
											*pDST = 0x80;
											*(pDST+1) = 0x80;
											*(pDST+2) = 0x80;
										}
									}
								} else {
									r = 1.0;
									s = 0;
									if( srcYcnt ) {
										s += ((*(pAND-TastepY))?(0.5*fXfirst):0);
										s += ((*(pAND-TastepY+TastepX))?(0.5*fX2):0);
										r += 0.5;
									}
									if( srcYcnt<hs-1 ) {
										s += ((*(pAND+TastepY))?(0.5*fXfirst):0);
										s += ((*(pAND+TastepY+TastepX))?(0.5*fX2):0);
										r += 0.5;
									}
									s += ((*pAND)?fXfirst:0);
									s += ((*(pAND+TastepX))?fX2:0);
									if( s/r<=TRANPOINT ) {
										if( *pAND==0 ) {
											*pDST = (BYTE)((double)(*pSRC)*fXfirst);
											*(pDST+1) = (BYTE)((double)(*(pSRC+1))*fXfirst);
											*(pDST+2) = (BYTE)((double)(*(pSRC+2))*fXfirst);
											if( *pDST==0x81 && *(pDST+1)==0x81 && *(pDST+2)==0x81 ) {	// don't allow stretching to generate mask color
												*pDST = 0x80;
												*(pDST+1) = 0x80;
												*(pDST+2) = 0x80;
											}
										} else if( *(pAND+TastepX)==0 ) {
											*pDST = (BYTE)((double)(*(pSRC+TsstepX))*fX2);
											*(pDST+1) = (BYTE)((double)(*(pSRC+TsstepX+1))*fX2);
											*(pDST+2) = (BYTE)((double)(*(pSRC+TsstepX+2))*fX2);
											if( *pDST==0x81 && *(pDST+1)==0x81 && *(pDST+2)==0x81 ) {	// don't allow stretching to generate mask color
												*pDST = 0x80;
												*(pDST+1) = 0x80;
												*(pDST+2) = 0x80;
											}
										}
									}
								}
							}
						} else {
					// fY2 != 0
							if( fX2==0 ) {
								if( !masked || pAND==NULL ) {
									*pDST = (BYTE)((double)(*pSRC)*fYfirst + (double)(*(pSRC+TsstepY))*fY2);
									*(pDST+1) = (BYTE)((double)(*(pSRC+1))*fYfirst + (double)(*(pSRC+TsstepY+1))*fY2);
									*(pDST+2) = (BYTE)((double)(*(pSRC+2))*fYfirst + (double)(*(pSRC+TsstepY+2))*fY2);
								} else if( (*pAND + *(pAND+TastepY))==0 ) {
									r = 1.0;
									s = 0;
									if( srcXcnt ) {
										s += ((*(pAND-TastepX))?(0.5*fYfirst):0);
										s += ((*(pAND+TastepY-TastepX))?(0.5*fY2):0);
										r += 0.5;
									}
									if( srcXcnt<ws-1 ) {
										s += ((*(pAND+TastepX))?(0.5*fYfirst):0);
										s += ((*(pAND+TastepY+TastepX))?(0.5*fY2):0);
										r += 0.5;
									}
									if( s/r<=TRANPOINT ) {
										*pDST = (BYTE)((double)(*pSRC)*fYfirst + (double)(*(pSRC+TsstepY))*fY2);
										*(pDST+1) = (BYTE)((double)(*(pSRC+1))*fYfirst + (double)(*(pSRC+TsstepY+1))*fY2);
										*(pDST+2) = (BYTE)((double)(*(pSRC+2))*fYfirst + (double)(*(pSRC+TsstepY+2))*fY2);
										if( *pDST==0x81 && *(pDST+1)==0x81 && *(pDST+2)==0x81 ) {	// don't allow stretching to generate mask color
											*pDST = 0x80;
											*(pDST+1) = 0x80;
											*(pDST+2) = 0x80;
										}
									}
								} else {
									r = 1.0;
									s = 0;
									if( srcXcnt ) {
										s += ((*(pAND-TastepX))?(0.5*fYfirst):0);
										s += ((*(pAND+TastepY-TastepX))?(0.5*fY2):0);
										r += 0.5;
									}
									if( srcXcnt<ws-1 ) {
										s += ((*(pAND+TastepX))?(0.5*fYfirst):0);
										s += ((*(pAND+TastepY+TastepX))?(0.5*fY2):0);
										r += 0.5;
									}
									s += ((*pAND)?fYfirst:0);
									s += ((*(pAND+TastepY))?fY2:0);
									if( s/r<=TRANPOINT ) {
										if( *pAND==0 ) {
											*pDST = (BYTE)((double)(*pSRC)*fYfirst);
											*(pDST+1) = (BYTE)((double)(*(pSRC+1))*fYfirst);
											*(pDST+2) = (BYTE)((double)(*(pSRC+2))*fYfirst);
											if( *pDST==0x81 && *(pDST+1)==0x81 && *(pDST+2)==0x81 ) {	// don't allow stretching to generate mask color
												*pDST = 0x80;
												*(pDST+1) = 0x80;
												*(pDST+2) = 0x80;
											}
										} else if( *(pAND+TastepY)==0 ) {
											*pDST = (BYTE)((double)(*(pSRC+TsstepY))*fY2);
											*(pDST+1) = (BYTE)((double)(*(pSRC+TsstepY+1))*fY2);
											*(pDST+2) = (BYTE)((double)(*(pSRC+TsstepY+2))*fY2);
											if( *pDST==0x81 && *(pDST+1)==0x81 && *(pDST+2)==0x81 ) {	// don't allow stretching to generate mask color
												*pDST = 0x80;
												*(pDST+1) = 0x80;
												*(pDST+2) = 0x80;
											}
										}
									}
								}
							} else {
							// fY2!=0 && fX2!=0
								long	tc;

								r = fXfirst*fYfirst;// top-left piece
								g = fX2*fYfirst;// top-right piece
								b = fXfirst*fY2;// bottom-left piece
								m = fX2*fY2;// bottom-right piece
								s = 0;
								tc= 0;
								if( masked && pAND!=NULL ) {
									if( *pAND ) {
										s += r;
										r = 0;
										++tc;
									}
									if( srcYcnt+1<hs && *(pAND+TastepY) ) {
										s += b;
										b = 0;
										++tc;
									}
									if( srcXcnt+1<ws && *(pAND+TastepX) ) {
										s += g;
										g = 0;
										++tc;
									}
									if( srcXcnt+1<ws && srcYcnt+1<hs && *(pAND+TastepY+TastepX) ) {
										s += m;
										m = 0;
										++tc;
									}
									if( s<=TRANPOINT ) {
										*pDST = (BYTE)((double)(*pSRC)*r + (double)(*(pSRC+TsstepY))*b + (double)(*(pSRC+TsstepX))*g + (double)(*(pSRC+TsstepY+TsstepX))*m);
										*(pDST+1) = (BYTE)((double)(*(pSRC+1))*r + (double)(*(pSRC+TsstepY+1))*b + (double)(*(pSRC+TsstepX+1))*g + (double)(*(pSRC+TsstepY+TsstepX+1))*m);
										*(pDST+2) = (BYTE)((double)(*(pSRC+2))*r + (double)(*(pSRC+TsstepY+2))*b + (double)(*(pSRC+TsstepX+2))*g + (double)(*(pSRC+TsstepY+TsstepX+2))*m);
										if( *pDST==0x81 && *(pDST+1)==0x81 && *(pDST+2)==0x81 ) {	// don't allow stretching to generate mask color
											*pDST = 0x80;
											*(pDST+1) = 0x80;
											*(pDST+2) = 0x80;
										}
									}
								} else {
									*pDST = (BYTE)((double)(*pSRC)*r + (double)(*(pSRC+TsstepY))*b + (double)(*(pSRC+TsstepX))*g + (double)(*(pSRC+TsstepY+TsstepX))*m);
									*(pDST+1) = (BYTE)((double)(*(pSRC+1))*r + (double)(*(pSRC+TsstepY+1))*b + (double)(*(pSRC+TsstepX+1))*g + (double)(*(pSRC+TsstepY+TsstepX+1))*m);
									*(pDST+2) = (BYTE)((double)(*(pSRC+2))*r + (double)(*(pSRC+TsstepY+2))*b + (double)(*(pSRC+TsstepX+2))*g + (double)(*(pSRC+TsstepY+TsstepX+2))*m);
									if( *pDST==0x81 && *(pDST+1)==0x81 && *(pDST+2)==0x81 ) {	// don't allow stretching to generate mask color
										*pDST = 0x80;
										*(pDST+1) = 0x80;
										*(pDST+2) = 0x80;
									}
								}
							}
						}
					}
					wP += wPPP;
					pDST += TdstepX;
				}
			}
			hP += hPPP;
		}
// ************* COMPRESS
	} else if( ws>=wd && hs>=hd ) {	// compress X and compress Y
		wPPP = (double)ws/(double)wd;
		hPPP = (double)hs/(double)hd;

		pD = Tdpbits + TdstepY*yd + TdstepX*xd;
		pS = Tspbits + TsstepY*ys + TsstepX*xs;
		if( masked ) pA = Tapbits + TastepY*ys + TastepX*xs;
		else pA = NULL;

		for(Ycnt=0, Ydest=yd; Ycnt<hd; Ycnt++, Ydest++) {
			pDST = pD + TdstepY*Ycnt;
			if( Ydest>=TdclipY1 && Ydest<TdclipY2 ) {
				srcYcnt = (int)floor((double)(hPPP*(double)Ycnt));
				for(Xcnt=0, Xdest=xd; Xcnt<wd; Xcnt++, Xdest++) {
					fYfirst = 1.0 - modf((hPPP*(double)Ycnt), &fysoff);	// how much of the first Y pixel line to use for this dest pixel
					srcXcnt = (int)floor((double)(wPPP*(double)Xcnt));
					pSRC = pS + TsstepY*srcYcnt + TsstepX*srcXcnt;
					pAND = pA;
					if( pAND != NULL ) pAND += TastepY*srcYcnt + TastepX*srcXcnt;
					a = r = g = b = d = transd = 0.0;
					srcYcnt2 = srcYcnt;
					for(hP=hPPP; hP>0 && srcYcnt2<hs; ) {
						fXfirst = 1.0 - modf((wPPP*(double)Xcnt), &fxsoff);
						pSS = pSRC;
						pAA = pAND;
						srcXcnt2 = srcXcnt;
						for(wP=wPPP; wP>0 && srcXcnt2<ws; ) {
							m = fYfirst * fXfirst;
							if( !masked || pAA==NULL || (masked && *pAA==0) ) {
								b += (double)(*pSS) * m;
								g += (double)(*(pSS+1)) * m;
								r += (double)(*(pSS+2)) * m;

								if( masked && pAA!=NULL ) a += (double)(*pAA) * m;
								d += m;
							} else {
								transd += m;
							}
							pSS += TsstepX;
							if( pAA!=NULL ) pAA += TastepX;
							++srcXcnt2;
							wP -= fXfirst;
							if( wP>=1.0 ) fXfirst = 1.0;
							else fXfirst = wP;
						}
						pSRC += TsstepY;
						if( pAND!=NULL ) pAND += TastepY;
						++srcYcnt2;
						hP -= fYfirst;
						if( hP>=1.0 ) fYfirst = 1.0;
						else fYfirst = hP;
					}
					if( Xdest>=TdclipX1 && Xdest<TdclipX2 ) {
						if( d>transd && (!masked || ((BYTE)(double)(a/d))<128) ) {
							*pDST = (BYTE)(double)(b/d);
							*(pDST+1) = (BYTE)(double)(g/d);
							*(pDST+2) = (BYTE)(double)(r/d);
							if( *pDST==0x81 && *(pDST+1)==0x81 && *(pDST+2)==0x81 ) {	// don't create mask color via stretching
								*pDST = 0x80;
								*(pDST+1) = 0x80;
								*(pDST+2) = 0x80;
							}
						}
					}
					pDST += TdstepX;
				}
			}
		}
	}
	if( bmD==kBuf ) InvalidateScreen(xd, yd, xd+wd, yd+hd);
}

//**************************************************************************
// Plot a point of a specific color in the destination bitmap
void PointBMB(PBMB bmD, int x, int y, DWORD clr) {

	BYTE	*pDST;

	SetupBMD(&bmD);
	if( x<TdclipX1 || x>=TdclipX2 || y<TdclipY1 || y>=TdclipY2 ) return;
	pDST = Tdpbits + TdstepY*y + TdstepX*x;
	if( AlphaBlendD ) {
		*pDST = (BYTE)(((WORD)(*pDST) * AlphaBlendD + (WORD)((clr & 0x0FF0000)>>16) * AlphaBlendS + 127)/255);
		*(pDST+1) = (BYTE)(((WORD)(*(pDST+1)) * AlphaBlendD + (WORD)((clr & 0x0FF00)>>8) * AlphaBlendS + 127)/255);
		*(pDST+2) = (BYTE)(((WORD)(*(pDST+2)) * AlphaBlendD + (WORD)(clr & 0x0FF) * AlphaBlendS + 127)/255);
	} else {
		*pDST++ = (BYTE)((clr & 0x0FF0000)>>16);
		*pDST++ = (BYTE)((clr & 0x0FF00)>>8);
		*pDST++ = (BYTE)(clr & 0x0FF);
	}
	if( bmD==kBuf ) InvalidateScreen(x, y, x+1, y+1);
}

//*****************************************************
// Get the width, cell-height, and ascending (non-descending) height
// of a specified string, using the currently selected font.
void TextSizeBMB(BYTE *str, WORD *w, WORD *h, WORD *a) {

	WORD	*pW;
	UNALIGNED DWORD	*pD;
	BYTE	c, i;

	pW = (WORD *)(pFontData + pFontTable[CurFont]);
	*h = *pW++;	// height of cells
	*a = *pW++;	// AscendingHeight of cells
	i  = (BYTE)(*pW++) & 4;	// italic?
	pD = (DWORD *)pW;	// pD is now a pointer to the table of character pointers
	*w  = 0;
	while( *str ) {
		c = *str++;
		if( !(c & 0x60) ) c = 127;	// map non-printing CTL characters into 127
		c -= (c & 0x80)?64:32;	// map characters 32-127 and 160-255 into 0-191
		if( !i || (i && *str) ) *w += (WORD)(*(pFontData + pD[c] + 3));	// character width on non-italic and italic-non-last characters
		else *w += (WORD)(*(pFontData + pD[c]));	// cell width on italic-last characters
	}
}

//**************************************************************************
// Output text to the destination bitmap using the currently selected font.
void TextBMB(PBMB bmD, int x, int y, BYTE *str) {

	UNALIGNED BYTE	*pDST, *pC, *pCsx;
	BYTE	r, g, b, bbits;
	UNALIGNED DWORD	*pF, dwbits;
	WORD	wbits, bw, bh, ba;
	int		sx, ey, cellH, cw, ch, c, toppad, cwtmp, cwmax, charw, tx, ty;
	PBMB	bmDO;

	bmDO = bmD;
	SetupBMD(&bmD);
	if( CurBackColor>=0 ) {
		TextSizeBMB(str, &bw, &bh, &ba);
		if( bw && bh ) {
			sx = Kmax(x, TdclipX1);	// clip background rectangle
			cw = Kmin(x+bw, TdclipX2);
			ch = Kmax(y, TdclipY1);
			ey = Kmin(y+bh, TdclipY2);
			if( sx<cw && ch<ey ) RectBMB(bmDO, sx, ch, cw, ey, CurBackColor, -1);
		}
	}
	sx = x;	// save starting x for possible InvalidateScreen()
	pF = (DWORD *)(pFontData + pFontTable[CurFont]);
	cellH = *((WORD *)pF);	// height of cells
	ey = y+cellH;
	pF = (DWORD *)(((BYTE *)pF)+6);	// pF is now a pointer to the table of character pointers

	r = (BYTE)(CurTextColor & 0x000000FF);
	g = (BYTE)((CurTextColor & 0x0000FF00) >> 8);
	b = (BYTE)((CurTextColor & 0x00FF0000) >> 16);
	for(; *str; x+=charw) {	// charw gets set a few lines further down
		pCsx = Tdpbits + TdstepY*y + TdstepX*x;
		c = (int)(short)(WORD)(*str++);
		if( !(c & 0x60) ) c = 127;	// map non-printing CTL characters into 127
		c -= (c & 0x80)?64:32;	// map characters 32-127 and 160-255 into 0-191
		pC = pFontData + pF[c];	// pointer to font data for this character
		cw = (int)(*pC++);		// # of non-space columns of pixels in char
		toppad = (int)(*pC++);	// # of blank lines of pixels at top of char 
		ch = (int)(*pC++);		// # of non-blank lines of pixel data
		charw = (int)(*pC++);	// # of non-space AND space columns of pixels in char

		pDST = (pCsx += TdstepY*toppad);

		if( x>=TdclipX1 && (x+charw)<=TdclipX2 && y>=TdclipY1 && ey<=TdclipY2 ) {	// character is NOT clipped
			if( AlphaBlendD ) {
				while( ch-- ) {
					cwtmp = cw;
					while( cwtmp>24 ) {
						cwmax = Kmin(32, cwtmp);
						cwtmp -= cwmax;
						dwbits = *((UNALIGNED DWORD *)pC);
						pC += 4;
						while( cwmax-- ) {
							if( dwbits & 1 ) {
								*pDST = (BYTE)(((WORD)(*pDST) * AlphaBlendD + (WORD)b * AlphaBlendS + 127)/255);
								*(pDST+1) = (BYTE)(((WORD)(*(pDST+1)) * AlphaBlendD + (WORD)g * AlphaBlendS + 127)/255);
								*(pDST+2) = (BYTE)(((WORD)(*(pDST+2)) * AlphaBlendD + (WORD)r * AlphaBlendS + 127)/255);
							}
							pDST += TdstepX;
							dwbits >>= 1;
						}
					}
					while( cwtmp>8 ) {
						cwmax = Kmin(16, cwtmp);
						cwtmp -= cwmax;
						wbits = *((UNALIGNED WORD *)pC);
						pC += 2;
						while( cwmax-- ) {
							if( wbits & 1 ) {
								*pDST = (BYTE)(((WORD)(*pDST) * AlphaBlendD + (WORD)b * AlphaBlendS + 127)/255);
								*(pDST+1) = (BYTE)(((WORD)(*(pDST+1)) * AlphaBlendD + (WORD)g * AlphaBlendS + 127)/255);
								*(pDST+2) = (BYTE)(((WORD)(*(pDST+2)) * AlphaBlendD + (WORD)r * AlphaBlendS + 127)/255);
							}
							pDST += TdstepX;
							wbits >>= 1;
						}
					}
					if( cwtmp ) {
						bbits = *pC++;
						while( cwtmp-- ) {
							if( bbits & 1 ) {
								*pDST = (BYTE)(((WORD)(*pDST) * AlphaBlendD + (WORD)b * AlphaBlendS + 127)/255);
								*(pDST+1) = (BYTE)(((WORD)(*(pDST+1)) * AlphaBlendD + (WORD)g * AlphaBlendS + 127)/255);
								*(pDST+2) = (BYTE)(((WORD)(*(pDST+2)) * AlphaBlendD + (WORD)r * AlphaBlendS + 127)/255);
							}
							pDST += TdstepX;
							bbits >>= 1;
						}
					}
					pDST = (pCsx += TdstepY);
				}
			} else {
				while( ch-- ) {
					cwtmp = cw;
					while( cwtmp>24 ) {
						cwmax = Kmin(32, cwtmp);
						cwtmp -= cwmax;
						dwbits = *((UNALIGNED DWORD *)pC);
						pC += 4;
						while( cwmax-- ) {
							if( dwbits & 1 ) {
								*pDST = b;
								*(pDST+1) = g;
								*(pDST+2) = r;
							}
							pDST += TdstepX;
							dwbits >>= 1;
						}
					}
					while( cwtmp>8 ) {
						cwmax = Kmin(16, cwtmp);
						cwtmp -= cwmax;
						wbits = *((UNALIGNED WORD *)pC);
						pC += 2;
						while( cwmax-- ) {
							if( wbits & 1 ) {
								*pDST = b;
								*(pDST+1) = g;
								*(pDST+2) = r;
							}
							pDST += TdstepX;
							wbits >>= 1;
						}
					}
					if( cwtmp ) {
						bbits = *pC++;
						while( cwtmp-- ) {
							if( bbits & 1 ) {
								*pDST = b;
								*(pDST+1) = g;
								*(pDST+2) = r;
							}
							pDST += TdstepX;
							bbits >>= 1;
						}
					}
					pDST = (pCsx += TdstepY);
				}
			}
		} else { // character is at least partially clipped
			if( (x+charw)<=TdclipX1 || x>=TdclipX2 || ey<=TdclipY1 || y>=TdclipY2 ) continue;	// character is completely clipped
			ty = y+toppad;
			if( AlphaBlendD ) {
				while( ty<TdclipY2 && ch-- ) {
					if( ty>=TdclipY1 ) {
						tx = x;
						cwtmp = cw;
						while( cwtmp>24 ) {
							cwmax = Kmin(32, cwtmp);
							cwtmp -= cwmax;
							dwbits = *((UNALIGNED DWORD *)pC);
							pC += 4;
							while( cwmax-- ) {
								if( (dwbits & 1) && tx>=TdclipX1 && tx<TdclipX2 ) {
									*pDST = (BYTE)(((WORD)(*pDST) * AlphaBlendD + (WORD)b * AlphaBlendS + 127)/255);
									*(pDST+1) = (BYTE)(((WORD)(*(pDST+1)) * AlphaBlendD + (WORD)g * AlphaBlendS + 127)/255);
									*(pDST+2) = (BYTE)(((WORD)(*(pDST+2)) * AlphaBlendD + (WORD)r * AlphaBlendS + 127)/255);
								}
								++tx;
								pDST += TdstepX;
								dwbits >>= 1;
							}
						}
						while( cwtmp>8 ) {
							cwmax = Kmin(16, cwtmp);
							cwtmp -= cwmax;
							wbits = *((UNALIGNED WORD *)pC);
							pC += 2;
							while( cwmax-- ) {
								if( (wbits & 1) && tx>=TdclipX1 && tx<TdclipX2 ) {
									*pDST = (BYTE)(((WORD)(*pDST) * AlphaBlendD + (WORD)b * AlphaBlendS + 127)/255);
									*(pDST+1) = (BYTE)(((WORD)(*(pDST+1)) * AlphaBlendD + (WORD)g * AlphaBlendS + 127)/255);
									*(pDST+2) = (BYTE)(((WORD)(*(pDST+2)) * AlphaBlendD + (WORD)r * AlphaBlendS + 127)/255);
								}
								++tx;
								pDST += TdstepX;
								wbits >>= 1;
							}
						}
						if( cwtmp ) {
							bbits = *pC++;
							while( cwtmp-- ) {
								if( (bbits & 1) && tx>=TdclipX1 && tx<TdclipX2 ) {
									*pDST = (BYTE)(((WORD)(*pDST) * AlphaBlendD + (WORD)b * AlphaBlendS + 127)/255);
									*(pDST+1) = (BYTE)(((WORD)(*(pDST+1)) * AlphaBlendD + (WORD)g * AlphaBlendS + 127)/255);
									*(pDST+2) = (BYTE)(((WORD)(*(pDST+2)) * AlphaBlendD + (WORD)r * AlphaBlendS + 127)/255);
								}
								++tx;
								pDST += TdstepX;
								bbits >>= 1;
							}
						}
					} else pC += (cw+7)/8;
					++ty;
					pDST = (pCsx += TdstepY);
				}
			} else {
				while( ty<TdclipY2 && ch-- ) {
					if( ty>=TdclipY1 ) {
						tx = x;
						cwtmp = cw;
						while( cwtmp>24 ) {
							cwmax = Kmin(32, cwtmp);
							cwtmp -= cwmax;
							dwbits = *((UNALIGNED DWORD *)pC);
							pC += 4;
							while( cwmax-- ) {
								if( (dwbits & 1) && tx>=TdclipX1 && tx<TdclipX2 ) {
									*pDST = b;
									*(pDST+1) = g;
									*(pDST+2) = r;
								}
								++tx;
								pDST += TdstepX;
								dwbits >>= 1;
							}
						}
						while( cwtmp>8 ) {
							cwmax = Kmin(16, cwtmp);
							cwtmp -= cwmax;
							wbits = *((UNALIGNED WORD *)pC);
							pC += 2;
							while( cwmax-- ) {
								if( (wbits & 1) && tx>=TdclipX1 && tx<TdclipX2 ) {
									*pDST = b;
									*(pDST+1) = g;
									*(pDST+2) = r;
								}
								++tx;
								pDST += TdstepX;
								wbits >>= 1;
							}
						}
						if( cwtmp ) {
							bbits = *pC++;
							while( cwtmp-- ) {
								if( (bbits & 1) && tx>=TdclipX1 && tx<TdclipX2 ) {
									*pDST = b;
									*(pDST+1) = g;
									*(pDST+2) = r;
								}
								++tx;
								pDST += TdstepX;
								bbits >>= 1;
							}
						}
					} else pC += (cw+7)/8;
					++ty;
					pDST = (pCsx += TdstepY);
				}
			}
		}
	}
	if( bmD==kBuf ) InvalidateScreen(sx, y, x, ey);
}

//**************************************************************************
// Draw a line between two points on the destination bitmap
void LineBMB(PBMB bmD, int x1, int y1, int x2, int y2, DWORD clr) {

	BYTE	*pDST, r, g, b;
	int		x, y, d, w, s;
	PBMB	bmDO;

	bmDO = bmD;
	SetupBMD(&bmD);
	r = (BYTE)(clr & 0x000000FF);
	g = (BYTE)((clr & 0x0000FF00) >> 8);
	b = (BYTE)((clr & 0x00FF0000) >> 16);
	if( x1==x2 ) {	// VERTICAL line
		if( x1<TdclipX1 || x1>=TdclipX2 ) return;
	// if end-points are same, just plot single point
		if( y1<y2 ) {
			d = 1;
			if( y1>=TdclipY2 || y2<TdclipY1 ) return;
			if( y1<TdclipY1 ) y1 = TdclipY1;
			if( y2>=TdclipY2 ) y2 = TdclipY2-1;
			w = y2-y1+1;
			s = TdstepY;
		} else if( y1>y2 ) {
			d = -1;
			if( y2>=TdclipY2 || y1<TdclipY1 ) return;
			if( y2<TdclipY1 ) y2 = TdclipY1;
			if( y1>=TdclipY2 ) y1 = TdclipY2-1;
			w = y1-y2+1;
			s = -TdstepY;
		} else w = 0;
		if( w==0 ) {
			PointBMB(bmDO, x1, y1, clr);
			return;
		}
		pDST = Tdpbits + TdstepY*y1 + TdstepX*x1;
		if( AlphaBlendD ) {
			for(y=y1; w--; y += d) {
				if( LineStyle & 0x80000000 ) {
					*pDST = (BYTE)(((WORD)(*pDST) * AlphaBlendD + (WORD)b * AlphaBlendS + 127)/255);
					*(pDST+1) = (BYTE)(((WORD)(*(pDST+1)) * AlphaBlendD + (WORD)g * AlphaBlendS + 127)/255);
					*(pDST+2) = (BYTE)(((WORD)(*(pDST+2)) * AlphaBlendD + (WORD)r * AlphaBlendS + 127)/255);
					LineStyle = (LineStyle<<1) | 1;
				} else LineStyle <<= 1;
				pDST += s;
			}
		} else {
			for(y=y1; w--; y += d) {
				if( LineStyle & 0x80000000 ) {
					*pDST = b;
					*(pDST+1) = g;
					*(pDST+2) = r;
					LineStyle = (LineStyle<<1) | 1;
				} else LineStyle <<= 1;
				pDST += s;
			}
		}
		++x2;	// for invalidate
	} else if( y1==y2 ) {	// HORIZONTAL line
		if( y1<TdclipY1 || y1>=TdclipY2 ) return;
	// if end-points are same, just plot single point
		if( x1<x2 ) {
			d = 1;
			if( x1>=TdclipX2 || x2<TdclipX1 ) return;
			if( x1<TdclipX1 ) x1 = TdclipX1;
			if( x2>=TdclipX2 ) x2 = TdclipX2-1;
			w = x2-x1+1;
			s = TdstepX;
		} else if( x1>x2 ) {
			d = -1;
			if( x2>=TdclipX2 || x1<TdclipX1 ) return;
			if( x2<TdclipX1 ) x2 = TdclipX1;
			if( x1>=TdclipX2 ) x1 = TdclipX2-1;
			w = x1-x2+1;
			s = -TdstepX;
		} else w = 0;
		if( w==0 ) {
			PointBMB(bmDO, x1, y1, clr);
			return;
		}
		pDST = Tdpbits + TdstepY*y1 + TdstepX*x1;
		if( AlphaBlendD ) {
			for(x=x1; w--; x += d) {
				if( LineStyle & 0x80000000 ) {
					*pDST = (BYTE)(((WORD)(*pDST) * AlphaBlendD + (WORD)b * AlphaBlendS + 127)/255);
					*(pDST+1) = (BYTE)(((WORD)(*(pDST+1)) * AlphaBlendD + (WORD)g * AlphaBlendS + 127)/255);
					*(pDST+2) = (BYTE)(((WORD)(*(pDST+2)) * AlphaBlendD + (WORD)r * AlphaBlendS + 127)/255);
					LineStyle = (LineStyle<<1) | 1;
				} else LineStyle <<= 1;
				pDST += s;
			}
		} else {
			for(x=x1; w--; x += d) {
				if( LineStyle & 0x80000000 ) {
					*pDST = b;
					*(pDST+1) = g;
					*(pDST+2) = r;
					LineStyle = (LineStyle<<1) | 1;
				} else LineStyle <<= 1;
				pDST += s;
			}
		}
		++y2;	// for invalidate
	} else {	// DIAGONAL line
		int dX, dY, Xincr, Yincr, dPr, dPru, P, Ax, Ay;

		pDST = Tdpbits + TdstepY*y1 + TdstepX*x1;
		if( x1 > x2 ) {
			Xincr = -1;
			TdstepX = -TdstepX;
		} else Xincr = 1;

		if( y1 > y2 ) {
			Yincr = -1;
			TdstepY = -TdstepY;
		} else Yincr = 1;
		Ax = x1;
		Ay = y1;
		dX = abs(x2-x1);
		dY = abs(y2-y1);
		if( dX >= dY ) {
			dPr = dY<<1;
			dPru = dPr - (dX<<1);
			P = dPr - dX;
			if( AlphaBlendD ) {
				for(; dX >= 0; dX--) {
					if( Ax>=TdclipX1 && Ax<TdclipX2 && Ay>=TdclipY1 && Ay<TdclipY2 ) {
						if( LineStyle & 0x80000000 ) {
							*pDST = (BYTE)(((WORD)(*pDST) * AlphaBlendD + (WORD)b * AlphaBlendS + 127)/255);
							*(pDST+1) = (BYTE)(((WORD)(*(pDST+1)) * AlphaBlendD + (WORD)g * AlphaBlendS + 127)/255);
							*(pDST+2) = (BYTE)(((WORD)(*(pDST+2)) * AlphaBlendD + (WORD)r * AlphaBlendS + 127)/255);
							LineStyle = (LineStyle<<1) | 1;
						} else LineStyle <<= 1;
					}
					if( P > 0 ) { 
						Ay += Yincr;
						pDST += TdstepY;
						P += dPru;
					} else P += dPr;
					Ax += Xincr;
					pDST += TdstepX;
				}		
			} else {
				for(; dX >= 0; dX--) {
					if( Ax>=TdclipX1 && Ax<TdclipX2 && Ay>=TdclipY1 && Ay<TdclipY2 ) {
						if( LineStyle & 0x80000000 ) {
							*pDST = b;
							*(pDST+1) = g;
							*(pDST+2) = r;
							LineStyle = (LineStyle<<1) | 1;
						} else LineStyle <<= 1;
					}
					if( P > 0 ) { 
						Ay += Yincr;
						pDST += TdstepY;
						P += dPru;
					} else P += dPr;
					Ax += Xincr;
					pDST += TdstepX;
				}		
			}
		} else {
			dPr = dX<<1;
			dPru = dPr - (dY<<1);
			P = dPr - dY;
			if( AlphaBlendD ) {
				for(; dY >= 0; dY--) {
					if( Ax>=TdclipX1 && Ax<TdclipX2 && Ay>=TdclipY1 && Ay<TdclipY2 ) {
						if( LineStyle & 0x80000000 ) {
							*pDST = (BYTE)(((WORD)(*pDST) * AlphaBlendD + (WORD)b * AlphaBlendS + 127)/255);
							*(pDST+1) = (BYTE)(((WORD)(*(pDST+1)) * AlphaBlendD + (WORD)g * AlphaBlendS + 127)/255);
							*(pDST+2) = (BYTE)(((WORD)(*(pDST+2)) * AlphaBlendD + (WORD)r * AlphaBlendS + 127)/255);
							LineStyle = (LineStyle<<1) | 1;
						} else LineStyle <<= 1;
					}
					if( P > 0 ) {
						Ax += Xincr;
						pDST += TdstepX;
						P += dPru;
					} else P += dPr;
					Ay += Yincr;
					pDST += TdstepY;
				}		
			} else {
				for(; dY >= 0; dY--) {
					if( Ax>=TdclipX1 && Ax<TdclipX2 && Ay>=TdclipY1 && Ay<TdclipY2 ) {
						if( LineStyle & 0x80000000 ) {
							*pDST = b;
							*(pDST+1) = g;
							*(pDST+2) = r;
							LineStyle = (LineStyle<<1) | 1;
						} else LineStyle <<= 1;
					}
					if( P > 0 ) {
						Ax += Xincr;
						pDST += TdstepX;
						P += dPru;
					} else P += dPr;
					Ay += Yincr;
					pDST += TdstepY;
				}		
			}
		}		
	}
	if( bmD==kBuf ) InvalidateScreen(x1, y1, x2, y2);
}

//**************************************************************************
// Draw a rectangle in the destination bitmap.  The rectangle may be
// SOLID, OUTLINED, or BOTH, depending upon the values of 'fill' and 'edge'.
// if 'fill' is not -1, fill the rectangle with that color
// if 'edge' is not -1, outline the rectangle with that color
void RectBMB(PBMB bmD, int x1, int y1, int x2, int y2, DWORD fill, DWORD edge) {

	BYTE	*pDST, *pD, r, g, b;
	int		x, y, w, h, ww, edged;
	PBMB	bmDO;

	if( x1>x2 ) {
		x = x1;
		x1 = x2;
		x2 = x;
	}
	if( y1>y2 ) {
		y = y1;
		y1 = y2;
		y2 = y;
	}
	bmDO = bmD;
	SetupBMD(&bmD);
	edged = (edge==-1)?0:1;
	if( fill != -1 ) {
		x = Kmax(TdclipX1, x1+edged);
		y = Kmax(TdclipY1, y1+edged);
		w = Kmin(TdclipX2, x2-edged)-x;
		h = Kmin(TdclipY2, y2-edged)-y;
		if( w>0 && h>0 ) {
			r = (BYTE)(fill & 0x000000FF);
			g = (BYTE)((fill & 0x0000FF00) >> 8);
			b = (BYTE)((fill & 0x00FF0000) >> 16);
			pD = Tdpbits + TdstepY*y + TdstepX*x;
			if( AlphaBlendD ) {
				while( h-- ) {
					pDST = pD;
					for(ww=w; ww--; ) {
						*pDST = (BYTE)(((WORD)(*pDST) * AlphaBlendD + (WORD)b * AlphaBlendS + 127)/255);
						*(pDST+1) = (BYTE)(((WORD)(*(pDST+1)) * AlphaBlendD + (WORD)g * AlphaBlendS + 127)/255);
						*(pDST+2) = (BYTE)(((WORD)(*(pDST+2)) * AlphaBlendD + (WORD)r * AlphaBlendS + 127)/255);
						pDST += TdstepX;
					}
					pD += TdstepY;
				}
			} else {
				while( h-- ) {
					pDST = pD;
					for(ww=w; ww--; ) {
						*pDST = b;
						*(pDST+1) = g;
						*(pDST+2) = r;
						pDST += TdstepX;
					}
					pD += TdstepY;
				}
			}
		}
	}
	if( edged ) {
		--x2;
		--y2;
		LineBMB(bmDO, x1, y1, x2, y1, edge);
		LineBMB(bmDO, x2, y1, x2, y2, edge);
		LineBMB(bmDO, x2, y2, x1, y2, edge);
		LineBMB(bmDO, x1, y2, x1, y1, edge);
		++x2;
		++y2;
	}
	if( bmD==kBuf ) InvalidateScreen(x1, y1, x2, y2);
}

//**************************************************************************
// Fills a rectangle with a gradient hue, over a vertical range of lightness
// and a horizontal range of saturation.
void RectGradientBMB(PBMB bmD, int xo, int yo, int wo, int ho, BYTE hue, BYTE satmin, BYTE satmax, BYTE lightmin, BYTE lightmax) {

	BYTE	*pDST, *pD, r, g, b;
	int		x, y, w, h, cy, cx, dsat, dlight, hs, ws;
	PBMB	bmDO;
	double	li, sa, hu;
	double	f, p, q, t;
	double	i;

	bmDO = bmD;
	SetupBMD(&bmD);
	if( ho < 0 ) {ho = -ho; hs = 1;}
	else hs = 0;
	if( wo < 0 ) {wo = -wo; ws = 1;}
	else ws = 0;
//	x = Kmax(TdclipX1, xo);
//	y = Kmax(TdclipY1, yo);
//	w = Kmin(TdclipX2, xo+wo)-x;
//	h = Kmin(TdclipY2, yo+ho)-y;
x = xo;
y = yo;
w = wo;
h = ho;
	if( w>0 && h>0 ) {
		pD = Tdpbits + TdstepY*y + TdstepX*x;
		dsat = satmax-satmin;
		dlight = lightmax-lightmin;
		if( AlphaBlendD ) {
			for(cy=0; cy<h; cy++) {
				pDST = pD;
				li = (double)(hs?(h-cy-1):cy)/(double)(h-1);
				for(cx=0; cx<w; cx++) {
					if( xo+cx>=TdclipX1 && xo+cx<TdclipX2 && yo+cy>=TdclipY1 && yo+cy<TdclipY2 ) {
						sa = (double)(ws?(w-cx-1):cx)/(double)(w-1);
						hu = (double)(255-hue)*((double)360.0/(double)255.0);
						if( sa==0 ) r = g = b = (BYTE)(255.0*li);
						else {
							hu /= 60.0;
							i = floor(hu);
							f = hu-i;
							p = li*(1.0-sa);
							q = li*(1.0-sa*f);
							t = li*(1.0-sa*(1.0-f));
							switch( (int)i ) {
							 case 0:
								r = (BYTE)(255.0*li);
								b = (BYTE)(255.0*t);
								g = (BYTE)(255.0*p);
								break;
							 case 1:
								r = (BYTE)(255.0*q);
								b = (BYTE)(255.0*li);
								g = (BYTE)(255.0*p);
								break;
							 case 2:
								r = (BYTE)(255.0*p);
								b = (BYTE)(255.0*li);
								g = (BYTE)(255.0*t);
								break;
							 case 3:
								r = (BYTE)(255.0*p);
								b = (BYTE)(255.0*q);
								g = (BYTE)(255.0*li);
								break;
							 case 4:
								r = (BYTE)(255.0*t);
								b = (BYTE)(255.0*p);
								g = (BYTE)(255.0*li);
								break;
							 default:
								r = (BYTE)(255.0*li);
								b = (BYTE)(255.0*p);
								g = (BYTE)(255.0*q);
								break;
							}
						}
						*pDST = (BYTE)(((WORD)(*pDST) * AlphaBlendD + (WORD)b * AlphaBlendS + 127)/255);
						*(pDST+1) = (BYTE)(((WORD)(*(pDST+1)) * AlphaBlendD + (WORD)g * AlphaBlendS + 127)/255);
						*(pDST+2) = (BYTE)(((WORD)(*(pDST+2)) * AlphaBlendD + (WORD)r * AlphaBlendS + 127)/255);
					}
					pDST += TdstepX;
				}
				pD += TdstepY;
			}
		} else {
			for(cy=0; cy<h; cy++) {
				pDST = pD;
				li = (double)(hs?(h-cy-1):cy)/(double)(h-1);
				for(cx=0; cx<w; cx++) {
					if( xo+cx>=TdclipX1 && xo+cx<TdclipX2 && yo+cy>=TdclipY1 && yo+cy<TdclipY2 ) {
						sa = (double)(ws?(w-cx-1):cx)/(double)(w-1);
						hu = (double)(255-hue)*((double)360.0/(double)256.0);
						if( sa==0 ) r = g = b = (BYTE)(255.0*li);
						else {
							hu /= 60.0;
							i = floor(hu);
							f = hu-i;
							p = li*(1.0-sa);
							q = li*(1.0-sa*f);
							t = li*(1.0-sa*(1.0-f));
							switch( (int)i ) {
							 case 0:
								r = (BYTE)(255.0*li);
								b = (BYTE)(255.0*t);
								g = (BYTE)(255.0*p);
								break;
							 case 1:
								r = (BYTE)(255.0*q);
								b = (BYTE)(255.0*li);
								g = (BYTE)(255.0*p);
								break;
							 case 2:
								r = (BYTE)(255.0*p);
								b = (BYTE)(255.0*li);
								g = (BYTE)(255.0*t);
								break;
							 case 3:
								r = (BYTE)(255.0*p);
								b = (BYTE)(255.0*q);
								g = (BYTE)(255.0*li);
								break;
							 case 4:
								r = (BYTE)(255.0*t);
								b = (BYTE)(255.0*p);
								g = (BYTE)(255.0*li);
								break;
							 default:
								r = (BYTE)(255.0*li);
								b = (BYTE)(255.0*p);
								g = (BYTE)(255.0*q);
								break;
							}
						}
						*pDST = b;
						*(pDST+1) = g;
						*(pDST+2) = r;
					}
					pDST += TdstepX;
				}
				pD += TdstepY;
			}
		}
	}
	if( bmD==kBuf ) InvalidateScreen(xo, yo, xo+wo, yo+ho);
}

//**************************************************************************
// Draw an oval in the destination bitmap.  The oval may be
// SOLID, OUTLINED, or BOTH, depending upon the values of 'fill' and 'edge'.
// if 'fill' is not -1, fill the rectangle with that color
// if 'edge' is not -1, outline the rectangle with that color

#define OvalPoint(pD, x, y, r, g, b) {\
			if( x>=TdclipX1 && x<TdclipX2 && y>=TdclipY1 && y<TdclipY2 ) {\
				if( AlphaBlendD ) {\
					*pD = (BYTE)(((WORD)(*pD) * AlphaBlendD + (WORD)b * AlphaBlendS + 127)/255);\
					*(pD+1) = (BYTE)(((WORD)(*(pD+1)) * AlphaBlendD + (WORD)g * AlphaBlendS + 127)/255);\
					*(pD+2) = (BYTE)(((WORD)(*(pD+2)) * AlphaBlendD + (WORD)r * AlphaBlendS + 127)/255);\
				} else {\
					*pD = b;\
					*(pD+1) = g;\
					*(pD+2) = r;\
				}\
			}\
		}

void OvalBMB(PBMB bmD, int x1, int y1, int x2, int y2, DWORD fill, DWORD edge) {

	BYTE	*pDSTnw, *pDSTne, *pDSTsw, *pDSTse, *pDfn, *pDfs, re, ge, be, rf, gf, bf;
	int		x, y, dx, dy, a_mul, b_mul, tt, xw, xe, yn, ys, xf;
	int		row, as, bs, as2, bs2, as4, bs4, dd, xo, yo, bs6, as6;
	PBMB	bmDO;
	DWORD	edge_color;

	edge_color = (edge==-1)?fill:edge;
	if( x1==x2 || y1==y2 ) {
		LineBMB(bmD, x1, y1, x2, y2, edge_color);
		return;
	}
	if( x1>x2 ) {
		x = x1;
		x1 = x2;
		x2 = x;
	}
	if( y1>y2 ) {
		y = y1;
		y1 = y2;
		y2 = y;
	}
	bmDO = bmD;
	SetupBMD(&bmD);

	dx = x2-x1;
	dy = y2-y1;
	xo = (dx+1) & 1;		// whether to add an extra line down center of ellipse
	yo = (dy+1) & 1;		// whether to add an extra line across center of ellipse
	x  = x1+(dx-xo)/2;				// center of ellipse
	y  = y1+(dy-yo)/2;
	as4 = (dx-1-xo)*(dx-1-xo);	// 4*A^2
	as2 = as4/2;				// 2*A^2
	as  = as2/2;				// A^2
	bs4 = (dy-1-yo)*(dy-1-yo);	// 4*B^2
	bs2 = bs4/2;				// 2*B^2
	bs  = bs2/2;				// B^2
	bs6 = bs4+bs2;
	as6 = as4+as2;

	row = (dy-yo)/2;		// starting y-offset from center
	a_mul= as4*row;			// amount to adjust decision variable when step in Y
	b_mul= 0;				// amount to adjust decision variable when step in X
	dd  = as - as2*row + bs2;	// initial value of decision variable
	tt  = bs - as4*row + as2;

	rf = (BYTE)(fill & 0x000000FF);
	gf = (BYTE)((fill & 0x0000FF00) >> 8);
	bf = (BYTE)((fill & 0x00FF0000) >> 16);
	re = (BYTE)(edge_color & 0x000000FF);
	ge = (BYTE)((edge_color & 0x0000FF00) >> 8);
	be = (BYTE)((edge_color & 0x00FF0000) >> 16);

	xw = x;
	xe = x+xo;
	yn = y-row;
	ys = y+row+yo;
	pDSTnw = Tdpbits + TdstepY*yn + TdstepX*xw;
	pDSTne = Tdpbits + TdstepY*yn + TdstepX*xe;
	pDSTsw = Tdpbits + TdstepY*ys + TdstepX*xw;
	pDSTse = Tdpbits + TdstepY*ys + TdstepX*xe;

	OvalPoint(pDSTnw, xw, yn, re, ge, be);
	if( xw!=xe ) OvalPoint(pDSTne, xe, yn, re, ge, be);
	if( yn!=ys ) {
		OvalPoint(pDSTsw, xw, ys, re, ge, be);
		if( xw!=xe ) OvalPoint(pDSTse, xe, ys, re, ge, be);
	}
	do {
		if( dd<0 ) {
			dd += b_mul + bs6;
			tt += b_mul + bs4;
			b_mul += bs4;
			pDSTnw -= TdstepX;
			pDSTne += TdstepX;
			pDSTsw -= TdstepX;
			pDSTse += TdstepX;
			--xw;
			++xe;
		} else if( tt<0 ) {
			dd += b_mul + bs6 - a_mul + as4;
			tt += b_mul + bs4 - a_mul + as6;
			a_mul -= as4;
			b_mul += bs4;
			--row;
			++yn;
			--ys;
			xf = xw;
			--xw;
			++xe;
			pDSTnw += TdstepY-TdstepX;
			pDSTne += TdstepY+TdstepX;
			pDSTsw -= TdstepY+TdstepX;
			pDSTse -= TdstepY-TdstepX;

			pDfn = pDSTnw+TdstepX;
			pDfs = pDSTsw+TdstepX;
			if( fill!=-1 ) {
				 while( pDfn!=pDSTne ) {
					OvalPoint(pDfn, xf, yn, rf, gf, bf);
					if( yn!=ys ) OvalPoint(pDfs, xf, ys, rf, gf, bf);
					pDfn += TdstepX;
					pDfs += TdstepX;
					++xf;
				}
			}
		} else {
			dd -= a_mul-as4;
			tt -= a_mul-as6;
			a_mul -= as4;
			--row;
			++yn;
			--ys;
			xf = xw+1;
			pDSTnw += TdstepY;
			pDSTne += TdstepY;
			pDSTsw -= TdstepY;
			pDSTse -= TdstepY;

			pDfn = pDSTnw+TdstepX;
			pDfs = pDSTsw+TdstepX;
			if( fill!=-1 && ys>=yn ) {
				while( xf<xe ) {
					OvalPoint(pDfn, xf, yn, rf, gf, bf);
					if( yn!=ys ) OvalPoint(pDfs, xf, ys, rf, gf, bf);
					pDfn += TdstepX;
					pDfs += TdstepX;
					++xf;
				}
			}
		}
		if( ys>=yn ) {	// never write the same dot twice in case AlphaBlend is active
			OvalPoint(pDSTnw, xw, yn, re, ge, be);
			if( xw!=xe) OvalPoint(pDSTne, xe, yn, re, ge, be);
			if( ys>yn ) {
				OvalPoint(pDSTsw, xw, ys, re, ge, be);
				if( xw!=xe ) OvalPoint(pDSTse, xe, ys, re, ge, be);
			}
		}
	} while( ys>yn );

	if( bmD==kBuf ) InvalidateScreen(x1, y1, x2+1, y2+1);
}

//********************************************************************************************
void SendMsgKE(KE *pKEtarget, KE *pKEcaller, WORD cmd, DWORD arg1, DWORD arg2, DWORD arg3) {

	DWORD	r3, r4, r5, r6, r7, r8;

	r3 = GETREGD(REG_3);
	r4 = GETREGD(REG_4);
	r5 = GETREGD(REG_5);
	r6 = GETREGD(REG_6);
	r7 = GETREGD(REG_7);
	r8 = GETREGD(REG_8);
	SETREGD(REG_11, pKEcaller);
	KEmsg(pKEtarget, cmd, arg1, arg2, arg3);
	SETREGD(REG_0, pKEcaller->pKode);	// restore code block base register to caller's codebase
	SETREGD(REG_3, r3);
	SETREGD(REG_4, r4);
	SETREGD(REG_5, r5);
	SETREGD(REG_6, r6);
	SETREGD(REG_7, r7);
	SETREGD(REG_8, r8);
}

//************************************************************
//* In general, DWORD arguments start with R15 and progress downwards
//*		WORD arguments start with R39 and progress downwards
//*		BYTE arguments start with R95 and progress downwards
//*		return values are generally in R15, R39, and/or R95
//*		of course, there's always exceptions to every rule...
void SysFunc(KE *pKE, WORD opcode) {

	BYTE		*pMem, *pMem2, a, b;
	WORD		w, h, *pW, asc;
	DWORD		dw, dh, dasc;
	int			i, x, y;
	PBMB		pKB;

	switch( opcode ) {
//**** SYS_FILESIZE
// NOTE: Maximum filesize usable by the KASM/KINT system is 2^32-1 bytes (ie, length will fit in DWORD)
// inputs:
//	R15 = offset of filename in memblk [R95]
//	R95 = memblk of filename
//	R94 = folder, as in:
//		0 = folder the game is installed in (home)
//		1 = UI (folder containing UI image files)
//		2 = IMAGES (folder containing game image set files)
//		3 = BACKGNDS ( folder containing 'decorative' bitmap files)
//		4 = MUSIC (folder containing music files)
//		5 = SOUNDS (folder containing sound effect files)
// outputs:
//	R13 = size of file (-1 if error)
//	R14 = time of last modification (seconds since Jan 1 1970)
//	R93 = 0 if doesn't exist, 1 if normal file, 2 if directory (KINT 4.0)
	 case SYS_FILESIZE:
		dw = Kstat((char *)(GETREGD(4*(DWORD)GETREGB(REG_95))) + GETREGD(REG_15), GETREGB(REG_94), &dh, &a);
		if( dw==0xFFFFFFFF ) {
			SETREGD(REG_13, 0xFFFFFFFF);
			SETREGD(REG_14, 0);
			SETREGB(REG_93, 0);
		} else {
			SETREGD(REG_13, dw);
			SETREGD(REG_14, dh);
			SETREGB(REG_93, a);
		}
		return;
//**** SYS_MAKEFOLDER
// inputs:
//	R15 = offset of filename in memory block specified by R95
//	R95 = memory block # of filename (0 if R0 based, 1 if R1 based, 2 if R2 based)
// outputs:
//	R15 = 0 if fail, non-0 if succeed
	 case	SYS_MAKEFOLDER:
		SETREGD(REG_15, KMakeFolder((char *)(GETREGD(4*(DWORD)GETREGB(REG_95))) + GETREGD(REG_15)));
		return;
//**** SYS_DELFOLDER
// NOTE: Folder MUST be empty or the DelFolder will fail!
// inputs:
//	R15 = offset of filename in memory block specified by R95
//	R95 = memory block # of filename (0 if R0 based, 1 if R1 based, 2 if R2 based)
	 case	SYS_DELFOLDER:
		KDelFolder((char *)(GETREGD(4*(DWORD)GETREGB(REG_95))) + GETREGD(REG_15));
		return;
//**** SYS_OPEN:
// inputs:
//	R15 = offset of filename in memory block specified by R95
//	R95 = memory block # of filename (0 if R0 based, 1 if R1 based, 2 if R2 based)
//	R94 = folder, as in:
//		0 = folder the game is installed in (home)
//		1 = UI (folder containing UI image files)
//		2 = IMAGES (folder containing game image set files)
//		3 = BACKGNDS ( folder containing 'decorative' bitmap files)
//		4 = MUSIC (folder containing music files)
//		5 = SOUNDS (folder containing sound effect files)
//
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
//	R39 = open flags.  Supported bits are:
//		O_RDBIN:		EQU	0x8000	; read only, binary
//		O_WRBIN:		EQU	0x8002	; read/write, binary
//		O_WRBINNEW:		EQU	0x8302	; read/write, binary, create or truncate
// outputs:
//	R15 = file handle if successful, -1 if failed
	 case	SYS_FOPEN:
		// WARN: PTR4
		SETREGD(REG_15, Kopen((char *)(GETREGD(4*(DWORD)GETREGB(REG_95))) + GETREGD(REG_15), (int)(DWORD)GETREGW(REG_39), GETREGB(REG_94)));
		return;
//**** SYS_EOF
// Inputs:
//	R15 = file handle
// Outputs:
//	R12 = 1 if at EOF, 0 if not at EOF, -1 if error
	 case	SYS_FEOF:
		SETREGD(REG_12, (DWORD)Keof(GETREGD(REG_15)));
		return;
//**** SYS_READ
// Inputs:
//	R15 = file handle
//	R14 = buffer offset
//	R13 = max number of bytes to read
//	R95 = memory block # (0 if R0 based, 1 if R1 based, 2 if R2 based)
// Outputs:
//	R12 = number of bytes read.  (-1 if error)
	 case	SYS_FREAD:
		// WARN: PTR4
		SETREGD(REG_12, Kread(GETREGD(REG_15), (BYTE *)(GETREGD(4*(DWORD)GETREGB(REG_95))) + GETREGD(REG_14), GETREGD(REG_13)));
		return;
//**** SYS_WRITE
// Inputs:
//	R15 = file handle
//	R14 = buffer offset in memory block specified by R95
//	R13 = number of bytes to write
//	R95 = memory block # (0 if R0 based, 1 if R1 based, 2 if R2 based)
// Outputs:
//	R12 = number of bytes written.  (-1 if error)
	 case	SYS_FWRITE:
		// WARN: PTR4
		SETREGD(REG_12, Kwrite(GETREGD(REG_15), (BYTE *)(GETREGD(4*(DWORD)GETREGB(REG_95))) + GETREGD(REG_14), GETREGD(REG_13)));
		return;
//**** SYS_SEEK
// Inputs:
//	R15 = file handle
//	R14 = offset to move the file pointer to
//	R95 = origin:
//		SEEK_SET    0
//		SEEK_CUR    1
//		SEEK_END    2
// Outputs:
//	R12 = offset in bytes from the beginning of the file of the new position (-1 if error)
	 case	SYS_FSEEK:
		SETREGD(REG_12, Kseek(GETREGD(REG_15), GETREGD(REG_14), (int)GETREGB(REG_95)));
		return;
//**** SYS_CLOSE
// Inputs:
//	R15 = file handle to close
	 case	SYS_FCLOSE:
		Kclose(GETREGD(REG_15));
		return;
//**** SYS_FINDFIRST
// Inputs:
//	R15 = offset in memblock[R95] of filespec to search for
//	R95 = 0 for R0, 1 for R1, 2 for R2
//	R14 = offset in memblock[R94] of buffer for found filename (must be minimum of 260 bytes)
//	R94 = 0 for R0, 1 for R1, 2 for R2
//	R93 = folder, as in:
//		0 = folder the game is installed in (home)
//		1 = UI (folder containing UI image files)
//		2 = IMAGES (folder containing game image set files)
//		3 = BACKGNDS ( folder containing 'decorative' bitmap files)
//		4 = MUSIC (folder containing music files)
//		5 = SOUNDS (folder containing sound effect files)
//		6 = game folder, but GRAPHICS file
// Outputs:
//	R15 = "findfirst/next" handle of first file found, or -1 if none found or error
//	  and found file name in buffer [R94]:R14
//	  starting with KINT 4.0, R15 is returned 0 (successful) or 0xFFFFFFFF (failed)
//  R95 = file attributes:
//		 attributes of file found: 0x00 = _A_NORMAL, 0x10 = _A_SUBDIR
//		NOTE: the returned file name must be JUST the filename, not
//			including any part of the full path or the file extension.
	 case	SYS_FFINDFIRST:
	 // WARN: PTR4
		SETREGD(REG_15, Kfindfirst((char *)(GETREGD(4*(DWORD)GETREGB(REG_95))) + GETREGD(REG_15), (BYTE *)(GETREGD(4*(DWORD)GETREGB(REG_94))) + GETREGD(REG_14), &a, GETREGB(REG_93)));
		SETREGB(REG_95, a);
		return;
//**** SYS_FINDNEXT
// Inputs:
//	R15 = "find first/next" handle returned by previous FINDFIRST (ignored if KINT >= 4.0)
//	R14 = offset in memblock[R94] of buffer for found filename (must be minimum of 260 bytes)
//	R94 = 0 for R0, 1 for R1, 2 for R2
// Outputs:
//	R15 = 0 if next file found, or -1 if none found or error
//	  and found file name in buffer [R94]:R14
//  R95 = file attributes:
//		 attributes of file found: 0x00 = _A_NORMAL, 0x10 = _A_SUBDIR
	 case	SYS_FFINDNEXT:
	 // WARN: PTR4
		SETREGD(REG_15, Kfindnext((BYTE *)(GETREGD(4*(DWORD)GETREGB(REG_94))) + GETREGD(REG_14), &a));
		SETREGB(REG_95, a);
		return;
//**** SYS_FINDCLOSE
// Inputs:
//	R15 = "find first/next" handle returned by previous FINDFIRST (ignored if KINT >= 4.0)
	 case	SYS_FFINDCLOSE:
		Kfindclose();
		return;
//**** SYS_DELETE
// Inputs:
//	R15 = address of filename
//	R95 = memblk of filename
//		KINT 3.0 - if R95 > 2, then memblk==0 and R95-3 = 'dir'
	 case	SYS_DELETE:
		dw = (DWORD)GETREGB(REG_95);
		if( (long)dw >= 3 ) Kdelete((char *)(GETREGD(REG_0)) + GETREGD(REG_15), (BYTE)(dw-3));
		else Kdelete((char *)(GETREGD(4*dw)) + GETREGD(REG_15), DIR_GAME);
		return;
//**** SYS_MAKESHARECOPY
// Inputs:
//	R95 = command
//	R15 = address of buffer in memblk R0 for next filename
//	R13 = address of buffer in memblk R0 containing destination path (if R95 != 0)
// Outputs:
//	R14 = -1 if error
//		else total size of files to be copied if cmd==0 (and filename of FIRST file in R15 buffer)
//		else 0 (and filename of NEXT file in R15 buffer, or NUL if last file)
//
// NOTE: KASM code always calls this FIRST with command=0 (read file names, return first file name in buffer R15 and total size in R14)
//  followed by a sequential series of calls with R95 != 0 to copy successive files.
//  each successive call returns the NEXT filename to be copied in buffer R15.
//	When no more files to be copied, buffer R15 starts with a NUL.

// NOTE: this function is FAR more complex than ordinarily need be, but I didn't want
// the KASM code to know anything about the files being copied for the shareware version
// (as their names and/or formats will undoubtedly be different on different OS's), and
// I didn't want the KINT interpreter (this code) to know anything about the specific
// list of files being copied either, as I want the KINT code to remain generic across
// all of the games.  So, the files to be copied are in an ASCII file call "swlist",
// as a comma-separated list of src/dst pairs.  For example:
//	eks0.ebk,setup.exe,eksctlss.ebk,setup.ctl,ekssubk.ebk,setupbk.ebk,shs.epk,shs.epk
// all on one line, no CRs, no LFs, source filename first, comma, destination filename
// second, comma, next pair....etc.
//
	 case	SYS_MAKESHARECOPY:
#if KINT_MAKE_COPY
		pMem = (BYTE *)GETREGD(REG_0) + GETREGD(REG_15);
		if( GETREGB(REG_95)==0 ) {	// get total sizes and return first filename in R15 buffer
			DWORD	hFile, fsize, dw2;

			pMSCmem = NULL;

			hFile = Kopen("swlist", O_RDBIN, DIR_GAME);
			if( hFile==-1 ) {
MSCerr:
				if( pMSCmem!=NULL ) free(pMSCmem);
				if( hFile!=-1 ) Kclose(hFile);
				SETREGD(REG_14, 0xFFFFFFFF);
			} else {
				fsize = Kseek(hFile, 0, 2);
				Kseek(hFile, 0, 0);
				pMSCmem = (char *)malloc(fsize+1);
				if( pMSCmem==NULL ) goto MSCerr;
				swllen = Kread(hFile, (BYTE *)pMSCmem, fsize);
				Kclose(hFile);
				for(pMem2=(BYTE *)pMSCmem; (char *)pMem2<(pMSCmem+swllen); pMem2++) if( *pMem2==',' ) *pMem2 = 0;
				*pMem2 = 0;	// make sure last entry is nul-terminated
				dw = dw2 = 0;
				for(pMem2=(BYTE *)pMSCmem; (char *)pMem2<(pMSCmem+swllen); ) {
					hFile = Kopen((char *)pMem2, O_RDBIN, DIR_GAME);
					if( hFile==-1 ) goto MSCerr;
					dw += (Kseek(hFile, 0, 2)+8191) & 0xFFFFE000;	// add it's size to the running total (rounded up to nearest 8Kbytes for estimated file system slop)
					Kclose(hFile);

					while( *pMem2!=0 && (char *)pMem2<(pMSCmem+swllen) ) ++pMem2;	// skip the src name
					++pMem2;	// skip the nul
					if( !dw2 ) strcpy((char *)pMem, (const char *)pMem2);	// if first one, copy dst name to R15 buffer
					++dw2;	// no longer on first one
					while( *pMem2!=0 && (char *)pMem2<(pMSCmem+swllen) ) ++pMem2;	// skip the dst name
					++pMem2;	// skip the nul
				}
				pNxtSrc = pMSCmem;		// set us ready to copy first file
				SETREGD(REG_14, dw);	// return total size of files
			}
		} else { // copying file
			char	target[256], *pSRC;

			pSRC = pNxtSrc;
			strcpy(target, (char *)(GETREGD(REG_0)) + GETREGD(REG_13));
// make sure target path is properly terminated for this OS for concatenating the filename
// onto the end of it.  For MS Windows, that means making sure it ends in a '\'
			i = strlen(target);
			if( !i || target[i-1]!='\\' ) strcat(target, "\\");

			while( *pNxtSrc!=0 && pNxtSrc<(pMSCmem+swllen) ) ++pNxtSrc;	// skip src filename
			++pNxtSrc;	// skip the nul
			strcat(target, pNxtSrc);	// finish building target filename
			while( *pNxtSrc!=0 && pNxtSrc<(pMSCmem+swllen) ) ++pNxtSrc;	// skip dst filename
			if( pNxtSrc<(pMSCmem+swllen) ) ++pNxtSrc;	// skip the nul if not the last one

			pMem2 = (BYTE *)pNxtSrc;
			while( *pMem2!=0 && (char *)pMem2<(pMSCmem+swllen) ) ++pMem2;	// skip the dst name
			if( (char *)pMem2<(pMSCmem+swllen) ) ++pMem2;	// skip the nul if this isn't the last one
			strcpy((char *)pMem, (const char *)pMem2);	// copy the next filename into R15 buffer (or nul if no more)

			if( KCopyFile(pSRC, target) ) {	// if copy successful
				SETREGD(REG_14, 0);
				if( *pMem==0 ) free(pMSCmem);	// free memory if this was the last file to copy
			} else {
				SETREGD(REG_14, -1);
				free(pMSCmem);
			}
		}
#endif
		return;
//**** SYS_PRINTSTART
// Inputs:
// Outputs:
//	R15 = 0 if fail, otherwise non-0
//	R14 = page size in X direction (total pixels in printable area)
//	R13 = page size in Y direction (total pixels in printable area)
//	R12 = printer resolution in X direction (pixels per inch)
//	R11 = printer resolution in Y direction (pixels per inch)
	 case	SYS_PRINTSTART:
		{
			DWORD	success, pw, ph, Xppi, Yppi;

			success = KPrintStart(&pw, &ph, &Xppi, &Yppi);
			SETREGD(REG_14, pw);
			SETREGD(REG_13, ph);
			SETREGD(REG_12, Xppi);
			SETREGD(REG_11, Yppi);
			SETREGD(REG_15, success);
		}
		return;
//**** SYS_PRINTBEGPAGE
// Output:
//	R95 = 1 if successful, 0 if fail
	 case	SYS_PRINTBEGPAGE:
		SETREGB(REG_95, KPrintBeginPage());
		return;
//**** SYS_PRINTENDPAGE
// Output:
//	R95 = 1 if successful, 0 if fail
	 case	SYS_PRINTENDPAGE:
		SETREGB(REG_95, KPrintEndPage());
		return;
//**** SYS_PRINTSTOP
	 case	SYS_PRINTSTOP:
		KPrintStop();
		return;
//**** SYS_PRINTTEXT
// inputs:
//	R14 = string buffer address in memblk[R95]
//	R13 = textcolor
//	R95 = memblk for string buffer
//	R12 = x-coordinate of point
//	R11 = y-coordinate of point
// On KINT versions prior to 2.0, R37 was the font handle.
// Starting on version 2.0, there is a SysPrintFont opcode which
// creates/selects the printer font.  This font is automatically
// deleted (if necessary by the SysPrintStop opcode).
	 case	SYS_PRINTTEXT:
		KPrintText((int)((long)GETREGD(REG_12)), (int)((long)GETREGD(REG_11)), (char *)GETREGD(4*GETREGB(REG_95)) + GETREGD(REG_14), GETREGD(REG_13));
		return;
//**** SYS_PRINTFONT
// NOTE!!!!!!!!!!!!!!!!!NOTE!!!!!!!!!!!!!NOTE!!!!!!!!!!!!!!!!!!!NOTE!
//		This function was added at rev 2.0 of KINT, and replaces the
//		use of SysGetFont when printing.
// inputs:
//	R38 = pixel height of font
//	R37 =
//		BIT 0 =0 for variable pitch, =1 for fixed pitch
//		BIT 1 =0 for normal, =1 for BOLD
//		BIT 2 =0 for normal, =1 for ITALIC
// outputs:
//	R38 = height of font
//	R37 = ascending height of font
	 case	SYS_PRINTFONT:
		KPrintFont(GETREGW(REG_38), GETREGW(REG_37), &h, &asc);
		SETREGW(REG_38, (WORD)h);
		SETREGW(REG_37, (WORD)asc);
		return;
//**** SYS_PRINTBM
// inputs:
//	R14 = handle of source bitmap
//	R19 = x-coord of destination
//	R18 = y-coord of destination
//	R17 = x-coord of source
//	R16 = y-coord of source
//	R13 = width of source
//	R12 = height of source
//	R11 = width of destination
//	R10 = height of destination
	 case	SYS_PRINTBM:
		KPrintBMB((int)GETREGD(REG_19), (int)GETREGD(REG_18), (int)GETREGD(REG_11), (int)GETREGD(REG_10),
			 (PBMB)GETREGD(REG_14), (int)GETREGD(REG_17), (int)GETREGD(REG_16), (int)GETREGD(REG_13), (int)GETREGD(REG_12));
		return;
//**** SYS_PRINTRECT (KINT 2.2)
// inputs:
//	R19 = X
//	R18 = Y
//	R17 = W
//	R16 = H
//	R14 = FILL color
//	R13 = EDGE color
	 case SYS_PRINTRECT:
		KPrintRect((int)GETREGD(REG_19), (int)GETREGD(REG_18), (int)GETREGD(REG_17), (int)GETREGD(REG_16), GETREGD(REG_14), GETREGD(REG_13));
		return;
//**** SYS_PRINTLINES
// inputs:
//	R14 = offset in memblock[R95] of (x,y) DWORD coordinate array
//	R13 = RGB color of lines
//	R39 = # of points in the [R14] array
//	R95 = 0 for R0, 1 for R1, 2 for R2 memory block for R14 offset
	 case	SYS_PRINTLINES:
	 // WARN: PTR4
		KPrintLines((DWORD *)((BYTE *)GETREGD(4*GETREGB(REG_95)) + GETREGD(REG_14)), GETREGW(REG_39), GETREGD(REG_13));
		return;
//**** SYS_PRINTGETTEXTSIZE
// assumes a SYS_PRINTSTART and SYS_BEGTEXT have already been done
// inputs:
//	R14 = string buffer address in memblk[R95]
//	R95 = memblk for string buffer
//	 On KINT versions prior to 1.6, R37 was the font handle.
//	 Starting on version 1.6, there is a SysPrintFont opcode which
//	 creates/selects the printer font.  This font is automatically
//	 deleted (if necessary by the SysPrintStop opcode).
// outputs:
//	R15 = width
//	R14 = height
//	R13 = height of non-descending portion of font
	 case SYS_PRINTGETTEXTSIZE:
		KPrintGetTextSize((char *)GETREGD(4*GETREGB(REG_95)) + GETREGD(REG_14), &dw, &dh, &dasc);
		SETREGD(REG_15, dw);	// width of font
		SETREGD(REG_14, dh);	// height of font
		SETREGD(REG_13, dasc);	// height of non-descending portion of font
		return;
//**** SYS_ALLOCMEM
// Inputs:
//	R15 = size of memory block to allocate
// Outputs:
//	R15 = "handle" of memory block allocated (must be non-zero for valid memory allocation)
//		this "handle" must be place in R1 or R2 to access the memory (using LD1/LD2 type instructions)
//		==NULL if error
	 case	SYS_ALLOCMEM:
		// WARN: PTR4
		SETREGD(REG_15, (DWORD)malloc((size_t)GETREGD(REG_15)));
		return;
//**** SYS_FREEMEM
// Inputs:
//	R15 = "handle" of memory block to free
	 case	SYS_FREEMEM:
		free((void *)GETREGD(REG_15));
		return;
//**** SYS_TIMEREG
// Outputs:
//	R39 = year (ie, 2001)
//	R38 = month (0-11, Jan=0, Feb=1, Mar=2, etc)
//	R37 = day (1-31)
//	R36 = hour (0-23)
//	R35 = minute (0-59)
//	R34 = second (0-59)
	 case	SYS_TIMEREG:
		{
			KTIME	t;

			KGetTime(&t);
			SETREGW(REG_39, t.y);
			SETREGW(REG_38, t.mo);
			SETREGW(REG_37, t.d);
			SETREGW(REG_36, t.h);
			SETREGW(REG_35, t.mi);
			SETREGW(REG_34, t.s);
		}
		return;
//**** SYS_TIMESTR
// Inputs:
//	R15 = offset in memblock[R95] of buffer for TIME string
//	R95 = 0 for R0, 1 for R1, 2 for R2
// Outputs:
//		buffer [R95]:R15 filled with string representing current time
	 case	SYS_TIMERSTR:
	 // WARN: PTR4
		KTimeToString((BYTE *)GETREGD(4*(DWORD)GETREGB(REG_95)) + GETREGD(REG_15));
		return;
//**** SYS_TIMEMILLI
// Outputs:
//	R15 = "progressive" time count in milliseconds.
	 case	SYS_TIMEMILLI:
		SETREGD(REG_15, KGetTicks());
		return;
//**** SYS_SLEEP
// Outputs:
//	R15 = time count in milliseconds.
	 case	SYS_SLEEP:
		KSleep(GETREGD(REG_15));
		return;
//**** SYS_STRCAT
// Inputs:
//	R15 = offset in memblk[R95] of destination buffer
//	R95 = 0 for R0, 1 for R1, 2 for R2
//	R14 = offset in memblk[R94] of source buffer
//	R94 = 0 for R0, 1 for R1, 2 for R2
	 case	SYS_STRCAT:
		pMem = (BYTE *)(GETREGD(4*(DWORD)GETREGB(REG_95))) + GETREGD(REG_15);
		pMem2= (BYTE *)(GETREGD(4*(DWORD)GETREGB(REG_94))) + GETREGD(REG_14);
		strcat((char *)pMem, (char *)pMem2);
		return;
//**** SYS_MEMCPY
// Inputs:
//	R15 = offset in memblk[R95] of destination buffer
//	R95 = 0 for R0, 1 for R1, 2 for R2
//	R14 = offset in memblk[R94] of source buffer
//	R94 = 0 for R0, 1 for R1, 2 for R2
//	R13 = number of bytes to copy
	 case	SYS_MEMCPY:
		pMem = (BYTE *)(GETREGD(4*(DWORD)GETREGB(REG_95))) + GETREGD(REG_15);
		pMem2= (BYTE *)(GETREGD(4*(DWORD)GETREGB(REG_94))) + GETREGD(REG_14);
		memcpy(pMem, pMem2, (size_t)GETREGD(REG_13));
		return;
//**** SYS_MEMMOVE
// Inputs:
//	R15 = offset in memblk[R95] of destination buffer
//	R95 = 0 for R0, 1 for R1, 2 for R2
//	R14 = offset in memblk[R94] of source buffer
//	R94 = 0 for R0, 1 for R1, 2 for R2
//	R13 = number of bytes to move
	 case	SYS_MEMMOVE:
		pMem = (BYTE *)(GETREGD(4*(DWORD)GETREGB(REG_95))) + GETREGD(REG_15);
		pMem2= (BYTE *)(GETREGD(4*(DWORD)GETREGB(REG_94))) + GETREGD(REG_14);
		memmove(pMem, pMem2, (size_t)GETREGD(REG_13));
		return;
//**** SYS_STRCPY
// Inputs:
//	R15 = offset in memblk[R95] of destination buffer
//	R95 = 0 for R0, 1 for R1, 2 for R2
//	R14 = offset in memblk[R94] of source buffer
//	R94 = 0 for R0, 1 for R1, 2 for R2
	 case	SYS_STRCPY:
		pMem = (BYTE *)(GETREGD(4*(DWORD)GETREGB(REG_95))) + GETREGD(REG_15);
		pMem2= (BYTE *)(GETREGD(4*(DWORD)GETREGB(REG_94))) + GETREGD(REG_14);
		strcpy((char *)pMem, (char *)pMem2);
		return;
//**** SYS_STRCMP
// Inputs:
//	R15 = offset in memblk[R95] of buffer1
//	R95 = 0 for R0, 1 for R1, 2 for R2
//	R14 = offset in memblk[R94] of buffer2
//	R94 = 0 for R0, 1 for R1, 2 for R2
// Outputs:
//	R95 = <0 if buffer1<buffer2, 0 if buffer1==buffer2, >0 if buffer1>buffer2
	 case	SYS_STRCMP:
		pMem = (BYTE *)(GETREGD(4*(DWORD)GETREGB(REG_95))) + GETREGD(REG_15);
		pMem2= (BYTE *)(GETREGD(4*(DWORD)GETREGB(REG_94))) + GETREGD(REG_14);
		SETREGB(REG_95, (BYTE)(char)strcmp((char *)pMem, (char *)pMem2));
		return;
//**** SYS_STRICMP
// Inputs:
//	R15 = offset in memblk[R95] of buffer1
//	R95 = 0 for R0, 1 for R1, 2 for R2
//	R14 = offset in memblk[R94] of buffer2
//	R94 = 0 for R0, 1 for R1, 2 for R2
// Outputs:
//	R95 = <0 if buffer1<buffer2, 0 if buffer1==buffer2, >0 if buffer1>buffer2
	 case	SYS_STRICMP:
		pMem = (BYTE *)(GETREGD(4*(DWORD)GETREGB(REG_95))) + GETREGD(REG_15);
		pMem2= (BYTE *)(GETREGD(4*(DWORD)GETREGB(REG_94))) + GETREGD(REG_14);
		SETREGB(REG_95, (BYTE)(char)_stricmp((char *)pMem, (char *)pMem2));
		return;
//**** SYS_STRLEN
// Inputs:
//	R15 = offset in memblk[R95] of buffer
//	R95 = 0 for R0, 1 for R1, 2 for R2
// Outputs:
//	R39 = length of nul-terminated string in buffer
	 case	SYS_STRLEN:
		pMem = (BYTE *)(GETREGD(4*(DWORD)GETREGB(REG_95))) + GETREGD(REG_15);
		SETREGW(REG_39, (WORD)strlen((char *)pMem));
		return;
//**** SYS_MEMSET
// Inputs:
//	R15 = offset in memblk[R95] of destination buffer
//	R95 = 0 for R0, 1 for R1, 2 for R2
//	R94 = value to write into bytes of buffer
//	R13 = number of bytes to set
	 case	SYS_MEMSET:
		pMem = (BYTE *)(GETREGD(4*(DWORD)GETREGB(REG_95))) + GETREGD(REG_15);
		memset(pMem, GETREGB(REG_94), (size_t)GETREGD(REG_13));
		return;
//**** SYS_MEMSET2
// Inputs:
//	R15 = offset in memblk[R95] of destination buffer
//	R95 = 0 for R0, 1 for R1, 2 for R2
//	R14 = WORD value to write into bytes of buffer
//	R13 = number of WORDs to set
	 case	SYS_MEMSET2:
		pMem = (BYTE *)(GETREGD(4*(DWORD)GETREGB(REG_95))) + GETREGD(REG_15);
		for(dw=GETREGD(REG_13), w=GETREGW(REG_14); dw--; ) *((WORD *)pMem)++ = w;
		return;
//**** SYS_MEMSET4
// Inputs:
//	R15 = offset in memblk[R95] of destination buffer
//	R95 = 0 for R0, 1 for R1, 2 for R2
//	R14 = DWORD value to write into bytes of buffer
//	R13 = number of DWORDs to set
	 case	SYS_MEMSET4:
		pMem = (BYTE *)(GETREGD(4*(DWORD)GETREGB(REG_95))) + GETREGD(REG_15);
		for(dw=GETREGD(REG_13), dh=GETREGD(REG_14); dw--; ) *((DWORD *)pMem)++ = dh;
		return;
//**** SYS_MEMCMP
// Inputs:
//	R15 = offset in memblk[R95] of buffer1
//	R95 = 0 for R0, 1 for R1, 2 for R2
//	R14 = offset in memblk[R94] of buffer2
//	R94 = 0 for R0, 1 for R1, 2 for R2
//	R13 = number of bytes to compare
// Outputs:
//	R95 = <0 if buffer1<buffer2, 0 if buffer1==buffer2, >0 if buffer1>buffer2
	 case	SYS_MEMCMP:
		pMem = (BYTE *)(GETREGD(4*(DWORD)GETREGB(REG_95))) + GETREGD(REG_15);
		pMem2= (BYTE *)(GETREGD(4*(DWORD)GETREGB(REG_94))) + GETREGD(REG_14);
		i = memcmp(pMem, pMem2, (size_t)GETREGD(REG_13));
		SETREGB(REG_95, (BYTE)((i==0)?0:((i & 0x8000000)?0xFF:0x01)));
		return;
//**** SYS_STRCAT00
// Inputs:
//	R15 = offset in memblk R0 of destination buffer
//	R14 = offset in memblk R0 of source buffer
	 case	SYS_STRCAT00:
		strcat((char *)(pKE->pKode + GETREGD(REG_15)), (char *)(pKE->pKode + GETREGD(REG_14)));
		return;
//**** SYS_MEMCPY00
// Inputs:
//	R15 = offset in memblk R0 of destination buffer
//	R14 = offset in memblk R0 of source buffer
//	R13 = number of bytes to copy
	 case	SYS_MEMCPY00:
		memcpy(pKE->pKode + GETREGD(REG_15), pKE->pKode + GETREGD(REG_14), (size_t)GETREGD(REG_13));
		return;
//**** SYS_MEMMOVE00
// Inputs:
//	R15 = offset in memblk[R95] of destination buffer
//	R95 = 0 for R0, 1 for R1, 2 for R2
//	R14 = offset in memblk[R94] of source buffer
//	R94 = 0 for R0, 1 for R1, 2 for R2
//	R13 = number of bytes to move
	 case	SYS_MEMMOVE00:
		memmove(pKE->pKode + GETREGD(REG_15), pKE->pKode + GETREGD(REG_14), (size_t)GETREGD(REG_13));
		return;
//**** SYS_STRCPY00
// Inputs:
//	R15 = offset in memblk R0 of destination buffer
//	R14 = offset in memblk R0 of source buffer
	 case	SYS_STRCPY00:
		strcpy((char *)(pKE->pKode + GETREGD(REG_15)), (char *)(pKE->pKode + GETREGD(REG_14)));
		return;
//**** SYS_STRCMP00
// Inputs:
//	R15 = offset in memblk R0 of buffer1
//	R14 = offset in memblk R0 of buffer2
// Outputs:
//	R95 = <0 if buffer1<buffer2, 0 if buffer1==buffer2, >0 if buffer1>buffer2
	 case	SYS_STRCMP00:
		SETREGB(REG_95, (BYTE)(char)strcmp((char *)(pKE->pKode + GETREGD(REG_15)), (char *)(pKE->pKode + GETREGD(REG_14))));
		return;
//**** SYS_STRICMP00
// Inputs:
//	R15 = offset in memblk R0 of buffer1
//	R14 = offset in memblk R0 of buffer2
// Outputs:
//	R95 = <0 if buffer1<buffer2, 0 if buffer1==buffer2, >0 if buffer1>buffer2
	 case	SYS_STRICMP00:
		SETREGB(REG_95, (BYTE)(char)_stricmp((char *)(pKE->pKode + GETREGD(REG_15)), (char *)(pKE->pKode + GETREGD(REG_14))));
		return;
//**** SYS_STRLEN00
// Inputs:
//	R15 = offset in memblk R0 of buffer
// Outputs:
//	R39 = length of nul-terminated string in buffer
	 case	SYS_STRLEN00:
		SETREGW(REG_39, (WORD)strlen((char *)(pKE->pKode + GETREGD(REG_15))));
		return;
//**** SYS_MEMSET00
// Inputs:
//	R15 = offset in memblk R0 of destination buffer
//	R94 = value to write into bytes of buffer
//	R13 = number of bytes to set
	 case	SYS_MEMSET00:
		memset(pKE->pKode + GETREGD(REG_15), GETREGB(REG_94), (size_t)GETREGD(REG_13));
		return;
//**** SYS_MEMSET200
// Inputs:
//	R15 = offset in memblk R0 of destination buffer
//	R14 = WORD value to write into bytes of buffer
//	R13 = number of WORDs to set
	 case	SYS_MEMSET200:
		pMem = pKE->pKode + GETREGD(REG_15);
		for(dw=GETREGD(REG_13), w=GETREGW(REG_14); dw--; ) *((WORD *)pMem)++ = w;
		return;
//**** SYS_MEMSET400
// Inputs:
//	R15 = offset in memblk R0 of destination buffer
//	R14 = DWORD value to write into bytes of buffer
//	R13 = number of DWORDs to set
	 case	SYS_MEMSET400:
		pMem = pKE->pKode + GETREGD(REG_15);
		for(dw=GETREGD(REG_13), dh=GETREGD(REG_14); dw--; ) *((DWORD *)pMem)++ = dh;
		return;
//**** SYS_MEMCMP00
// Inputs:
//	R15 = offset in memblk R0 of buffer1
//	R14 = offset in memblk R0 of buffer2
//	R13 = number of bytes to compare
// Outputs:
//	R95 = <0 if buffer1<buffer2, 0 if buffer1==buffer2, >0 if buffer1>buffer2
	 case	SYS_MEMCMP00:
		i = memcmp(pKE->pKode + GETREGD(REG_15), pKE->pKode + GETREGD(REG_14), (size_t)GETREGD(REG_13));
		SETREGB(REG_95, (BYTE)((i==0)?0:((i & 0x8000000)?0xFF:0x01)));
		return;

//**** SYS_SETCURSOR
// Inputs:
//	R39 = 0 for normal, 1 for wait, 2 for hotlink
	 case SYS_SETCURSOR:
		KSetCursor(GETREGW(REG_39));
		return;
//**** SYS_SETPGMTITLE
// Inputs:
//	R15 = offset in memblock[R95] of buffer containing program title
//	R95 = 0 for R0, 1 for R1, 2 for R2
	 case	SYS_SETPGMTITLE:
	 // WARN: PTR4
		KSetTitle((BYTE *)GETREGD(4*(DWORD)GETREGB(REG_95)) + GETREGD(REG_15));
		return;
//**** SYS_CAPTUREMOUSE
// Inputs:
//	R15 = 1 to capture mouse, 0 to release mouse capture
	 case	SYS_CAPTUREMOUSE:
		KCaptureMouse(GETREGD(REG_15));
		return;
/*************************************************************
 * BITMAP functions: the KINT program creates a "display bitmap"
 * automatically.  A PBMB handle of NULL refers to this
 * "display bitmap".  The NULL PBMB cannot be deleted, but
 * it can be written to by all of the drawing functions.  The
 * KINT program is also responsible for any "cleanup" work that
 * needs to be done regarding the NULL "display bitmap" when
 * the program is exiting, as well as deleting all resources
 * allocated for other bitmaps created by SysCreBM, SysLoadBM,
 * and SysLoadMBM.
 *************************************************************/

//**** SYS_CREBM
// inputs:
//	R39 = width
//	R38 = height
// outputs:
//	R15 = bitmap handle of created bitmap, NULL if creation error.
	 case	SYS_CREBM:
		SETREGD(REG_15, (DWORD)CreateBMB(GETREGW(REG_39), GETREGW(REG_38), FALSE));	// return to the Kode the pointer to the bitmap structure as the "bitmap handle"
		return;
//**** SYS_DELBM
// inputs:
//	R15 = handle (PBMB) of bitmap to delete
	 case	SYS_DELBM:
		DeleteBMB((PBMB)GETREGD(REG_15));
		return;
//**** SYS_SIZEBM
// Inputs:
//	R15 = handle of bitmap
// Outputs:
//	R39 = width of bitmap
//	R38 = height of bitmap
//	R37 = orientation of bitmap (BMO_ value) (KINT 2.0)
	 case SYS_SIZEBM:
		pKB = (PBMB)GETREGD(REG_15);
		if( pKB==NULL ) pKB = kBuf;
		SETREGW(REG_39, pKB->logW);
		SETREGW(REG_38, pKB->logH);
		SETREGW(REG_37, pKB->orient);
		return;
//**** SYS_ALPHABLEND (KINT 2.0)
// Inputs:
//	R95 = Dst ratio (0 to disable alpha-blending, 255 to disable src writing)
// Outputs:
//	R95 = previous setting
	 case SYS_ALPHABLEND:
		w = (WORD)GETREGB(REG_95);
		SETREGB(REG_95, (BYTE)AlphaBlendD);
		AlphaBlendD = w;
		AlphaBlendS = 255-AlphaBlendD;
		return;
//**** SYS_DRAWBM
// Inputs:
//	R15 = handle of destination bitmap
//	R14 = handle of source bitmap
//	R39 = x-coord of destination
//	R38 = y-coord of destination
//	R37 = x-coord of source
//	R36 = y-coord of source
//	R35 = width of source
//	R34 = height of source
	 case	SYS_DRAWBM:
		BltBMB((PBMB)GETREGD(REG_15), (int)(short)GETREGW(REG_39), (int)(short)GETREGW(REG_38),
			   (PBMB)GETREGD(REG_14), (int)(short)GETREGW(REG_37), (int)(short)GETREGW(REG_36),
									  (int)(short)GETREGW(REG_35), (int)(short)GETREGW(REG_34), (int)FALSE, (int)FALSE);
		return;
//**** SYS_INVDRAWBM (Added KINT rev 2.3)
// Inputs:
//	R15 = handle of destination bitmap
//	R14 = handle of source bitmap
//	R39 = x-coord of destination
//	R38 = y-coord of destination
//	R37 = x-coord of source
//	R36 = y-coord of source
//	R35 = width of source
//	R34 = height of source
	 case	SYS_INVDRAWBM:
		BltBMB((PBMB)GETREGD(REG_15), (int)(short)GETREGW(REG_39), (int)(short)GETREGW(REG_38),
			   (PBMB)GETREGD(REG_14), (int)(short)GETREGW(REG_37), (int)(short)GETREGW(REG_36),
									  (int)(short)GETREGW(REG_35), (int)(short)GETREGW(REG_34), (int)FALSE, (int)TRUE);
		return;
//**** SYS_STRETCHBM
// Inputs:
//	R15 = handle of destination bitmap
//	R14 = handle of source bitmap
//	R39 = x-coord of destination
//	R38 = y-coord of destination
//	R37 = x-coord of source
//	R36 = y-coord of source
//	R35 = width of source
//	R34 = height of source
//	R33 = width of destination
//	R32 = height of destination
	 case	SYS_STRETCHBM:	// CHANGED at REV 2.3 of KINT!!!! (To add CompressBMB when R35 is negative)
		w = GETREGW(REG_35);
		h = GETREGW(REG_34);
		if( w & 0x8000 )
		 	CompressBMB((PBMB)GETREGD(REG_15), (int)(short)GETREGW(REG_39), (int)(short)GETREGW(REG_38),
										  (int)(short)GETREGW(REG_33), (int)(short)GETREGW(REG_32),
			   (PBMB)GETREGD(REG_14), (int)(short)GETREGW(REG_37), (int)(short)GETREGW(REG_36),
									  -((int)(short)w), (int)(short)h, FALSE);

		else
			StretchBMB((PBMB)GETREGD(REG_15), (int)(short)GETREGW(REG_39), (int)(short)GETREGW(REG_38),
										  (int)(short)GETREGW(REG_33), (int)(short)GETREGW(REG_32),
			   (PBMB)GETREGD(REG_14), (int)(short)GETREGW(REG_37), (int)(short)GETREGW(REG_36),
									  (int)(short)GETREGW(REG_35), (int)(short)GETREGW(REG_34), FALSE);
		return;
//**** SYS_STRETCHMBM	NOTE: This opcode added at REV 2.0 of KINT!!!!
// Inputs:
//	R15 = handle of destination bitmap
//	R14 = handle of source bitmap
//	R39 = x-coord of destination
//	R38 = y-coord of destination
//	R37 = x-coord of source
//	R36 = y-coord of source
//	R35 = width of source
//	R34 = height of source
//	R33 = width of destination
//	R32 = height of destination
	 case	SYS_STRETCHMBM:	// modified at KINT 3.2: added smooth stretching of MASKED bitmaps (when R35 is negative)
		w = GETREGW(REG_35);
		h = GETREGW(REG_34);
		if( w & 0x8000 )
		 	CompressBMB((PBMB)GETREGD(REG_15), (int)(short)GETREGW(REG_39), (int)(short)GETREGW(REG_38),
										  (int)(short)GETREGW(REG_33), (int)(short)GETREGW(REG_32),
			   (PBMB)GETREGD(REG_14), (int)(short)GETREGW(REG_37), (int)(short)GETREGW(REG_36),
									  -((int)(short)w), (int)(short)h, TRUE);
		else
			StretchBMB((PBMB)GETREGD(REG_15), (int)(short)GETREGW(REG_39), (int)(short)GETREGW(REG_38),
										  (int)(short)GETREGW(REG_33), (int)(short)GETREGW(REG_32),
			   (PBMB)GETREGD(REG_14), (int)(short)GETREGW(REG_37), (int)(short)GETREGW(REG_36),
									  (int)(short)GETREGW(REG_35), (int)(short)GETREGW(REG_34), TRUE);
		return;
//**** SYS_DRAWMBM (Masked BitMap)
// Inputs:
//	R15 = handle of destination bitmap
//	R14 = handle of source bitmap
//	R39 = x-coord of destination
//	R38 = y-coord of destination
//	R37 = x-coord of source
//	R36 = y-coord of source
//	R35 = width of source
//	R34 = height of source
	 case	SYS_DRAWMBM:
		BltBMB((PBMB)GETREGD(REG_15), (int)(short)GETREGW(REG_39), (int)(short)GETREGW(REG_38),
				(PBMB)GETREGD(REG_14), (int)(short)GETREGW(REG_37), (int)(short)GETREGW(REG_36),
									   (int)(short)GETREGW(REG_35), (int)(short)GETREGW(REG_34), (int)TRUE, (int)FALSE);
		return;
//**** SYS_INVDRAWMBM (Masked BitMap) (Added KINT rev 2.3)
// Inputs:
//	R15 = handle of destination bitmap
//	R14 = handle of source bitmap
//	R39 = x-coord of destination
//	R38 = y-coord of destination
//	R37 = x-coord of source
//	R36 = y-coord of source
//	R35 = width of source
//	R34 = height of source
	 case	SYS_INVDRAWMBM:
		BltBMB((PBMB)GETREGD(REG_15), (int)(short)GETREGW(REG_39), (int)(short)GETREGW(REG_38),
				(PBMB)GETREGD(REG_14), (int)(short)GETREGW(REG_37), (int)(short)GETREGW(REG_36),
									   (int)(short)GETREGW(REG_35), (int)(short)GETREGW(REG_34), (int)TRUE, (int)TRUE);
		return;
//**** SYS_LOADBM
// Inputs:
//	R15 = offset into memory block[R95] of filename
//	R95 = 0 for R0, 1 for R1, 2 for R2
//	R94 =
//		0 file is in GAME folder
//		1 file is in UI folder
//		2 file is in IMAGES folder
//		3 file is in BACKGNDS folder
// Outputs:
//	R15 = handle of loaded bitmap, or NULL if failed
	 case	SYS_LOADBM:
	 // WARN: PTR4
		SETREGD(REG_15, LoadBMB((char *)(GETREGD(4*(DWORD)GETREGB(REG_95)) + GETREGD(REG_15)), -1, -1, -1, -1, GETREGB(REG_94)));
		return;
//**** SYS_LOADMBM (Masked BitMap)
// Inputs:
//	R15 = offset into memory block[R95] of filename
//	R39 = x-coord of transparent pixel 1
//	R38 = y-coord of transparent pixel 1
//	R37 = x-coord of transparent pixel 2 (KINT 2.0)
//	R36 = y-ccord of transparent pixel 2 (KINT 2.0)
//	R95 = 0 for R0, 1 for R1, 2 for R2
//	R94 =
//		0 file is in GAME folder
//		1 file is in UI folder
//		2 file is in IMAGES folder
//		3 file is in BACKGNDS folder
// Outputs:
//	R15 = handle of loaded bitmap, or NULL if failed
	 case	SYS_LOADMBM:
	 // WARN: PTR4
		if( pKE->pKode[KHDR_MAJOR_INTERP_REV] < 2 ) x = y = -1;
		else {
			x = (int)(short)GETREGW(REG_37);
			y = (int)(short)GETREGW(REG_36);
		}
		SETREGD(REG_15, LoadBMB((char *)(GETREGD(4*(DWORD)GETREGB(REG_95)) + GETREGD(REG_15)), GETREGW(REG_39), GETREGW(REG_38), x, y, GETREGB(REG_94)));
		return;
//**** SYS_SAVEBM (Masked BitMap) (KINT 2.0)
// Inputs:
//	R15 = bitmap handle
//	R14 = offset into memory block[R95] of filename
//	R95 = 0 for R0, 1 for R1, 2 for R2
//	R94 =
//		0 file is in GAME folder
//		1 file is in UI folder
//		2 file is in IMAGES folder
//		3 file is in BACKGNDS folder
// Outputs:
//	R94 = 0 if failed, non-0 if succeeded
	 case	SYS_SAVEBM:	// KINT 2.0
	 // WARN: PTR4
		SETREGB(REG_94, (BYTE)SaveBMB((PBMB)GETREGD(REG_15), (char *)(GETREGD(4*(DWORD)GETREGB(REG_95)) + GETREGD(REG_14)), GETREGB(REG_94)));
		return;
//**** SYS_MASKBM (Create new masked for BitMap)
// Inputs:
//	R15 = bitmap handle
//	R14 = transparent color 1
//	R13 = transparent color 2
	 case	SYS_MASKBM:
		CreateMaskBMB((PBMB)GETREGD(REG_15), GETREGD(REG_14), GETREGD(REG_13));
		return;
//**** SYS_LOADLOSSYBM
// Inputs:
//	R15 = offset into memory block[R95] of filename
//	R95 = 0 for R0, 1 for R1, 2 for R2
//	R94 =
//		0 file is in GAME folder
//		1 file is in UI folder
//		2 file is in IMAGES folder
//		3 file is in BACKGNDS folder
// Outputs:
//	R15 = handle of loaded bitmap, or NULL if failed
	 case	SYS_LOADLOSSYBM:
	 // WARN: PTR4
	 // try to load lossless (BMP) first
	 // if that fails, try to load lossy (JPG)
		if( 0==(dw = (DWORD)LoadBMB((char *)(GETREGD(4*(DWORD)GETREGB(REG_95)) + GETREGD(REG_15)), -1, -1, -1, -1, GETREGB(REG_94))) )
			    dw =(DWORD)LoadJPEG((char *)(GETREGD(4*(DWORD)GETREGB(REG_95)) + GETREGD(REG_15)), GETREGB(REG_94));
		SETREGD(REG_15, dw);
		return;
//**** SYS_GETSYSTEMDATA
// Inputs:
//	R15 = offset of buffer in memblk R0; buffer MUST be at least 4 Kbytes
//	R95 = what to get
//		= 0 for AppDataPath (NOTE: *ONLY* to be used for Help output!!!)
//		= 1 for PgmPath  (NOTE: *ONLY* to be used for Help output!!!)
//		= 2 for OS version
	 case	SYS_GETSYSTEMDATA:
		pMem = (BYTE *)(GETREGD(REG_0)) + GETREGD(REG_15);
		switch( GETREGB(REG_95) ) {
		 case 0:
			strcpy(pMem, AppDataPath);
			break;
		 case 1:
			strcpy(pMem, PgmPath);
			break;
		 case 2:
			KGetOSversion(pMem);
			break;
		 case 3:
			strcpy(pMem, OSctrl_cmdSTR);
			break;
		 default:
			break;
		}
		return;
//**** SYS_GETWINDOWPOS
// outputs:
//	R15 = X
//	R14 = Y
//	R13 = W
//	R12 = H
//	R11 = system dependant
	 case	SYS_GETWINDOWPOS:
		{
			DWORD	r11, r12, r13, r14, r15;

			KINT_GetWindowPos(&r11, &r12, &r13, &r14, &r15);
			SETREGD(REG_15, r15);
			SETREGD(REG_14, r14);
			SETREGD(REG_13, r13);
			SETREGD(REG_12, r12);
			SETREGD(REG_11, r11);
		}
		return;
//**** SYS_SETWINDOWPOS
// inputs:
//	R15 = X
//	R14 = Y
//	R13 = W
//	R12 = H
//	R11 = system dependant
// NOTE!!!!! This MUST NOT generate an OnWindowSize or OnWindowMove event!  
// It is assumed that the Kode knows that it has changed the window and will 
// take appropriate action, if need be.  This is handled by the 'InSetWindowPos'
// variable in MAIN.C that 'gates' activity in the WM_SIZE code.
	 case	SYS_SETWINDOWPOS:
		if( GETREGD(REG_10)==0 || pKE->pKode[KHDR_MAJOR_INTERP_REV] < 2 || (pKE->pKode[KHDR_MAJOR_INTERP_REV]==2 && pKE->pKode[KHDR_MINOR_INTERP_REV]<2) )
			KINT_SetWindowPos(GETREGD(REG_11), GETREGD(REG_12), GETREGD(REG_13), GETREGD(REG_14), GETREGD(REG_15));
		else
			KINT_SetWindowPos(GETREGD(REG_10), 0,0,0,0);
		return;
//**** SYS_GETVIEWSIZE
// outputs:
//	R39 = window width
//	R38 = window height
	 case	SYS_GETVIEWSIZE:
		SETREGW(REG_39, kBuf->logW);
		SETREGW(REG_38, kBuf->logH);
		return;
//**** SYS_ORIENTBM
// inputs:
//	R15 = Bitmap handle
//	R39 = BMO_ value
	 case	SYS_ORIENTBM:
		pKB = (PBMB)GETREGD(REG_15);
		if( pKB==0 ) pKB = kBuf;
		w = GETREGW(REG_39);
		if( (w ^ pKB->orient) & BMO_90 ) {	// if rotating multiple of 90 degrees
			h = pKB->logH;					//	swap logW and logH
			pKB->logH = pKB->logW;
			pKB->logW = h;
		}
		pKB->orient = w;
		pKB->clipX1 = pKB->clipY1 = 0;
		if( w & BMO_90 ) {	// if rotated multiple of 90 degrees
			pKB->clipX2 = pKB->physH;
			pKB->clipY2 = pKB->physW;
		} else {
			pKB->clipX2 = pKB->physW;
			pKB->clipY2 = pKB->physH;
		}
		return;
//**** SYS_GETSCREENSIZE
// outputs:
//	R39 = screen width
//	R38 = screen height
//	R37 = size index (0=640, 1=800, 2=1024, 3=1152, 4=1280, 5=1600)

// NOTE: PhysicalScreenW, PhysicalScreenH, and PhysicalScreenSizeIndex *MUST* be initialized by code in 'main' module

	 case	SYS_GETSCREENSIZE:
		SETREGW(REG_39, PhysicalScreenW);
		SETREGW(REG_38, PhysicalScreenH);
		SETREGW(REG_37, PhysicalScreenSizeIndex);
		return;
//**** SYS_FLUSHSCREEN
// force display update/refresh RIGHT NOW (rather than waiting a later "update message" or whatever)
	 case	SYS_FLUSHSCREEN:
		FlushScreen();
		return;
//**** SYS_RECTEDGED: (Kept for backwards compatibility only.  Use SysRect.)
// inputs:
//	R15 = bitmap handle (NULL==screen)
//	R14 = RGB color fill
//	R13 = RGB color frame
//	R39 = left
//	R38 = top
//	R37 = right
//	R36 = bottom
	 case	SYS_RECTEDGED:
// WARN: PTR4
		RectBMB((PBMB)GETREGD(REG_15), (int)(short)GETREGW(REG_39), (int)(short)GETREGW(REG_38),
					 (int)(short)GETREGW(REG_37)+1, (int)(short)GETREGW(REG_36)+1,
					 GETREGD(REG_14), GETREGD(REG_13));
		return;
//**** SYS_RECTSOLID: (Kept for backwards compatibility only.  Use SysRect.)
// inputs:
//	R15 = bitmap handle (NULL==screen)
//	R14 = RGB color fill
//	R39 = left
//	R38 = top
//	R37 = right
//	R36 = bottom
	 case	SYS_RECTSOLID:
// WARN: PTR4
		RectBMB((PBMB)GETREGD(REG_15), (int)(short)GETREGW(REG_39), (int)(short)GETREGW(REG_38),
									   (int)(short)GETREGW(REG_37)+1, (int)(short)GETREGW(REG_36)+1,
					 GETREGD(REG_14), -1);
		return;
//**** SYS_RECTOUTLINE: (Kept for backwards compatibility only.  Use SysRect.)
// inputs:
//	R15 = bitmap handle (NULL==screen)
//	R13 = RGB color frame
//	R39 = left
//	R38 = top
//	R37 = right
//	R36 = bottom
	 case	SYS_RECTOUTLINE:
// WARN: PTR4
		RectBMB((PBMB)GETREGD(REG_15), (int)(short)GETREGW(REG_39), (int)(short)GETREGW(REG_38),
					 (int)(short)GETREGW(REG_37)+1, (int)(short)GETREGW(REG_36)+1,
					 -1, GETREGD(REG_13));
		return;
//**** SYS_RECT (KINT 2.0) and (KINT 2.2)
// inputs:
//	R15 = bitmap handle (NULL==screen)
//	R14 = RGB color fill (-1 if none) (on 2.2, if -2, then R90-R94 are valid and do Gradient fill)
//	R13 = RGB color frame (-1 if none) (on 2.2, if -2, then R90-R94 are valid and do Gradient fill)
//	R39 = left
//	R38 = top
//	R37 = w
//	R36 = h
//	R90 = hue (if R13 or R14 == -2)
//	R91 = satmin (if R13 or R14 == -2)
//	R92 = satmax (if R13 or R14 == -2)
//	R93 = lightmin (if R13 or R14 == -2)
//	R94 = lightmax (if R13 or R14 == -2)
	 case	SYS_RECT:
// WARN: PTR4
		x = (int)(short)GETREGW(REG_39);
		y = (int)(short)GETREGW(REG_38);
		dw= GETREGD(REG_13);
		dh= GETREGD(REG_14);
		if( dw==-2 || dh==-2 ) RectGradientBMB((PBMB)GETREGD(REG_15), x, y, (int)(short)GETREGW(REG_37), (int)(short)GETREGW(REG_36), GETREGB(REG_90), GETREGB(REG_91), GETREGB(REG_92), GETREGB(REG_93), GETREGB(REG_94));
		else RectBMB((PBMB)GETREGD(REG_15), x, y, x+(int)(short)GETREGW(REG_37), y+(int)(short)GETREGW(REG_36), dh, dw);
		return;
//**** SYS_OVAL (KINT 2.0)
// inputs:
//	R15 = bitmap handle (NULL==screen)
//	R14 = RGB color fill (-1 if no fill)
//	R13 = RGB color frame (-1 if no fill)
//	R39 = left
//	R38 = top
//	R37 = w
//	R36 = h
	 case	SYS_OVAL:
// WARN: PTR4
		x = (int)(short)GETREGW(REG_39);
		y = (int)(short)GETREGW(REG_38);
		OvalBMB((PBMB)GETREGD(REG_15), x, y,
					 x+(int)(short)GETREGW(REG_37), y+(int)(short)GETREGW(REG_36),
					 GETREGD(REG_14), GETREGD(REG_13));
		return;
//**** SYS_LINES
// inputs:
//	R15 = bitmap handle (NULL==screen)
//	R14 = offset in memblock[R95] of (x,y) WORD coordinate array
//	R13 = RGB color of lines
//	R39 = # of points in the [R14] array
//	R95 = 0 for R0, 1 for R1, 2 for R2 memory block for R14 offset
	 case	SYS_LINES:
	 // WARN: PTR4
		pW = (WORD *)((BYTE *)GETREGD(4*GETREGB(REG_95)) + GETREGD(REG_14));
		pKB = (PBMB)GETREGD(REG_15);

		w = GETREGW(REG_39);
		while( --w ) {
			x = (int)(short)(GETMEMW(pW)); pW++;
			y = (int)(short)(GETMEMW(pW)); pW++;
			LineBMB(pKB, x, y, (int)(short)(GETMEMW(pW)), (int)(short)(GETMEMW(pW+1)), GETREGD(REG_13));
		}
		return;
//**** SYS_LINESTYLE
// inputs:
//	R12 = new linestyle (DWORD, bits set where points plotted, gets rotated as used by LineBMB and RectBMB)
// outputs
//	R12 = old linestyle
	 case	SYS_LINESTYLE:
		dw = GETREGD(REG_12);
		SETREGD(REG_12, LineStyle);
		LineStyle = dw;
		return;
//**** SYS_SETPIXEL:
// inputs:
//	R15 = bitmap handle (NULL==screen)
//	R14 = RGB color of point
//	R39 = x-coordinate of point
//	R38 = y-coordinate of point
	 case	SYS_SETPIXEL:
		PointBMB((PBMB)GETREGD(REG_15), (int)(short)GETREGW(REG_39), (int)(short)GETREGW(REG_38), GETREGD(REG_14));
		return;
//**** SYS_GETPIXEL:
// inputs:
//	R15 = bitmap handle (NULL==screen)
//	R39 = x-coordinate of point
//	R38 = y-coordinate of point
// outputs:
//	R14 = RGB color of point
	 case	SYS_GETPIXEL:
		pKB = (PBMB)GETREGD(REG_15);
		if( pKB==0 ) pKB = kBuf;
		x = (int)(short)GETREGW(REG_39);
		y = (int)(short)GETREGW(REG_38);

		SetupBMD(&pKB);
		if( x>=0 && x<pKB->logW && y>=0 && y<pKB->logH ) {
			pMem = Tdpbits + TdstepY*y + TdstepX*x;
			dw = ((DWORD)(*pMem++))<<16;
			dw |=((DWORD)(*pMem++))<<8;
			dw |=((DWORD)(*pMem++));
			SETREGD(REG_14, dw);
		}
		return;
//**** SYS_CLIP (KINT 2.0)
// inputs:
//	R15 = bitmap handle (NULL==screen) to clip
//	R39 = left-coordinate of clip region (nothing is drawn < R39)
//	R38 = top-coordinate of clip region  (nothing is drawn < R38)
//	R37 = width of clip region (nothing is drawn >= R39+R37)
//	R36 = height of clip region (nothing is drawn >= R38+R36)
// NOTE: if R39==-1, then UNclip (set clip to bitmap's size)
	 case	SYS_CLIP:
		pKB = (PBMB)GETREGD(REG_15);
		if( pKB==0 ) pKB = kBuf;
		if( -1==((int)(short)GETREGW(REG_39)) ) {
			pKB->clipX1 = 0;
			pKB->clipY1 = 0;
			pKB->clipX2 = pKB->logW;
			pKB->clipY2 = pKB->logH;
		} else {
			int		x1, y1, x2, y2;

			x1 = Kmax((int)(short)GETREGW(REG_39), 0);
			y1 = Kmax((int)(short)GETREGW(REG_38), 0);
			x2 = Kmin((int)(short)GETREGW(REG_39) + (int)(short)GETREGW(REG_37), pKB->logW);
			y2 = Kmin((int)(short)GETREGW(REG_38) + (int)(short)GETREGW(REG_36), pKB->logH);

			if( x2 > pKB->physW ) x2 = pKB->physW;
			if( y2 > pKB->physH ) y2 = pKB->physH;

			pKB->clipX1 = x1;
			pKB->clipY1 = y1;
			pKB->clipX2 = x2;
			pKB->clipY2 = y2;
		}
		return;
//**** SYS_GETCLIP (KINT 2.0)
// inputs:
//	R15 = bitmap handle (NULL==screen) to get clip settings from
// outputs:
//	R39 = left-coordinate of clip region (nothing is drawn < R39)
//	R38 = top-coordinate of clip region  (nothing is drawn < R38)
//	R37 = width of clip region (nothing is drawn >= R39+R37)
//	R36 = height of clip region (nothing is drawn >= R38+R36)
	 case	SYS_GETCLIP:
		pKB = (PBMB)GETREGD(REG_15);
		if( pKB==0 ) pKB = kBuf;
		SETREGW(REG_39, (WORD)(short)pKB->clipX1);
		SETREGW(REG_38, (WORD)(short)pKB->clipY1);
		SETREGW(REG_37, (WORD)(short)(pKB->clipX2-pKB->clipX1));
		SETREGW(REG_36, (WORD)(short)(pKB->clipY2-pKB->clipY1));
		return;
//**** SYS_COLORIZE
// inputs:
//	R15 = bitmap handle (NULL==screen)
//	R39 = x
//	R38 = y
//	R37 = w
//	R36 = h
//	R35 = hue (-1==no change, else 0-255)
//	R34 = saturation (-1==no change, 0=convert to grayscale, else 1-255.  When sat==0, hue is ignored.)
//	R33 = brightness (-255 to 255, 0==no change, else newbrightness = oldbrightness + R33 (result limited to 0-255)
//	R32 = contrast (-255 to 255, 0==no change, else 
	 case	SYS_COLORIZE:
		if( GETREGW(REG_34)==0 ) GrayBMB((PBMB)GETREGD(REG_15), (int)(short)GETREGW(REG_39), (int)(short)GETREGW(REG_38),
									(int)(short)GETREGW(REG_37), (int)(short)GETREGW(REG_36), (int)(short)GETREGW(REG_33), (int)(short)GETREGW(REG_32));
		else ColorizeBMB((PBMB)GETREGD(REG_15), (int)(short)GETREGW(REG_39), (int)(short)GETREGW(REG_38),
				 (int)(short)GETREGW(REG_37), (int)(short)GETREGW(REG_36), (int)(short)GETREGW(REG_35), (int)(short)GETREGW(REG_34), (int)(short)GETREGW(REG_33), (int)(short)GETREGW(REG_32));
		return;
//**** SYS_RGB2HSL	(KINT 2.0.  Changed input registers from 83-85 to 84-86 on 2.2)
// inputs:
//	R80 = r
//	R81 = g
//	R82 = b
// outputs:
//	R84 = hue
//	R85 = saturation
//	R86 = lightness
//	R87 = 0
	 case	SYS_RGB2HSL:
		{
			int		r, g, b, mx, mn, v, s, hh, rc, gc, bc;

			r = (int)(short)(WORD)(BYTE)GETREGB(REG_80);
			g = (int)(short)(WORD)(BYTE)GETREGB(REG_81);
			b = (int)(short)(WORD)(BYTE)GETREGB(REG_82);
			mx= (r>=g && r>=b)?r:((g>=r && g>=b)?g:b);
			mn= (r<=g && r<=b)?r:((g<=r && g<=b)?g:b);
			v = mx;	// lightness
			if( mx ) s = 255*(mx-mn)/mx;	// saturation
			else s = 0;
			if( s==0 ) hh = 0;
			else {
				rc = 255*(mx-r)/(mx-mn);
				gc = 255*(mx-g)/(mx-mn);
				bc = 255*(mx-b)/(mx-mn);

				if( r==mx ) hh = (bc-gc)/6;
				else if( g==mx ) hh = 85+(rc-bc)/6;
				else hh = 171+(gc-rc)/6;
			}
			if( s<0 ) s = 0;
			else if( s>255 ) s = 255;

			if( v>255 ) v = 255;
			else if( v<0 ) v = 0;

			SETREGB(REG_84, (BYTE)hh);
			SETREGB(REG_85, (BYTE)s);
			SETREGB(REG_86, (BYTE)v);
			SETREGB(REG_87, 0);
		}
		return;
//**** SYS_HSL2RGB	(KINT 2.0.  Changed input registers from 83-85 to 84-86 on 2.2)
// inputs:
//	R84 = hue
//	R85 = saturation
//	R86 = lightness
// outputs:
//	R80 = r
//	R81 = g
//	R82 = b
//	R83 = 0
	 case	SYS_HSL2RGB:
		{
			int		r, g, b, v, s, hh, f, p, q, t;

			v = (int)(short)(WORD)(BYTE)GETREGB(REG_86);
			s = (int)(short)(WORD)(BYTE)GETREGB(REG_85);
			hh= (int)(short)(WORD)(BYTE)GETREGB(REG_84);
			if( s==0 ) {
				r = g = b = v;
			} else {
				if( hh>255 ) hh = 0;
				f = 6*(hh % 42);
				hh= hh / 42;
				if( hh>5 || hh<0 ) hh = 0;
				p = (v*(255-s))/255;
				q = (f==0)?v:((v*(255*246-s*f))/(255*246));
				t = (v*(255*246-(s*(246-f))))/(255*266);
				switch( hh ) {
				 case 0:
					r = (BYTE)v;
					g = (BYTE)t;
					b = (BYTE)p;
					break;
				 case 1:
					r = (BYTE)q;
					g = (BYTE)v;
					b = (BYTE)p;
					break;
				 case 2:
					r = (BYTE)p;
					g = (BYTE)v;
					b = (BYTE)t;
					break;
				 case 3:
					r = (BYTE)p;
					g = (BYTE)q;
					b = (BYTE)v;
					break;
				 case 4:
					r = (BYTE)t;
					g = (BYTE)p;
					b = (BYTE)v;
					break;
				 case 5:
					r = (BYTE)v;
					g = (BYTE)p;
					b = (BYTE)q;
					break;
				}
			}
			SETREGB(REG_80, (BYTE)r);
			SETREGB(REG_81, (BYTE)g);
			SETREGB(REG_82, (BYTE)b);
			SETREGB(REG_83, 0);
		}
		return;
//**** SYS_GETTEXTSIZE
// assumes a SYS_BEGTEXT and a SYS_TEXTSTYLE have already been done
// inputs:
//	R14 = string buffer address in memblk[R95]
//	R95 = memblk for string buffer
// outputs:
//	R39 = width
//	R38 = height
//	R37 = height of non-descending portion of font
	 case SYS_GETTEXTSIZE:
		pMem = (BYTE *)(GETREGD(4*GETREGB(REG_95)) + GETREGD(REG_14));
		TextSizeBMB(pMem, &w, &h, &asc);

		SETREGW(REG_39, w);	// width of font
		SETREGW(REG_38, h);	// height of font
		SETREGW(REG_37, asc);	// height of non-descending portion of font
		return;
//**** SYS_GETFONT
// inputs:
//	R13 = ptr to 512-byte buffer in memblk [R94] to be filled with font-widths (KINT 2.0)
//			set to NULL if not needed
//			256 byte-pairs, 1st byte is base-width, 2nd byte is cell-width
//			those are the same for non-italic fonts, different for italic fonts (usually)
//	R94 = memblk for font-width buffer (KINT 2.0)
//	R14 = string buffer address in memblk[R95]
//	R95 = memblk for string buffer
//	R39 = pixel width of rectangle to fit text into
//	R38 = pixel height of rectangle to fit text into
//	R37 =
//		BIT 0 =0 for variable pitch, =1 for fixed pitch
//		BIT 1 =0 for normal, =1 for BOLD
//		BIT 2 =0 for normal, =1 for ITALIC
// outputs:
//	R39 = handle of font
//	R38 = height of font
//	R37 = ascending height of font
	 case	SYS_GETFONT:
		if( GETREGW(REG_38)==0 ) w = GETREGW(REG_39);	// getting info for specific font # in R39 ADDED: KINT 3.0
		else {
			pMem = (BYTE *)(GETREGD(4*GETREGB(REG_95)) + GETREGD(REG_14));
			w = FitFont(pMem, GETREGW(REG_39), GETREGW(REG_38), GETREGW(REG_37));
		}
		SETREGW(REG_39, w);
		SETREGW(REG_38, *((WORD *)(pFontData + pFontTable[w])));	// height of font
		SETREGW(REG_37, *((WORD *)(pFontData + pFontTable[w] + 2)));// height of non-descending portion of font
		if( pKE->pKode[KHDR_MAJOR_INTERP_REV] >= 2 && GETREGD(REG_13)!=0 ) {	// KINT 2.0
			pMem = (BYTE *)(GETREGD(4*GETREGB(REG_94)) + GETREGD(REG_13));	// where to put widths
			a = *((BYTE *)(pFontData + *((DWORD *)(pFontData+pFontTable[w]+6+4*(127-32)))));
			b = *((BYTE *)(pFontData + *((DWORD *)(pFontData+pFontTable[w]+6+4*(127-32)))+3));
			for(i=0; i<32; i++) {
				*pMem++ = b;
				*pMem++ = a;
			}
			for(; i<128; i++) {
				*pMem++ = *((BYTE *)(pFontData + *((DWORD *)(pFontData+pFontTable[w]+6+4*(i-32)))+3));
				*pMem++ = *((BYTE *)(pFontData + *((DWORD *)(pFontData+pFontTable[w]+6+4*(i-32)))));
			}
			for(; i<160; i++) {
				*pMem++ = b;
				*pMem++ = a;
			}
			for(; i<256; i++) {
				*pMem++ = *((BYTE *)(pFontData + *((DWORD *)(pFontData+pFontTable[w]+6+4*(i-64)))+3));
				*pMem++ = *((BYTE *)(pFontData + *((DWORD *)(pFontData+pFontTable[w]+6+4*(i-64)))));
			}
		}
		return;
//**** SYS_RELFONT
// inputs:
//	R39 = handle of font to release
	 case	SYS_RELFONT:
		// nothing to do with "internal" fonts
		return;
//**** SYS_BEGTEXT
// save whatever states you need to save regarding text output for "ddc"
// so that they can be restored at SYS_ENDTEXT
// inputs:
	 case	SYS_BEGTEXT:
		Moldie[MoldCnt].BkColor = CurBackColor;
		Moldie[MoldCnt].TextColor = CurTextColor;
		Moldie[MoldCnt].Font = CurFont;
		if( MoldCnt < MAXFONTS-1 ) ++MoldCnt;
		return;
//**** SYS_ENDTEXT
// restore whatever states need to be restored that were saved at SYS_BEGTEXT
	 case	SYS_ENDTEXT:
		if( MoldCnt ) --MoldCnt;
		CurBackColor = Moldie[MoldCnt].BkColor;
		CurTextColor = Moldie[MoldCnt].TextColor;
		CurFont = Moldie[MoldCnt].Font;
		return;
//**** SYS_TEXTSTYLE
//TextStyle(backcolor=-1 if transparent, textcolor, font, align)
// inputs:
//	R14 = backcolor (-1 if transparent, otherwise opaque)
//	R13 = textcolor
//	R39 = font handle
	 case	SYS_TEXTSTYLE:
		CurBackColor = GETREGD(REG_14);
		CurTextColor = GETREGD(REG_13);
		CurFont = GETREGW(REG_39);
		return;
//**** SYS_TEXT
// inputs:
//	R15 = bitmap handle (NULL==screen)
//	R14 = string buffer address in memblk[R95]
//	R95 = memblk for string buffer
//	R39 = x-coordinate of point
//	R38 = y-coordinate of point
	 case	SYS_TEXT:
		pMem = (BYTE *)(GETREGD(4*GETREGB(REG_95)) + GETREGD(REG_14));
		TextBMB((PBMB)GETREGD(REG_15), (int)(short)GETREGW(REG_39), (int)(short)GETREGW(REG_38), pMem);
		return;
//**** SYS_PLAYSOUND
// Inputs:
//	R15 = offset to memblock[R95] memory containing filename of sound to play
//	R95 = 0 if R0, 1 if R1, 2 if R2
// KINT 3.4
//	if R95 > 2, then:
//		R95 = 3 + (0,1,2 memblock), ie, 3,4, or 5
//		R15 = offset to memblock[R95-3] memory containing RAM copy of a ".wav" file to play
//		R96 = non-0 if don't play if another sound is already playing
	 case	SYS_PLAYSOUND:
		if( GETREGB(REG_95)<3 ) KPlaySound((char *)GETREGD(4*(DWORD)GETREGB(REG_95)) + GETREGD(REG_15));
		else KPlaySoundMem((char *)GETREGD(4*(DWORD)(GETREGB(REG_95)-3)) + GETREGD(REG_15), (DWORD)GETREGB(REG_96));
		return;
//**** SYS_PLAYMUSIC
// Inputs:
//	R15 = offset to memblock[R95] memory containing filename of music to play
//	R95 = 0 if R0, 1 if R1, 2 if R2
	 case	SYS_PLAYMUSIC:
		PlayMusic((char *)GETREGD(4*(DWORD)GETREGB(REG_95)) + GETREGD(REG_15));
		return;
//**** SYS_STOPMUSIC
	 case	SYS_STOPMUSIC:
		StopMusic();
		return;
//**** SYS_ISMUSIC
// outputs:
//	R39=0 if no music playing, !=0 if music IS playing
	 case	SYS_ISMUSIC:
		SETREGW(REG_39, MusicPlaying);
		return;
//**** SYS_STARTTIMER
// inputs:
//	R15 = timer period in milliseconds
	 case	SYS_STARTTIMER:
		StartTimer(GETREGD(REG_15));
		return;
//**** SYS_STOPTIMER
	 case	SYS_STOPTIMER:
		StopTimer();
		return;
//**** SYS_GETEOL
// inputs:
//	R15 = address in memblk R0 of where to push the OS's EOL sequence.
//		On MS-Windows, this pushes out a 13 and a 10.
//		On Linux and other OS's, it probably will push out ONLY a 10.
	 case SYS_GETEOL:
		SETREGD(REG_15, GETREGD(REG_15) + GetSystemEOL((BYTE *)(GETREGD(REG_0)) + GETREGD(REG_15)));
		return;
//**** SYS_SYSTEMMSG
// inputs:
//	R15 = address of string in memblk R0
	 case SYS_SYSTEMMSG:
		SystemMessage((char *)GETREGD(REG_0) + GETREGD(REG_15));
		return;
//**** SYS_LOADKE (KINT 2.0)
// inputs:
//	R15 = address in memblk R95 of filename (WITHOUT the .KE extension) (may include sub-folder path)
//	R95 = memblk for R15
	 case SYS_LOADKE:
		if( NULL!=(pMem2=(BYTE *)malloc(sizeof(KE))) ) {
			memset(pMem2, 0, sizeof(KE));
			pMem = (BYTE *)GetFileName((char *)(GETREGD(4*(DWORD)GETREGB(REG_95)) + GETREGD(REG_15)), ".ke");
			if( LoadKEFile((char *)pMem, (KE *)pMem2) ) {
				SETREGD(REG_12, (DWORD)pMem2);
				SendMsgKE((KE *)pMem2, pKE, KHDR_BEGIN_STARTUP, (DWORD)&mainKE, 0, 0);
			} else {
				free(pMem2);
				pMem2 = NULL;
			}
		}
		SETREGD(REG_12, (DWORD)pMem2);
		return;
//**** SYS_UNLOADKE (KINT 2.0)
// inputs:
//	R12 = KE handle to unload
	 case SYS_UNLOADKE:
		pMem = (BYTE *)GETREGD(REG_12);
		SendMsgKE((KE *)pMem, pKE, KHDR_SHUT_DOWN, (DWORD)&mainKE, 0, 0);
#if KINT_DEBUG
		if( DbgMem!=NULL && KDBG_HERE ) {
			KDBG_LOADKE_KENUM = (DWORD)((KE *)pMem)->KEnum;
			Ksig(KSIG_UNLOADKE);
		}
#endif
		free(((KE *)pMem)->pKode);
		free(pMem);
		return;
//**** SYS_MSGKE (KINT 2.0)
// inputs:
//	R32 = msg (@vector table offset, see KHDR_ #defines in PMACH.H)
//	R15 = arg1
//	R14 = arg2
//	R13 = arg3
//	R12 = target KE handle (as returned by SysLoadKE, etc)
// Sets R11 = caller's KE handle before passing the message to the target.
// outputs:
//	all registers as left by target .KE (R39=value set before target's HALT instruction, etc)
	 case SYS_MSGKE:
		a = signedmath;	// save state of SM flag
		SendMsgKE((KE *)GETREGD(REG_12), pKE, GETREGW(REG_32), GETREGD(REG_15), GETREGD(REG_14), GETREGD(REG_13));
		signedmath = a;	// restore state of SM flag
#if KINT_DEBUG
		UpdateBrkpts(pKE);
#endif
		return;
	}
}

//***************************************************************
// Find a font such that "text" will be as large as possible
// while still fitting within a rectangle of width "w" and height "h"
// and matching the attribute "flags".
WORD FitFont(BYTE *text, WORD w, WORD h, WORD flags) {

	int		i, i1;
	WORD	*pF;
	WORD	tw, th, ta, saveCF;

	if( NumFonts==0 ) return 0;	// to save us from checking all the time within this function

	saveCF = CurFont;
	for(i=0; i<NumFonts; i++) {
		FontSelection[i] = 0;
		pF = (WORD *)(pFontData + pFontTable[i]);
		th = pF[0];
		if( th <= h ) {	// if height is <= desired height
			th = pF[2];
			if( !((th ^ flags) & 1) ) {// if correct FIXED/VARIABLE pitch
				FontSelection[i] = 1;	// mark as a possibility
				if( !((th ^ flags) & 6) ) FontSelection[i] = 2; // if correct BOLD/ITALICS
				CurFont = (WORD)i;
				TextSizeBMB(text, &tw, &th, &ta);
				if( tw>w ) FontSelection[i] = 0;
			}
		}
	}
	CurFont = saveCF;

	// Fonts are assumed to be sorted in order of increasing height within the FONTS file.
	// So, start at the top and work down, remembering the first file with FontSelection[]==1
	// and TAKING the first file with FontSelection[]==2.  If no 2's are found, and there was
	// a '1', take the first '1'.  Else, select font 0 as the default, which should be the
	// smallest, variable pitch font.

	i1 = 0;
	for(i=NumFonts-1; i>=0; i--) {
		if( FontSelection[i]==2 ) return i;
		if( i1==0 && FontSelection[i]==1 ) i1 = i;
	}
	if( i1 ) return i1;

	return 0;
}

//***********************************************************
//* SysStartup() is called from MAIN during startup, and is *
//* a chance to do any one-time initialization stuff.       *
//***********************************************************
void SysStartup() {

	DWORD	FileSize, BytesRead, hFile;

	NumFonts = 0;
	// Open the file: read access, prohibit write access
	hFile = Kopen("fonts", O_RDBIN, DIR_GAME);
	if( -1 != hFile ) {
//		SEEK_SET    0
//		SEEK_CUR    1
//		SEEK_END    2
		FileSize = Kseek(hFile, 0, 2);	// get the file size
		Kseek(hFile, 0, 0);	// move back to start of file
		if( NULL!=(pFontData=(BYTE *)malloc(FileSize)) ) {
			BytesRead = Kread(hFile, pFontData, FileSize);
			if( BytesRead==FileSize ) {
				NumFonts = *((WORD *)pFontData);
				pFontTable = (DWORD *)(pFontData+2);
			} else {
				free(pFontData);
			}
		}
		Kclose(hFile);
	} else {
		SystemMessage("FONTS file missing");
		exit(0);
	}
}

//***********************************************************
//* SysCleanup() is called when KINT is about to exit.		*
//* It is responsible for deleting/freeing all resources	*
//* which were allocated by SYS_ opcodes.					*
//***********************************************************
void SysCleanup() {

	PBMB	pBM, pBMnext;

	for(pBM=pbFirst; pBM!=NULL; ) {
		pBMnext = (PBMB)pBM->pNext;
		DeleteBMB(pBM);
		pBM = pBMnext;
	}
	if( pFontData!=NULL ) free(pFontData);
}

