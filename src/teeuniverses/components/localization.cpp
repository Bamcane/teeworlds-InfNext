

#include "localization.h"

#include <base/math.h>

#include <engine/shared/config.h>
#include <engine/storage.h>
#include <unicode/ushape.h>
#include <unicode/ubidi.h>

#include <engine/shared/json.h>

#include <fstream>

CLocalization::CLanguage::CLanguage() :
	m_Loaded(false),
	m_Direction(CLocalization::DIRECTION_LTR),
	m_pPluralRules(NULL),
	m_pNumberFormater(NULL),
	m_pPercentFormater(NULL),
	m_pTimeUnitFormater(NULL)
{
	m_aName[0] = 0;
	m_aFilename[0] = 0;
	m_aParentFilename[0] = 0;
}

CLocalization::CLanguage::CLanguage(const char* pName, const char* pFilename, const char* pParentFilename) :
	m_Loaded(false),
	m_Direction(CLocalization::DIRECTION_LTR),
	m_pPluralRules(NULL),
	m_pNumberFormater(NULL),
	m_pPercentFormater(NULL)
{
	str_copy(m_aName, pName, sizeof(m_aName));
	str_copy(m_aFilename, pFilename, sizeof(m_aFilename));
	if(pParentFilename)
		str_copy(m_aParentFilename, pParentFilename, sizeof(m_aParentFilename));
	else
		m_aParentFilename[0] = 0;
	UErrorCode Status;
	
	Status = U_ZERO_ERROR;
	m_pNumberFormater = unum_open(UNUM_DECIMAL, NULL, -1, m_aFilename, NULL, &Status);
	if(U_FAILURE(Status))
	{
		if(m_pNumberFormater)
		{
			unum_close(m_pNumberFormater);
			m_pNumberFormater = NULL;
		}
		dbg_msg("Localization", "Can't create number formater for %s (error #%d)", m_aFilename, Status);
	}
	
	Status = U_ZERO_ERROR;
	m_pPercentFormater = unum_open(UNUM_PERCENT, NULL, -1, m_aFilename, NULL, &Status);
	if(U_FAILURE(Status))
	{
		if(m_pPercentFormater)
		{
			unum_close(m_pPercentFormater);
			m_pPercentFormater = NULL;
		}
		dbg_msg("Localization", "Can't create percent formater for %s (error #%d)", m_aFilename, Status);
	}
	
	Status = U_ZERO_ERROR;
	m_pPluralRules = uplrules_openForType(m_aFilename, UPLURAL_TYPE_CARDINAL, &Status);
	if(U_FAILURE(Status))
	{
		if(m_pPluralRules)
		{
			uplrules_close(m_pPluralRules);
			m_pPluralRules = NULL;
		}
		dbg_msg("Localization", "Can't create plural rules for %s (error #%d)", m_aFilename, Status);
	}
	
	//Time unit for second formater
	Status = U_ZERO_ERROR;
	m_pTimeUnitFormater = new icu::TimeUnitFormat(m_aFilename,  UTMUTFMT_ABBREVIATED_STYLE, Status);
	if(U_FAILURE(Status))
	{
		dbg_msg("Localization", "Can't create timeunit formater %s (error #%d)", pFilename, Status);
		delete m_pTimeUnitFormater;
		m_pTimeUnitFormater = NULL;
	}
}

CLocalization::CLanguage::~CLanguage()
{
	hashtable< CEntry, 128 >::iterator Iter = m_Translations.begin();
	while(Iter != m_Translations.end())
	{
		if(Iter.data())
			Iter.data()->Free();
		
		++Iter;
	}
	
	if(m_pNumberFormater)
		unum_close(m_pNumberFormater);
	
	if(m_pPercentFormater)
		unum_close(m_pPercentFormater);
		
	if(m_pPluralRules)
		uplrules_close(m_pPluralRules);
		
	if(m_pTimeUnitFormater)
		delete m_pTimeUnitFormater;
}

