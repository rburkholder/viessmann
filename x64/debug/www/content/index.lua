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
    H_head( "Viessmann API" ),
    H_body{
      h.a{
        href="/start_auth.lua",
        "start viessmann authorization"
      }
    }
  }
  return 'text/html', tostring( output )
end