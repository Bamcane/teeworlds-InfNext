import os
files = os.listdir("data/maps")

for file in files:
    if os.path.splitext(file)[1] == '.map':
            arg = "./tools/map_convert_07 " + "data/maps/" + file + " " + "data/maps7/" + file
            os.system(arg)