bool CLocalization::CLanguage::Load(CLocalization* pLocalization, CStorage* pStorage)
{
	// read file data into buffer
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "languages/%s.json", m_aFilename);
	
	void* pBuf;
	unsigned Length;
	
	if(!pStorage->ReadFile(aBuf, IStorage::TYPE_ALL, &pBuf, &Length))
	{
		dbg_msg("Localization", "Couldn't open %s", aBuf);
		return false;
	}

	json_value *rStart = json_parse( (json_char *) pBuf, Length);
	free(pBuf);
	
	dynamic_string Buffer;
	int StrLength;

	if(rStart && rStart->type == json_array)
	{
		for(unsigned i = 0; i < json_array_length(rStart); ++i)
		{
			const json_value *pCurrent = json_array_get(rStart, i);

			const char* pKey = json_string_get(json_object_get(pCurrent, "key"));
			if(pKey && pKey[0])
			{
				CEntry* pEntry = m_Translations.set(pKey);
				
				const char* pSingular = json_string_get(json_object_get(pCurrent, "value"));
				if(pSingular && pSingular[0])
				{
					StrLength = str_length(pSingular)+1;
					pEntry->m_apVersions[PLURALTYPE_NONE] = new char[StrLength];
					str_copy(pEntry->m_apVersions[PLURALTYPE_NONE], pSingular, StrLength);
				}
				else
				{
					const char* pPlural;
					
					//Zero
					pPlural = json_string_get(json_object_get(pCurrent, "zero"));
					if(pPlural && pPlural[PLURALTYPE_ZERO])
					{
						StrLength = str_length(pPlural)+1;
						pEntry->m_apVersions[PLURALTYPE_ZERO] = new char[StrLength];
						str_copy(pEntry->m_apVersions[PLURALTYPE_ZERO], pPlural, StrLength);
					}
					//One
					pPlural = json_string_get(json_object_get(pCurrent, "one"));
					if(pPlural && pPlural[PLURALTYPE_ONE])
					{
						StrLength = str_length(pPlural)+1;
						pEntry->m_apVersions[PLURALTYPE_ONE] = new char[StrLength];
						str_copy(pEntry->m_apVersions[PLURALTYPE_ONE], pPlural, StrLength);
					}
					//Two
					pPlural = json_string_get(json_object_get(pCurrent, "two"));
					if(pPlural && pPlural[PLURALTYPE_TWO])
					{
						StrLength = str_length(pPlural)+1;
						pEntry->m_apVersions[PLURALTYPE_TWO] = new char[StrLength];
						str_copy(pEntry->m_apVersions[PLURALTYPE_TWO], pPlural, StrLength);
					}
					//Few
					pPlural = json_string_get(json_object_get(pCurrent, "few"));
					if(pPlural && pPlural[PLURALTYPE_FEW])
					{
						StrLength = str_length(pPlural)+1;
						pEntry->m_apVersions[PLURALTYPE_FEW] = new char[StrLength];
						str_copy(pEntry->m_apVersions[PLURALTYPE_FEW], pPlural, StrLength);
					}
					//Many
					pPlural = json_string_get(json_object_get(pCurrent, "many"));
					if(pPlural && pPlural[PLURALTYPE_MANY])
					{
						StrLength = str_length(pPlural)+1;
						pEntry->m_apVersions[PLURALTYPE_MANY] = new char[StrLength];
						str_copy(pEntry->m_apVersions[PLURALTYPE_MANY], pPlural, StrLength);
					}
					//Other
					pPlural = json_string_get(json_object_get(pCurrent, "other"));
					if(pPlural && pPlural[PLURALTYPE_OTHER])
					{
						StrLength = str_length(pPlural)+1;
						pEntry->m_apVersions[PLURALTYPE_OTHER] = new char[StrLength];
						str_copy(pEntry->m_apVersions[PLURALTYPE_OTHER], pPlural, StrLength);
					}
				}
			}
		}
	}
	
	m_Loaded = true;
	
	return true;
}

