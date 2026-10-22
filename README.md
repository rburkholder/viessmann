# viessmann
connect to viessmann api for boiler data

```options
# viessmann.cfg
# scripts
content_directory = www/content
# local server listens on this for token
listen_address = 127.0.0.1
listen_port = 4200
# auth initiation
auth_fqdn = iam.viessmann-climatesolutions.com
auth_port = 443
auth_path = /idp/v3/
# from viessmann agent registration
auth_client_id = \<string\>
# client redirects to this
auth_redirect = http://localhost:4200/auth.lua
# local server for general queries
api_fqdn = api.viessmann-climatesolutions.com
api_port = 443
```