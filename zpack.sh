#!/bin/sh

#Correct version number first
VER_MASHED="123"
VER_DOT="1.23"
VER_DASH="1_23"

#Place these two files in current directory
WIN_EXE="zsnesw.exe"
DPMI_EXE="cwsdpmi.exe"

#You need dchroot with 32 bit chroot installed
#You also need upx, advzip, unix2dos



mkdir -p zsnes-${VER_MASHED}/DOS/docs
mkdir -p zsnes-${VER_MASHED}/Win/docs
cp $DPMI_EXE zsnes-${VER_MASHED}/DOS
cp $WIN_EXE zsnes-${VER_MASHED}/Win
cd zsnes-$VER_MASHED
svn export https://svn.bountysource.com/zsnes/trunk zsnes_${VER_DASH}
svn export https://zsnes-docs.svn.sourceforge.net/svnroot/zsnes-docs/trunk/zsnes-docs zdocs
cp zdocs/manpage/zsnes.1 zsnes_${VER_DASH}/src/linux
unix2dos zdocs/text/*.txt
cd zsnes_${VER_DASH}/docs
mkdir readme.htm
mkdir readme.txt
cp "../../zdocs/chm/ZSNES Documentation.chm" readme.chm
cp -r ../../zdocs/html/* readme.htm
cp -r ../../zdocs/text/* readme.txt
cp -r readme.htm readme.txt readme.chm ../../DOS/docs
cp -r readme.htm readme.txt readme.chm ../../Win/docs
cd ../src
./autogen.sh
mv configure ..
make distclean
rm -rf autom4te.cache
mv ../configure .
cd ../..
tar -cf zsnes${VER_MASHED}src.tar zsnes_$VER_DASH
bzip2 --best zsnes${VER_MASHED}src.tar
cd zsnes_${VER_DASH}/src
dchroot -d "make -f makefile.ms PLATFORM=dos-cross RELEASE=yes"
upx --best zsnes.exe
mv zsnes.exe ../../DOS
cd ../../DOS
advzip -4 -a ../zsnes${VER_MASHED}.zip *
cd ../Win
advzip -4 -a ../zsnesw${VER_MASHED}.zip *
cd ..

cd zsnes_${VER_DASH}/src
mkdir i586
cp linux/zsnes.1 linux/zsnes.desktop i586
cp -r icons i586
cp -r ../docs/readme.htm ../docs/readme.txt ../docs/readme.chm i586
cp -r i586 athlon64
cp -r i586 pentium-m
cp -r i586 pentium4
dchroot -d "./configure --enable-release --enable-libao force_arch=i586"
dchroot -d "make"
upx --best zsnes
mv zsnes i586
make cclean
dchroot -d "./configure --enable-release --enable-libao force_arch=athlon64"
dchroot -d "make"
upx --best zsnes
mv zsnes athlon64
make cclean
dchroot -d "./configure --enable-release --enable-libao force_arch=pentium-m"
dchroot -d "make"
upx --best zsnes
mv zsnes pentium-m
make cclean
dchroot -d "./configure --enable-release --enable-libao force_arch=pentium4"
dchroot -d "make"
upx --best zsnes
mv zsnes pentium4

tar -cf ../../zsnes-${VER_DOT}-i586.tar i586
tar -cf ../../zsnes-${VER_DOT}-athlon64.tar athlon64
tar -cf ../../zsnes-${VER_DOT}-pentium-m.tar pentium-m
tar -cf ../../zsnes-${VER_DOT}-pentium4.tar pentium4
cd ../..
gzip -9 zsnes-${VER_DOT}-i586.tar
gzip -9 zsnes-${VER_DOT}-athlon64.tar
gzip -9 zsnes-${VER_DOT}-pentium-m.tar
gzip -9 zsnes-${VER_DOT}-pentium4.tar

