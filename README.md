dmm
===

LTE's Distributed Mobility Management solutions implementation in NS3


unfortunately due to some modifications to ns3 source code, my modifications were causing some bugs which I have fixed now.

In the dmm repository you can find two scripts. The first one "script_17.sh" is to be used if you are using ns 3.17, while the second one "script_18-19.sh" is to be used if you are using either ns 3.18 or ns 3.19.

Thus, the steps to be executed to correctly setup ns3 are as follow:

1) configure ns3:
./waf configure --with-openflow="absolute-path-to-openflow-directory"
(please note you don't have to enable examples and tests because due to the modifications to ns3 core they will cause the waf building to crash. If you want to run test or example programs you have to compile them separately and cope/solve with the possible errors yourself)

2) copy the content of "path-to-openflow-sources"/include/ into build/

3) download the appropriate script and store it in ns3 main folder (you should be already positioned here because this folder is the one containing waf)

4) make the script runnable:
e.g. chmod a+x script_17.sh

5) run the script
e.g ./script_17.sh

This will download my code and apply the necessary modifications. It will then build ns3.

...take a cup of coffee in the meanwhile...

The build should conclude successfully. I have tested it with ns 3.17, ns 3.18 and ns 3.19 while ns 3.16 is considered deprecated for my code and is not to be used.

Last thing, for the successive builds you do not have to use my script but simply use ./waf build.

Luca
