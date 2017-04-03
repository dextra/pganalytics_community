#include "ConfigParser.h"
#include "debug.h"

BEGIN_APP_NAMESPACE

namespace ConfigParserPrivate
{

static bool is_space(char c)
{
	return (c == ' ' || c == '\t');
}

/**
 * Consume spaces from line[pos], advancing pos to the next non-blank
 * character or the end of string.
 */
static void consume_spaces(std::string &line, size_t &pos)
{
	for ( ; pos < line.size() && is_space(line[pos]); pos++);
}

/**
 * Consume spaces and comments at end of a line.
 * Returns true if really at end of a line, or false if found something.
 */
static bool parse_end_of_line(std::string &line, size_t &pos)
{
	consume_spaces(line, pos);
	if (pos == line.size() || line[pos] == '#')
		return true;
	return false;
}

/**
 * Parse a key value at beginning of line[pos].
 * Return the key or empty string if not pointing to a valid key.
 */
static bool parse_key(std::string &line, size_t &pos, std::string &ret)
{
	size_t i = pos;
	ret = "";
	consume_spaces(line, i);
	for (; i < line.size() && !is_space(line[i]); i++)
	{
		if (i == pos)
		{
			/* Keys must start with [a-zA-Z_] */
			if (!(
						(line[i] >= 'a' && line[i] <= 'z')
						|| (line[i] >= 'A' && line[i] <= 'Z')
						|| (line[i] == '_')
						|| (line[i] == '-')
					)
			   )
			{
				ret = ""; /* Invalid */
				break;
			}
		}
		else
		{
			/* Keys must have [-a-zA-Z_0-9] characters only */
			if (!(
						(line[i] >= 'a' && line[i] <= 'z')
						|| (line[i] >= 'A' && line[i] <= 'Z')
						|| (line[i] >= '0' && line[i] <= '9')
						|| (line[i] == '_')
						|| (line[i] == '-')
					)
			   )
			{
				ret = ""; /* Invalid */
				break;
			}
		}
		/* If we got here, means that the current character is valid, so just add it to `ret` */
		ret += line[i];
	}
	if (!ret.empty())
	{
		/* If we parsed a valid key, advance pos */
		pos = i;
		return true;
	}
	else
	{
		return false;
	}
}

static bool parse_string(std::string &line, size_t &pos, std::string &ret)
{
	size_t i = pos;
	ret = "";
	consume_spaces(line, i);
	if (line[i] != '"')
	{
		/* Does not start with quote, so must be like a key */
		return parse_key(line, pos, ret);
	}
	else
	{
		i++;
	}
	/* It starts with a quote, so parse until the quote is closed */
	for (; i < line.size() && line[i] != '"'; i++)
	{
		if (line[i] == '\\')
		{
			/* It is escaping, check next character */
			i++;
			switch(line[i])
			{
				/* XXX: This could be on another function if we need to use again */
			case 'a':
				ret += '\a';
				break;
			case 'r':
				ret += '\r';
				break;
			case 'n':
				ret += '\n';
				break;
			case 't':
				ret += '\t';
				break;
			case '"':
				ret += '"';
				break;
			}
		}
		else
		{
			ret += line[i];
		}
	}
	if (line[i] == '"')
	{
		/* Advance pos */
		pos = i + 1;
		return true;
	}
	else
	{
		/* It didn't close the quote, so invalid string */
		return false;
	}
}

bool parse_bracket(char bracket, std::string &line, size_t &pos)
{
	size_t i = pos;
	consume_spaces(line, i);
	if (line[i] == bracket)
	{
		i++;
		if (parse_end_of_line(line, i))
		{
			pos = i;
			return true;
		}
	}
	return false;
}
bool parse_open_bracket(std::string &line, size_t &pos)
{
	return parse_bracket('{', line, pos);
}
bool parse_close_bracket(std::string &line, size_t &pos)
{
	return parse_bracket('}', line, pos);
}

} // namespace ConfigParserPrivate

