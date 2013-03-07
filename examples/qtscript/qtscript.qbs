import qbs.base 1.0

Application {
	files : ["qtscript.cpp"]
	Depends { name: "Qt"; submodules: ["core", "script"] }
	Depends { name: "pillowcore" }
}

