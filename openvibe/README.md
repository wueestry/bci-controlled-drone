modularBCI driver
created by Thiemo Zaugg and Ryan Wüest
in December 2020 ETHZ

Build instructions
    - Download latest OpenViBE source code ("http://openvibe.inria.fr/downloads/") and extract it (data path shouldn't have spaces)
    - Download Visual Studio 2013 Express. As it is no longer on the official Microsoft website and server, offline installer has to be used ("www.hanselman.com/blog/download-visual-studio-express").
    - Extract ModularBCI driver into OpenViBE directory and apply all changes.
    - Click on the cmd file called "install\_dependencies" to install additional files. Alternatively for 64-bit version "install\_dependencies.cmd --platform-target x64"
    -Click on the cmd file called "build" or "build.cmd --platform-target x64" for 64-bit (Can also be read in build intruction on OpenViBE website).


Start up
After building the application, go into the folder and follow the data path "..\dist\extras-Release-x64". 
There command files can be found. One is titled openvibe-acquisiton-server and one is called openvibe-designer. 
In acquisition server choose correct driver ("ModularBCI") and click on connect and then play. Now the designed acquisition can be run in the designer.


IMPORTANT!!

- Daisy Chain untested
- GUI in acqusition server direct copy of the OpenBCI driver -> information shown incorrect or not applicable.
