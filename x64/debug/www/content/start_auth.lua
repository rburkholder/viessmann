--
-- 2026/10/22
-- test viessmann auth
--
package.path="www/lib/?.lua"

local h = require( 'html' )
require( 'template' )

local function show_link()
  local output = h.Document{

    lang = "en",
    H_head( "viessmann - show link" ),
    H_body{
      h.div{
        "login: ",
        h.a{
          href=get_viessmann_body(),
          --target="_blank",
          "viessmann"
        }
      },
      h.div{
        h.a{
          href="/",
          "home"
        }
      }
    }
  }
  return 'text/html', tostring( output )
end

local function process_is_done()
  local output = h.Document{

    lang = "en",
    H_head( "viessmann - process done" ),
    H_body{
      h.div{
        "processing is done"
      },
      h.div{
        h.a{
          href="/",
          "home"
        }
      }
    }
  }
  return 'text/html', tostring( output )
end

render = function()
  local required = authz_required()
  --io.print( 'required ' .. required )
  if ( required ) then
    return show_link()
  else
    return process_is_done()
  end
end
