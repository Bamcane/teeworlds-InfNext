#xgettext --keyword=_ --keyword="_P:1,2" --language=C --from-code=UTF-8 -o ../infclass-translation/infclasspot.po $(find ./src -name \*.cpp -or -name \*.h)

import polib, json, os

def ConvertPo2Json(languageCode, plurals):
	poFileName = "other/translations/"+languageCode+"/infnext.po"
	if os.path.isfile(poFileName):
		jsonFileName = "./data/languages/"+languageCode+".json"

		po = polib.pofile(poFileName)

		f = open(jsonFileName, "w")

		print('{"translation":[', end="\n", file=f)

		for entry in po:
			if entry.msgstr:
				print('\t{', end="\n", file=f)
				print('\t\t"key": '+json.dumps(str(entry.msgid))+',', end="\n", file=f)
				print('\t\t"value": '+json.dumps(str(entry.msgstr))+'', end="\n", file=f)
			elif entry.msgstr_plural.keys():
				print('\t{', end="\n", file=f)
				print('\t\t"key": '+json.dumps(str(entry.msgid_plural))+',', end="\n", file=f)
				for index in sorted(entry.msgstr_plural.keys()):
					if index < len(sorted(entry.msgstr_plural.keys()))-1:
						print('\t\t"'+plurals[index]+'": '+json.dumps(entry.msgstr_plural[index])+',', end="\n", file=f)
					else:
						print('\t\t"'+plurals[index]+'": '+json.dumps(entry.msgstr_plural[index]), end="\n", file=f)
			
			if po.index(entry) < len(po)-1:
				print('\t},', end="\n", file=f) 
			else:
				print('\t}', end="\n", file=f)

		print(']}', end="\n", file=f)

ConvertPo2Json("zh_CN", ["other"])
