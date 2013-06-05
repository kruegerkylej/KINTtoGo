// KINT_GO project main.go
package main

import (
	"Github.com/AllenDang/w32"
	"KINT_Go/win" // windows interface routines
	"bytes"
	"io"
	"os"
	"path"
	"strings"
	"syscall"
)

// globals

var KINT_HELP bool = false
var szAppName = "KINT 3.0"
var KEname string
var PgmPath string
var EKSname string
var TmpPath string
var AppDataPath string
var KINTmsgs bytes.Buffer
var CopyAppData bool

const UM_GETPATH = 0X0400
const CSIDL_PERSONAL = 5
const MAXMSGS = 1024

// The following are the KASM/KINT file-open flag definitions.
// These are consistent with standard C library #define values
// but if this is being translated to another language, it's
// good to know what the KASM code is going to be passing you and
// what it means....
const O_RDBIN = 0x8000    // read only, binary
const O_WRBIN = 0x8002    // read/write, binary
const O_WRBINNEW = 0x8302 // read/write, binary, create or truncate

// The following are the KASM/KINT file-open directories

const DIR_GAME = 0
const DIR_UI = 1
const DIR_IMAGES = 2
const DIR_BACKGNDS = 3
const DIR_MUSIC = 4
const DIR_SOUNDS = 5
const DIR_GAME_IMG = 6
const DIR_UI_COMMON = 7
const DIR_LOSSY = 0x40
const DIR_NOEXT = 0x80

type KE struct {
	pKide      *byte  // ptr to memory block containing .KE
	memlen     uint16 // len of this .KE memory block
	StackBase  uint16 // offset in pKode of base of K stack for this .KE
	reg4, reg5 uint16 // save/init locations for SB, SP for this .KE
	KEnum      uint16
}

func main() {

	//506 If no command line argument, get path program was run from and save in PgmPath
	// ALL other file access by this program are relative to PgmPath.

	if len(os.Args) == 1 { // name and path of program only
		PgmPath = os.Args[0]
		if KINT_HELP {
			KEname = "help.ke"
		} else {
			KEname = "main.ke"
		}
	} else {
		KEname = os.Args[1] // absolute path to the .KE file to run
	}
	// 533 see if this is the only instance running

	tempPath := strings.Replace(PgmPath, "\\", "_", -1) // replace \ with _

	var null uintptr = 0
	var name string = tempPath
	param, err := syscall.UTF16PtrFromString(name)
	mutexhandle, err := win.CreateMutex(null, false, param)
	if err == nil {
		defer syscall.CloseHandle(syscall.Handle(mutexhandle)) // reset to nil on exit

	} else {
		// bring previous instance to front and exit
		win.EnumWindows(syscall.NewCallback(enumfunc), 0)

		//550 Set AppDataPath[] so we know where to create our files
		// os.getenv("LOCALAPPDATA")
		// 564  Allocate message buffer  No need to preallocate memory for buffer

	}

}

func enumfunc(hwnd syscall.Handle, lparam uintptr) uintptr {
	var tempPath [256]uint16
	namelen := win.GetClassName(hwnd, &tempPath[0], 256)
	// compare returned class to our class name
	// first convert byte array to string

	classname := syscall.UTF16ToString(tempPath[:namelen])
	if classname == szAppName {
		thisPath := win.SendMessage(hwnd, UM_GETPATH, 0, 0)
		sThisPath := string(thisPath)
		if sThisPath == PgmPath {
			win.SetForegroundWindow(hwnd)
			return 0
		}
	}
	return 1
}

//260 ********************************************************************
// Sets AppDataPath[] to absolute path into which to store Game-specific user modified files

func setAppDataPath() {

	const CSIDL_PERSONAL = 5
	var pid uintptr
	hResult := win.SHGetSpecialFolderLocation(0, CSIDL_PERSONAL, &pid)
	AppDataPath = w32.SHGetPathFromIDList(pid)
	if hResult == 0 || AppDataPath == "" {

		AppDataPath = PgmPath
		CopyAppData = false

	} else {
		EKSname = path.Dir(AppDataPath)
		GameName := path.Base(AppDataPath)
		AppDataPath = path.Join(AppDataPath, EKSname, GameName)
		os.MkdirAll(AppDataPath, os.ModeDir)
		CopyAppData = true
	}

}

