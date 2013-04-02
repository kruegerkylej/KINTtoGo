/*******************************************

 PMACH.H - The KINT Interpreter
	Copyright 2003 Everett Kaser
	All rights reserved.

	Do not copy or redistribute without the
	author's explicit written permission.
 *******************************************/

#ifndef PMACH_INCLUDED
 #define PMACH_INCLUDED
//***************************
//* KINT version numbers
//***************************
#if BETA
 #define	INTERP_MAJOR_REV	132
 #define	INTERP_MINOR_REV	0
#else
 #define	INTERP_MAJOR_REV	4
 #define	INTERP_MINOR_REV	3
#endif

//*********************************************************************************
//* The following macros are used in PMACH.CPP in order to make
//* it easier for porting this code to "big-endian" CPUs.  "All" you have to do
//* is rewrite these macros, and MOST of the little-to-big-endian conversion will
//* be done.  Although, NO DOUBT, I've used these macros in some places where
//* I shouldn't have, and haven't used them in some places where I should have.
//* BEWARE!
//*********************************************************************************
#if KINT_DESKTOP_WINDOWS
	#define GETREGD(regoff)			(*((DWORD *)(Kregs + (regoff))))
	#define	GETREGW(regoff)			(*(( WORD *)(Kregs + (regoff))))
	#define GETREGB(regoff)			(*(( BYTE *)(Kregs + (regoff))))

	#define	SETREGD(regoff, value)	(*((DWORD *)(Kregs + (regoff))) = (DWORD)(value))
	#define	SETREGW(regoff, value)	(*(( WORD *)(Kregs + (regoff))) = ( WORD)(value))
	#define SETREGB(regoff, value)	(*(( BYTE *)(Kregs + (regoff))) = ( BYTE)(value))

	#define	GETMEMD(addr)			(*((DWORD *)(addr)))
	#define	GETMEMW(addr)			(*(( WORD *)(addr)))
	#define	GETMEMB(addr)			(*(( BYTE *)(addr)))

	#define	SETMEMD(addr, value)	(*((DWORD *)(addr)) = (DWORD)(value))
	#define	SETMEMW(addr, value)	(*(( WORD *)(addr)) = ( WORD)(value))
	#define	SETMEMB(addr, value)	(*(( BYTE *)(addr)) = ( BYTE)(value))
#endif
#if KINT_POCKET_PC
  #ifdef CHECKWORDALIGN
	#define GETREGD(regoff)			((DWORD)(*((UNALIGNED DWORD *)(Kregs + (regoff)))))
	//#define GETREGD(regoff)			((DWORD)(*((WORD *)(Kregs + (regoff)))) | (((DWORD)(*((WORD *)(Kregs + (regoff))+1)))<<16))
	#define	GETREGW(regoff)			(( WORD)(*(( WORD *)(Kregs + (regoff)))))
	#define GETREGB(regoff)			(( BYTE)(*(( BYTE *)(Kregs + (regoff)))))

	#define	SETREGD(regoff, value)	((DWORD)(*((UNALIGNED DWORD *)(Kregs + (regoff))) = (DWORD)(value)))
	//#define	SETREGD(regoff, value)	 ((*(( WORD *)(Kregs + (regoff))) = ( WORD)(value)),(*((( WORD *)(Kregs + (regoff))+1)) = ( WORD)(((DWORD)(value))>>16)),value)
	#define	SETREGW(regoff, value)	(( WORD)(*(( WORD *)(Kregs + (regoff))) = ( WORD)(value)))
	#define SETREGB(regoff, value)	(( BYTE)(*(( BYTE *)(Kregs + (regoff))) = ( BYTE)(value)))

	#define	GETMEMD(addr)			((DWORD)(*((UNALIGNED DWORD *)(addr))))
	//#define	GETMEMD(addr)			((DWORD)(*((WORD *)(addr))) | (((DWORD)(*((WORD *)(addr)+1)))<<16))
	//#define	GETMEMW(addr)			(( WORD)(*((UNALIGNED WORD *)(addr))))
