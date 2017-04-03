#ifndef __CONFIG_PARSER_H__
#define __CONFIG_PARSER_H__


#include <map>
#include <string>
#include <list>
#include <vector>
#include <iostream>
#include "config.h"

BEGIN_APP_NAMESPACE

class ConfigHandler
{
public:
	virtual void begin() {}
	virtual void end() {}
	virtual void handleString(const std::string &parents, const std::string &key, const std::string &value) {}
	virtual void handleInteger(const std::string &parents, const std::string &key, long int value) {}
	virtual void handleFloat(const std::string &parents, const std::string &key, double value) {}
	virtual void handleStringArray(const std::string &parents, const std::string &key, const std::vector<std::string> &value) {}
	virtual void beginGroup(const std::string &groupKey) {}
	virtual void endGroup(const std::string &groupKey) {}
};

class ConfigParser
{
public:
	enum ParamType
	{
		P_INT,
		P_STR,
		P_STR_LIST,
		P_FLOAT,
		P_BYTE,
		P_SECOND,
		P_VOID
	};
protected:
	struct ParamDef
	{
		ParamType m_type;
		bool m_required;
		bool m_acceptMultiple;
		ParamDef() {}
		inline ParamDef(ParamType type, bool required, bool acceptMultiple)
			: m_type(type), m_required(required), m_acceptMultiple(acceptMultiple)
		{}
	};
	/* Parameters definition. The key is like: <grandparent>.<parent>.<name> */
	std::map<std::string, ParamDef> m_paramDefs;
public:

	/**
	* @brief Add a parameter definition
	*
	* @param key 				name of the parameter. Nested parameters must
	* 							be given in the form "parentName.childName"
	* @param type				The type of the parameter value
	* @param required			Is the parameter required?
	* @param acceptMultiple		If true, this parameter can be accepted multiple
	* 							times (if required is true, means that at least one value must
	* 							be given)
	*/
	void addParamDef(const std::string &key, ParamType type, bool required, bool acceptMultiple = false);
	bool parse(std::istream &istr, ConfigHandler &handler);
};

END_APP_NAMESPACE

#endif // __CONFIG_PARSER_H__

