实验三运行方式：
> sudo python my_topo.py
mininet> xterm h1 h2 n1 n2
n1> ./nat <path>/example_config1.txt
n2> ./nat <path>/example_config2.txt
h2> python http_server.py
h1> wget http://159.226.39.43:8000