const char* CLocalization::CLanguage::Localize(const char* pText) const
{	
	const CEntry* pEntry = m_Translations.get(pText);
	if(!pEntry)
		return NULL;
	
	return pEntry->m_apVersions[PLURALTYPE_NONE];
}

const char* CLocalization::CLanguage::Localize_P(int Number, const char* pText) const
{
	const CEntry* pEntry = m_Translations.get(pText);
	if(!pEntry)
		return NULL;
	
	UChar aPluralKeyWord[6];
	UErrorCode Status = U_ZERO_ERROR;
	uplrules_select(m_pPluralRules, static_cast<double>(Number), aPluralKeyWord, 6, &Status);
	
	if(U_FAILURE(Status))
		return NULL;
	
	int PluralCode = PLURALTYPE_NONE;
	
	if(aPluralKeyWord[0] == 0x007A) //z
		PluralCode = PLURALTYPE_ZERO;
	else if(aPluralKeyWord[0] == 0x0074) //t
		PluralCode = PLURALTYPE_TWO;
	else if(aPluralKeyWord[0] == 0x0066) //f
		PluralCode = PLURALTYPE_FEW;
	else if(aPluralKeyWord[0] == 0x006D) //m
		PluralCode = PLURALTYPE_MANY;
	else if(aPluralKeyWord[0] == 0x006F) //o
	{
		if(aPluralKeyWord[1] == 0x0074) //t
			PluralCode = PLURALTYPE_OTHER;
		else if(aPluralKeyWord[1] == 0x006E) //n
			PluralCode = PLURALTYPE_ONE;
	}
	
	return pEntry->m_apVersions[PluralCode];
}

CLocalization::CLocalization(class CStorage* pStorage) :
	m_pStorage(pStorage),
	m_pMainLanguage(NULL),
	m_pUtf8Converter(NULL)
{
	
}

CLocalization::~CLocalization()
{
	for(int i=0; i<m_pLanguages.size(); i++)
		delete m_pLanguages[i];
	
	if(m_pUtf8Converter)
		ucnv_close(m_pUtf8Converter);
}

bool CLocalization::Init()
{
	UErrorCode Status = U_ZERO_ERROR;
	m_pUtf8Converter = ucnv_open("utf8", &Status);
	if(U_FAILURE(Status))
	{
		dbg_msg("Localization", "Couldn't create UTF8/UTF16 convertert");
		return false;
	}
	
	
	void* pBuf;
	unsigned Length;
	
	if(!Storage()->ReadFile("languages/index.json", IStorage::TYPE_ALL, &pBuf, &Length))
	{
		dbg_msg("Localization", "Couldn't open languages/index.json");
		return false;
	}
	
	json_value *rStart = json_parse( (json_char *) pBuf, Length);
	free(pBuf);

	// extract data
	m_pMainLanguage = 0;
	if(rStart && rStart->type == json_array)
	{
		for(unsigned i = 0; i < json_array_length(rStart); ++i)
		{
			const json_value *pCurrent = json_array_get(rStart, i);

			CLanguage*& pLanguage = m_pLanguages.increment();
			pLanguage = new CLanguage(json_string_get(json_object_get(pCurrent, "Name")),
				json_string_get(json_object_get(pCurrent, "File")), 
				json_string_get(json_object_get(pCurrent, "Parent")));
				
			if(json_string_get(json_object_get(pCurrent, "Direction")) && str_comp(json_string_get(json_object_get(pCurrent, "Direction")), "RTL") == 0)
				pLanguage->SetWritingDirection(DIRECTION_RTL);
				
			if(str_comp(g_Config.m_SvDefaultLanguage, pLanguage->GetFilename()) == 0)
			{
				pLanguage->Load(this, Storage());
				
				m_pMainLanguage = pLanguage;
			}
		}
	}
	
	return true;
}
	
