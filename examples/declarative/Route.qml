import Pillow 1.0

RouteMatcher {
	signal request(variant request)
	
	function handleRequest(request)
	{
		if (match(request))
		{
			this.request(request);
			return true;
		}
		return false;
	}
}