func LoadKEFile(nameKE string) {

	//

}

func Kopen(fname string, oflags uint, dirSelector byte) (hFile *os.File, err error) {

	if oflags == O_RDBIN {
		BuildPath(fname, dirSelector, true)
		hFile, err = os.OpenFile(TmpPath, os.O_RDWR, 0)
		if err != nil {
			BuildPath(fname, dirSelector, false)
			hFile, err = os.OpenFile(TmpPath, os.O_RDWR, 0)
		}
	} else {
		BuildPath(fname, dirSelector, true)
		tmpbuf := TmpPath
		hFile, err = os.OpenFile(tmpbuf, os.O_RDWR, 0)
		if err != nil {
			// make sure target AppData path exists
			dirPath, _ := path.Split(tmpbuf) // cut off file name portion, just keep path portion
			_, err = os.Stat(dirPath)
			if err != nil {
				os.MkdirAll(dirPath, os.ModeDir)
			}
			BuildPath(fname, dirSelector, true)

			hFile, err = os.OpenFile(TmpPath, os.O_RDWR, 0)
			if err == nil {
				hFile.Close()
				KCopyFile(fname, tmpbuf)
			}
		} else {
			hFile.Close()
			hFile, err = os.OpenFile(tmpbuf, os.O_RDWR, 0)

		}
	}
	return hFile, err
}

func KCopyFile(src string, dest string) (err error) {
	BuildPath(src, DIR_GAME, false)
	srcFile, _ := os.OpenFile(TmpPath, os.O_RDWR, 0)
	destFile, _ := os.OpenFile(dest, os.O_RDWR, 0)
	_, err = io.Copy(destFile, srcFile)
	return
}

func BuildPath(pMem string, folder byte, AppData bool) {

	if AppData {
		TmpPath = AppDataPath
	} else {
		TmpPath = PgmPath
	}

	switch folder & 0x0f {

	case DIR_GAME:
		TmpPath = path.Join(TmpPath, GetFileName(pMem, ""))

	case DIR_UI:
		TmpPath = path.Join(TmpPath, "UI", pMem)
		if folder&DIR_NOEXT == 0 {
			if folder&DIR_LOSSY != 0 {
				TmpPath += ".JPG"
			} else {
				TmpPath += "BMP"
			}
		}

	case DIR_IMAGES:
		TmpPath = path.Join(TmpPath, "IMAGES", pMem)
		if folder&DIR_NOEXT == 0 {
			if folder&DIR_LOSSY != 0 {
				TmpPath += ".JPG"
			} else {
				TmpPath += "BMP"
			}
		}
	case DIR_BACKGNDS:
		TmpPath = path.Join(TmpPath, "BACKGROUNDS", pMem)
		if !(folder&DIR_NOEXT == 0) {
			if folder&DIR_LOSSY != 0 {
				TmpPath += ".JPG"
			} else {
				TmpPath += "BMP"
			}
		}

	case DIR_MUSIC:
		TmpPath = path.Join(TmpPath, "..", "MUSIC", pMem)
		if !(folder&DIR_NOEXT == 0) {
			TmpPath += "WAV"
		}

	case DIR_SOUNDS:
		TmpPath = path.Join(TmpPath, GetFileName(pMem, ""))
		if !(folder&DIR_NOEXT == 0) {
			TmpPath += "WAV"
		}

	case DIR_GAME_IMG:
		TmpPath = path.Join(TmpPath, GetFileName(pMem, ""))
		if !(folder&DIR_NOEXT == 0) {
			if folder&DIR_LOSSY != 0 {
				TmpPath += ".JPG"
			} else {
				TmpPath += "BMP"
			}
		}

	case DIR_UI_COMMON:
		TmpPath = path.Join(TmpPath, "..", "COMMONUI", pMem)
		if !(folder&DIR_NOEXT == 0) {
			if folder&DIR_LOSSY != 0 {
				TmpPath += ".JPG"
			} else {
				TmpPath += "BMP"
			}

		}
	}
}

func GetFileName(fileName string, xname string) string {
	if path.IsAbs(fileName) {
		return ""
	}
	partPath := path.Clean(fileName)
	partPath += xname
	return partPath
}