void ConfigParser::addParamDef(const std::string &key, ParamType type, bool required, bool acceptMultiple)
{
	m_paramDefs[key] = ParamDef(type, required, acceptMultiple);
}

bool ConfigParser::parse(std::istream &istr, ConfigHandler &handler)
{
	std::string line;
	std::string curr_prefix;
	size_t line_num;
	handler.begin();
	for (line_num = 1; std::getline(istr, line); line_num++)
	{
		/* Ignore \r at end (let's make Windows users happy) */
		if (line.size() > 0 && line[line.size()-1] == '\r')
		{
			line.resize(line.size() - 1);
		}
		//DMSG("line " << line_num);
		/* Parse the one line */
		std::string key;
		std::string str;
		size_t lastpos = 0;
		std::vector<std::string> values;
		if (ConfigParserPrivate::parse_end_of_line(line, lastpos))
		{
			/* Just an empty line, ignore it */
			continue;
		}
		else if (ConfigParserPrivate::parse_close_bracket(line, lastpos))
		{
			/* It is ending a group */
			size_t dot_pos = 0;
			dot_pos = curr_prefix.find_last_of(".");
			if (dot_pos == std::string::npos && !curr_prefix.empty())
			{
				dot_pos = 0;
			}
			if (dot_pos == std::string::npos)
			{
				std::cerr << "Error at line " << line_num << ": non matching close bracket" << std::endl;
				return false;
			}
			handler.endGroup(curr_prefix);
			//DMSG("Closing from [" << curr_prefix << "] to [" << curr_prefix.substr(0, dot_pos) << "]");
			curr_prefix = curr_prefix.substr(0, dot_pos);
		}
		else
		{
			/* Not ending a group, so it is a config line */
			if (!ConfigParserPrivate::parse_key(line, lastpos, key))
			{
				/* TODO: Proper exception */
				std::cerr << "Error at line " << line_num << ": invalid key" << std::endl;
				return false;
			}
			//DMSG("Got key: [" << curr_prefix << "] [" << key << "]");
			while (ConfigParserPrivate::parse_string(line, lastpos, str))
			{
				//DMSG("Got string: [" << str << "]");
				values.push_back(str);
			}
			/* Got key and values, now let's validate/handle them */
			std::map<std::string, ParamDef>::iterator p = m_paramDefs.find(curr_prefix + (curr_prefix.empty() ? "" : ".") + key);
			if (p == m_paramDefs.end())
			{
				/* TODO: Proper exception */
				std::cerr << "Error at line " << line_num << ": key \"" << (curr_prefix + (curr_prefix.empty() ? "" : ".") + key) << "\" not recognized!" << std::endl;
				return false;
			}
			switch(p->second.m_type)
			{
			case P_STR_LIST:
				handler.handleStringArray(curr_prefix, key, values);
				break;
			case P_STR:
			default: /* TODO: Handle other types */
				if (values.size() != 1)
				{
					/* TODO: Proper exception */
					std::cerr << "Error at line " << line_num << ": expecting 1 parameter " << values.size() << " given!" << std::endl;
					return false;
				}
				handler.handleString(curr_prefix, key, values[0]);
				break;
			}
			/* If it is starting a group, add the key to prefix */
			if (ConfigParserPrivate::parse_open_bracket(line, lastpos))
			{
				if (!curr_prefix.empty())
				{
					curr_prefix.append(".");
				}
				curr_prefix.append(key);
				handler.beginGroup(curr_prefix);
			}
		}
		if (!ConfigParserPrivate::parse_end_of_line(line, lastpos))
		{
			/* TODO: Proper exception */
			std::cerr << "Error at line " << line_num << ": extra after " << (lastpos + 1) << std::endl;
			std::cerr << "     Extra: " << line.substr(lastpos) << std::endl;
			return false;
		}
	}
	handler.end();
	return true;
}

END_APP_NAMESPACE

