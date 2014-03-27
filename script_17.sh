#!/bin/bash

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

./waf build
