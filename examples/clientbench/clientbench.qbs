import qbs.base 1.0

Application {
	files : ["clientbench.cpp"]
	Depends { name: "Qt"; submodules: ["core"] }
	Depends { name: "pillowcore" }
}

