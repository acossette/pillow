import qbs.base 1.0

Application {
    files : [
        "Helpers.h", "HttpConnectionTest.h", "HttpHandlerProxyTest.h", "HttpHandlerTest.h", "HttpServerTest.h", "HttpsServerTest.h",
        "main.cpp", "ByteArrayHelpersTest.cpp", "HttpConnectionTest.cpp", "HttpHandlerProxyTest.cpp", "HttpHandlerTest.cpp", "HttpHeaderTest.cpp", "HttpServerTest.cpp", "HttpsServerTest.cpp"
    ]
    Depends { name: "cpp" }
    Depends { name: "Qt"; submodules: ["core", "network", "declarative", "script", "test"] }
    Depends { name: "pillowcore" }
}