//	#define	GETMEMW(addr)			((((DWORD)(addr))&1)?(wsprintf(TmpPath, L"GODD %8.8X",(DWORD)((BYTE *)PC-pKode),MessageBox(NULL, TmpPath, L"DEBUG", MB_OK))):(( WORD)(*(( WORD *)(addr)))))
//	#define	GETMEMWNONPC(addr)			(( WORD)(*(( WORD *)(addr))))
	#define	GETMEMB(addr)			(( BYTE)(*(( BYTE *)(addr))))

	#define	SETMEMD(addr, value)	((DWORD)(*((UNALIGNED DWORD *)(addr)) = (DWORD)(value)))
	//#define	SETMEMD(addr, value) (*(( WORD *)(addr)) = ( WORD)(value));(*((( WORD *)(addr)+1)) = ( WORD)(((DWORD)(value))>>16))
	//#define	SETMEMW(addr, value)	(( WORD)(*((UNALIGNED WORD *)(addr)) = ( WORD)(value)))
//	#define	SETMEMW(addr, value)	((((DWORD)(addr))&1)?(wsprintf(TmpPath, L"GODD %8.8X",(DWORD)((BYTE *)PC-pKode),MessageBox(NULL, TmpPath, L"DEBUG", MB_OK))):(( WORD)(*(( WORD *)(addr)) = ( WORD)(value))))
//	#define	SETMEMWNONPC(addr, value)	(( WORD)(*(( WORD *)(addr)) = ( WORD)(value)))
	#define	SETMEMB(addr, value)	(( BYTE)(*(( BYTE *)(addr)) = ( BYTE)(value)))

	#ifdef defgetmemw
		void SystemMessage(char *);

		WORD SETMEMW(WORD *addr, WORD value) {

			char	xxx[64];
			
			if( (DWORD)addr & 1 ) {
				sprintf(xxx, "S Odd W-addr=%8.8X", savePC);
				SystemMessage(xxx);
			} else return (*addr = value);
			return 0;
		}


		WORD GETMEMW(WORD *addr) {

			char	xxx[64];

			if( (DWORD)addr & 1 ) {
				sprintf(xxx, "G Odd W-addr=%8.8X", savePC);
				SystemMessage(xxx);
			} else return *addr;
			return 0;
		}
	#endif
  #else
	#define GETREGD(regoff)			((DWORD)(*((UNALIGNED DWORD *)(Kregs + (regoff)))))
	//#define GETREGD(regoff)			((DWORD)(*((WORD *)(Kregs + (regoff)))) | (((DWORD)(*((WORD *)(Kregs + (regoff))+1)))<<16))
	#define	GETREGW(regoff)			(( WORD)(*(( WORD *)(Kregs + (regoff)))))
	#define GETREGB(regoff)			(( BYTE)(*(( BYTE *)(Kregs + (regoff)))))

	#define	SETREGD(regoff, value)	((DWORD)(*((UNALIGNED DWORD *)(Kregs + (regoff))) = (DWORD)(value)))
	//#define	SETREGD(regoff, value)	 ((*(( WORD *)(Kregs + (regoff))) = ( WORD)(value)),(*((( WORD *)(Kregs + (regoff))+1)) = ( WORD)(((DWORD)(value))>>16)),value)
	#define	SETREGW(regoff, value)	(( WORD)(*(( WORD *)(Kregs + (regoff))) = ( WORD)(value)))
	#define SETREGB(regoff, value)	(( BYTE)(*(( BYTE *)(Kregs + (regoff))) = ( BYTE)(value)))

	#define	GETMEMD(addr)			((DWORD)(*((UNALIGNED DWORD *)(addr))))
	//#define	GETMEMD(addr)			((DWORD)(*((WORD *)(addr))) | (((DWORD)(*((WORD *)(addr)+1)))<<16))
	#define	GETMEMW(addr)			(( WORD)(*((UNALIGNED WORD *)(addr))))
	//#define	GETMEMW(addr)			(( WORD)(*(( WORD *)(addr))))
