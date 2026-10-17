# viessmann
connect to viessmann api for boiler data

```options
# viessmann.cfg
# auth initiation
auth_fqdn = iam.viessmann-climatesolutions.com
auth_port = 443
auth_path = /idp/v3/authorize
# from viessmann agent registration
auth_client_id = string
# local server for general queries
port_mgmt = 8080
# local server listens on this for token
listen_address = 127.0.0.1
listen_port = 4200
# viessmann api url for authentication token
url_api = https://iam.viessmann-climatesolutions.com/idp/v3/authorize
# client redirects to this
url_redirect = http://localhost:4200/auth.lua
# scripts
content_directory = www/content
```