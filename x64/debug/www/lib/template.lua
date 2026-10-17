local h = require( 'html' )

function H_head( title )
	return h.head{
		h.meta{charset = "UTF-8"},
		h.title{ title },
	}
end

function H_link( url, comment )
  return h.a{
    href=url,
    target="_blank",
    comment
  }
end

function H_body_header()
  return h.div{
  }
end

function H_body_footer()
  return h.div{
    h.span{
      "(C) 2026 One Unified Net Limited"
    }
  }
end

function H_body( content )
  return 	h.body{
    h.div{
      style="height:100.0%;",
      class="classOverallContent",

      H_body_header(),
      content,
      H_body_footer()
    }
    -- if H_body_footer is here, it is presented below the lower edge of the window
  }
end

