@echo off
git submodule update --init --recursive
cd SFML
cmake -DBUILD_SHARED_LIBS:BOOL="0" -G "Visual Studio 15 2017" -T "v141_xp"
cd ..