dmm
===

LTE's Distributed Mobility Management solutions implementation in NS3


unfortunately due to some modifications to ns-3 source code, my modifications were causing some bugs which I have fixed now.

In the dmm repository you can find two scripts. The first one "script_17.sh" is to be used if you are using ns-3.17, while the second one "script_18-19.sh" is to be used if you are using either ns-3.18 or ns-3.19.
Both scripts provide an easy one-click installation of your ns-3 project with both openflow switch support implementation and dmm code modifications included.

To do so just download the script and store it in ns-3 main folder (i.e. the one where waf is). Make the script runnable (e.g. chmod a+x script_17.sh) and then run it (e.g. ./script.sh).

The script will do everything for you, even build the entire ns-3.

Currently the scripts configure ns-3 to support only openflow. If you want to add any other option to the configure command you have to do it yourself [please note you don't have to enable examples and tests because due to the modifications to ns-3 core they will cause the waf building to crash. If you want to run test or example programs you have to compile them separately and cope/solve with the possible errors yourself]

Versions of ns-3 prior to ns-3.17 are considered deprecated for my code and are not to be used.

Last thing, for the successive builds you do not have to use my script but simply use ./waf build.

Luca