void CLocalization::AddListener(IListener* pListener)
{
	m_pListeners.increment() = pListener;
}

void CLocalization::RemoveListener(IListener* pListener)
{
	for(int i=0; i<m_pListeners.size(); i++)
	{
		if(m_pListeners[i] == pListener)
			m_pListeners.remove_index(i);
	}
}

bool CLocalization::PreUpdate()
{
	if(!m_pMainLanguage || str_comp(g_Config.m_SvDefaultLanguage, m_pMainLanguage->GetFilename()))
	{
		CLanguage* pLanguage = 0;
		
		for(int i=0; i<m_pLanguages.size(); i++)
		{
			if(str_comp(g_Config.m_SvDefaultLanguage, m_pMainLanguage->GetFilename()) == 0)
			{
				pLanguage = m_pLanguages[i];
				break;
			}
		}
		
		if(m_pMainLanguage != pLanguage)
		{
			m_pMainLanguage = pLanguage;
			
			for(int i=0; i<m_pListeners.size(); i++)
				m_pListeners[i]->OnLocalizationModified();
		}
	}
	
	return true;
}

const char* CLocalization::LocalizeWithDepth(const char* pLanguageCode, const char* pText, int Depth)
{
	CLanguage* pLanguage = m_pMainLanguage;
	if(pLanguageCode)
	{
		for(int i=0; i<m_pLanguages.size(); i++)
		{
			if(str_comp(m_pLanguages[i]->GetFilename(), pLanguageCode) == 0)
			{
				pLanguage = m_pLanguages[i];
				break;
			}
		}
	}
	
	if(!pLanguage)
		return pText;
	
	if(!pLanguage->IsLoaded())
		pLanguage->Load(this, Storage());
	
	const char* pResult = pLanguage->Localize(pText);
	if(pResult)
		return pResult;
	else if(pLanguage->GetParentFilename()[0] && Depth < 4)
		return LocalizeWithDepth(pLanguage->GetParentFilename(), pText, Depth+1);
	else
		return pText;
}

const char* CLocalization::Localize(const char* pLanguageCode, const char* pText)
{
	return LocalizeWithDepth(pLanguageCode, pText, 0);
}

const char* CLocalization::LocalizeWithDepth_P(const char* pLanguageCode, int Number, const char* pText, int Depth)
{
	CLanguage* pLanguage = m_pMainLanguage;
	if(pLanguageCode)
	{
		for(int i=0; i<m_pLanguages.size(); i++)
		{
			if(str_comp(m_pLanguages[i]->GetFilename(), pLanguageCode) == 0)
			{
				pLanguage = m_pLanguages[i];
				break;
			}
		}
	}
	
	if(!pLanguage)
		return pText;
	
	if(!pLanguage->IsLoaded())
		pLanguage->Load(this, Storage());
	
	const char* pResult = pLanguage->Localize_P(Number, pText);
	if(pResult)
		return pResult;
	else if(pLanguage->GetParentFilename()[0] && Depth < 4)
		return LocalizeWithDepth_P(pLanguage->GetParentFilename(), Number, pText, Depth+1);
	else
		return pText;
}

const char* CLocalization::Localize_P(const char* pLanguageCode, int Number, const char* pText)
{
	return LocalizeWithDepth_P(pLanguageCode, Number, pText, 0);
}

void CLocalization::AppendNumber(dynamic_string& Buffer, int& BufferIter, CLanguage* pLanguage, int Number)
{
	UChar aBufUtf16[128];
	
	UErrorCode Status = U_ZERO_ERROR;
	unum_format(pLanguage->m_pNumberFormater, Number, aBufUtf16, sizeof(aBufUtf16), NULL, &Status);
	if(U_FAILURE(Status))
		BufferIter = Buffer.append_at(BufferIter, "_NUMBER_");
	else
	{
		//Update buffer size
		int SrcLength = u_strlen(aBufUtf16);
		int NeededSize = UCNV_GET_MAX_BYTES_FOR_STRING(SrcLength, ucnv_getMaxCharSize(m_pUtf8Converter));
		
		while(Buffer.maxsize() - BufferIter <= NeededSize)
			Buffer.resize_buffer(Buffer.maxsize()*2);
		
		int Length = ucnv_fromUChars(m_pUtf8Converter, Buffer.buffer()+BufferIter, Buffer.maxsize() - BufferIter, aBufUtf16, SrcLength, &Status);
		if(U_FAILURE(Status))
			BufferIter = Buffer.append_at(BufferIter, "_NUMBER_");
		else
			BufferIter += Length;
	}
}

