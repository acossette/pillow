import QtQuick 1.0
import Pillow 1.0

Rectangle {
	
	HttpServer {
		id: server
		port: 4567
		
		onListeningChanged: if (listening) console.log("Server is listening on port", port)		
		onRequest: {
//            request.writeResponseString(200, undefined, "hello")
			console.log("PillowDream.HttpServer", request.requestParamValue("popo"));

			for (var i = 0, iE = data.length; i < iE; ++i)
			{
				var child = data[i];
				if (child.handleRequest && child.handleRequest(request))
					return;
			}
			console.log("PillowDream.HttpServer: request for url", request.requestUri, "was not handled");
		}		
				
		HttpHandlerLog {}
		HttpHandlerFile { publicPath: "/home/acossette/Public" }

		property variant headersSet: server.makeHeaders({poops: "yeeeha", momo: true})
	
        
//        Route {
//			path: "/"
//            onRequest: request.writeResponseString(200, undefined, "hello")
//		}
        
//		Route {
//			path: "/hello"
//			onRequest: request.writeResponse(200, server.makeHeaders({poops: "yeah", momo: true, tata:2}), "hello")
//		}
		
		Route {
			path: "/poops/:yeah"
			onRequest: request.writeResponseString(200, server.headersSet, "world" + request.requestParamValue['yeah'])
		}
    
		Route {
			path: "/other"
			onRequest: request.writeResponseString(200, undefined, "other")
		}

		Route {
			path: "/1"
			onRequest: request.writeResponseString(200, undefined, "other")
		}

		Route {
			path: "/2"
			onRequest: request.writeResponseString(200, undefined, "other")
		}

		Route {
			path: "/3"
			onRequest: request.writeResponseString(200, undefined, "other")
		}

		Route {
			path: "/4"
			onRequest: request.writeResponseString(200, undefined, "other")
		}

		Route {
			path: "/5"
			onRequest: request.writeResponseString(200, undefined, "other")
		}

		Route {
			path: "/6"
			onRequest: request.writeResponseString(200, undefined, "other")
		}

		Route {
			path: "/7"
			onRequest: request.writeResponseString(200, undefined, "other")
		}

		Route {
			path: "/8"
			onRequest: request.writeResponseString(200, undefined, "other")
		}

		Route {
			path: "/9"
			onRequest: request.writeResponseString(200, undefined, "other")
		}

		Route {
			path: "/10"
			onRequest: request.writeResponseString(200, undefined, "other")
		}
		
		HttpHandler404 {}
	}	
}