//	#define	GETMEMWNONPC(addr)		(( WORD)(*(( WORD *)(addr))))
	#define	GETMEMB(addr)			(( BYTE)(*(( BYTE *)(addr))))

	#define	SETMEMD(addr, value)	((DWORD)(*((UNALIGNED DWORD *)(addr)) = (DWORD)(value)))
	//#define	SETMEMD(addr, value) (*(( WORD *)(addr)) = ( WORD)(value));(*((( WORD *)(addr)+1)) = ( WORD)(((DWORD)(value))>>16))
	#define	SETMEMW(addr, value)	(( WORD)(*((UNALIGNED WORD *)(addr)) = ( WORD)(value)))
	//#define	SETMEMW(addr, value)	(( WORD)(*((UNALIGNED WORD *)(addr)) = ( WORD)(value)))
//	#define	SETMEMWNONPC(addr, value)	(( WORD)(*(( WORD *)(addr)) = ( WORD)(value)))
	#define	SETMEMB(addr, value)	(( BYTE)(*(( BYTE *)(addr)) = ( BYTE)(value)))
  #endif
#endif

//**********************************************
//* "cmd" (event) offsets into a .KE file header
//**********************************************
#define	KHDR_NULLPTR			0
#define	KHDR_MAJOR_INTERP_REV	6
#define	KHDR_MINOR_INTERP_REV	7
#define	KHDR_CODE_SIZE			8
#define	KHDR_ZERODATA_SIZE		12
#define	KHDR_STACK_SIZE			16
#define	KHDR_UNUSED1			20
#define	KHDR_UNUSED2			24
#define	KHDR_UNUSED3			28
#define	KHDR_UNUSED4			32
#define	KHDR_UNUSED5			36
#define	KHDR_UNUSED6			40
#define	KHDR_UNUSED7			44
#define	KHDR_UNUSED8			48
#define	KHDR_UNUSED9			52
#define	KHDR_UNUSED10			56
#define	KHDR_SCREEN_CHANGE		60
#define	KHDR_ALPHAKEY_DOWN		64
#define	KHDR_ALPHAKEY_UP		68
#define	KHDR_BEGIN_STARTUP		72
#define	KHDR_END_STARTUP		76
#define	KHDR_WINDOW_HIDDEN		80
#define	KHDR_WINDOW_MOVE		84
#define	KHDR_WINDOW_SIZE		88
#define	KHDR_POINTER_MOVE		92
#define	KHDR_KEY_DOWN			96
#define	KHDR_KEY_UP				100
#define	KHDR_CHAR				104
#define	KHDR_TIMER				108
#define	KHDR_IDLE				112
#define	KHDR_MUSIC_DONE			116
#define	KHDR_QUERY_QUIT			120
#define	KHDR_SHUT_DOWN			124

//*************************************
//* offsets in REGISTER BYTE array for
//* each numbered register.
//*************************************
#define	REG_0	0
#define	REG_1	4
#define	REG_2	8
#define	REG_3	12
#define	REG_4	16
#define	REG_5	20
#define	REG_6	24
#define	REG_7	28
#define	REG_8	32
#define	REG_9	36
#define	REG_10	40
#define	REG_11	44
#define	REG_12	48
#define	REG_13	52
#define	REG_14	56
#define	REG_15	60
#define	REG_16	64
#define	REG_17	68
#define	REG_18	72
#define	REG_19	76
#define	REG_20	80
#define	REG_21	84
#define	REG_22	88
#define	REG_23	92
#define	REG_24	96
#define	REG_25	100
#define	REG_26	104
#define	REG_27	108
#define	REG_28	112
#define	REG_29	116
#define	REG_30	120
#define	REG_31	124
#define	REG_32	128
#define	REG_33	130
#define	REG_34	132
#define	REG_35	134
#define	REG_36	136
#define	REG_37	138
#define	REG_38	140
#define	REG_39	142
#define	REG_40	144
#define	REG_41	146
#define	REG_42	148
#define	REG_43	150
#define	REG_44	152
#define	REG_45	154
#define	REG_46	156
#define	REG_47	158
#define	REG_48	160
#define	REG_49	162
#define	REG_50	164
#define	REG_51	166
#define	REG_52	168
#define	REG_53	170
#define	REG_54	172
#define	REG_55	174
#define	REG_56	176
#define	REG_57	178
#define	REG_58	180
#define	REG_59	182
#define	REG_60	184
#define	REG_61	186
#define	REG_62	188
#define	REG_63	190
#define	REG_64	192
#define	REG_65	194
#define	REG_66	196
#define	REG_67	198
#define	REG_68	200
#define	REG_69	202
#define	REG_70	204
#define	REG_71	206
#define	REG_72	208
#define	REG_73	210
#define	REG_74	212
#define	REG_75	214
#define	REG_76	216
#define	REG_77	218
#define	REG_78	220
#define	REG_79	222
#define	REG_80	224
#define	REG_81	225
#define	REG_82	226
#define	REG_83	227
#define	REG_84	228
#define	REG_85	229
#define	REG_86	230
#define	REG_87	231
#define	REG_88	232
#define	REG_89	233
#define	REG_90	234
#define	REG_91	235
#define	REG_92	236
#define	REG_93	237
#define	REG_94	238
#define	REG_95	239
#define	REG_96	240
#define	REG_97	241
#define	REG_98	242
#define	REG_99	243
#define	REG_100	244
#define	REG_101	245
#define	REG_102	246
#define	REG_103	247
#define	REG_104	248
#define	REG_105	249
#define	REG_106	250
#define	REG_107	251
#define	REG_108	252
#define	REG_109	253
#define	REG_110	254
#define	REG_111	255

