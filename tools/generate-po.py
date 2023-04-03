import os
import pathlib

binary = os.getcwd()

print("Generate start! Work dir = "+ binary)

cwd = pathlib.Path(binary + "/src")

list = list(cwd.rglob('*.cpp'))

buffer = ""

for i in list:
    buffer += str(i)
    buffer += " "

os.system("xgettext -o " + binary + "/other/translations/infnext.pot" " -k_ -k_P:1,2 -k_C:1c,2 -k_CP:1c,2,3 -s --from-code=UTF-8 " + buffer)