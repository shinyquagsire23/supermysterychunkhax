import zlib
import sys

data_2 = open("build/game_header.decomp", "rb").read()
decomp_2 = zlib.compress(data_2)
file_2 = open("build/game_header.comp", "wb")
file_2.write(decomp_2)

