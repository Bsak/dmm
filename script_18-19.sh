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

cd src/
rm -rf lte
git clone https://github.com/lucval/lte.git

rm -rf csma
git clone https://github.com/lucval/csma.git

rm -rf openflow
git clone https://github.com/lucval/openflow.git

rm csma/ipv4_17.h
mv csma/ipv4.* internet/model
cd ..


#############################################
#            finally build ns-3             #
#############################################

./waf build
