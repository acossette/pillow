import qbs.base 1.0

Application {
	files : ["fileserver.cpp"]
	Depends { name: "Qt"; submodules: ["core"] }
	Depends { name: "pillowcore" }
}

