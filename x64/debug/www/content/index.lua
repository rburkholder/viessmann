--
-- 2026/10/18
-- test return values
--

package.path="www/lib/?.lua"

require( 'template' )
local h = require( 'html' )

render = function()
  local output = h.Document{
    lang = "en",
    H_head( "Viessmann API Test version 2" ),

    H_body{
      "body text goes here v4"
    }
  }
  return 'text/html', tostring( output )
end