void CLocalization::AppendPercent(dynamic_string& Buffer, int& BufferIter, CLanguage* pLanguage, double Number)
{
	UChar aBufUtf16[128];
	
	UErrorCode Status = U_ZERO_ERROR;
	unum_formatDouble(pLanguage->m_pPercentFormater, Number, aBufUtf16, sizeof(aBufUtf16), NULL, &Status);
	if(U_FAILURE(Status))
		BufferIter = Buffer.append_at(BufferIter, "_PERCENT_");
	else
	{
		//Update buffer size
		int SrcLength = u_strlen(aBufUtf16);
		int NeededSize = UCNV_GET_MAX_BYTES_FOR_STRING(SrcLength, ucnv_getMaxCharSize(m_pUtf8Converter));
		
		while(Buffer.maxsize() - BufferIter <= NeededSize)
			Buffer.resize_buffer(Buffer.maxsize()*2);
		
		int Length = ucnv_fromUChars(m_pUtf8Converter, Buffer.buffer()+BufferIter, Buffer.maxsize() - BufferIter, aBufUtf16, SrcLength, &Status);
		if(U_FAILURE(Status))
			BufferIter = Buffer.append_at(BufferIter, "_PERCENT_");
		else
			BufferIter += Length;
	}
}

void CLocalization::AppendDuration(dynamic_string& Buffer, int& BufferIter, CLanguage* pLanguage, int Number, icu::TimeUnit::UTimeUnitFields Type)
{
	UErrorCode Status = U_ZERO_ERROR;
	icu::UnicodeString BufUTF16;
	
	icu::TimeUnitAmount* pAmount = new icu::TimeUnitAmount((double) Number, Type, Status);
	icu::Formattable Formattable;
	Formattable.adoptObject(pAmount);
	pLanguage->m_pTimeUnitFormater->format(Formattable, BufUTF16, Status);
	
	if(U_FAILURE(Status))
		BufferIter = Buffer.append_at(BufferIter, "_DURATION_");
	else
	{
		int SrcLength = BufUTF16.length();
		
		int NeededSize = UCNV_GET_MAX_BYTES_FOR_STRING(SrcLength, ucnv_getMaxCharSize(m_pUtf8Converter));
		
		while(Buffer.maxsize() - BufferIter <= NeededSize)
			Buffer.resize_buffer(Buffer.maxsize()*2);
		
		Status = U_ZERO_ERROR;
		int Length = ucnv_fromUChars(m_pUtf8Converter, Buffer.buffer()+BufferIter, Buffer.maxsize() - BufferIter, BufUTF16.getBuffer(), SrcLength, &Status);
		
		if(U_FAILURE(Status))
			BufferIter = Buffer.append_at(BufferIter, "_DURATION_");
		else
			BufferIter += Length;
	}
}

