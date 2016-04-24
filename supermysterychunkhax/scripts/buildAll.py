import sys
import os
import itertools
from shutil import copyfile

firmVersions={"NEW":"New3DS", "OLD":"Old3DS"}
regions={"us": "0004000000174600", "eu": "0004000000174400", "jp": "0004000000149b00"}

a=[firmVersions.keys(), regions.keys()]

scriptdir = os.path.dirname(os.path.realpath(__file__))
haxxdir = os.path.abspath(os.path.join(scriptdir, os.pardir))
builddir = os.path.join(haxxdir, "build")
rootdir = os.path.abspath(os.path.join(haxxdir, os.pardir))
installdir = os.path.join(rootdir, "supermysterychunkhax_installer")
romfsdir = os.path.join(installdir, "romfs")

cwd = os.getcwd()
os.chdir(haxxdir)

for v in (list(itertools.product(*a))):
    os.system("make clean")

    if os.system("make FIRM_VERSION="+str(v[0])+" REGION="+str(v[1])) != 0:
        os.chdir(cwd)
        exit(1)

    titledir = os.path.join(romfsdir, regions[v[1]])
    savedir = os.path.join(titledir, firmVersions[v[0]])
    copyfile(os.path.join(builddir, "game_header"), os.path.join(savedir, "game_header"))

os.chdir(cwd)
