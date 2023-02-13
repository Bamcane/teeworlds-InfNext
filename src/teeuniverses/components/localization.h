#ifndef __SHARED_LOCALIZATION__
#define __SHARED_LOCALIZATION__

/* BEGIN EDIT *********************************************************/
#include <teeuniverses/tl/hashtable.h>
#define CStorage IStorage
/* END EDIT ***********************************************************/

#include <unicode/ucnv.h>
#include <unicode/numfmt.h>
#include <unicode/upluralrules.h>
#include <unicode/tmutfmt.h>

#include <stdarg.h>

struct CLocalizableString
{
	const char* m_pText;
	
	CLocalizableString(const char* pText) :
		m_pText(pText)
	{ }
};

/* BEGIN EDIT *********************************************************/
#define _(TEXT) TEXT
#define _P(TEXT_SINGULAR, TEXT_PLURAL) TEXT_PLURAL
/* END EDIT ***********************************************************/

/* BEGIN EDIT *********************************************************/
class CLocalization
{
private:
	class CStorage* m_pStorage;
	inline class CStorage* Storage() { return m_pStorage; }
/* END EDIT ***********************************************************/
public:

	class CLanguage
	{
	protected:
		class CEntry
		{
		public:
			char* m_apVersions;

			CEntry() : m_apVersions(nullptr) {}

			void Free()
			{
				if (m_apVersions)
				{
					delete[] m_apVersions;
					m_apVersions = nullptr;
				}
			}
		};
		
	protected:
		char m_aName[64];
		char m_aFilename[64];
		char m_aParentFilename[64];
		bool m_Loaded;
		int m_Direction;
		
		hashtable< CEntry, 128 > m_Translations;

	public:
		CLanguage();
		CLanguage(const char* pName, const char* pFilename, const char* pParentFilename);
		~CLanguage();
		
		inline const char* GetParentFilename() const { return m_aParentFilename; }
		inline const char* GetFilename() const { return m_aFilename; }
		inline const char* GetName() const { return m_aName; }
		inline bool IsLoaded() const { return m_Loaded; }
		bool Load(CLocalization* pLocalization, class CStorage* pStorage);
		const char* Localize(const char* pKey) const;
	};
	
	enum
	{
		DIRECTION_LTR=0,
		DIRECTION_RTL,
		NUM_DIRECTIONS,
	};

protected:
	CLanguage* m_pMainLanguage;

public:
	array<CLanguage*> m_pLanguages;
	fixed_string128 m_Cfg_MainLanguage;

protected:
	const char* LocalizeWithDepth(const char* pLanguageCode, const char* pText, int Depth);
public:

	CLocalization(class CStorage* pStorage);

	virtual ~CLocalization();
	
	virtual bool InitConfig(int argc, const char** argv);
	virtual bool Init();

	//localize
	const char* Localize(const char* pLanguageCode, const char* pText);
	
	//format
	void Format_V(dynamic_string& Buffer, const char* pLanguageCode, const char* pText, va_list VarArgs);
	void Format(dynamic_string& Buffer, const char* pLanguageCode, const char* pText, ...);
	//localize, format
	void Format_VL(dynamic_string& Buffer, const char* pLanguageCode, const char* pText, va_list VarArgs);
	void Format_L(dynamic_string& Buffer, const char* pLanguageCode, const char* pText, ...);
};

#endif