//********************************
//* SysFunc sub-function numbers *
//********************************
#define	SYS_FILESIZE	0x00
#define	SYS_FOPEN		0x01
#define	SYS_FEOF		0x02
#define	SYS_FREAD		0x03
#define	SYS_FWRITE		0x04
#define	SYS_FSEEK		0x05
#define	SYS_FCLOSE		0x06
#define	SYS_FFINDFIRST	0x07
#define	SYS_FFINDNEXT	0x08
#define	SYS_FFINDCLOSE	0x09
#define	SYS_DELETE		0x0A
#define	SYS_MAKESHARECOPY	0x0B
#define	SYS_PRINTSTART	0x0C
#define	SYS_PRINTBEGPAGE 0x0D
#define	SYS_PRINTENDPAGE 0x0E
#define	SYS_PRINTSTOP	0x0F

#define	SYS_ALLOCMEM	0x10
#define	SYS_FREEMEM		0x11
#define	SYS_TIMEREG		0x12
#define	SYS_TIMERSTR	0x13
#define	SYS_TIMEMILLI	0x14
#define	SYS_STRCAT		0x15
#define	SYS_MEMCPY		0x16
#define	SYS_MEMMOVE		0x17
#define	SYS_STRCPY		0x18
#define	SYS_STRCMP		0x19
#define	SYS_STRICMP		0x1A
#define	SYS_STRLEN		0x1B
#define	SYS_MEMSET		0x1C
#define	SYS_MEMCMP		0x1D
#define	SYS_SETCURSOR	0x1E
#define	SYS_SETPGMTITLE	0x1F

#define	SYS_CREBM		0x20
#define	SYS_DELBM		0x21
#define	SYS_ALPHABLEND	0x22
#if BETA
 #define	SYS_SIZEBM		0x28
 #define	SYS_DRAWBM		0x27
 #define	SYS_STRETCHBM	0x26
 #define	SYS_DRAWMBM		0x25
 #define	SYS_LOADBM		0x24
 #define	SYS_LOADMBM		0x23
#else
 #define	SYS_SIZEBM		0x23
 #define	SYS_DRAWBM		0x24
 #define	SYS_STRETCHBM	0x25
 #define	SYS_DRAWMBM		0x26
 #define	SYS_LOADBM		0x27
 #define	SYS_LOADMBM		0x28
#endif
#define	SYS_STRETCHMBM	0x29	// NOTE: This opcode was added at REV 2.0 of KINT

#define SYS_GETWINDOWPOS 0x2A
#define	SYS_SETWINDOWPOS 0x2B
#define	SYS_GETVIEWSIZE 0x2C
#define	SYS_ORIENTBM	0x2D
#define	SYS_GETSCREENSIZE 0x2E
#define	SYS_FLUSHSCREEN	0x2F

