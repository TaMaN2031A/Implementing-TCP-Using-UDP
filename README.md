# Implementing-TCP-Using-UDP
Video Expalnation of the lab idea in general and our approach:
https://www.youtube.com/watch?v=9CkyjgG9bfQ&list=PL8FyCiQBpynmFWSmYCC4IHQhLPS564q03&pp=gAQBiAQB
How to build and run client and server:
Client:

g++ main.cpp -o udp_client .

./udp_client client.in

Server:

g++ main.cpp Formatter.cpp Formatter.h -o udp_server  .

 ./udp_server server.in

Brief Explanation of the code and the analysis part:
https://drive.google.com/file/d/1ctNUJeT3rV9i_RGSVQ0WSyYFVjfrFL8T/view?usp=sharing
"TCP Tahoe graph in report with p = .05 and MSS = 3 is actually not representing Tahow we put a reno graph instead by mistake"
