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
port_redirect = 4200
# viessmann api url for authentication token
url_api = http://whatever
# client redirects to this
url_redirect = http://localhost/auth
```