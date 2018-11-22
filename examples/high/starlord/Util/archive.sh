INSTALL_DIR=./StarLord

rm -rf $INSTALL_DIR

mkdir $INSTALL_DIR

mkdir $INSTALL_DIR/Source
mkdir $INSTALL_DIR/Exec

cp -r ../Source/* $INSTALL_DIR/Source/
cp ../Exec/inputs* $INSTALL_DIR/Exec/
cp ../Exec/probin* $INSTALL_DIR/Exec/
cp ../Exec/helm_table.dat $INSTALL_DIR/Exec/
cp ../Exec/GNUmakefile $INSTALL_DIR/Exec/
sed -i "s/AMREX_HOME ?= \/path\/to\/amrex/AMREX_HOME := ..\/amrex/" $INSTALL_DIR/Exec/GNUmakefile
echo -e "MICROPHYSICS_HOME=../Microphysics\n$(cat $INSTALL_DIR/Exec/GNUmakefile)" > $INSTALL_DIR/Exec/GNUmakefile

git clone https://github.com/AMReX-Codes/amrex.git $INSTALL_DIR/amrex
git clone https://github.com/starkiller-astro/Microphysics.git $INSTALL_DIR/Microphysics

cd $INSTALL_DIR/amrex
git checkout gpu
amrex_hash=$(git rev-parse HEAD)
rm -rf .git
cd -

cd $INSTALL_DIR/Microphysics
git checkout development
microphysics_hash=$(git rev-parse HEAD)
rm -rf .git
cd -

echo "Self-contained CASTRO mini-app" >> $INSTALL_DIR/README
echo "Compile in the Exec/ directory with make" >> $INSTALL_DIR/README

starlord_hash=$(git rev-parse HEAD)

echo "Generated with StarLord commit hash $starlord_hash" >> $INSTALL_DIR/README
echo "Generated with AMReX commit hash $amrex_hash" >> $INSTALL_DIR/README
echo "Generated with Microphysics commit hash $microphysics_hash" >> $INSTALL_DIR/README

tar -czvf starlord.tar.gz $INSTALL_DIR

