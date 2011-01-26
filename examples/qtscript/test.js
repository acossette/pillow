var Sinatra = {};
Sinatra.App = function(setupFunc)
{
	this.routes = [];
	if (setupFunc instanceof Function) setupFunc.call(this);
};
Sinatra.App.prototype = 
{
	addRoute: function(method, path)
	{
		var func = arguments[arguments.length - 1];
		if (!func instanceof Function) throw new Error("addRoute: last argument should be a function");
		
		var paramNames = [];
		var param;

		var paramRegex = /:(\w+)/g;
		while (param = paramRegex.exec(path))
			paramNames.push(param[1]);
		
		var splatRegex = /\*(\w+)/g;
		while (param = splatRegex.exec(path))
			paramNames.push(param[1]);
		
		path = path.replace(paramRegex, '([\\w_-]+?)');
		path = path.replace(splatRegex, '(.*)');
		
		this.routes.push({
				method: new RegExp(method, 'i'), 
				path: new RegExp("^" + path + "/?$"), 
				paramNames: paramNames, 
				func: func
			});			
	},
	
	get: function(path, func) { this.addRoute('GET', path, func); },
	post: function(path, func) { this.addRoute('POST', path, func); },
	put: function(path, func) { this.addRoute('PUT', path, func); },
	'delete': function(path, func) { this.addRoute('DELETE', path, func); },
	
	handleRequest: function(request)
	{
		var handler = new Object();
		handler.__proto__ = this;
		handler.request = request;
		routes = handler.routes || [];
		for (var i = 0; i < routes.length; ++i)
		{
			var route = routes[i];
			var match = request.requestMethod.match(route.method) && request.requestPath.match(route.path);
			if (match)
			{
				handler.params = {};
				for (var p in request.requestQueryParams)
					handler.params[p] = request.requestQueryParams[p];
				for (var j = 0; j < route.paramNames.length; ++j)
					handler.params[route.paramNames[j]] = match[j + 1];
				
				var result = route.func.call(handler);
				
				if (result instanceof Array)
					request.nativeRequest.writeResponseString(result[0], result[1], result[2]);					
				else if (typeof result == "string")
					request.nativeRequest.writeResponseString(200, {}, result);					
				else if (typeof result == "number")
					request.nativeRequest.writeResponseString(result);					

				return true;
			}
		}
		
		return false;
	}
};

var sinatraApp = new Sinatra.App(function()
{
	this.get('/', function()
	{
		return [200, {'Content-Type': 'text/html'},"<html><body><form method='post' enctype='multipart/form-data'><input type='file' name='somefile'><input type='text' name='sometext'><input type='submit'></form></body></html>"];
	});

	this.post('/', function()
	{
		return "Thanks for your data!";
	});

	this.put('/', function()
	{
		return "Thanks for your data!";
	});

	this.get('/hello', function()
	{
		return [200, {}, "Hello there!"];
	});

	this.get('/world/:id/:other/:another', function()
	{
		var output = "";
		for (var p in this.params)
			output += p + ": " + this.params[p] + "\n";
		return [200, {}, output];
	});
	
	this.get('/world/:id', function()
	{
		return [200, {}, "World with id:" + this.params['id']];
	});

	
	this.get('/world', function()
	{
		return [302, {'Location': '/hello'}, ""];
	});

	this.get('/splatted/*yeah', function()
	{
		var output = "";
		for (var p in this.params)
			output += p + ": " + this.params[p] + "\n";
		return [200, {}, output];
	});	

	this.get('/doublesplat/*yeah/*hehe', function()
	{
		var output = "";
		for (var p in this.params)
			output += p + ": " + this.params[p] + "\n";
		return [200, {}, output];
	});	
});

function handleRequest(request)
{
	return sinatraApp.handleRequest(request);
}
