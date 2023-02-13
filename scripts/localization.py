#xgettext --keyword=_ --keyword="_P:1,2" --language=C --from-code=UTF-8 -o ../infclass-translation/infclasspot.po $(find ./src -name \*.cpp -or -name \*.h)

import polib, json, os

def ConvertPo2Json(languageCode):
	poFileName = "other/translations/"+languageCode+"/localize.po"
	if os.path.isfile(poFileName):
		jsonFileName = "data/server_lang/"+languageCode+".json"

		po = polib.pofile(poFileName)

		f = open(jsonFileName, "w")

		print('{"translation":[', end="\n", file=f)

		for entry in po:
			print('\t{', end="\n", file=f)
			print('\t\t"key": "'+str(entry.msgid)+'",', end="\n", file=f)
			print('\t\t"value": "'+str(entry.msgstr)+'"', end="\n", file=f)
			print('\t},', end="\n", file=f)

		print(']}', end="\n", file=f)

ConvertPo2Json("zh-cn")
