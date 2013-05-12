// KINT_GO project main.go
package main

import (
	"os"
	"strings"
	"win" // windows interface routines
)

// globals

var KINT_HELP bool = false

func main() {

	//506 If no command line argument, get path program was run from and save in PgmPath
	// ALL other file access by this program are relative to PgmPath.
	KEname := ""
	PgmPath := ""

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

	isRunning := os.Getenv(tempPath)
	if isRunning == "" {
		os.Setenv(tempPath, "Running")
		defer os.Setenv(tempPath, "") // reset to nil on exit
	} else { // bring previous instance to front and exit

	}
}
