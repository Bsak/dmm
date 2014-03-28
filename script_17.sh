#!/bin/bash

#############################################
# download modified openflow switch support #
# implementation, configure it and build it #
#############################################

git clone https://github.com/lucval/ofsid.git
cd ofsid/
./waf configure
./waf build
var=$(pwd)
cd ../


#############################################
# configure ns-3 enabling openflow support  #
#############################################

./waf distclean
./waf configure --with-openflow=$var ###add here further options if required 
cp -r ofsid/include/openflow/ build/


#############################################
# download modified lte, csma and openflow  #
# modules in order to supporting dmm in ns3 #
#############################################

git clone https://github.com/lucval/dmm.git
cp -r dmm/lte/ src/
cp -r dmm/csma/ src/
rm -rf dmm/

git clone https://github.com/lucval/csma.git
cp csma/ipv4_17.h src/internet/model/ipv4.h
cp csma/ipv4.cc src/internet/model/
rm -rf csma/

git clone https://github.com/lucval/openflow.git
rm -rf src/openflow/
mv openflow/ src/


#############################################
#            finally build ns-3             #
#############################################

./waf build