void CLocalization::Format_V(dynamic_string& Buffer, const char* pLanguageCode, const char* pText, va_list VarArgs)
{
	CLanguage* pLanguage = m_pMainLanguage;	
	if(pLanguageCode)
	{
		for(int i=0; i<m_pLanguages.size(); i++)
		{
			if(str_comp(m_pLanguages[i]->GetFilename(), pLanguageCode) == 0)
			{
				pLanguage = m_pLanguages[i];
				break;
			}
		}
	}
	if(!pLanguage)
	{
		Buffer.append(pText);
		return;
	}
	
	const char* pVarArgName = NULL;
	const void* pVarArgValue = NULL;
	
	int Iter = 0;
	int Start = Iter;
	int ParamTypeStart = -1;
	int ParamTypeLength = 0;
	int ParamNameStart = -1;
	int ParamNameLength = 0;
	
	int BufferStart = Buffer.length();
	int BufferIter = BufferStart;
	
	while(pText[Iter])
	{
		if(ParamNameStart >= 0)
		{
			if(pText[Iter] == '}') //End of the macro, try to apply it
			{
				//Try to find an argument with this name
				va_list VarArgsIter;

				//windows
				#if defined(CONF_FAMILY_WINDOWS)
					#define va_copy(d,s) ((d) = (s))
				#endif

				va_copy(VarArgsIter, VarArgs);
				pVarArgName = va_arg(VarArgsIter, const char*);
				while(pVarArgName)
				{
					pVarArgValue = va_arg(VarArgsIter, const void*);
					if(str_comp_num(pText+ParamNameStart, pVarArgName, ParamNameLength) == 0)
					{
						//Get argument type
						if(str_comp_num("str:", pText+ParamTypeStart, 4) == 0)
						{
							BufferIter = Buffer.append_at(BufferIter, (const char*) pVarArgValue);
						}
						else if(str_comp_num("lstr:", pText+ParamTypeStart, 5) == 0)
						{
							BufferIter = Buffer.append_at(BufferIter, Localize(pLanguageCode, (const char*) pVarArgValue));
						}
						else if(str_comp_num("int:", pText+ParamTypeStart, 4) == 0)
						{
							int Number = *((const int*) pVarArgValue);
							AppendNumber(Buffer, BufferIter, pLanguage, Number);
						}
						else if(str_comp_num("percent:", pText+ParamTypeStart, 8) == 0)
						{
							float Number = (*((const float*) pVarArgValue));
							AppendPercent(Buffer, BufferIter, pLanguage, Number);
						}
						else if(str_comp_num("sec:", pText+ParamTypeStart, 4) == 0)
						{
							int Duration = *((const int*) pVarArgValue);
							int Minutes = Duration / 60;
							int Seconds = Duration - Minutes*60;
							if(Minutes > 0)
							{
								AppendDuration(Buffer, BufferIter, pLanguage, Minutes, icu::TimeUnit::UTIMEUNIT_MINUTE);
								if(Seconds > 0)
								{
									BufferIter = Buffer.append_at(BufferIter, ", ");
									AppendDuration(Buffer, BufferIter, pLanguage, Seconds, icu::TimeUnit::UTIMEUNIT_SECOND);
								}
							}
							else
								AppendDuration(Buffer, BufferIter, pLanguage, Seconds, icu::TimeUnit::UTIMEUNIT_SECOND);
						}
						break;
					}
					
					pVarArgName = va_arg(VarArgsIter, const char*);
				}
				va_end(VarArgsIter);
				
				//Close the macro
				Start = Iter+1;
				ParamTypeStart = -1;
				ParamNameStart = -1;
			}
			else
				ParamNameLength++;
		}
		else if(ParamTypeStart >= 0)
		{
			if(pText[Iter] == ':') //End of the type, start of the name
			{
				ParamNameStart = Iter+1;
				ParamNameLength = 0;
			}
			else if(pText[Iter] == '}') //Invalid: no name found
			{
				//Close the macro
				Start = Iter+1;
				ParamTypeStart = -1;
				ParamNameStart = -1;
			}
			else
				ParamTypeLength++;
		}
		else
		{
			if(pText[Iter] == '{')
			{
				//Flush the content of pText in the buffer
				BufferIter = Buffer.append_at_num(BufferIter, pText+Start, Iter-Start);
				Iter++;
				ParamTypeStart = Iter;
			}
		}
		
		Iter = str_utf8_forward(pText, Iter);
	}
	
	if(Iter > 0 && ParamTypeStart == -1 && ParamNameStart == -1)
	{
		BufferIter = Buffer.append_at_num(BufferIter, pText+Start, Iter-Start);
	}
	
	if(pLanguage && pLanguage->GetWritingDirection() == DIRECTION_RTL)
		ArabicShaping(Buffer, BufferStart);
}