#define	SYS_RECTEDGED	0x30
#define	SYS_RECTSOLID	0x31
#define	SYS_RECTOUTLINE	0x32
#define	SYS_LINES		0x33
#define	SYS_SETPIXEL	0x34
#define	SYS_GETPIXEL	0x35
#define	SYS_COLORIZE	0x36
#define	SYS_RECT		0x37
#define	SYS_OVAL		0x38
#define	SYS_GETTEXTSIZE	0x39
#define	SYS_GETFONT		0x3A
#define	SYS_RELFONT		0x3B
#define	SYS_BEGTEXT		0x3C
#define	SYS_TEXTSTYLE	0x3D
#define	SYS_TEXT		0x3E
#define	SYS_ENDTEXT		0x3F

#define	SYS_PLAYSOUND	0x40
#define	SYS_PLAYMUSIC	0x41
#define	SYS_STOPMUSIC	0x42
#define	SYS_ISMUSIC		0x43
#define	SYS_CAPTUREMOUSE 0x44
#define	SYS_MEMSET2		0x45
#define	SYS_MEMSET4		0x46
#define	SYS_PRINTFONT	0x47
#define	SYS_STARTTIMER	0x48
#define	SYS_STOPTIMER	0x49
#define	SYS_GETEOL		0x4A
#define	SYS_PRINTGETTEXTSIZE 0x4B
#define	SYS_PRINTTEXT	0x4C
#define	SYS_PRINTBM		0x4D
#define	SYS_PRINTLINES	0x4E
#define	SYS_SYSTEMMSG	0x4F

#define	SYS_LOADKE		0x50
#define	SYS_UNLOADKE	0x51
#define	SYS_MSGKE		0x52

#define	SYS_LOADLOSSYBM	0x54
#define	SYS_INVDRAWBM	0x55
#define	SYS_INVDRAWMBM	0x56
#define	SYS_LINESTYLE	0x57
#define	SYS_MAKEFOLDER	0x58
#define	SYS_DELFOLDER	0x59
#define	SYS_CLIP		0x5A
#define	SYS_GETCLIP		0x5B
#define	SYS_MASKBM		0x5C
#define	SYS_RGB2HSL		0x5D
#define	SYS_HSL2RGB		0x5E
#define	SYS_SAVEBM		0x5F

#define	SYS_PRINTRECT	0x60
#define	SYS_SLEEP		0x61
#define	SYS_GETSYSTEMDATA 0x62
#define	SYS_MEMSET200	0x63
#define	SYS_MEMSET400	0x64
#define	SYS_STRCAT00	0x65
#define	SYS_MEMCPY00	0x66
#define	SYS_MEMMOVE00	0x67
#define	SYS_STRCPY00	0x68
#define	SYS_STRCMP00	0x69
#define	SYS_STRICMP00	0x6A
#define	SYS_STRLEN00	0x6B
#define	SYS_MEMSET00	0x6C
#define	SYS_MEMCMP00	0x6D

//*****************************
//* function prototypes
//*****************************
int LoadKEFile(char *, KE *pKE);
WORD KEmsg(KE *pKE, WORD cmd, DWORD arg1, DWORD arg2, DWORD arg3);
WORD ExecKE(KE *pKE);
void SysFunc(KE *pKE, WORD opcode);
void SysStartup();
void SysCleanup();

// graphic function prototypes
void BltBMB(PBMB bmD, int xd, int yd, PBMB bmS, int xs, int ys, int ws, int hs, int masked, int invert);
void StretchBMB(PBMB bmD, int xd, int yd, int wd, int hd, PBMB bmS, int xs, int ys, int ws, int hs, int masked);
void LineBMB(PBMB bmD, int x1, int y1, int x2, int y2, DWORD clr);
void RectBMB(PBMB bmD, int x1, int y1, int x2, int y2, DWORD fill, DWORD edge);
void TextBMB(PBMB bmD, int x, int y, BYTE *str);
void TextSizeBMB(BYTE *str, WORD *w, WORD *h, WORD *a);
void PointBMB(PBMB bmD, int x, int y, DWORD clr);
PBMB LoadBMB(char *fname, int transX1, int transY1, int transX2, int transY2, BYTE dir);
WORD FitFont(BYTE *text, WORD w, WORD h, WORD fixedpitch);
void SOerr(PBMB kB);
PBMB LoadJPEG(char *jname, BYTE dir);

#if KINT_DEBUG
void KDebug(char *str);
#endif

#endif
