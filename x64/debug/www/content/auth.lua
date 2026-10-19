--
-- 2026/10/18
-- test viessmann auth
--
package.path="www/lib/?.lua"

require( 'template' )
local h = require( 'html' )

local b = get_viessmann_body()

render = function()
  local output = h.Document{
    lang = "en",
    H_head( "Viessmann API Test" ),

    H_body{
      "auth processing goes here: ", h.b{ get_viessmann_body() }, " ", GetTimeStamp(), " ", url_params[ "code" ]
    }
  }
  return 'text/html', tostring( output )
end
