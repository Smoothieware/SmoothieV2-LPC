HTTP server dual core example using lwIP ethernet stack

Example description
The LWIP HTTP Server example demonstrates the HTTP Server example using LWIP ethernet stack.
The HTTP server can be configured to run on any of the cores (M4 or M0).
The user should connect the board to the network by using the ethernet cable. The board will
get an IP address by using DHCP method. The use can access the HTTP Server by using a
web browser from the host PC.
If the USB Mass storage is compiled in the application (on either M0/M4 core), then it will
read the HTTP contents from USB Mass storage disk & provided to the user.
If the USB Mass storage is not compiled in the application (on either M0/M4 core), then the
default HTTP page will be displayed.
In FreeRTOS/uCOS-III configurations, the net_conn API interface will be used.
In stand-alone configuration, HTTPD interface will be used.

Special connection requirements
There are no special connection requirements for this example.