void CLocalization::Format(dynamic_string& Buffer, const char* pText, ...)
{
	va_list VarArgs;
	va_start(VarArgs, pText);
	
	// There should be "en" (no localize)
	Format_V(Buffer, "en", pText, VarArgs);
	
	va_end(VarArgs);
}

void CLocalization::Format_VL(dynamic_string& Buffer, const char* pLanguageCode, const char* pText, va_list VarArgs)
{
	const char* pLocalText = Localize(pLanguageCode, pText);
	
	Format_V(Buffer, pLanguageCode, pLocalText, VarArgs);
}

void CLocalization::Format_L(dynamic_string& Buffer, const char* pLanguageCode, const char* pText, ...)
{
	va_list VarArgs;
	va_start(VarArgs, pText);
	
	Format_VL(Buffer, pLanguageCode, pText, VarArgs);
	
	va_end(VarArgs);
}

void CLocalization::Format_VLP(dynamic_string& Buffer, const char* pLanguageCode, int Number, const char* pText, va_list VarArgs)
{
	const char* pLocalText = Localize_P(pLanguageCode, Number, pText);
	
	Format_V(Buffer, pLanguageCode, pLocalText, VarArgs);
}

void CLocalization::Format_LP(dynamic_string& Buffer, const char* pLanguageCode, int Number, const char* pText, ...)
{
	va_list VarArgs;
	va_start(VarArgs, pText);
	
	Format_VLP(Buffer, pLanguageCode, Number, pText, VarArgs);
	
	va_end(VarArgs);
}

void CLocalization::ArabicShaping(dynamic_string& Buffer, int BufferStart)
{
	UErrorCode Status = U_ZERO_ERROR;
	
	int Length = (Buffer.length() - BufferStart + 1);
	int LengthUTF16 = Length*2;
	UChar* pBuf0 = new UChar[LengthUTF16];
	UChar* pBuf1 = new UChar[LengthUTF16];
	
	ucnv_toUChars(m_pUtf8Converter, pBuf0, LengthUTF16, Buffer.buffer() + BufferStart, Length, &Status);
	
	UBiDi* pBiDi = ubidi_openSized(LengthUTF16, 0, &Status);
	ubidi_setPara(pBiDi, pBuf0, -1, UBIDI_DEFAULT_LTR, 0, &Status);
	ubidi_writeReordered(pBiDi, pBuf1, LengthUTF16, UBIDI_DO_MIRRORING, &Status);
	ubidi_close(pBiDi);
	
	u_shapeArabic(
		pBuf1, LengthUTF16,
		pBuf0, LengthUTF16,
			U_SHAPE_LETTERS_SHAPE |
			U_SHAPE_PRESERVE_PRESENTATION |
			U_SHAPE_TASHKEEL_RESIZE |
			U_SHAPE_LENGTH_GROW_SHRINK |
			U_SHAPE_TEXT_DIRECTION_VISUAL_LTR |
			U_SHAPE_LAMALEF_RESIZE,
		&Status
	);
	
	int ShapedLength = u_strlen(pBuf0);
	int NeededSize = UCNV_GET_MAX_BYTES_FOR_STRING(ShapedLength, ucnv_getMaxCharSize(m_pUtf8Converter));
	
	while(Buffer.maxsize() - BufferStart <= NeededSize)
		Buffer.resize_buffer(Buffer.maxsize()*2);
	
	ucnv_fromUChars(m_pUtf8Converter, Buffer.buffer() + BufferStart, Buffer.maxsize() - BufferStart, pBuf0, ShapedLength, &Status);
	
	delete[] pBuf0;
	delete[] pBuf1;
}
