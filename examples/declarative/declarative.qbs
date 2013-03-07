import qbs.base 1.0

Application {
	files : ["declarative.cpp"]
	Depends { name: "Qt"; submodules: ["core", "declarative"] }
	Depends { name: "pillowcore" }
}

