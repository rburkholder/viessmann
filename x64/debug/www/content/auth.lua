--
-- 2026/10/18
-- test viessmann auth
--
package.path="www/lib/?.lua"

--
-- todo: add button to refresh the code
--

local h = require( 'html' )
require( 'template' )

local function process_code( code )
  local output = h.Document{

    lang = "en",
    H_head( "viessmann - process code" ),
    H_body{
      h.div{
        "code: ", code
      },
      h.div{
        h.a{
          href="/",
          "home"
        }
      }

    }
  }
  post_authorization( code )
  return 'text/html', tostring( output )
end

local function failure()
  local output = h.Document{
    lang = "en",
    H_head( "viessmann - failure" ),
    H_body{
      h.div{
        "no code"
      }
    }
  }
  return 'text/html', tostring( output )
end

render = function()
  local code = url_params[ "code" ]
  if nil ~= code then
    return process_code( code )
  else
    failure()
  end
end
