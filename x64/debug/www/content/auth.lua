package.path="www/lib/?.lua"

require( 'template' )
local h = require( 'html' )

local output = h.Document{
	lang = "en",
  H_head( "Viessmann API Test" ),

  H_body{
    "auth processing goes here"
  }
}
Render( 'text/html', tostring( output ) ) -- set by caller to render into response
