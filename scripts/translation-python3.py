# thanks to InfectionDust
import polib, json, os

def ConvertPo2Json(languageCode, plurals):
	poFileName = "other/translations/"+languageCode+"/infnext.po"
	if os.path.isfile(poFileName):
		jsonFileName = "data/languages/"+languageCode+".json"

		po = polib.pofile(poFileName)

		f = open(jsonFileName, "w")

		print('{"translation":[', end="\n", file=f)

		for entry in po:
			if entry.msgstr:
				print('\t{', end="\n", file=f)
				print('\t\t"key": '+json.dumps(str(entry.msgid))+',', end="\n", file=f)
				print('\t\t"value": '+json.dumps(str(entry.msgstr))+'', end="\n", file=f)
				print('\t},', end="\n", file=f)
			elif entry.msgstr_plural.keys():
				print('\t{', end="\n", file=f)
				print('\t\t"key": '+json.dumps(str(entry.msgid_plural))+',', end="\n", file=f)
				for index in sorted(entry.msgstr_plural.keys()):
					print('\t\t"'+plurals[index]+'": '+json.dumps(entry.msgstr_plural[index])+',', end="\n", file=f)
				print('\t},', end="\n", file=f)

		print(']}', end="\n", file=f)

ConvertPo2Json("zh-CN", ["other"])
ConvertPo2Json("zh-CL", ["other"])
