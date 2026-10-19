--
-- 2026/10/18
-- test viessmann auth
--
package.path="www/lib/?.lua"

local h = require( 'html' )
require( 'template' )

local function show_link()
  local output = h.Document{

    lang = "en",
    H_head( "Viessmann API Test - show link" ),

    H_body{
      "viessmann login: ",
      h.a{
        href=get_viessmann_body(),
        --target="_blank",
        "viessmann login"
      }
    }
  }
  return 'text/html', tostring( output )
end

local function process_code( code )
  --local code = url_params[ "code" ]
  local output = h.Document{

    lang = "en",
    H_head( "Viessmann API Test - process code" ),
    H_body{
      "code: ", code
    }
  }
  post_authorization( code )
  return 'text/html', tostring( output )
end

local function process_done( done )
  local output = h.Document{

    lang = "en",
    H_head( "Viessmann API Test - process done" ),
    H_body{
      "processing is done:", type( done ), done
    }
  }
  return 'text/html', tostring( output )
end

render = function()
  local done = authorization_done()
  if ( done ) then
    return process_done( done )
  else
    local code = url_params[ "code" ]
    if nil ~= code then
      return process_code( code )
    else
      return show_link()
    end
  end
end
