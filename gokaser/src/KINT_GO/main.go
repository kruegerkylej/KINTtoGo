// KINT_GO project main.go
package main

import (
	"Github.com/AllenDang/w32"
	"KINT_Go/win" // windows interface routines
	"bytes"
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
var AppDataPath string
var KINTmsgs bytes.Buffer
var CopyAppData bool

const UM_GETPATH = 0X0400
const CSIDL_PERSONAL = 5
const MAXMSGS = 1024

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

func LoadKEFile(nameKE string)
